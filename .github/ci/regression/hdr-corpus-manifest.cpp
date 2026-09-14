/*
    File:       hdr-corpus-manifest.cpp

    Contains:   Enforces Testing/HDR/hdr-corpus-manifest.tsv - the expected
                clause 8.10 CLASSIFICATION of every fixture in Testing/HDR.

    Why this exists separately from iccdev.qa-profile-manifest: that test
    records the VALIDATION verdict, which cannot express what most of these
    fixtures were built to pin.  Failing clause 8.10.1's membership conditions
    does not make a profile invalid, so all six membership negatives validate
    `valid` and are indistinguishable there.  Without this test they assert
    nothing at all.

    The manifest is a specification, not a snapshot.  Its `purpose` column
    says what each fixture is for, and a row whose numbers change is either a
    classifier regression or a deliberate ruling that has to be re-argued in
    that column.

    Copyright:  See ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
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
 * individuals on behalf of the The International Color Consortium.
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *
 *
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "IccProfile.h"
#include "IccHdrProfile.h"

static int g_fail = 0;

static void failure(const std::string &fixture, const char *what,
                    const std::string &want, const std::string &got)
{
  printf("FAIL %-28s %-18s expected %-14s got %s\n",
         fixture.c_str(), what, want.c_str(), got.c_str());
  g_fail++;
}

/* icFloatNumber is float, so an exact compare fails on values like 4,92611
 * that were written out at printf's default precision.  Relative tolerance,
 * with an absolute floor so that the many legitimate zeroes compare cleanly. */
static bool closeEnough(double a, double b)
{
  double d = fabs(a - b);
  if (d <= 1e-6)
    return true;
  double m = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
  return m > 0.0 && (d / m) <= 1e-5;
}

static const char *className(icHdrProfileClass c)
{
  switch (c) {
    case icHdrProfileNone:       return "none";
    case icHdrProfileConforming: return "conforming";
    case icHdrProfileHdrContent: return "hdr-content";
  }
  return "?";
}

static const char *displaySourceName(icHdrHeadroomSource s)
{
  switch (s) {
    case icHdrHeadroomNone:    return "-";
    case icHdrHeadroomDerh:    return "derh";
    case icHdrHeadroomDcvDrwl: return "dcv-drwl";
    case icHdrHeadroomDcvCrwl: return "dcv-crwl";
  }
  return "?";
}

static const char *contentSourceName(icHdrContentHeadroomSource s)
{
  switch (s) {
    case icHdrContentHeadroomNone:    return "-";
    case icHdrContentHeadroomCll:     return "cll";
    case icHdrContentHeadroomMdcv:    return "mdcv";
    case icHdrContentHeadroomDefault: return "default-1000";
  }
  return "?";
}

static std::string fmt(double v)
{
  char buf[64];
  snprintf(buf, sizeof(buf), "%g", v);
  return buf;
}

/* The attribute value of name="..." inside one element's text, or "" when
 * absent.  Requires the preceding character to be whitespace so that Name is
 * never matched inside another attribute's name. */
static std::string attrValue(const std::string &tag, const char *name)
{
  std::string key = std::string(name) + "=\"";
  size_t pos = 0;
  while ((pos = tag.find(key, pos)) != std::string::npos) {
    if (pos > 0 && (tag[pos - 1] == ' ' || tag[pos - 1] == '\t' ||
                    tag[pos - 1] == '\n' || tag[pos - 1] == '\r')) {
      size_t start = pos + key.size();
      size_t end = tag.find('"', start);
      return end == std::string::npos ? std::string() : tag.substr(start, end - start);
    }
    pos += key.size();
  }
  return std::string();
}

/* Every HDR metadata entry in the corpus XML has the value count its registry
 * entry defines: CRWL, DRWL and DERH one value; CLL (max, average, primaries),
 * MDCV (max, min, primaries) and DCV (max, min, primaries) three; CCV (max,
 * average, min, primaries) four.  Every value must also be a number.
 *
 * Checked on the tracked XML rather than through icGetHdrProfileInfo(),
 * because the resolver reads only the entries its rule selects.  An entry no
 * rule reads can have any shape while every value assertion in the manifest
 * stays green - which is exactly how a ten-value DCV once passed a clean value
 * check downstream, on a fixture whose display rule read no DCV at all.
 *
 * Returns the number of failures; entriesChecked receives how many entries
 * were checked, so the caller can refuse a check that saw none. */
static int checkEntryArity(const std::vector<std::string> &xmlFiles, int &entriesChecked)
{
  std::map<std::string, size_t> arity;
  arity["CRWL"] = 1;
  arity["DRWL"] = 1;
  arity["DERH"] = 1;
  arity["CLL"]  = 3;
  arity["MDCV"] = 3;
  arity["DCV"]  = 3;
  arity["CCV"]  = 4;

  int failures = 0;
  entriesChecked = 0;

  for (size_t i = 0; i < xmlFiles.size(); i++) {
    std::string path = std::string("Testing/HDR/") + xmlFiles[i];
    std::ifstream f(path.c_str());
    if (!f) {
      printf("FAIL %-28s cannot read %s\n", xmlFiles[i].c_str(), path.c_str());
      failures++;
      continue;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string text = buf.str();

    /* Drop XML comments first: fixture headers discuss entries in prose, and a
     * DictEntry written inside a comment is not an entry. */
    size_t c;
    while ((c = text.find("<!--")) != std::string::npos) {
      size_t e = text.find("-->", c);
      text.erase(c, e == std::string::npos ? std::string::npos : e + 3 - c);
    }

    size_t pos = 0;
    while ((pos = text.find("<DictEntry", pos)) != std::string::npos) {
      size_t end = text.find('>', pos);
      if (end == std::string::npos)
        break;
      std::string tag = text.substr(pos, end - pos);
      pos = end;

      std::string name = attrValue(tag, "Name");
      std::map<std::string, size_t>::const_iterator it = arity.find(name);
      if (it == arity.end())
        continue;

      entriesChecked++;
      std::string value = attrValue(tag, "Value");
      std::stringstream vs(value);
      std::string tok;
      size_t n = 0;
      bool numeric = true;
      while (vs >> tok) {
        n++;
        char *stop = NULL;
        strtod(tok.c_str(), &stop);
        if (!stop || *stop)
          numeric = false;
      }

      if (n != it->second || !numeric) {
        printf("FAIL %-28s %s has %u value(s)%s; the registry defines %u: \"%s\"\n",
               xmlFiles[i].c_str(), name.c_str(), (unsigned)n,
               numeric ? "" : ", not all numeric", (unsigned)it->second, value.c_str());
        failures++;
      }
    }
  }

  return failures;
}

int main()
{
  const char *kManifest = "Testing/HDR/hdr-corpus-manifest.tsv";
  const char *kDir      = "Testing/HDR/";

  std::ifstream in(kManifest);
  if (!in) {
    /* The fixtures are generated by iccdev.create-profiles, but the manifest
     * is tracked, so its absence is a broken checkout rather than a skip. */
    printf("FAIL cannot open %s\n", kManifest);
    return 1;
  }

  std::map<std::string, bool> listed;
  std::string line;
  int rows = 0;

  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    std::vector<std::string> f;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, '\t'))
      f.push_back(cell);

    if (f.size() < 6) {
      printf("FAIL malformed row (%u fields): %s\n",
             (unsigned)f.size(), line.c_str());
      g_fail++;
      continue;
    }

    rows++;
    const std::string &name = f[0];
    listed[name] = true;

    std::string path = std::string(kDir) + name + ".icc";
    CIccProfile *pProfile = ReadIccProfile(path.c_str());
    if (!pProfile) {
      printf("FAIL %-28s cannot open %s\n", name.c_str(), path.c_str());
      g_fail++;
      continue;
    }

    icHdrProfileInfo info;
    icGetHdrProfileInfo(pProfile, info);

    if (f[1] != className(info.nClass))
      failure(name, "class", f[1], className(info.nClass));

    if (!closeEnough(atof(f[2].c_str()), (double)info.displayHeadroom))
      failure(name, "display_headroom", f[2], fmt((double)info.displayHeadroom));

    if (f[3] != displaySourceName(info.nHeadroomSource))
      failure(name, "headroom_source", f[3], displaySourceName(info.nHeadroomSource));

    if (!closeEnough(atof(f[4].c_str()), (double)info.contentHeadroom))
      failure(name, "content_headroom", f[4], fmt((double)info.contentHeadroom));

    if (f[5] != contentSourceName(info.nContentHeadroomSource))
      failure(name, "content_source", f[5], contentSourceName(info.nContentHeadroomSource));

    delete pProfile;
  }

  /* An unlisted fixture is a failure, not a pass.  Adding a fixture without a
   * row here is exactly how a corpus drifts into asserting nothing, which is
   * the condition this file was written to end. */
  int unlisted = 0;
  int arityEntries = 0;
  {
    std::vector<std::string> found;

#if defined(_WIN32)
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA("Testing\\HDR\\*", &fd);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          found.push_back(fd.cFileName);
      } while (FindNextFileA(h, &fd));
      FindClose(h);
    }
#else
    DIR *d = opendir("Testing/HDR");
    if (d) {
      struct dirent *e;
      while ((e = readdir(d)) != NULL)
        found.push_back(e->d_name);
      closedir(d);
    }
#endif

    for (size_t i = 0; i < found.size(); i++) {
      const std::string &fn = found[i];
      if (fn.size() < 5 || fn.compare(fn.size() - 4, 4, ".icc") != 0)
        continue;
      std::string base = fn.substr(0, fn.size() - 4);
      if (listed.find(base) == listed.end()) {
        printf("FAIL %-28s built by mkprofiles but has no manifest row\n", base.c_str());
        unlisted++;
        g_fail++;
      }
    }

    std::vector<std::string> xmlFiles;
    for (size_t i = 0; i < found.size(); i++) {
      const std::string &fn = found[i];
      if (fn.size() >= 5 && fn.compare(fn.size() - 4, 4, ".xml") == 0)
        xmlFiles.push_back(fn);
    }
    g_fail += checkEntryArity(xmlFiles, arityEntries);
  }

  printf("hdr-corpus-manifest: %d rows checked, %d unlisted, %d metadata entries arity-checked, %d failures\n",
         rows, unlisted, arityEntries, g_fail);

  /* A check that saw no entries asserts nothing, which is not a pass. */
  if (arityEntries == 0) {
    printf("FAIL no HDR metadata entries found to arity-check\n");
    return 1;
  }

  if (rows == 0) {
    printf("FAIL manifest contained no rows\n");
    return 1;
  }

  return g_fail ? 1 : 0;
}
