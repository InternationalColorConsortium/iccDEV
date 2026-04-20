#include <httpext.h>
#include <windows.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "IccIsapiHttp.h"
#include "IccIsapiSanitize.h"

#include "IccDefs.h"
#include "IccLibXMLVer.h"
#include "IccProfLibVer.h"
#include "IccProfile.h"
#include "IccProfileXml.h"
#include "IccUtil.h"

#ifdef USE_ICCJSON
#include "IccLibJSONVer.h"
#include "IccProfileJson.h"
#endif

using iccIsapi::GetLastErrorString;
using iccIsapi::HtmlEscape;
using iccIsapi::IsMethod;
using iccIsapi::JsonEscape;
using iccIsapi::ReadRequestBody;
using iccIsapi::SanitizeFilename;
using iccIsapi::SanitizeErrorMessage;
using iccIsapi::Send400;
using iccIsapi::Send405;
using iccIsapi::Send500;
using iccIsapi::SendResponse;
using iccIsapi::TruncateForBrowser;
using iccIsapi::QueryHasValue;
using iccIsapi::GetQueryValue;

namespace {

constexpr char kExtensionDescription[] = "iccDEV IIS ISAPI shared-library sample";
constexpr DWORD kToolTimeoutMs = 30000;
constexpr size_t kMaxUploadBytes = 16 * 1024 * 1024;
constexpr size_t kMaxPreviewBytes = 65536;

struct ToolResult {
  std::string name;
  std::string command;
  int exitCode = -1;
  bool ok = false;
  bool skipped = false;
  std::string note;
  std::string output;
  std::string logName;
  uintmax_t logBytes = 0;
  std::string logUrl;
  std::string artifactName;
  uintmax_t artifactBytes = 0;
  std::string artifactUrl;
  std::string artifactPreview;
};

struct ProcessResult {
  bool launched = false;
  bool timedOut = false;
  DWORD exitCode = static_cast<DWORD>(-1);
  std::string output;
  std::filesystem::path logPath;
};

std::filesystem::path GetModuleDirectory()
{
  HMODULE module = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(&GetModuleDirectory),
                          &module)) {
    throw std::runtime_error("Unable to resolve DLL module handle: " + GetLastErrorString(GetLastError()));
  }

  char path[MAX_PATH]{};
  const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
  if (!length) {
    throw std::runtime_error("Unable to resolve DLL directory: " + GetLastErrorString(GetLastError()));
  }

  return std::filesystem::path(path).parent_path();
}

std::filesystem::path CreateTempDirectory()
{
  const std::filesystem::path base = GetModuleDirectory() / "_tool-work";
  std::filesystem::create_directories(base);

  char tempFile[MAX_PATH]{};
  const UINT result = GetTempFileNameA(base.string().c_str(), "iis", 0, tempFile);
  if (!result) {
    throw std::runtime_error("Unable to create temporary workspace: " + GetLastErrorString(GetLastError()));
  }

  std::filesystem::path tempPath(tempFile);
  std::filesystem::remove(tempPath);
  std::filesystem::create_directories(tempPath);
  return tempPath;
}

void WriteBinaryFile(const std::filesystem::path& path, const std::vector<unsigned char>& data)
{
  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Unable to open '" + path.string() + "' for writing.");
  }

  if (!data.empty()) {
    stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  if (!stream) {
    throw std::runtime_error("Unable to write '" + path.string() + "'.");
  }
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Unable to open '" + path.string() + "' for writing.");
  }

  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!stream) {
    throw std::runtime_error("Unable to write '" + path.string() + "'.");
  }
}

std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return std::string();
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::string BuildPublicUrl(const std::filesystem::path& path, bool directory = false)
{
  std::error_code ec;
  const std::filesystem::path relative = std::filesystem::relative(path, GetModuleDirectory(), ec);
  if (ec || relative.empty()) {
    return std::string();
  }

  std::string url = "./";
  url += relative.generic_string();
  if (directory && !url.empty() && url.back() != '/') {
    url.push_back('/');
  }
  return url;
}

ProcessResult RunProcess(const std::filesystem::path& exePath,
                         const std::vector<std::string>& args,
                         const std::filesystem::path& workDir)
{
  ProcessResult result;

  const std::filesystem::path logPath = workDir / (exePath.stem().string() + ".stdout.txt");
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE output = CreateFileA(logPath.string().c_str(),
                              GENERIC_WRITE | GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              &sa,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY,
                              nullptr);
  if (output == INVALID_HANDLE_VALUE) {
    result.output = "Unable to create process log file: " + GetLastErrorString(GetLastError());
    return result;
  }

  std::string command = "\"" + exePath.string() + "\"";
  for (const std::string& arg : args) {
    std::string escaped;
    escaped.reserve(arg.size());
    for (char ch : arg) {
      if (ch == '"') {
        escaped += "\\\"";
      }
      else {
        escaped.push_back(ch);
      }
    }
    command += " \"" + escaped + "\"";
  }

  std::vector<char> commandLine(command.begin(), command.end());
  commandLine.push_back('\0');

  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = output;
  startup.hStdError = output;

  PROCESS_INFORMATION processInfo{};
  result.launched = CreateProcessA(exePath.string().c_str(),
                                   commandLine.data(),
                                   nullptr,
                                   nullptr,
                                   TRUE,
                                   CREATE_NO_WINDOW,
                                   nullptr,
                                   workDir.string().c_str(),
                                   &startup,
                                   &processInfo) != FALSE;

  CloseHandle(output);

  if (!result.launched) {
    result.output = "Unable to launch " + exePath.filename().string() + ": " + GetLastErrorString(GetLastError());
    return result;
  }

  const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, kToolTimeoutMs);
  if (waitResult == WAIT_TIMEOUT) {
    result.timedOut = true;
    TerminateProcess(processInfo.hProcess, 124);
  }

  GetExitCodeProcess(processInfo.hProcess, &result.exitCode);
  CloseHandle(processInfo.hThread);
  CloseHandle(processInfo.hProcess);

  result.logPath = logPath;
  result.output = ReadTextFile(logPath);
  return result;
}

void AttachProcessLog(ToolResult& tool, const ProcessResult& process)
{
  if (!process.logPath.empty() && std::filesystem::exists(process.logPath)) {
    tool.logName = process.logPath.filename().string();
    tool.logBytes = std::filesystem::file_size(process.logPath);
    tool.logUrl = BuildPublicUrl(process.logPath);
  }
}

ToolResult MakeSkippedTool(const std::string& name, const std::string& note)
{
  ToolResult result;
  result.name = name;
  result.skipped = true;
  result.note = note;
  result.output = note;
  return result;
}

std::string BuildToolJson(const std::string& inputKind,
                          const std::string& filename,
                          size_t bytes,
                          const std::string& inputUrl,
                          const std::string& workspaceUrl,
                          const std::vector<ToolResult>& tools)
{
  std::ostringstream json;
  json << "{";
  json << "\"mode\":\"tools\",";
  json << "\"input\":{";
  json << "\"kind\":\"" << JsonEscape(inputKind) << "\",";
  json << "\"filename\":\"" << JsonEscape(filename) << "\",";
  json << "\"bytes\":" << bytes << ",";
  json << "\"url\":\"" << JsonEscape(inputUrl) << "\"";
  json << "},";
  json << "\"workspace_url\":\"" << JsonEscape(workspaceUrl) << "\",";
  json << "\"tools\":[";

  for (size_t i = 0; i < tools.size(); ++i) {
    const ToolResult& tool = tools[i];
    if (i) {
      json << ",";
    }
    json << "{";
    json << "\"name\":\"" << JsonEscape(tool.name) << "\",";
    json << "\"command\":\"" << JsonEscape(tool.command) << "\",";
    json << "\"exit_code\":" << tool.exitCode << ",";
    json << "\"ok\":" << (tool.ok ? "true" : "false") << ",";
    json << "\"skipped\":" << (tool.skipped ? "true" : "false") << ",";
    json << "\"note\":\"" << JsonEscape(tool.note) << "\",";
    json << "\"output\":\"" << JsonEscape(tool.output) << "\",";
    json << "\"log_name\":\"" << JsonEscape(tool.logName) << "\",";
    json << "\"log_bytes\":" << tool.logBytes << ",";
    json << "\"log_url\":\"" << JsonEscape(tool.logUrl) << "\",";
    json << "\"artifact_name\":\"" << JsonEscape(tool.artifactName) << "\",";
    json << "\"artifact_bytes\":" << tool.artifactBytes << ",";
    json << "\"artifact_url\":\"" << JsonEscape(tool.artifactUrl) << "\",";
    json << "\"artifact_preview\":\"" << JsonEscape(tool.artifactPreview) << "\"";
    json << "}";
  }

  json << "]}";
  return json.str();
}

/// Run the core ICC tools (iccDumpProfile, iccToXml, iccFromXml, iccRoundTrip,
/// and optionally iccToJson/iccFromJson) against an uploaded file.  Each tool
/// is executed as a child process with a timeout of kToolTimeoutMs.  Returns a
/// vector of ToolResult with exit codes, captured stdout, log paths, and
/// generated artifact metadata.
std::vector<ToolResult> RunTopTools(const std::filesystem::path& workspace,
                                    const std::string& inputKind,
                                    const std::string& filename,
                                    const std::vector<unsigned char>& body)
{
  const std::filesystem::path binDir = GetModuleDirectory();
  const std::filesystem::path dumpExe = binDir / "iccDumpProfile.exe";
  const std::filesystem::path toXmlExe = binDir / "iccToXml.exe";
  const std::filesystem::path fromXmlExe = binDir / "iccFromXml.exe";
  const std::filesystem::path roundTripExe = binDir / "iccRoundTrip.exe";
  const std::filesystem::path toJsonExe = binDir / "iccToJson.exe";
  const std::filesystem::path fromJsonExe = binDir / "iccFromJson.exe";

  const bool inputIsXml = inputKind == "xml";
  const std::string safeFilename = SanitizeFilename(filename,
                                                    inputIsXml ? "upload.xml" : "upload.icc");
  const std::filesystem::path uploadedPath = workspace / safeFilename;
  WriteBinaryFile(uploadedPath, body);

  std::vector<ToolResult> results;
  const std::filesystem::path generatedXml = workspace / "generated-from-upload.xml";
  const std::filesystem::path generatedIcc = workspace / "generated-from-xml.icc";
  const std::filesystem::path workingIcc = inputIsXml ? generatedIcc : uploadedPath;

  if (!inputIsXml) {
    ToolResult toXml;
    toXml.name = "iccToXml";
    toXml.command = "iccToXml \"" + uploadedPath.filename().string() + "\" \"" + generatedXml.filename().string() + "\"";
    const ProcessResult toXmlProcess = RunProcess(toXmlExe,
                                                  { uploadedPath.string(), generatedXml.string() },
                                                  workspace);
    toXml.exitCode = static_cast<int>(toXmlProcess.exitCode);
    toXml.ok = toXmlProcess.launched && !toXmlProcess.timedOut && toXml.exitCode == 0;
    toXml.note = toXmlProcess.timedOut ? "Timed out while generating XML." : std::string();
    toXml.output = TruncateForBrowser(toXmlProcess.output.empty() ? "No console output." : toXmlProcess.output);
    AttachProcessLog(toXml, toXmlProcess);
    if (std::filesystem::exists(generatedXml)) {
      toXml.artifactName = generatedXml.filename().string();
      toXml.artifactBytes = std::filesystem::file_size(generatedXml);
      toXml.artifactUrl = BuildPublicUrl(generatedXml);
      toXml.artifactPreview = TruncateForBrowser(ReadTextFile(generatedXml));
    }
    results.push_back(toXml);

    if (std::filesystem::exists(generatedXml)) {
      ToolResult fromXml;
      fromXml.name = "iccFromXml";
      fromXml.command = "iccFromXml \"" + generatedXml.filename().string() + "\" \"" + generatedIcc.filename().string() + "\"";
      const ProcessResult fromXmlProcess = RunProcess(fromXmlExe,
                                                      { generatedXml.string(), generatedIcc.string() },
                                                      workspace);
      fromXml.exitCode = static_cast<int>(fromXmlProcess.exitCode);
      fromXml.ok = fromXmlProcess.launched && !fromXmlProcess.timedOut && fromXml.exitCode == 0;
      fromXml.note = fromXmlProcess.timedOut ? "Timed out while regenerating ICC from XML." : std::string();
      fromXml.output = TruncateForBrowser(fromXmlProcess.output.empty() ? "No console output." : fromXmlProcess.output);
      AttachProcessLog(fromXml, fromXmlProcess);
      if (std::filesystem::exists(generatedIcc)) {
        fromXml.artifactName = generatedIcc.filename().string();
        fromXml.artifactBytes = std::filesystem::file_size(generatedIcc);
        fromXml.artifactUrl = BuildPublicUrl(generatedIcc);
      }
      results.push_back(fromXml);
    }
    else {
      results.push_back(MakeSkippedTool("iccFromXml",
                                        "Skipped because iccToXml did not produce an XML file."));
    }

    ToolResult dumpProfile;
    dumpProfile.name = "iccDumpProfile";
    dumpProfile.command = "iccDumpProfile 100 \"" + uploadedPath.filename().string() + "\" ALL";
    const ProcessResult dumpProcess = RunProcess(dumpExe,
                                                 { "100", uploadedPath.string(), "ALL" },
                                                 workspace);
    dumpProfile.exitCode = static_cast<int>(dumpProcess.exitCode);
    dumpProfile.ok = dumpProcess.launched && !dumpProcess.timedOut && dumpProfile.exitCode == 0;
    dumpProfile.note = dumpProcess.timedOut ? "Timed out while dumping the profile." : std::string();
    dumpProfile.output = TruncateForBrowser(dumpProcess.output.empty() ? "No console output." : dumpProcess.output);
    AttachProcessLog(dumpProfile, dumpProcess);
    results.push_back(dumpProfile);

    ToolResult roundTrip;
    roundTrip.name = "iccRoundTrip";
    roundTrip.command = "iccRoundTrip \"" + uploadedPath.filename().string() + "\"";
    const ProcessResult roundTripProcess = RunProcess(roundTripExe,
                                                      { uploadedPath.string() },
                                                      workspace);
    roundTrip.exitCode = static_cast<int>(roundTripProcess.exitCode);
    roundTrip.ok = roundTripProcess.launched && !roundTripProcess.timedOut && roundTrip.exitCode == 0;
    roundTrip.note = roundTripProcess.timedOut ? "Timed out while running round-trip validation." : std::string();
    roundTrip.output = TruncateForBrowser(roundTripProcess.output.empty() ? "No console output." : roundTripProcess.output);
    AttachProcessLog(roundTrip, roundTripProcess);
    results.push_back(roundTrip);

    // -- JSON tools (conditional on binary presence) ----------------------
    if (std::filesystem::exists(toJsonExe)) {
      const std::filesystem::path generatedJson = workspace / "generated-from-upload.json";
      ToolResult toJson;
      toJson.name = "iccToJson";
      toJson.command = "iccToJson \"" + uploadedPath.filename().string() + "\" \"" + generatedJson.filename().string() + "\"";
      const ProcessResult toJsonProcess = RunProcess(toJsonExe,
                                                     { uploadedPath.string(), generatedJson.string() },
                                                     workspace);
      toJson.exitCode = static_cast<int>(toJsonProcess.exitCode);
      toJson.ok = toJsonProcess.launched && !toJsonProcess.timedOut && toJson.exitCode == 0;
      toJson.note = toJsonProcess.timedOut ? "Timed out while generating JSON." : std::string();
      toJson.output = TruncateForBrowser(toJsonProcess.output.empty() ? "No console output." : toJsonProcess.output);
      AttachProcessLog(toJson, toJsonProcess);
      if (std::filesystem::exists(generatedJson)) {
        toJson.artifactName = generatedJson.filename().string();
        toJson.artifactBytes = std::filesystem::file_size(generatedJson);
        toJson.artifactUrl = BuildPublicUrl(generatedJson);
        toJson.artifactPreview = TruncateForBrowser(ReadTextFile(generatedJson));
      }
      results.push_back(toJson);

      if (std::filesystem::exists(generatedJson)) {
        const std::filesystem::path generatedFromJson = workspace / "generated-from-json.icc";
        ToolResult fromJson;
        fromJson.name = "iccFromJson";
        fromJson.command = "iccFromJson \"" + generatedJson.filename().string() + "\" \"" + generatedFromJson.filename().string() + "\"";
        const ProcessResult fromJsonProcess = RunProcess(fromJsonExe,
                                                         { generatedJson.string(), generatedFromJson.string() },
                                                         workspace);
        fromJson.exitCode = static_cast<int>(fromJsonProcess.exitCode);
        fromJson.ok = fromJsonProcess.launched && !fromJsonProcess.timedOut && fromJson.exitCode == 0;
        fromJson.note = fromJsonProcess.timedOut ? "Timed out while regenerating ICC from JSON." : std::string();
        fromJson.output = TruncateForBrowser(fromJsonProcess.output.empty() ? "No console output." : fromJsonProcess.output);
        AttachProcessLog(fromJson, fromJsonProcess);
        if (std::filesystem::exists(generatedFromJson)) {
          fromJson.artifactName = generatedFromJson.filename().string();
          fromJson.artifactBytes = std::filesystem::file_size(generatedFromJson);
          fromJson.artifactUrl = BuildPublicUrl(generatedFromJson);
        }
        results.push_back(fromJson);
      }
      else {
        results.push_back(MakeSkippedTool("iccFromJson",
                                          "Skipped because iccToJson did not produce a JSON file."));
      }
    }
  }
  else {
    ToolResult fromXml;
    fromXml.name = "iccFromXml";
    fromXml.command = "iccFromXml \"" + uploadedPath.filename().string() + "\" \"" + generatedIcc.filename().string() + "\"";
    const ProcessResult fromXmlProcess = RunProcess(fromXmlExe,
                                                    { uploadedPath.string(), generatedIcc.string() },
                                                    workspace);
    fromXml.exitCode = static_cast<int>(fromXmlProcess.exitCode);
    fromXml.ok = fromXmlProcess.launched && !fromXmlProcess.timedOut && fromXml.exitCode == 0;
    fromXml.note = fromXmlProcess.timedOut ? "Timed out while converting XML to ICC." : std::string();
    fromXml.output = TruncateForBrowser(fromXmlProcess.output.empty() ? "No console output." : fromXmlProcess.output);
    AttachProcessLog(fromXml, fromXmlProcess);
    if (std::filesystem::exists(generatedIcc)) {
      fromXml.artifactName = generatedIcc.filename().string();
      fromXml.artifactBytes = std::filesystem::file_size(generatedIcc);
      fromXml.artifactUrl = BuildPublicUrl(generatedIcc);
    }
    results.push_back(fromXml);

    if (!std::filesystem::exists(generatedIcc)) {
      results.push_back(MakeSkippedTool("iccDumpProfile", "Skipped because iccFromXml did not create an ICC file."));
      results.push_back(MakeSkippedTool("iccToXml", "Skipped because iccFromXml did not create an ICC file."));
      results.push_back(MakeSkippedTool("iccRoundTrip", "Skipped because iccFromXml did not create an ICC file."));
      return results;
    }

    ToolResult dumpProfile;
    dumpProfile.name = "iccDumpProfile";
    dumpProfile.command = "iccDumpProfile 100 \"" + generatedIcc.filename().string() + "\" ALL";
    const ProcessResult dumpProcess = RunProcess(dumpExe,
                                                 { "100", generatedIcc.string(), "ALL" },
                                                 workspace);
    dumpProfile.exitCode = static_cast<int>(dumpProcess.exitCode);
    dumpProfile.ok = dumpProcess.launched && !dumpProcess.timedOut && dumpProfile.exitCode == 0;
    dumpProfile.note = dumpProcess.timedOut ? "Timed out while dumping the regenerated profile." : std::string();
    dumpProfile.output = TruncateForBrowser(dumpProcess.output.empty() ? "No console output." : dumpProcess.output);
    AttachProcessLog(dumpProfile, dumpProcess);
    results.push_back(dumpProfile);

    ToolResult toXml;
    toXml.name = "iccToXml";
    toXml.command = "iccToXml \"" + generatedIcc.filename().string() + "\" \"" + generatedXml.filename().string() + "\"";
    const ProcessResult toXmlProcess = RunProcess(toXmlExe,
                                                  { generatedIcc.string(), generatedXml.string() },
                                                  workspace);
    toXml.exitCode = static_cast<int>(toXmlProcess.exitCode);
    toXml.ok = toXmlProcess.launched && !toXmlProcess.timedOut && toXml.exitCode == 0;
    toXml.note = toXmlProcess.timedOut ? "Timed out while regenerating XML." : std::string();
    toXml.output = TruncateForBrowser(toXmlProcess.output.empty() ? "No console output." : toXmlProcess.output);
    AttachProcessLog(toXml, toXmlProcess);
    if (std::filesystem::exists(generatedXml)) {
      toXml.artifactName = generatedXml.filename().string();
      toXml.artifactBytes = std::filesystem::file_size(generatedXml);
      toXml.artifactUrl = BuildPublicUrl(generatedXml);
      toXml.artifactPreview = TruncateForBrowser(ReadTextFile(generatedXml));
    }
    results.push_back(toXml);

    ToolResult roundTrip;
    roundTrip.name = "iccRoundTrip";
    roundTrip.command = "iccRoundTrip \"" + generatedIcc.filename().string() + "\"";
    const ProcessResult roundTripProcess = RunProcess(roundTripExe,
                                                      { generatedIcc.string() },
                                                      workspace);
    roundTrip.exitCode = static_cast<int>(roundTripProcess.exitCode);
    roundTrip.ok = roundTripProcess.launched && !roundTripProcess.timedOut && roundTrip.exitCode == 0;
    roundTrip.note = roundTripProcess.timedOut ? "Timed out while running round-trip validation." : std::string();
    roundTrip.output = TruncateForBrowser(roundTripProcess.output.empty() ? "No console output." : roundTripProcess.output);
    AttachProcessLog(roundTrip, roundTripProcess);
    results.push_back(roundTrip);

    // -- JSON tools (conditional on binary presence) ----------------------
    if (std::filesystem::exists(toJsonExe) && std::filesystem::exists(generatedIcc)) {
      const std::filesystem::path generatedJson = workspace / "generated-from-xml-icc.json";
      ToolResult toJson;
      toJson.name = "iccToJson";
      toJson.command = "iccToJson \"" + generatedIcc.filename().string() + "\" \"" + generatedJson.filename().string() + "\"";
      const ProcessResult toJsonProcess = RunProcess(toJsonExe,
                                                     { generatedIcc.string(), generatedJson.string() },
                                                     workspace);
      toJson.exitCode = static_cast<int>(toJsonProcess.exitCode);
      toJson.ok = toJsonProcess.launched && !toJsonProcess.timedOut && toJson.exitCode == 0;
      toJson.note = toJsonProcess.timedOut ? "Timed out while generating JSON." : std::string();
      toJson.output = TruncateForBrowser(toJsonProcess.output.empty() ? "No console output." : toJsonProcess.output);
      AttachProcessLog(toJson, toJsonProcess);
      if (std::filesystem::exists(generatedJson)) {
        toJson.artifactName = generatedJson.filename().string();
        toJson.artifactBytes = std::filesystem::file_size(generatedJson);
        toJson.artifactUrl = BuildPublicUrl(generatedJson);
        toJson.artifactPreview = TruncateForBrowser(ReadTextFile(generatedJson));
      }
      results.push_back(toJson);
    }
  }

  return results;
}

void WriteWorkspaceIndex(const std::filesystem::path& workspace,
                         const std::string& inputKind,
                         const std::string& safeFilename,
                         size_t bytes,
                         const std::vector<ToolResult>& tools)
{
  const std::filesystem::path uploadedPath = workspace / safeFilename;
  const std::string workspaceUrl = BuildPublicUrl(workspace, true);
  const std::string inputUrl = BuildPublicUrl(uploadedPath);

  std::ostringstream html;
  html << "<!doctype html>\n"
       << "<html lang=\"en\">\n"
       << "<head>\n"
       << "  <meta charset=\"utf-8\">\n"
       << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
       << "  <title>iccDEV IIS Tool Workspace</title>\n"
       << "  <style>\n"
       << "    body{font-family:Segoe UI,Trebuchet MS,sans-serif;margin:0;background:#f5f3ed;color:#1a252d;}\n"
       << "    main{max-width:980px;margin:0 auto;padding:28px 20px 40px;}\n"
       << "    section{background:#fff;border:1px solid rgba(26,37,45,.12);border-radius:18px;padding:20px;margin-top:18px;box-shadow:0 14px 30px rgba(26,37,45,.08);}\n"
       << "    h1,h2{font-family:Georgia,'Times New Roman',serif;}\n"
       << "    dl{display:grid;grid-template-columns:max-content 1fr;gap:8px 14px;}\n"
       << "    dt{font-weight:700;}\n"
       << "    a{color:#0b7f7a;}\n"
       << "    pre{background:#10181d;color:#eef7f7;padding:14px;border-radius:14px;overflow:auto;white-space:pre-wrap;word-break:break-word;}\n"
       << "    .status{display:inline-block;padding:4px 10px;border-radius:999px;font-weight:700;background:#ecebe7;}\n"
       << "    .status.ok{background:#d8f2e4;color:#0f6a43;}\n"
       << "    .status.fail{background:#f7d9d9;color:#9a2f2f;}\n"
       << "    .status.skipped{background:#ebe6db;color:#785d1f;}\n"
       << "  </style>\n"
       << "</head>\n"
       << "<body>\n"
       << "<main>\n"
       << "  <section>\n"
       << "    <h1>iccDEV IIS Tool Workspace</h1>\n"
       << "    <p>This request directory is intentionally left in place so uploads, generated files, and tool logs can be fetched directly over HTTP.</p>\n"
       << "    <dl>\n"
       << "      <dt>Workspace</dt><dd><a href=\"" << HtmlEscape(workspaceUrl) << "\">" << HtmlEscape(workspaceUrl) << "</a></dd>\n"
       << "      <dt>Input kind</dt><dd>" << HtmlEscape(inputKind) << "</dd>\n"
       << "      <dt>Input file</dt><dd><a href=\"" << HtmlEscape(inputUrl) << "\">" << HtmlEscape(safeFilename) << "</a> (" << bytes << " bytes)</dd>\n"
       << "    </dl>\n"
       << "  </section>\n";

  for (const ToolResult& tool : tools) {
    const char* statusClass = tool.skipped ? "skipped" : (tool.ok ? "ok" : "fail");
    const char* statusText = tool.skipped ? "Skipped" : (tool.ok ? "Succeeded" : "Failed");

    html << "  <section>\n"
         << "    <h2>" << HtmlEscape(tool.name) << "</h2>\n"
         << "    <p><span class=\"status " << statusClass << "\">" << statusText << "</span></p>\n"
         << "    <dl>\n"
         << "      <dt>Command</dt><dd><code>" << HtmlEscape(tool.command.empty() ? "n/a" : tool.command) << "</code></dd>\n"
         << "      <dt>Exit code</dt><dd>" << tool.exitCode << "</dd>\n";

    if (!tool.note.empty()) {
      html << "      <dt>Note</dt><dd>" << HtmlEscape(tool.note) << "</dd>\n";
    }
    if (!tool.logUrl.empty()) {
      html << "      <dt>Log</dt><dd><a href=\"" << HtmlEscape(tool.logUrl) << "\">" << HtmlEscape(tool.logName) << "</a> (" << tool.logBytes << " bytes)</dd>\n";
    }
    if (!tool.artifactUrl.empty()) {
      html << "      <dt>Artifact</dt><dd><a href=\"" << HtmlEscape(tool.artifactUrl) << "\">" << HtmlEscape(tool.artifactName) << "</a> (" << tool.artifactBytes << " bytes)</dd>\n";
    }

    html << "    </dl>\n"
         << "    <pre>" << HtmlEscape(tool.output.empty() ? "No console output." : tool.output) << "</pre>\n";

    if (!tool.artifactPreview.empty()) {
      html << "    <h2>Artifact preview</h2>\n"
           << "    <pre>" << HtmlEscape(tool.artifactPreview) << "</pre>\n";
    }

    html << "  </section>\n";
  }

  html << "</main>\n"
       << "</body>\n"
       << "</html>\n";

  WriteTextFile(workspace / "index.html", html.str());
}

void WriteWorkspaceRootIndex(const std::filesystem::path& workspaceRoot)
{
  using WorkspaceEntry = std::pair<std::filesystem::file_time_type, std::string>;
  std::vector<WorkspaceEntry> workspaces;
  std::error_code ec;

  for (std::filesystem::directory_iterator it(workspaceRoot, ec), end; !ec && it != end; it.increment(ec)) {
    const std::filesystem::directory_entry& entry = *it;
    if (!entry.is_directory(ec)) {
      ec.clear();
      continue;
    }

    const std::string name = entry.path().filename().string();
    if (name.empty() || name.front() == '.') {
      continue;
    }

    const std::filesystem::path indexPath = entry.path() / "index.html";
    if (!std::filesystem::exists(indexPath)) {
      continue;
    }

    const auto modified = std::filesystem::last_write_time(entry.path(), ec);
    if (ec) {
      ec.clear();
      continue;
    }

    workspaces.emplace_back(modified, name);
  }

  std::sort(workspaces.begin(),
            workspaces.end(),
            [](const WorkspaceEntry& lhs, const WorkspaceEntry& rhs) {
              if (lhs.first == rhs.first) {
                return lhs.second > rhs.second;
              }
              return lhs.first > rhs.first;
            });

  std::ostringstream html;
  html << "<!doctype html>\n"
       << "<html lang=\"en\">\n"
       << "<head>\n"
       << "  <meta charset=\"utf-8\">\n"
       << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
       << "  <title>iccDEV IIS Tool Workspaces</title>\n"
       << "  <style>\n"
       << "    body{margin:0;font-family:Segoe UI,Trebuchet MS,sans-serif;background:#f6f3eb;color:#18262e;}\n"
       << "    main{max-width:980px;margin:0 auto;padding:28px 20px 40px;}\n"
       << "    section{background:#fff;border:1px solid rgba(24,38,46,.12);border-radius:20px;padding:20px;margin-top:18px;box-shadow:0 14px 30px rgba(24,38,46,.08);}\n"
       << "    h1,h2{font-family:Georgia,'Times New Roman',serif;margin:0 0 12px;}\n"
       << "    p,li{line-height:1.6;color:#4e5d66;}\n"
       << "    ul{margin:0;padding-left:18px;}\n"
       << "    a{color:#0b7f7a;font-weight:700;text-decoration:none;}\n"
       << "    a:hover,a:focus{text-decoration:underline;}\n"
       << "    code{font-family:Consolas,'Courier New',monospace;}\n"
       << "  </style>\n"
       << "</head>\n"
       << "<body>\n"
       << "<main>\n"
       << "  <section>\n"
       << "    <h1>iccDEV IIS Tool Workspaces</h1>\n"
       << "    <p>This directory holds persisted uploads, generated artifacts, and tool logs for <code>POST /iccIisIsapi.dll?mode=tools</code>.</p>\n"
       << "  </section>\n"
       << "  <section>\n"
       << "    <h2>Primary links</h2>\n"
       << "    <ul>\n"
       << "      <li><a href=\"../index.html\">IIS landing page</a></li>\n"
       << "      <li><a href=\"../endpoints.html\">Endpoint Console</a></li>\n"
       << "      <li><a href=\"../integration.html\">Integration Guide</a></li>\n"
       << "      <li><a href=\"../iccIisIsapi.dll\">Summary endpoint</a></li>\n"
       << "      <li><a href=\"../iccIisIsapi.dll?mode=health\">Health endpoint</a></li>\n"
       << "      <li><a href=\"../iccIisIsapi.dll?format=xml\">XML endpoint</a></li>\n"
       << "    </ul>\n"
       << "  </section>\n"
       << "  <section>\n"
       << "    <h2>Recent workspaces</h2>\n";

  if (workspaces.empty()) {
    html << "    <p>No tool jobs have been persisted yet. Use <a href=\"../endpoints.html\">Endpoint Console</a> to upload an ICC profile or XML file.</p>\n";
  }
  else {
    html << "    <ul>\n";
    for (const WorkspaceEntry& workspace : workspaces) {
      html << "      <li><a href=\"./" << HtmlEscape(workspace.second) << "/\">./"
           << HtmlEscape(workspace.second) << "/</a></li>\n";
    }
    html << "    </ul>\n";
  }

  html << "  </section>\n"
       << "</main>\n"
       << "</body>\n"
       << "</html>\n";

  WriteTextFile(workspaceRoot / "index.html", html.str());
}

std::string BuildToolSuiteResponse(LPEXTENSION_CONTROL_BLOCK ecb)
{
  if (!ecb) {
    throw std::runtime_error("Missing extension control block.");
  }

  const std::string inputKind = [&]() {
    const std::string value = GetQueryValue(ecb->lpszQueryString, "input");
    return value == "xml" ? std::string("xml") : std::string("icc");
  }();

  const std::string filename = GetQueryValue(ecb->lpszQueryString, "filename");
  const std::vector<unsigned char> body = ReadRequestBody(ecb, kMaxUploadBytes);
  if (body.empty()) {
    throw std::runtime_error("No upload body or upload exceeds the 16 MB limit.");
  }

  const std::filesystem::path tempDir = CreateTempDirectory();
  try {
    const std::string safeFilename = SanitizeFilename(filename, inputKind == "xml" ? "upload.xml" : "upload.icc");
    const std::vector<ToolResult> tools = RunTopTools(tempDir, inputKind, filename, body);
    WriteWorkspaceIndex(tempDir, inputKind, safeFilename, body.size(), tools);
    return BuildToolJson(inputKind,
                         safeFilename,
                         body.size(),
                         BuildPublicUrl(tempDir / safeFilename),
                         BuildPublicUrl(tempDir, true),
                         tools);
  }
  catch (...) {
    // Clean up workspace on failure to prevent disk exhaustion (CWE-789).
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    throw;
  }
}

std::string BuildPlainTextBody()
{
  CIccProfile profile;
  profile.InitHeader();
  profile.m_Header.colorSpace = icSigRgbData;
  profile.m_Header.pcs = icSigLabData;
  profile.m_Header.deviceClass = icSigDisplayClass;

  CIccInfo info;
  CIccProfileXml xmlProfile;
  xmlProfile.InitHeader();
  xmlProfile.m_Header.colorSpace = icSigRgbData;
  xmlProfile.m_Header.pcs = icSigLabData;
  xmlProfile.m_Header.deviceClass = icSigDisplayClass;

  std::string xml;
  xmlProfile.ToXml(xml);

  std::ostringstream oss;
  oss << "IccProfLib version: " << ICCPROFLIBVER << "\r\n"
      << "IccLibXML version: " << ICCLIBXMLVER << "\r\n"
#ifdef USE_ICCJSON
      << "IccLibJSON version: " << ICCLIBJSONVER << "\r\n"
#endif
      << "Profile spec ver: " << info.GetVersionName(profile.m_Header.version) << "\r\n"
      << "XML payload bytes: " << xml.size() << "\r\n"
      << "Hello from iccDEV IIS ISAPI!\r\n";

  return oss.str();
}

std::string BuildJsonBody()
{
  using iccIsapi::JsonEscape;

  CIccProfile profile;
  profile.InitHeader();
  profile.m_Header.colorSpace = icSigRgbData;
  profile.m_Header.pcs = icSigLabData;
  profile.m_Header.deviceClass = icSigDisplayClass;

  CIccInfo info;
  CIccProfileXml xmlProfile;
  xmlProfile.InitHeader();
  xmlProfile.m_Header.colorSpace = icSigRgbData;
  xmlProfile.m_Header.pcs = icSigLabData;
  xmlProfile.m_Header.deviceClass = icSigDisplayClass;

  std::string xml;
  xmlProfile.ToXml(xml);

  std::ostringstream js;
  js << "{";
  js << "\"iccProfLib_version\":\"" << JsonEscape(ICCPROFLIBVER) << "\",";
  js << "\"iccLibXML_version\":\"" << JsonEscape(ICCLIBXMLVER) << "\",";
#ifdef USE_ICCJSON
  js << "\"iccLibJSON_version\":\"" << JsonEscape(ICCLIBJSONVER) << "\",";
  // Real symbol reference into IccJSON2.dll so MSVC retains the import
  // (issue #823: ci-shared-exports asserts iccIisIsapi.dll depends on
  // IccJSON2*.dll).
  CIccProfileJson jsonProfile;
  jsonProfile.InitHeader();
  jsonProfile.m_Header.colorSpace = icSigRgbData;
  jsonProfile.m_Header.pcs = icSigLabData;
  jsonProfile.m_Header.deviceClass = icSigDisplayClass;
  std::string jsonPayload;
  const bool jsonOk = jsonProfile.ToJson(jsonPayload, 0);
  js << "\"json_payload_bytes\":" << (jsonOk ? jsonPayload.size() : 0u) << ",";
  js << "\"json_status\":\"" << (jsonOk ? "ok" : "empty") << "\",";
#endif
  js << "\"profile_spec_version\":\"" << JsonEscape(info.GetVersionName(profile.m_Header.version)) << "\",";
  js << "\"xml_payload_bytes\":" << xml.size() << ",";
  js << "\"status\":\"ok\"";
  js << "}";
  return js.str();
}

std::string BuildXmlBody()
{
  CIccProfileXml xmlProfile;
  xmlProfile.InitHeader();
  xmlProfile.m_Header.colorSpace = icSigRgbData;
  xmlProfile.m_Header.pcs = icSigLabData;
  xmlProfile.m_Header.deviceClass = icSigDisplayClass;

  std::string xml;
  if (!xmlProfile.ToXml(xml) || xml.empty()) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<iccdev status=\"empty\"/>\n";
  }

  return xml;
}

#ifdef USE_ICCJSON
// Returns the ICC profile serialized as IccLibJSON. Used by the
// `?format=fromjson` GET endpoint to demonstrate a working IccJSON2 round-trip
// (ToJson -> ParseJson) entirely inside the IIS extension. The result is
// also a real symbol reference into IccJSON2.dll (issue #823).
std::string BuildFromJsonBody()
{
  using iccIsapi::JsonEscape;

  CIccProfileJson source;
  source.InitHeader();
  source.m_Header.colorSpace = icSigRgbData;
  source.m_Header.pcs = icSigLabData;
  source.m_Header.deviceClass = icSigDisplayClass;

  std::string jsonOut;
  if (!source.ToJson(jsonOut, 2) || jsonOut.empty()) {
    return std::string("{\"status\":\"empty\"}");
  }

  // Round-trip: parse the JSON we just produced and confirm the header is
  // recoverable. This exercises CIccProfileJson::ParseJson, which is another
  // exported symbol from IccJSON2.dll.
  CIccProfileJson roundTrip;
  std::string parseStatus;
  bool parsedOk = false;
  try {
    IccJson parsedDoc = IccJson::parse(jsonOut);
    parsedOk = roundTrip.ParseJson(parsedDoc, parseStatus);
  }
  catch (const std::exception& ex) {
    parseStatus = ex.what();
    parsedOk = false;
  }

  std::ostringstream js;
  js << "{";
  js << "\"endpoint\":\"fromjson\",";
  js << "\"iccLibJSON_version\":\"" << JsonEscape(ICCLIBJSONVER) << "\",";
  js << "\"json_payload_bytes\":" << jsonOut.size() << ",";
  js << "\"roundtrip_parsed\":" << (parsedOk ? "true" : "false") << ",";
  if (!parseStatus.empty()) {
    js << "\"roundtrip_status\":\"" << JsonEscape(parseStatus) << "\",";
  }
  js << "\"status\":\"ok\"";
  js << "}";
  return js.str();
}
#endif

}  // namespace

BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO* versionInfo)
{
  if (!versionInfo) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  versionInfo->dwExtensionVersion = MAKELONG(HSE_VERSION_MINOR, HSE_VERSION_MAJOR);
  std::strncpy(versionInfo->lpszExtensionDesc,
               kExtensionDescription,
               HSE_MAX_EXT_DLL_NAME_LEN - 1);
  versionInfo->lpszExtensionDesc[HSE_MAX_EXT_DLL_NAME_LEN - 1] = '\0';
  return TRUE;
}

DWORD WINAPI HttpExtensionProc(LPEXTENSION_CONTROL_BLOCK ecb)
{
  if (!ecb) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return HSE_STATUS_ERROR;
  }

  const bool isGet = ecb->lpszMethod && std::strcmp(ecb->lpszMethod, "GET") == 0;
  const bool isPost = ecb->lpszMethod && std::strcmp(ecb->lpszMethod, "POST") == 0;

  try {
    if (QueryHasValue(ecb->lpszQueryString, "mode", "tools")) {
      if (!isPost) {
        return Send405(ecb, "POST", "Tool mode requires HTTP POST.")
          ? HSE_STATUS_SUCCESS
          : HSE_STATUS_ERROR;
      }
      return SendResponse(ecb,
                          "200 OK",
                          "application/json; charset=utf-8",
                          BuildToolSuiteResponse(ecb))
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (!isGet) {
      return Send405(ecb, "GET", "This endpoint accepts GET only.")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (QueryHasValue(ecb->lpszQueryString, "mode", "health")) {
      return SendResponse(ecb, "200 OK", "text/plain; charset=utf-8", "ok\r\n")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (QueryHasValue(ecb->lpszQueryString, "format", "xml")) {
      return SendResponse(ecb, "200 OK", "application/xml; charset=utf-8", BuildXmlBody())
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (QueryHasValue(ecb->lpszQueryString, "format", "json")) {
      return SendResponse(ecb, "200 OK", "application/json; charset=utf-8", BuildJsonBody())
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

#ifdef USE_ICCJSON
    if (QueryHasValue(ecb->lpszQueryString, "format", "fromjson")) {
      return SendResponse(ecb, "200 OK", "application/json; charset=utf-8", BuildFromJsonBody())
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }
#endif

    return SendResponse(ecb, "200 OK", "text/plain; charset=utf-8", BuildPlainTextBody())
      ? HSE_STATUS_SUCCESS
      : HSE_STATUS_ERROR;
  }
  catch (const std::exception& ex) {
    Send400(ecb, SanitizeErrorMessage(ex.what()));
    SetLastError(ERROR_INVALID_DATA);
    return HSE_STATUS_ERROR;
  }
  catch (...) {
    Send500(ecb);
    SetLastError(ERROR_GEN_FAILURE);
    return HSE_STATUS_ERROR;
  }
}

BOOL WINAPI TerminateExtension(DWORD)
{
  return TRUE;
}
