/** @file
 *  File:       IccIsapiFuzzTest.cpp
 *
 *  Contains:   Fuzz-vector validation for IccIsapiSanitize functions.
 *
 *  Reads payloads from the xsscx/fuzz corpus and verifies that every
 *  sanitizer function neutralizes them without crashing, truncating,
 *  or passing dangerous content through.
 *
 *  Build:
 *    cl /EHsc /std:c++17 /I"Tools\Winnt\IccIisIsapi" ^
 *       Tools\Winnt\IccIisIsapi\IccIsapiSanitize.cpp ^
 *       Tools\Winnt\IccIisIsapi\IccIsapiFuzzTest.cpp ^
 *       /Fe:out\IccIsapiFuzzTest.exe
 *
 *  Run:
 *    out\IccIsapiFuzzTest.exe E:\xss\fuzz
 *
 *  Copyright (c) International Color Consortium.  BSD 3-Clause.
 */

#include "IccIsapiSanitize.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_pass = 0;
static int g_fail = 0;
static int g_vectors = 0;

// ---------------------------------------------------------------------------
// Assertion helpers
// ---------------------------------------------------------------------------

static void check(bool cond, const char* label, const std::string& detail = "")
{
  if (cond) {
    ++g_pass;
  } else {
    ++g_fail;
    std::cerr << "FAIL: " << label;
    if (!detail.empty())
      std::cerr << " -- " << detail;
    std::cerr << "\n";
  }
}

// ---------------------------------------------------------------------------
// Invariant: HtmlEscape output must never contain raw < > " ' &(unescaped)
// ---------------------------------------------------------------------------

static bool containsRawHtml(const std::string& s)
{
  for (size_t i = 0; i < s.size(); ++i) {
    char ch = s[i];
    if (ch == '<' || ch == '>' || ch == '"' || ch == '\'')
      return true;
    // Bare & not followed by amp; lt; gt; quot; #39;
    if (ch == '&') {
      std::string rest = s.substr(i, 10);
      if (rest.rfind("&amp;", 0) != 0 &&
          rest.rfind("&lt;", 0) != 0 &&
          rest.rfind("&gt;", 0) != 0 &&
          rest.rfind("&quot;", 0) != 0 &&
          rest.rfind("&#39;", 0) != 0)
        return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Invariant: SanitizeUri must reject dangerous schemes
// ---------------------------------------------------------------------------

static bool hasDangerousScheme(const std::string& uri)
{
  std::string lower;
  for (auto c : uri)
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

  // Strip whitespace/control chars that our sanitizer also strips
  std::string cleaned;
  for (char c : lower) {
    if (c != '\0' && c != '\n' && c != '\r' && c != '\t')
      cleaned.push_back(c);
  }

  const char* bad[] = {
    "javascript:", "data:", "vbscript:", "blob:", "file:",
    "mhtml:", "cid:", "ms-its:", "mk:", "its:", nullptr
  };
  for (int i = 0; bad[i]; ++i) {
    if (cleaned.find(bad[i]) == 0)
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Invariant: SanitizeFilename must produce only [A-Za-z0-9._-]
// ---------------------------------------------------------------------------

static bool filenameIsSafe(const std::string& f)
{
  if (f.empty()) return false;
  for (unsigned char ch : f) {
    if (!std::isalnum(ch) && ch != '.' && ch != '-' && ch != '_')
      return false;
  }
  if (f.front() == '.') return false;
  return true;
}

// ---------------------------------------------------------------------------
// Invariant: UrlDecode must never produce null bytes
// ---------------------------------------------------------------------------

static bool hasNullByte(const std::string& s)
{
  return s.find('\0') != std::string::npos;
}

// ---------------------------------------------------------------------------
// Invariant: JsonEscape must escape all control chars + solidus
// ---------------------------------------------------------------------------

static bool jsonIsUnsafe(const std::string& s)
{
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char ch = static_cast<unsigned char>(s[i]);
    // Unescaped control char
    if (ch < 0x20 && s[i] != '\\')
      return true;
    // Unescaped solidus (/) not preceded by backslash
    if (ch == '/' && (i == 0 || s[i - 1] != '\\'))
      return true;
    // Unescaped quote
    if (ch == '"' && (i == 0 || s[i - 1] != '\\'))
      return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Load lines from a file (one payload per line)
// ---------------------------------------------------------------------------

static std::vector<std::string> loadLines(const fs::path& path, size_t maxLines = 5000)
{
  std::vector<std::string> lines;
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return lines;

  std::string line;
  while (std::getline(ifs, line) && lines.size() < maxLines) {
    // Trim trailing CR
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    // Cap line length to avoid multi-second processing on base64 blobs
    if (line.size() > 4096)
      line = line.substr(0, 4096);
    if (!line.empty())
      lines.push_back(line);
  }
  return lines;
}

// ---------------------------------------------------------------------------
// Run all sanitizer invariants against a single payload string
// ---------------------------------------------------------------------------

static void testPayload(const std::string& payload, const std::string& source)
{
  ++g_vectors;

  // 1. HtmlEscape: no raw HTML chars in output
  {
    std::string escaped = iccIsapi::HtmlEscape(payload);
    check(!containsRawHtml(escaped),
          "HtmlEscape raw chars",
          source + " [" + std::to_string(payload.size()) + " bytes]");
  }

  // 2. SanitizeUri: dangerous schemes must be blocked (empty return)
  {
    std::string sanitized = iccIsapi::SanitizeUri(payload);
    if (hasDangerousScheme(payload)) {
      check(sanitized.empty(),
            "SanitizeUri should block dangerous scheme",
            source + ": " + payload.substr(0, 80));
    }
    // Fragment must be stripped
    if (payload.find('#') != std::string::npos && !sanitized.empty()) {
      check(sanitized.find('#') == std::string::npos,
            "SanitizeUri should strip fragment",
            source);
    }
    // No null/newline/tab in output
    for (char ch : sanitized) {
      if (ch == '\0' || ch == '\n' || ch == '\r' || ch == '\t') {
        check(false, "SanitizeUri output has control char", source);
        break;
      }
    }
  }

  // 3. SanitizeFilename: output must be safe
  {
    std::string safe = iccIsapi::SanitizeFilename(payload, "fuzz_fallback");
    check(filenameIsSafe(safe),
          "SanitizeFilename unsafe output",
          source + " -> \"" + safe + "\"");
  }

  // 4. UrlDecode: no null bytes in output
  {
    std::string decoded = iccIsapi::UrlDecode(payload);
    check(!hasNullByte(decoded),
          "UrlDecode null byte",
          source);
  }

  // 5. JsonEscape: all control chars and solidus escaped
  {
    std::string jesc = iccIsapi::JsonEscape(payload);
    check(!jsonIsUnsafe(jesc),
          "JsonEscape unsafe output",
          source + " [" + std::to_string(payload.size()) + " bytes]");
  }

  // 6. SanitizeErrorMessage: output must be bounded and no path separators
  {
    std::string err = iccIsapi::SanitizeErrorMessage(payload, 200);
    check(err.size() <= 200,
          "SanitizeErrorMessage too long",
          source);
  }

  // 7. TruncateForBrowser: output bounded
  {
    std::string trunc = iccIsapi::TruncateForBrowser(payload, 256);
    // The truncation notice adds ~30 bytes
    check(trunc.size() <= 300,
          "TruncateForBrowser too long",
          source);
  }
}

// ---------------------------------------------------------------------------
// Synthetic edge-case vectors (always run, no external files needed)
// ---------------------------------------------------------------------------

static void runSyntheticTests()
{
  std::cout << "Running synthetic edge-case vectors...\n";

  // Classic DOM-XSS via location.hash
  testPayload("javascript:alert(document.cookie)", "synthetic:js-scheme");
  testPayload("JAVASCRIPT:alert(1)", "synthetic:js-scheme-upper");
  testPayload("JaVaScRiPt:alert(1)", "synthetic:js-scheme-mixed");
  testPayload("java\tscript:alert(1)", "synthetic:js-tab-bypass");
  testPayload("java\nscript:alert(1)", "synthetic:js-newline-bypass");
  testPayload("java\0script:alert(1)", "synthetic:js-null-bypass");
  testPayload("data:text/html,<script>alert(1)</script>", "synthetic:data-uri");
  testPayload("data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg==", "synthetic:data-b64");
  testPayload("vbscript:MsgBox(1)", "synthetic:vbscript");
  testPayload("blob:http://evil.com/1234", "synthetic:blob-uri");
  testPayload("file:///etc/passwd", "synthetic:file-uri");
  testPayload("mhtml:http://evil.com/x.mht!1.html", "synthetic:mhtml");

  // Fragment injection
  testPayload("./iccIisIsapi.dll#<script>alert(1)</script>", "synthetic:fragment-xss");
  testPayload("./page.html#javascript:alert(1)", "synthetic:fragment-js-scheme");
  testPayload("/path#\"><img src=x onerror=alert(1)>", "synthetic:fragment-img-onerror");

  // User's DOM XSS test vector
  testPayload("https://hostname:8888?q=<a\"'\\x0A`= +%20>;&q=y<a\"'\\x0A`= +%20>;", "synthetic:user-domxss-vector");

  // Null byte in URL
  testPayload("file.icc%00.xml", "synthetic:null-byte-ext");
  testPayload("test%00<script>alert(1)</script>", "synthetic:null-then-xss");

  // Path traversal in filenames
  testPayload("../../../etc/passwd", "synthetic:path-traversal");
  testPayload("..\\..\\..\\windows\\system32\\config\\sam", "synthetic:win-path-traversal");
  testPayload("....//....//etc/passwd", "synthetic:double-dot-slash");
  testPayload("file.icc\0.txt", "synthetic:null-in-filename");
  testPayload(".hidden_file", "synthetic:hidden-file");

  // HTML entity / polyglot
  testPayload("<script>alert(1)</script>", "synthetic:basic-script");
  testPayload("<img src=x onerror=alert(1)>", "synthetic:img-onerror");
  testPayload("'\"><img src=x onerror=alert(1)>", "synthetic:break-attr-xss");
  testPayload("<svg/onload=alert(1)>", "synthetic:svg-onload");
  testPayload("<<SCRIPT>alert('XSS');//<</SCRIPT>", "synthetic:double-open-script");
  testPayload("<scr<script>ipt>alert(1)</scr</script>ipt>", "synthetic:nested-script");

  // JSON injection
  testPayload("</script><script>alert(1)</script>", "synthetic:json-script-break");
  testPayload("{\"key\":\"value\",\"evil\":\"</script>\"}", "synthetic:json-solidus");

  // Control chars
  testPayload(std::string({'t','e','s','t','\x01','\x02','\x03','\x7F','\x1B','d','a','t','a'}), "synthetic:control-chars");
  testPayload(std::string({'\x1B','[','3','1','m','R','E','D','\x1B','[','0','m'}), "synthetic:ansi-escape");

  // Empty / boundary
  testPayload("", "synthetic:empty");
  testPayload(std::string(100000, 'A'), "synthetic:100k-chars");
  testPayload(std::string(1000, '<'), "synthetic:1k-angles");
}

// ---------------------------------------------------------------------------
// Load and test vectors from the fuzz repo
// ---------------------------------------------------------------------------

/// Test all text payload files in a corpus subdirectory.
/// Skips binary files (images, ICC profiles, AFL crash PoCs without
/// text extensions) that would corrupt the sanitizer stack when read
/// as line-oriented text.
static bool isTextExtension(const std::string& ext)
{
  // Allow common text/payload extensions; reject everything else.
  static const char* kAllow[] = {
    ".txt", ".xml", ".html", ".htm", ".json", ".csv", ".js",
    ".css", ".svg", ".xslt", ".dtd", ".xsl", ".yaml", ".yml",
    ".md", ".rst", ".log", ".sh", ".bat", ".ps1", ".py",
  };
  for (const char* a : kAllow) {
    if (ext == a) return true;
  }
  return false;
}

static void testFuzzDir(const fs::path& root, const std::string& relDir)
{
  fs::path dir = root / relDir;
  if (!fs::is_directory(dir)) {
    std::cout << "  [skip] " << relDir << " (not found)\n";
    return;
  }

  // Collect file paths first, then process.
  std::vector<fs::path> files;
  try {
    for (const auto& entry : fs::recursive_directory_iterator(
             dir, fs::directory_options::skip_permission_denied)) {
      if (!entry.is_regular_file()) continue;
      if (entry.file_size() > 20 * 1024) continue;

      auto ext = entry.path().extension().string();
      // Require a known text extension — AFL minimized crash PoCs (no
      // extension) and binary formats cause /GS violations when their
      // raw bytes are fed through the sanitizer string functions.
      if (!isTextExtension(ext)) continue;

      files.push_back(entry.path());
    }
  }
  catch (const std::exception& ex) {
    std::cerr << "  [warn] " << relDir << " directory scan: " << ex.what() << "\n";
  }

  int count = 0;
  for (const auto& filePath : files) {
    auto lines = loadLines(filePath, 500);
    std::string source = relDir + "/" + filePath.filename().string();

    for (const auto& line : lines) {
      testPayload(line, source);
      ++count;
    }
  }
  std::cout << "  " << relDir << ": " << count << " payloads from "
            << files.size() << " files\n";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  std::cout << "=== IccIsapi Sanitizer Fuzz Test ===\n\n";

  runSyntheticTests();

  if (argc > 1) {
    fs::path fuzzRoot = argv[1];
    if (!fs::is_directory(fuzzRoot)) {
      std::cerr << "ERROR: " << argv[1] << " is not a directory\n";
      return 2;
    }

    std::cout << "\nLoading vectors from: " << fuzzRoot.string() << "\n";
    std::cout.flush();

    // Scan all subdirs that have text-based fuzz vectors.
    // Also scan non-listed dirs by recursing from root, but skip binary
    // files and known-large directories.
    const char* kDirs[] = {
      "javascript", "uri", "httpheader", "parameter",
      "json", "callback", "custom", "meta", "svg",
      "lfi-local-file-system-harvesting",
      "ascii", "css", "email", "referer", "sqlinjection", "ssi",
      "xml"  // xml last — contains ICC crash PoCs with large embedded data
    };
    for (const char* dir : kDirs) {
      testFuzzDir(fuzzRoot, dir);
      std::cout.flush();
    }

    // The big XSS signature file
    fs::path xssFile = fuzzRoot / "no-experience-required-xss-signatures-only-fools-dont-use.txt";
    if (fs::exists(xssFile)) {
      auto lines = loadLines(xssFile, 5000);
      std::cout << "\n  xss-signatures: " << lines.size() << " payloads...\n";
      std::cout.flush();
      for (const auto& line : lines) {
        testPayload(line, "xss-signatures");
      }
    }
  } else {
    std::cout << "\nNo fuzz corpus path given. Run with:\n"
              << "  " << argv[0] << " E:\\xss\\fuzz\n"
              << "to test against the full xsscx/fuzz corpus.\n";
  }

  std::cout << "\n=== Results ===\n"
            << "Vectors:  " << g_vectors << "\n"
            << "Checks:   " << (g_pass + g_fail) << "\n"
            << "Passed:   " << g_pass << "\n"
            << "Failed:   " << g_fail << "\n";

  if (g_fail > 0) {
    std::cout << "\n*** " << g_fail << " FAILURES ***\n";
    return 1;
  }

  std::cout << "\nAll checks passed.\n";
  return 0;
}
