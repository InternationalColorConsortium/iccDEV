#include <httpext.h>
#include <windows.h>
#include <bcrypt.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
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
using iccIsapi::GetServerVariableString;
using iccIsapi::HtmlEscape;
using iccIsapi::IsMethod;
using iccIsapi::JsonEscape;
using iccIsapi::ReadRequestBody;
using iccIsapi::SanitizeFilename;
using iccIsapi::SanitizeErrorMessage;
using iccIsapi::Send400;
using iccIsapi::Send403;
using iccIsapi::Send405;
using iccIsapi::Send415;
using iccIsapi::Send429;
using iccIsapi::Send500;
using iccIsapi::SendResponse;
using iccIsapi::TruncateForBrowser;
using iccIsapi::QueryHasValue;
using iccIsapi::GetQueryValue;

namespace {

constexpr char kExtensionDescription[] = "iccDEV IIS ISAPI shared-library sample";
constexpr DWORD kToolTimeoutMs = 30000;
constexpr size_t kMaxUploadBytes = 16 * 1024 * 1024;
constexpr size_t kMaxConcurrentToolRequests = 4;
constexpr size_t kMaxPersistedWorkspaces = 100;
constexpr auto kWorkspaceRetention = std::chrono::hours(24);

std::atomic<size_t> gActiveToolRequests{0};

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

void CleanupWorkspaces(const std::filesystem::path& base)
{
  using WorkspaceEntry =
      std::pair<std::filesystem::file_time_type, std::filesystem::path>;
  std::vector<WorkspaceEntry> workspaces;
  const auto cutoff = std::filesystem::file_time_type::clock::now() -
                      kWorkspaceRetention;
  std::error_code ec;

  for (std::filesystem::directory_iterator it(base, ec), end;
       !ec && it != end;
       it.increment(ec)) {
    const std::filesystem::directory_entry& entry = *it;
    if (entry.is_symlink(ec) || !entry.is_directory(ec)) {
      ec.clear();
      continue;
    }

    const std::string name = entry.path().filename().string();
    const bool capabilityName =
        name.size() == 32 &&
        std::all_of(name.begin(), name.end(), [](unsigned char ch) {
          return std::isxdigit(ch) != 0;
        });
    if (!capabilityName) {
      std::filesystem::remove_all(entry.path(), ec);
      ec.clear();
      continue;
    }

    const auto modified = entry.last_write_time(ec);
    if (ec) {
      ec.clear();
      continue;
    }

    if (modified < cutoff) {
      std::filesystem::remove_all(entry.path(), ec);
      ec.clear();
      continue;
    }

    workspaces.emplace_back(modified, entry.path());
  }

  std::sort(workspaces.begin(),
            workspaces.end(),
            [](const WorkspaceEntry& lhs, const WorkspaceEntry& rhs) {
              return lhs.first < rhs.first;
            });

  while (workspaces.size() >= kMaxPersistedWorkspaces) {
    std::filesystem::remove_all(workspaces.front().second, ec);
    ec.clear();
    workspaces.erase(workspaces.begin());
  }
}

std::filesystem::path CreateTempDirectory()
{
  const std::filesystem::path base = GetModuleDirectory() / "_tool-work";
  std::filesystem::create_directories(base);
  CleanupWorkspaces(base);

  constexpr char kHexDigits[] = "0123456789abcdef";
  for (size_t attempt = 0; attempt < 8; attempt++) {
    std::array<unsigned char, 16> randomBytes{};
    const NTSTATUS status = BCryptGenRandom(
        nullptr,
        randomBytes.data(),
        static_cast<ULONG>(randomBytes.size()),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
      throw std::runtime_error("Unable to create a workspace identifier.");
    }

    std::string workspaceName;
    workspaceName.reserve(randomBytes.size() * 2);
    for (const unsigned char value : randomBytes) {
      workspaceName.push_back(kHexDigits[value >> 4]);
      workspaceName.push_back(kHexDigits[value & 0x0f]);
    }

    const std::filesystem::path tempPath = base / workspaceName;
    std::error_code ec;
    if (std::filesystem::create_directory(tempPath, ec)) {
      return tempPath;
    }
    if (ec) {
      throw std::runtime_error("Unable to create a workspace directory.");
    }
  }

  throw std::runtime_error("Unable to allocate a unique workspace identifier.");
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

  std::string url = "/";
  url += relative.generic_string();
  if (directory && !url.empty() && url.back() != '/') {
    url.push_back('/');
  }
  return url;
}

std::string BuildUploadFilename(const std::string& filename,
                                const std::string& inputKind)
{
  const bool isXml = inputKind == "xml";
  const char* fallback = isXml ? "upload.xml" : "upload.icc";
  std::string safe = SanitizeFilename(filename, fallback);
  std::string stem = std::filesystem::path(safe).stem().string();
  if (stem.empty()) {
    stem = "upload";
  }
  return stem + (isXml ? ".xml" : ".icc");
}

bool EqualsCaseInsensitive(std::string lhs, std::string rhs)
{
  const auto lower = [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  };
  std::transform(lhs.begin(), lhs.end(), lhs.begin(), lower);
  std::transform(rhs.begin(), rhs.end(), rhs.begin(), lower);
  return lhs == rhs;
}

bool StartsWithContentType(const std::string& value,
                           const std::string& expected)
{
  const size_t separator = value.find(';');
  std::string mediaType = value.substr(0, separator);
  while (!mediaType.empty() &&
         std::isspace(static_cast<unsigned char>(mediaType.back()))) {
    mediaType.pop_back();
  }
  size_t start = 0;
  while (start < mediaType.size() &&
         std::isspace(static_cast<unsigned char>(mediaType[start]))) {
    ++start;
  }
  return EqualsCaseInsensitive(mediaType.substr(start), expected);
}

bool ContainsDoctype(const std::vector<unsigned char>& body)
{
  static const std::string marker = "<!doctype";
  if (body.size() < marker.size()) {
    return false;
  }

  return std::search(body.begin(),
                     body.end(),
                     marker.begin(),
                     marker.end(),
                     [](unsigned char lhs, char rhs) {
                       return std::tolower(lhs) ==
                              std::tolower(static_cast<unsigned char>(rhs));
                     }) != body.end();
}

class ToolRequestGuard {
public:
  ToolRequestGuard()
  {
    size_t active = gActiveToolRequests.load(std::memory_order_relaxed);
    while (active < kMaxConcurrentToolRequests) {
      if (gActiveToolRequests.compare_exchange_weak(active,
                                                    active + 1,
                                                    std::memory_order_acquire,
                                                    std::memory_order_relaxed)) {
        acquired_ = true;
        break;
      }
    }
  }

  ~ToolRequestGuard()
  {
    if (acquired_) {
      gActiveToolRequests.fetch_sub(1, std::memory_order_release);
    }
  }

  bool Acquired() const
  {
    return acquired_;
  }

private:
  bool acquired_ = false;
};

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

ToolResult RunPawgReport(const std::filesystem::path& pawgExe,
                         const std::filesystem::path& profilePath,
                         const std::filesystem::path& workspace)
{
  if (!std::filesystem::exists(pawgExe)) {
    return MakeSkippedTool("iccPawgReport",
                           "Skipped because iccPawgReport.exe is not installed.");
  }

  ToolResult report;
  report.name = "iccPawgReport";
  report.command = "iccPawgReport \"" + profilePath.filename().string() + "\"";
  const ProcessResult process =
      RunProcess(pawgExe, { profilePath.filename().string() }, workspace);
  report.exitCode = static_cast<int>(process.exitCode);
  report.ok = process.launched && !process.timedOut &&
              (report.exitCode == 0 || report.exitCode == 1);
  if (process.timedOut) {
    report.note = "Timed out while assessing the profile.";
  }
  else if (process.launched && report.exitCode == 1) {
    report.note = "Assessment completed with one or more PAWG findings.";
  }
  report.output = TruncateForBrowser(process.output.empty()
                                       ? "No report output."
                                       : process.output);
  AttachProcessLog(report, process);

  if (process.launched && !process.output.empty()) {
    const std::filesystem::path reportPath = workspace / "icc-pawg-report.txt";
    WriteTextFile(reportPath, process.output);
    report.artifactName = reportPath.filename().string();
    report.artifactBytes = std::filesystem::file_size(reportPath);
    report.artifactUrl = BuildPublicUrl(reportPath);
    report.artifactPreview = report.output;
  }

  return report;
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

/// Run the ICC conversion, assessment, dump, and round-trip tools against an
/// uploaded file. Each tool is executed as a child process with a timeout of
/// kToolTimeoutMs. Returns a vector of ToolResult with exit codes, captured
/// stdout, log paths, and generated artifact metadata.
std::vector<ToolResult> RunTopTools(const std::filesystem::path& workspace,
                                    const std::string& inputKind,
                                    const std::string& filename,
                                    const std::vector<unsigned char>& body)
{
  const std::filesystem::path binDir = GetModuleDirectory();
  const std::filesystem::path dumpExe = binDir / "iccDumpProfile.exe";
  const std::filesystem::path toXmlExe = binDir / "iccToXml.exe";
  const std::filesystem::path fromXmlExe = binDir / "iccFromXml.exe";
  const std::filesystem::path pawgExe = binDir / "iccPawgReport.exe";
  const std::filesystem::path roundTripExe = binDir / "iccRoundTrip.exe";
  const std::filesystem::path toJsonExe = binDir / "iccToJson.exe";
  const std::filesystem::path fromJsonExe = binDir / "iccFromJson.exe";

  const bool inputIsXml = inputKind == "xml";
  const std::string safeFilename = BuildUploadFilename(filename, inputKind);
  const std::filesystem::path uploadedPath = workspace / safeFilename;
  WriteBinaryFile(uploadedPath, body);

  std::vector<ToolResult> results;
  const std::filesystem::path generatedXml = workspace / "generated-from-upload.xml";
  const std::filesystem::path generatedIcc = workspace / "generated-from-xml.icc";

  if (!inputIsXml) {
    ToolResult toXml;
    toXml.name = "iccToXml";
    toXml.command = "iccToXml \"" + uploadedPath.filename().string() + "\" \"" + generatedXml.filename().string() + "\"";
    const ProcessResult toXmlProcess = RunProcess(toXmlExe,
                                                  { uploadedPath.filename().string(),
                                                    generatedXml.filename().string() },
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
                                                      { generatedXml.filename().string(),
                                                        generatedIcc.filename().string() },
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
                                                 { "100",
                                                   uploadedPath.filename().string(),
                                                   "ALL" },
                                                 workspace);
    dumpProfile.exitCode = static_cast<int>(dumpProcess.exitCode);
    dumpProfile.ok = dumpProcess.launched && !dumpProcess.timedOut && dumpProfile.exitCode == 0;
    dumpProfile.note = dumpProcess.timedOut ? "Timed out while dumping the profile." : std::string();
    dumpProfile.output = TruncateForBrowser(dumpProcess.output.empty() ? "No console output." : dumpProcess.output);
    AttachProcessLog(dumpProfile, dumpProcess);
    results.push_back(dumpProfile);

    results.push_back(RunPawgReport(pawgExe, uploadedPath, workspace));

    ToolResult roundTrip;
    roundTrip.name = "iccRoundTrip";
    roundTrip.command = "iccRoundTrip \"" + uploadedPath.filename().string() + "\"";
    const ProcessResult roundTripProcess = RunProcess(roundTripExe,
                                                      { uploadedPath.filename().string() },
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
                                                     { uploadedPath.filename().string(),
                                                       generatedJson.filename().string() },
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
                                                         { generatedJson.filename().string(),
                                                           generatedFromJson.filename().string() },
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
                                                    { uploadedPath.filename().string(),
                                                      generatedIcc.filename().string() },
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
      results.push_back(MakeSkippedTool("iccPawgReport", "Skipped because iccFromXml did not create an ICC file."));
      results.push_back(MakeSkippedTool("iccToXml", "Skipped because iccFromXml did not create an ICC file."));
      results.push_back(MakeSkippedTool("iccRoundTrip", "Skipped because iccFromXml did not create an ICC file."));
      return results;
    }

    ToolResult dumpProfile;
    dumpProfile.name = "iccDumpProfile";
    dumpProfile.command = "iccDumpProfile 100 \"" + generatedIcc.filename().string() + "\" ALL";
    const ProcessResult dumpProcess = RunProcess(dumpExe,
                                                 { "100",
                                                   generatedIcc.filename().string(),
                                                   "ALL" },
                                                 workspace);
    dumpProfile.exitCode = static_cast<int>(dumpProcess.exitCode);
    dumpProfile.ok = dumpProcess.launched && !dumpProcess.timedOut && dumpProfile.exitCode == 0;
    dumpProfile.note = dumpProcess.timedOut ? "Timed out while dumping the regenerated profile." : std::string();
    dumpProfile.output = TruncateForBrowser(dumpProcess.output.empty() ? "No console output." : dumpProcess.output);
    AttachProcessLog(dumpProfile, dumpProcess);
    results.push_back(dumpProfile);

    results.push_back(RunPawgReport(pawgExe, generatedIcc, workspace));

    ToolResult toXml;
    toXml.name = "iccToXml";
    toXml.command = "iccToXml \"" + generatedIcc.filename().string() + "\" \"" + generatedXml.filename().string() + "\"";
    const ProcessResult toXmlProcess = RunProcess(toXmlExe,
                                                  { generatedIcc.filename().string(),
                                                    generatedXml.filename().string() },
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
                                                      { generatedIcc.filename().string() },
                                                      workspace);
    roundTrip.exitCode = static_cast<int>(roundTripProcess.exitCode);
    roundTrip.ok = roundTripProcess.launched && !roundTripProcess.timedOut && roundTrip.exitCode == 0;
    roundTrip.note = roundTripProcess.timedOut ? "Timed out while running round-trip validation." : std::string();
    roundTrip.output = TruncateForBrowser(roundTripProcess.output.empty() ? "No console output." : roundTripProcess.output);
    AttachProcessLog(roundTrip, roundTripProcess);
    results.push_back(roundTrip);

    // -- JSON tools (conditional on binary presence) ----------------------
    if (std::filesystem::exists(toJsonExe)) {
      const std::filesystem::path generatedJson = workspace / "generated-from-xml-icc.json";
      ToolResult toJson;
      toJson.name = "iccToJson";
      toJson.command = "iccToJson \"" + generatedIcc.filename().string() + "\" \"" + generatedJson.filename().string() + "\"";
      const ProcessResult toJsonProcess = RunProcess(toJsonExe,
                                                     { generatedIcc.filename().string(),
                                                       generatedJson.filename().string() },
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

[[maybe_unused]] void WriteWorkspaceRootIndex(
    const std::filesystem::path& workspaceRoot)
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

  const std::string inputKind = GetQueryValue(ecb->lpszQueryString, "input");
  if (inputKind != "icc" && inputKind != "xml") {
    throw std::runtime_error("The input query parameter must be 'icc' or 'xml'.");
  }

  const std::string filename = GetQueryValue(ecb->lpszQueryString, "filename");
  const std::vector<unsigned char> body = ReadRequestBody(ecb, kMaxUploadBytes);
  if (body.empty()) {
    throw std::runtime_error("No upload body or upload exceeds the 16 MB limit.");
  }
  if (inputKind == "xml" && ContainsDoctype(body)) {
    throw std::runtime_error("DOCTYPE declarations are not accepted for XML uploads.");
  }

  const std::filesystem::path tempDir = CreateTempDirectory();
  try {
    const std::string safeFilename = BuildUploadFilename(filename, inputKind);
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
  oss << "iccDEV IIS service\r\n"
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
#ifdef USE_ICCJSON
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
  const size_t copyLen = std::min(std::strlen(kExtensionDescription),
                                  static_cast<size_t>(HSE_MAX_EXT_DLL_NAME_LEN - 1));
  std::memcpy(versionInfo->lpszExtensionDesc, kExtensionDescription, copyLen);
  versionInfo->lpszExtensionDesc[copyLen] = '\0';
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
    const std::string query =
        ecb->lpszQueryString ? ecb->lpszQueryString : "";
    if (query.find("%00") != std::string::npos) {
      return Send400(ecb, "Invalid request.")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    const std::string mode = GetQueryValue(ecb->lpszQueryString, "mode");
    const std::string format = GetQueryValue(ecb->lpszQueryString, "format");
    if (!mode.empty() && !format.empty()) {
      return Send400(ecb, "Invalid request.")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (mode == "tools") {
      if (!isPost) {
        return Send405(ecb, "POST", "Method not allowed.")
          ? HSE_STATUS_SUCCESS
          : HSE_STATUS_ERROR;
      }

      if (GetServerVariableString(ecb, "HTTP_X_ICCDEV_REQUEST") != "1") {
        return Send403(ecb, "Tool request rejected.")
          ? HSE_STATUS_SUCCESS
          : HSE_STATUS_ERROR;
      }

      const std::string fetchSite =
          GetServerVariableString(ecb, "HTTP_SEC_FETCH_SITE");
      if (EqualsCaseInsensitive(fetchSite, "cross-site")) {
        return Send403(ecb, "Tool request rejected.")
          ? HSE_STATUS_SUCCESS
          : HSE_STATUS_ERROR;
      }

      const std::string inputKind =
          GetQueryValue(ecb->lpszQueryString, "input");
      if (inputKind != "icc" && inputKind != "xml") {
        return Send400(ecb, "Invalid request.")
          ? HSE_STATUS_SUCCESS
          : HSE_STATUS_ERROR;
      }
      const std::string contentType =
          GetServerVariableString(ecb, "CONTENT_TYPE");
      const bool contentTypeAccepted =
          (inputKind == "icc" &&
           StartsWithContentType(contentType, "application/octet-stream")) ||
          (inputKind == "xml" &&
           (StartsWithContentType(contentType, "application/xml") ||
            StartsWithContentType(contentType, "text/xml")));
      if (!contentTypeAccepted) {
        return Send415(ecb, "Unsupported media type.")
          ? HSE_STATUS_SUCCESS
          : HSE_STATUS_ERROR;
      }

      ToolRequestGuard requestGuard;
      if (!requestGuard.Acquired()) {
        return Send429(ecb, "Service busy.")
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
      return Send405(ecb, "GET", "Method not allowed.")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (mode == "health") {
      return SendResponse(ecb, "200 OK", "text/plain; charset=utf-8", "ok\r\n")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (!mode.empty()) {
      return Send400(ecb, "Invalid request.")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (format == "xml") {
      return SendResponse(ecb, "200 OK", "application/xml; charset=utf-8", BuildXmlBody())
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    if (format == "json") {
      return SendResponse(ecb, "200 OK", "application/json; charset=utf-8", BuildJsonBody())
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

#ifdef USE_ICCJSON
    if (format == "fromjson") {
      return SendResponse(ecb, "200 OK", "application/json; charset=utf-8", BuildFromJsonBody())
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }
#endif

    if (!format.empty()) {
      return Send400(ecb, "Invalid request.")
        ? HSE_STATUS_SUCCESS
        : HSE_STATUS_ERROR;
    }

    return SendResponse(ecb, "200 OK", "text/plain; charset=utf-8", BuildPlainTextBody())
      ? HSE_STATUS_SUCCESS
      : HSE_STATUS_ERROR;
  }
  catch (const std::exception&) {
    Send400(ecb, "Request processing failed.");
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
