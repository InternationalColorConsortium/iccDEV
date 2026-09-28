/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// #2692.  icIsSameFile() decides whether an output path names the same file as
// an input, so a tool can refuse before truncating its own input.  It compares
// file IDENTITY -- st_dev and st_ino on POSIX, the volume serial number and file
// index on Windows -- because the spellings that matter all differ as strings:
// "./a" for "a", a hard link, a symlink.
//
// This test is registered for Windows as well as POSIX.  The tools' own
// same-file regression is a bash script, which Windows CTest does not run, so
// without this the Win32 half of icIsSameFile() would never execute in CI.
//
// What is asserted:
//   - the same path, and the same path spelled through "./", are the same file;
//   - a hard link to a file is the same file;
//   - a symlink to a file is the same file, where the platform lets a user
//     create one (Windows needs a privilege, so there a refusal to create the
//     link skips that one case rather than failing);
//   - two distinct files are not, INCLUDING a byte-for-byte copy, which is what
//     proves this is an identity test and not a content test;
//   - a path that does not exist, an empty path and NULL are never an input;
//   - icOutputIsInput() agrees and reports only when they are the same file.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "IccSameFile.h"

namespace fs = std::filesystem;

int g_fail = 0;

static void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "  [FAIL] %s\n", what);
  }
  else {
    std::fprintf(stdout, "  [PASS] %s\n", what);
  }
}

static bool writeFile(const fs::path &p, const char *text)
{
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << text;
  return static_cast<bool>(f);
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <scratch-dir>\n", argv[0]);
    return 2;
  }

  std::error_code ec;
  const fs::path dir = fs::path(argv[1]) / "same-file-identity";
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  if (ec) {
    std::fprintf(stderr, "cannot create %s\n", dir.string().c_str());
    return 1;
  }

  const fs::path a = dir / "a.bin";
  const fs::path b = dir / "b.bin";
  const fs::path copy = dir / "copy-of-a.bin";
  if (!writeFile(a, "same bytes") || !writeFile(b, "other bytes") ||
      !writeFile(copy, "same bytes")) {
    std::fprintf(stderr, "cannot write fixtures under %s\n", dir.string().c_str());
    return 1;
  }

  const std::string sa = a.string(), sb = b.string(), scopy = copy.string();
  const std::string sdot = (dir / "." / "a.bin").string();
  const std::string smissing = (dir / "missing.bin").string();

  std::fprintf(stdout, "=== #2692 icIsSameFile: identity, not spelling ===\n");

  check(icIsSameFile(sa.c_str(), sa.c_str()), "a path is the same file as itself");
  check(icIsSameFile(sdot.c_str(), sa.c_str()), "\"./\" spelling of the same path is the same file");

  const fs::path hard = dir / "hard-link.bin";
  fs::create_hard_link(a, hard, ec);
  check(!ec, "hard link created");
  if (!ec)
    check(icIsSameFile(hard.string().c_str(), sa.c_str()), "a hard link is the same file");

  const fs::path sym = dir / "sym-link.bin";
  fs::create_symlink(a, sym, ec);
  if (ec) {
    std::fprintf(stdout, "  [SKIP] symlink: this platform refused to create one (%s)\n",
                 ec.message().c_str());
  }
  else {
    check(icIsSameFile(sym.string().c_str(), sa.c_str()), "a symlink to a file is the same file");
  }

  check(!icIsSameFile(sa.c_str(), sb.c_str()), "two distinct files are not the same file");
  check(!icIsSameFile(scopy.c_str(), sa.c_str()),
        "a byte-for-byte copy is NOT the same file (identity, not content)");
  check(!icIsSameFile(smissing.c_str(), sa.c_str()), "a path that does not exist is not an input");
  check(!icIsSameFile(sa.c_str(), smissing.c_str()), "an input that does not exist matches nothing");
  check(!icIsSameFile("", sa.c_str()), "an empty path is not an input");
  check(!icIsSameFile(NULL, sa.c_str()), "NULL is not an input");

  check(icOutputIsInput(sdot.c_str(), sa.c_str()), "icOutputIsInput refuses the same file");
  check(!icOutputIsInput(sb.c_str(), sa.c_str()), "icOutputIsInput allows a different file");

  fs::remove_all(dir, ec);

  if (g_fail) {
    std::fprintf(stderr, "[same-file-identity] %d check(s) failed\n", g_fail);
    return 1;
  }
  std::fprintf(stdout, "[same-file-identity] all checks passed\n");
  return 0;
}
