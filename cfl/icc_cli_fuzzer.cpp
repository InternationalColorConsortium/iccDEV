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

  ssize_t written = write(fd, data, size);
  close(fd);
  if (written != static_cast<ssize_t>(size)) {
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

static int RunChild(const std::vector<std::string> &args) {
  pid_t pid = fork();
  if (pid < 0)
    return 0;

  if (pid == 0) {
    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const std::string &arg : args)
      argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    setenv("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1:abort_on_error=1:allocator_may_return_null=1", 0);
    setenv("UBSAN_OPTIONS", "halt_on_error=1:abort_on_error=1:print_stacktrace=1", 0);
    freopen("/dev/null", "w", stdout);
    execv(argv[0], argv.data());
    _exit(127);
  }

  for (int i = 0; i < kChildTimeoutSeconds * 100; i++) {
    int status = 0;
    pid_t done = waitpid(pid, &status, WNOHANG);
    if (done == pid) {
      if (WIFSIGNALED(status))
        __builtin_trap();
      return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
    }
    usleep(10000);
  }

  kill(pid, SIGKILL);
  waitpid(pid, nullptr, 0);
  __builtin_trap();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!data || size < 2 || size > kMaxInputSize)
    return 0;

  std::string target = ICCDEV_CFL_TARGET;
  std::string tool = ToolPathForTarget(target);
  if (tool.empty() || access(tool.c_str(), X_OK) != 0)
    return 0;

  std::string input;
  if (!WriteInput(data, size, input))
    return 0;

  std::string output;
  std::vector<std::string> args = BuildArgs(target, tool, input, output);
  if (!args.empty())
    (void)RunChild(args);

  unlink(input.c_str());
  if (!output.empty())
    unlink(output.c_str());
  return 0;
}
