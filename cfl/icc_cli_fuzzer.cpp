/*
 * Copyright (c) 2026 International Color Consortium.
 *                 All rights reserved.
 *                 https://color.org
 *
 * This source file is licensed under the BSD 3-Clause "New" or "Revised"
 * License used by ICC software projects.
 *
 * CLI-fidelity libFuzzer harness for bounded iccDEV command-line smoke tests.
 */

#include <stddef.h>
#include <stdint.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef ICCDEV_CFL_TARGET
#define ICCDEV_CFL_TARGET "dump"
#endif

static constexpr size_t kMaxInputSize = 1024 * 1024;
static constexpr int kChildTimeoutSeconds = 10;
static constexpr size_t kMaxCapturedStderr = 256 * 1024;

static std::string GetEnvOrDefault(const char *name, const char *fallback) {
  const char *value = getenv(name);
  if (value && value[0])
    return value;
  return fallback;
}

static std::string JoinPath(const std::string &dir, const char *name) {
  if (!dir.empty() && dir[dir.size() - 1] == '/')
    return dir + name;
  return dir + "/" + name;
}

static bool WriteInput(const uint8_t *data, size_t size, std::string &path) {
  std::string tmpdir = GetEnvOrDefault("TMPDIR", "/tmp");
  std::string pattern = JoinPath(tmpdir, "iccdev-cfl-input-XXXXXX");
  std::vector<char> writable(pattern.begin(), pattern.end());
  writable.push_back('\0');

  int fd = mkstemp(writable.data());
  if (fd < 0)
    return false;

  size_t total = 0;
  while (total < size) {
    ssize_t written = write(fd, data + total, size - total);
    if (written < 0) {
      if (errno == EINTR)
        continue;
      close(fd);
      unlink(writable.data());
      return false;
    }
    if (written == 0) {
      close(fd);
      unlink(writable.data());
      return false;
    }
    total += static_cast<size_t>(written);
  }

  if (close(fd) != 0) {
    unlink(writable.data());
    return false;
  }

  path = writable.data();
  return true;
}

static std::string MakeOutputPath(const char *suffix) {
  std::string tmpdir = GetEnvOrDefault("TMPDIR", "/tmp");
  std::string pattern = JoinPath(tmpdir, "iccdev-cfl-output-XXXXXX");
  std::vector<char> writable(pattern.begin(), pattern.end());
  writable.push_back('\0');

  int fd = mkstemp(writable.data());
  if (fd >= 0)
    close(fd);

  std::string path = writable.data();
  unlink(path.c_str());
  path += suffix;
  return path;
}

static std::string ToolPathForTarget(const std::string &target) {
  std::string tool_dir = GetEnvOrDefault("ICCDEV_CFL_TOOL_DIR", "Build/Tools");
  if (target == "dump")
    return JoinPath(tool_dir, "IccDumpProfile/iccDumpProfile");
  if (target == "toxml")
    return JoinPath(tool_dir, "IccToXml/iccToXml");
  if (target == "fromxml")
    return JoinPath(tool_dir, "IccFromXml/iccFromXml");
  if (target == "tojson")
    return JoinPath(tool_dir, "IccToJson/iccToJson");
  if (target == "fromjson")
    return JoinPath(tool_dir, "IccFromJson/iccFromJson");
  if (target == "roundtrip")
    return JoinPath(tool_dir, "IccRoundTrip/iccRoundTrip");
  return "";
}

static std::vector<std::string> BuildArgs(const std::string &target,
                                          const std::string &tool,
                                          const std::string &input,
                                          std::string &output) {
  if (target == "dump")
    return {tool, input, "ALL"};
  if (target == "toxml") {
    output = MakeOutputPath(".xml");
    return {tool, input, output};
  }
  if (target == "fromxml") {
    output = MakeOutputPath(".icc");
    return {tool, input, output};
  }
  if (target == "tojson") {
    output = MakeOutputPath(".json");
    return {tool, input, output};
  }
  if (target == "fromjson") {
    output = MakeOutputPath(".icc");
    return {tool, input, output};
  }
  if (target == "roundtrip")
    return {tool, input, "1", "0"};
  return {};
}

// Open the temp file that the child's stderr is redirected into. On success the
// caller owns both the descriptor and the path.
static bool MakeStderrCapture(std::string &path, int &fd) {
  std::string tmpdir = GetEnvOrDefault("TMPDIR", "/tmp");
  std::string pattern = JoinPath(tmpdir, "iccdev-cfl-stderr-XXXXXX");
  std::vector<char> writable(pattern.begin(), pattern.end());
  writable.push_back('\0');

  fd = mkstemp(writable.data());
  if (fd < 0)
    return false;

  path = writable.data();
  return true;
}

// True when the capture has to be shown even though the child exited normally.
// A sanitizer report can reach stderr without aborting if UBSAN_OPTIONS is
// overridden to recover, and an oversized capture is by definition not the
// short per-input parser message this filter exists to drop.
static bool StderrCaptureHasReport(const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb");
  if (!f)
    return false;

  std::string text;
  char buf[4096];
  size_t n;
  bool oversized = false;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    text.append(buf, n);
    if (text.size() > kMaxCapturedStderr) {
      oversized = true;
      break;
    }
  }
  fclose(f);

  if (oversized)
    return true;

  static const char *const kReportMarkers[] = {
      "runtime error:",
      "ERROR: AddressSanitizer",
      "ERROR: LeakSanitizer",
      "SUMMARY: AddressSanitizer",
      "SUMMARY: UndefinedBehaviorSanitizer",
  };
  for (const char *marker : kReportMarkers) {
    if (text.find(marker) != std::string::npos)
      return true;
  }
  return false;
}

static void StreamStderrCapture(const std::string &path) {
  FILE *f = fopen(path.c_str(), "rb");
  if (!f)
    return;

  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    fwrite(buf, 1, n, stderr);

  fclose(f);
  fflush(stderr);
}

static int RunChild(const std::vector<std::string> &args) {
  // The child's stderr is captured instead of inherited. The fromxml target
  // hands every input straight to libxml2, whose default error handler prints a
  // "Start tag expected, '<' not found" block per file, so for a fuzzer -- where
  // almost no input is well-formed XML -- that is one block per iteration
  // burying the real output. Only stdout was redirected before, which is why the
  // tool's own "Unable to Parse" line was already hidden while libxml2's was
  // not. The capture is replayed whenever it could carry signal, so no sanitizer
  // report is lost; installing a quiet libxml2 error handler in IccXML instead
  // would suppress those diagnostics for every real caller of iccFromXml.
  std::string err_path;
  int err_fd = -1;
  if (!MakeStderrCapture(err_path, err_fd))
    __builtin_trap();

  pid_t pid = fork();
  if (pid < 0) {
    close(err_fd);
    unlink(err_path.c_str());
    __builtin_trap();
  }

  if (pid == 0) {
    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const std::string &arg : args)
      argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    setenv("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1:abort_on_error=1:allocator_may_return_null=1", 0);
    setenv("UBSAN_OPTIONS", "halt_on_error=1:abort_on_error=1:print_stacktrace=1", 0);
    freopen("/dev/null", "w", stdout);
    if (err_fd != STDERR_FILENO) {
      dup2(err_fd, STDERR_FILENO);
      close(err_fd);
    }
    execv(argv[0], argv.data());
    _exit(126);
  }

  close(err_fd);

  bool reaped = false;
  bool crashed = false;
  int exit_code = 0;
  for (int i = 0; i < kChildTimeoutSeconds * 100; i++) {
    int status = 0;
    pid_t done = waitpid(pid, &status, WNOHANG);
    if (done == pid) {
      reaped = true;
      crashed = WIFSIGNALED(status) ||
                (WIFEXITED(status) && WEXITSTATUS(status) == 126);
      exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
      break;
    }
    usleep(10000);
  }

  if (!reaped) {
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    crashed = true;
  }

  // Replay before trapping so the crashing run's diagnostics still reach the
  // log, and on a clean exit only when the capture holds something worth
  // reading. The routine parse-error chatter is dropped on both paths.
  if (crashed || StderrCaptureHasReport(err_path))
    StreamStderrCapture(err_path);
  unlink(err_path.c_str());

  if (crashed)
    __builtin_trap();

  return exit_code;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 2 || size > kMaxInputSize)
    return 0;

  std::string target = ICCDEV_CFL_TARGET;
  std::string tool = ToolPathForTarget(target);
  if (tool.empty() || access(tool.c_str(), X_OK) != 0)
    __builtin_trap();

  std::string input;
  if (!WriteInput(data, size, input))
    __builtin_trap();

  std::string output;
  std::vector<std::string> args = BuildArgs(target, tool, input, output);
  if (args.empty())
    __builtin_trap();

  RunChild(args);

  unlink(input.c_str());
  if (!output.empty())
    unlink(output.c_str());
  return 0;
}
