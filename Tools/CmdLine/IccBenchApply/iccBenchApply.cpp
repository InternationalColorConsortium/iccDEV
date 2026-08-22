/*
    File:       iccBenchApply.cpp

    Contains:   Console app that measures apply-path throughput for a profile chain

    Version:    V1

    Copyright:  (c) see below
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
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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

//////////////////////////////////////////////////////////////////////
// HISTORY:
//
// -Initial implementation by Claude Opus 5 8-20-2026
//
//////////////////////////////////////////////////////////////////////


#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "IccCmm.h"
#include "IccCmmThread.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include "IccProfile.h"
#include "IccTagLut.h"
#include "IccUtil.h"

#include "BenchCases.h"
#include "BenchTimer.h"

static icUInt32Number   g_nPixels   = 1048576;
static int              g_nRepeats  = 7;
static bool             g_bPerXform = false;
static bool             g_bLeaf     = false;
static std::vector<int> g_threads;
static bool             g_bSuite    = false;
static bool             g_bCsv      = false;

static void Usage()
{
  printf("iccBenchApply built with IccProfLib version " ICCPROFLIBVER "\n\n");
  printf("Usage: iccBenchApply {options} interpolation"
         " {profile_path rendering_intent {-PCC pcc_path}}...\n\n");
  printf("  interpolation      0 = Linear, 1 = Tetrahedral\n");
  printf("  rendering_intent   0..3, plus +1000 / +10000 modifiers"
         " (as iccApplyToLink)\n\n");
  printf("  -pixels N          pixels per buffer      (default 1048576)\n");
  printf("  -repeats N         timed repeats per case (default 7)\n");
  printf("  -perxform          per-xform breakdown, including PCS steps\n");
  printf("  -leaf              isolated hot leaf functions\n");
  printf("  -threads L         comma list of thread counts, e.g. 1,2,8\n");
  printf("  -suite             run the built-in case table; takes no chain\n");
  printf("  -csv               machine-readable output\n");
  printf("\nNo timing threshold is ever asserted. This tool records.\n");
}

// Shares iccApplyToLink.cpp's parsing contract, so the two tools accept and
// reject exactly the same argument strings.
static bool ParseIntArg(const char *arg, int minValue, int maxValue, int &value)
{
  char *end = NULL;
  long parsed;

  if (!arg || !*arg)
    return false;

  errno = 0;
  parsed = strtol(arg, &end, 10);

  if (errno == ERANGE || end == arg || *end != '\0' ||
      parsed < minValue || parsed > maxValue) {
    return false;
  }

  value = (int)parsed;
  return true;
}

// Decodes one encoded rendering intent the way iccApplyToLink.cpp:1054-1059 does,
// so a chain given to this tool and the same chain given to that one resolve
// identically. Returns false when the decoded intent is out of range.
static bool DecodeIntent(int nEncoded, int &nIntent, int &nType,
                         bool &bUseSubProfile, bool &bUseD2BxB2DxTags)
{
  bUseSubProfile = (nEncoded / 1000) > 0;
  nIntent = nEncoded % 1000;
  nIntent = nIntent % 100;
  nType   = abs(nIntent) / 10;
  nIntent = nIntent % 10;

  bUseD2BxB2DxTags = true;
  if (nType == 1) {
    nType = 0;
    bUseD2BxB2DxTags = false;
  }

  return nIntent >= (int)icPerceptual && nIntent <= (int)icAbsoluteColorimetric;
}

// Names for the icXformType values in IccCmm.h:177-187. PCS is named explicitly
// rather than falling through to "Unknown" because the CIccPcsXform entries
// Begin() inserts between profiles are the rows this harness exists to expose.
static const char *XformTypeName(icXformType t)
{
  switch (t) {
    case icXformTypeMatrixTRC:   return "MatrixTRC";
    case icXformType3DLut:       return "3DLut";
    case icXformType4DLut:       return "4DLut";
    case icXformTypeNDLut:       return "NDLut";
    case icXformTypeNamedColor:  return "NamedColor";
    case icXformTypeMpe:         return "Mpe";
    case icXformTypeMonochrome:  return "Monochrome";
    case icXformTypePCS:         return "PCS";
    default:                     return "Unknown";
  }
}

// Collects the resolved chain. CIccCmm::IterateXforms (IccCmm.h:1814) is the
// public hook, and it reports the chain after Begin() has inserted the PCS
// xforms, which is the only way to see them from outside the library.
class CChainReporter : public IXformIterator
{
public:
  virtual void iterate(const CIccXform *pXform)
  {
    if (!pXform)
      return;
    // IterateXforms hands out const pointers, but GetNewApply() is non-const
    // and allocates a per-apply object without mutating the xform's own state.
    // The CMM owns these and outlives this tool's use of them.
    m_xforms.push_back(const_cast<CIccXform *>(pXform));
  }

  std::vector<CIccXform *> m_xforms;
};

// Runs the chain at each requested thread count.
//
// The checksum must be identical at every count. That is the assertion, not a
// nicety: it is what catches precomputed state parked on a shared CIccXform or
// CIccPcsStep when it belonged in the per-thread apply object. A hoist that
// looks correct single-threaded and races under load fails here.
//
// Returns false if any count disagreed with the first.
static bool RunThreadSweep(CIccCmm &cmm, const std::vector<int> &threads,
                           const char *szCaseName)
{
  const icUInt16Number nSrc = cmm.GetSourceSamples();
  const icUInt16Number nDst = cmm.GetDestSamples();

  std::vector<icFloatNumber> src((size_t)g_nPixels * nSrc);
  std::vector<icFloatNumber> dst((size_t)g_nPixels * nDst);
  icBenchFill(src.data(), g_nPixels, nSrc, 20260820u);

  bool bOk = true;
  icUInt32Number sumFirst = 0;
  double rateFirst = 0.0;
  bool bHaveFirst = false;

  for (size_t i = 0; i < threads.size(); i++) {
    const int n = threads[i];

    BenchStats st;
    icUInt32Number sum;

    // #2254: emit the row's identity BEFORE the measurement, not after it. The
    // suite's per-case cost spans two orders of magnitude at a fixed pixel
    // budget -- 33.77 Mpx/s for monochrome against 0.18 for mpe-calc on the same
    // host -- so on a slow or instrumented build one case can run for tens of
    // minutes. With nothing on stdout until it finished, that was reported as a
    // hang: the last line printed was the previous case, and the running one was
    // invisible. The completed row is byte-identical to before; only the moment
    // the first half of it appears has changed. CSV is untouched, since a
    // half-written record is not a record.
    if (g_bCsv) {
      // CSV cannot carry a half-written row, so the progress goes to stderr --
      // the same channel the empty-suite diagnostic uses, and for the same
      // reason. CI drives the suite with -csv, which is precisely where a case
      // that runs for tens of minutes is least visible, so gating the progress
      // on !g_bCsv would have left the reported symptom in place exactly where
      // it was reported from.
      fprintf(stderr, "running %s t=%d\n", szCaseName, n);
      fflush(stderr);
    }
    else {
      printf("  %-16s t=%-4d ", i ? "" : szCaseName, n);
      fflush(stdout);
    }

    if (n == 1) {
      st = icBenchRun(
        [&]() { cmm.Apply(dst.data(), src.data(), g_nPixels); },
        g_nPixels, g_nRepeats);
      sum = icBenchChecksum(dst.data(), dst.size());
    }
    else {
      // bDeleteCmm=false because this tool owns theCmm and reuses it across
      // every thread count. ~CIccThreadedCmm does "if (m_bDeleteCmm) delete
      // m_pCmm" (IccCmmThread.cpp:256-260), so passing true would free the CMM
      // after the first threaded iteration and the next one would use freed
      // memory.
      CIccThreadedCmm *pT = CIccThreadedCmm::Attach(&cmm, n, false);
      // The CSV arm is new with #2254. This branch used to print the human row
      // unconditionally, so an Attach failure under -csv injected a padded text
      // line into the middle of the record stream. Reporting it as a record
      // keeps the file parseable and keeps the failure visible.
      if (!pT) {
        if (g_bCsv)
          printf("%s,%d,,,,,attach-failed\n", szCaseName, n);
        else
          printf(" (Attach failed; skipped)\n");
        continue;
      }
      st = icBenchRun(
        [&]() { pT->Apply(dst.data(), src.data(), g_nPixels); },
        g_nPixels, g_nRepeats);
      sum = icBenchChecksum(dst.data(), dst.size());
      delete pT;
    }

    if (!bHaveFirst) {
      sumFirst   = sum;
      rateFirst  = st.medianMpxPerSec;
      bHaveFirst = true;
    }

    const bool bMatch = (sum == sumFirst);
    if (!bMatch)
      bOk = false;

    if (g_bCsv) {
      printf("%s,%d,%.3f,%.3f,%.3f,0x%08x,%s\n",
             szCaseName, n, st.medianMpxPerSec, st.minMpxPerSec,
             st.maxMpxPerSec, sum, bMatch ? "ok" : "checksum-mismatch");
    }
    else {
      printf("%9.2f %7.2fx  0x%08x  %s\n", st.medianMpxPerSec,
             rateFirst > 0.0 ? st.medianMpxPerSec / rateFirst : 0.0,
             sum, bMatch ? "OK" : "CHECKSUM MISMATCH");
    }
  }

  if (!bOk) {
    printf("\n  FAIL: %s produced different output at different thread counts.\n"
           "        Per-thread state is being shared. This is a correctness\n"
           "        defect, not a performance one.\n", szCaseName);
  }
  return bOk;
}

// Times the hot leaf functions in isolation.
//
// Reported in Mval/s, not Mpx/s, and printed under its own heading: these
// numbers are not comparable to the chain figure. Isolation changes cache
// behaviour, so a win measured here may not survive in a real chain -- which is
// exactly why the chain tier exists. This tier answers "did the change to this
// function work", not "does it matter".
static void RunLeaf(const char *szProfilePath)
{
  CIccProfile *pProfile = ReadIccProfile(szProfilePath);
  if (!pProfile) {
    printf("  (leaf: cannot read '%s')\n", szProfilePath);
    return;
  }

  printf("\n  isolated leaf functions (%s):\n", szProfilePath);
  printf("  %-28s %9s\n", "function", "Mval/s");

  // A2B0 is where a LUT-based profile keeps its device-to-PCS transform.
  CIccTag *pTag = pProfile->FindTag(icSigAToB0Tag);
  CIccMBB *pMBB = NULL;
  if (pTag) {
    switch (pTag->GetType()) {
      case icSigLut8Type:
      case icSigLut16Type:
      case icSigLutAtoBType:
        pMBB = (CIccMBB *)pTag;
        break;
      default:
        break;
    }
  }

  if (!pMBB) {
    printf("  (no A2B0 LUT tag; nothing to time)\n");
    delete pProfile;
    return;
  }

  // No pMBB->Begin() here: CIccMBB does not declare Begin() and neither does
  // CIccTag, so that call would not compile. The curve and the CLUT each carry
  // their own Begin() and are initialised individually below -- which is all
  // this tier needs, since it drives them directly rather than through the tag.

  LPIccCurve *pCurves = pMBB->GetCurvesA();
  if (pCurves && pCurves[0]) {
    CIccCurve *pCurve = pCurves[0];
    pCurve->Begin();

    std::vector<icFloatNumber> vals(g_nPixels);
    icBenchFill(vals.data(), g_nPixels, 1, 20260820u);
    volatile icFloatNumber sink = 0;

    const BenchStats cs = icBenchRun(
      [&]() {
        icFloatNumber acc = 0;
        for (icUInt32Number k = 0; k < g_nPixels; k++)
          acc += pCurve->Apply(vals[k]);
        sink = acc;   // consume, so the loop is not elided
      },
      g_nPixels, g_nRepeats);
    (void)sink;

    printf("  %-28s %9.2f\n", "CIccCurve::Apply", cs.medianMpxPerSec);
  }

  CIccCLUT *pCLUT = pMBB->GetCLUT();
  if (pCLUT && pMBB->InputChannels() == 3) {
    pCLUT->Begin();

    const icUInt16Number nOut = pMBB->OutputChannels();
    std::vector<icFloatNumber> in((size_t)g_nPixels * 3);
    std::vector<icFloatNumber> out((size_t)g_nPixels * nOut);
    icBenchFill(in.data(), g_nPixels, 3, 20260820u);

    const BenchStats t3 = icBenchRun(
      [&]() {
        for (icUInt32Number k = 0; k < g_nPixels; k++)
          pCLUT->Interp3dTetra(&out[(size_t)k * nOut], &in[(size_t)k * 3]);
      },
      g_nPixels, g_nRepeats);
    printf("  %-28s %9.2f\n", "CIccCLUT::Interp3dTetra", t3.medianMpxPerSec);

    const BenchStats l3 = icBenchRun(
      [&]() {
        for (icUInt32Number k = 0; k < g_nPixels; k++)
          pCLUT->Interp3d(&out[(size_t)k * nOut], &in[(size_t)k * 3]);
      },
      g_nPixels, g_nRepeats);
    printf("  %-28s %9.2f\n", "CIccCLUT::Interp3d", l3.medianMpxPerSec);
  }

  printf("  note: isolated figures are not comparable to the chain number;\n"
         "        isolation changes cache behaviour.\n");

  delete pProfile;
}

// Runs the built-in table.
//
// A case whose profile is missing is a SKIP with a reason, not a failure, so a
// corpus that is short one profile still measures the rest. #2254 corrected the
// scope of that tolerance: it used to extend to a run in which NOTHING
// measured, and the original note here claimed "the cases whose profiles are
// tracked should still run" on a build configured without ENABLE_ICCXML. That
// set is empty. All nine cases open a generated profile FIRST -- the only
// tracked profile in the table, ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc,
// is never first -- so without the corpus generator the suite measures zero
// cases, every time. Tolerating that made the exit status say "pass" for a run
// that did nothing. Zero measured is now a failure, and the CTests that drive
// the suite are registered only where a profile-generating target exists. An
// explicit chain on the command line is held to a stricter standard still, and
// fails loudly on the first profile it cannot open.
static int RunSuite()
{
  const std::vector<BenchCase> &cases = icBenchBuiltinCases();
  bool bAllOk = true;
  int  nRan = 0, nSkipped = 0;

  if (g_bCsv)
    printf("case,threads,mpx_per_sec,min,max,checksum,status\n");
  else
    printf("\n  %-16s %-6s %9s %8s  %-10s %s\n",
           "case", "thr", "Mpx/s", "scale", "checksum", "status");

  for (size_t c = 0; c < cases.size(); c++) {
    const BenchCase &bc = cases[c];

    std::vector<std::string> resolved;
    bool bHaveAll = true;
    std::string sMissing;

    for (size_t i = 0; i < bc.chain.size(); i++) {
      std::string abs;
      if (!icBenchResolveProfile(bc.chain[i].profile, abs)) {
        bHaveAll = false;
        sMissing = bc.chain[i].profile;
        break;
      }
      resolved.push_back(abs);
    }

    if (!bHaveAll) {
      if (g_bCsv)
        printf("%s,,,,,,skip-missing-profile\n", bc.name.c_str());
      else
        printf("  %-16s SKIP   %s not found\n", bc.name.c_str(),
               sMissing.c_str());
      nSkipped++;
      continue;
    }

    CIccCmm theCmm(icSigUnknownData, icSigUnknownData, true);
    bool bBuilt = true;

    for (size_t i = 0; i < bc.chain.size(); i++) {
      int nIntent, nType;
      bool bSub, bD2B;
      if (!DecodeIntent(bc.chain[i].encodedIntent, nIntent, nType, bSub, bD2B)) {
        printf("  %-16s SKIP   bad encoded intent %d\n",
               bc.name.c_str(), bc.chain[i].encodedIntent);
        bBuilt = false;
        break;
      }

      icStatusCMM stat = theCmm.AddXform(resolved[i].c_str(),
                                         (icRenderingIntent)nIntent,
                                         bc.interpolation ? icInterpTetrahedral
                                                          : icInterpLinear,
                                         NULL, (icXformLutType)nType,
                                         bD2B, NULL, bSub);
      if (stat != icCmmStatOk) {
        if (g_bCsv)
          printf("%s,,,,,,skip-addxform\n", bc.name.c_str());
        else
          printf("  %-16s SKIP   AddXform(%s): %s\n", bc.name.c_str(),
                 bc.chain[i].profile.c_str(), CIccCmm::GetStatusText(stat));
        bBuilt = false;
        break;
      }
    }

    if (!bBuilt) {
      nSkipped++;
      continue;
    }

    icStatusCMM stat = theCmm.Begin();
    if (stat != icCmmStatOk) {
      if (g_bCsv)
        printf("%s,,,,,,skip-begin\n", bc.name.c_str());
      else
        printf("  %-16s SKIP   Begin(): %s\n", bc.name.c_str(),
               CIccCmm::GetStatusText(stat));
      nSkipped++;
      continue;
    }

    if (!RunThreadSweep(theCmm, g_threads, bc.name.c_str()))
      bAllOk = false;
    nRan++;

    // CIccCmm::Begin() returns early when m_pApply is set -- the idempotency
    // contract e4e05c3a restored for #1940. The optimisation branches add
    // Begin()-time precomputation, so a second Begin() must stay a no-op in both
    // status and output. Checked on one representative case rather than all
    // eight, because it costs a full extra apply pass.
    if (bc.name == "lut-3d-tetra") {
      const icUInt16Number nS = theCmm.GetSourceSamples();
      const icUInt16Number nD = theCmm.GetDestSamples();
      std::vector<icFloatNumber> src((size_t)g_nPixels * nS);
      std::vector<icFloatNumber> dst((size_t)g_nPixels * nD);
      icBenchFill(src.data(), g_nPixels, nS, 20260820u);

      theCmm.Apply(dst.data(), src.data(), g_nPixels);
      const icUInt32Number sumBefore = icBenchChecksum(dst.data(), dst.size());

      const icStatusCMM again = theCmm.Begin();
      if (again != icCmmStatOk) {
        printf("  FAIL: second Begin() on %s reported %s, expected icCmmStatOk\n",
               bc.name.c_str(), CIccCmm::GetStatusText(again));
        bAllOk = false;
      }
      else {
        theCmm.Apply(dst.data(), src.data(), g_nPixels);
        if (icBenchChecksum(dst.data(), dst.size()) != sumBefore) {
          printf("  FAIL: second Begin() on %s changed the output\n",
                 bc.name.c_str());
          bAllOk = false;
        }
        else if (!g_bCsv) {
          printf("  %-16s begin-reentry   OK (status and checksum unchanged)\n",
                 bc.name.c_str());
        }
      }
    }
  }

  if (!g_bCsv)
    printf("\n  %d case(s) measured, %d skipped.%s\n", nRan, nSkipped,
           bAllOk ? "" : "  SOME CASES FAILED.");

  // #2254: measuring nothing is a failure, not a pass. Every case SKIPs when the
  // profile roots are wrong -- running from Testing/ is enough, because the
  // roots default to "." and the tool then looks for ./Testing/Testing/... -- and
  // the old exit status made that indistinguishable from a clean run. That also
  // made iccdev.apply-throughput vacuous: it drives this function and asserts
  // only the exit code, so a build whose profile fixture produced nothing still
  // reported a passing benchmark. Individual SKIPs stay tolerated; a case that
  // cannot resolve on one platform is expected. Zero of nine is not.
  if (!nRan) {
    // stderr, unlike every other diagnostic in this file, because this one is
    // the only one that can be reached with -csv in effect: stdout is then a
    // record stream and a paragraph of prose in the middle of it is not
    // parseable. Flush stdout first so the two streams keep the order they were
    // written in when both land in one log.
    fflush(stdout);
    fprintf(stderr,
            "FAIL: no benchmark cases were measured. All %d were skipped.\n"
            "      Each of ICCDEV_BENCH_SOURCE_ROOT and ICCDEV_BENCH_BUILD_ROOT\n"
            "      (both defaulting to \".\") is tried as a directory holding a\n"
            "      Testing/ tree and as a Testing tree itself, so run this from\n"
            "      the repository root or from Testing/, or set those roots.\n"
            "      Generated profiles come from the create-profiles fixture.\n",
            nSkipped);
    return 1;
  }

  return bAllOk ? 0 : 1;
}

int main(int argc, const char *argv[])
{
  // #2254: this floor was 3, which rejected every two-argument invocation --
  // `iccBenchApply -suite` among them, the shortest and most obvious way to ask
  // for the built-in table. The chain needs two arguments and -suite needs
  // none, so an argc floor cannot express the requirement for both; the real
  // per-mode checks are below, where the mode is known. An invocation with no
  // arguments at all is still just the usage text.
  //
  // Worth being exact about what this did and did not break: the Readme's
  // examples all carried enough options to clear the floor, so nothing
  // documented was failing. The cost was that the natural spelling did not
  // work, and callers padded it with a stray trailing token instead -- which is
  // how the silent chain-discard below went unnoticed. The bare form is now an
  // example, because it now runs.
  if (argc < 2) {
    Usage();
    return 1;
  }

  int nArg = 1;

  while (nArg < argc && argv[nArg][0] == '-') {
    if (!stricmp(argv[nArg], "-pixels")) {
      int n;
      if (nArg + 1 >= argc || !ParseIntArg(argv[nArg + 1], 1, 1 << 26, n)) {
        printf("Invalid -pixels value: expected 1..67108864\n");
        return 1;
      }
      g_nPixels = (icUInt32Number)n;
      nArg += 2;
    }
    else if (!stricmp(argv[nArg], "-repeats")) {
      int n;
      if (nArg + 1 >= argc || !ParseIntArg(argv[nArg + 1], 1, 1000, n)) {
        printf("Invalid -repeats value: expected 1..1000\n");
        return 1;
      }
      g_nRepeats = n;
      nArg += 2;
    }
    else if (!stricmp(argv[nArg], "-perxform")) {
      g_bPerXform = true;
      nArg++;
    }
    else if (!stricmp(argv[nArg], "-leaf")) {
      g_bLeaf = true;
      nArg++;
    }
    else if (!stricmp(argv[nArg], "-suite")) {
      g_bSuite = true;
      nArg++;
    }
    else if (!stricmp(argv[nArg], "-csv")) {
      g_bCsv = true;
      nArg++;
    }
    else if (!stricmp(argv[nArg], "-threads")) {
      if (nArg + 1 >= argc) {
        printf("-threads needs a comma-separated list, e.g. 1,2,8\n");
        return 1;
      }
      g_threads.clear();
      const char *p = argv[nArg + 1];
      std::string tok;
      while (true) {
        if (*p == ',' || *p == '\0') {
          int n;
          if (!ParseIntArg(tok.c_str(), 1, 1024, n)) {
            printf("Invalid thread count '%s': expected 1..1024\n", tok.c_str());
            return 1;
          }
          g_threads.push_back(n);
          tok.clear();
          if (!*p)
            break;
        }
        else {
          tok.push_back(*p);
        }
        p++;
      }
      nArg += 2;
    }
    else {
      printf("Unknown option '%s'\n", argv[nArg]);
      Usage();
      return 1;
    }
  }

  if (g_threads.empty())
    g_threads.push_back(1);

  // #2254: -suite used to ignore trailing arguments outright, which is how a QA
  // run of `iccBenchApply -suite 1 sRGB_v4_ICC_preference.icc 10002` produced
  // the built-in table and never touched that profile or that intent. Silently
  // discarding a chain the caller spelled out in full is worse than refusing it,
  // so refuse it and name what was dropped. Now that the argc floor above lets
  // `-suite` stand alone, there is a correct spelling to point at.
  if (g_bSuite && nArg < argc) {
    printf("-suite runs the built-in case table and takes no chain;"
           " '%s' and everything after it would be ignored.\n"
           "Drop them, or drop -suite to benchmark that chain.\n", argv[nArg]);
    return 1;
  }

  // Same reason, different argument: RunSuite reads neither g_bPerXform nor
  // g_bLeaf, so both were accepted and dropped. A caller who asked for a
  // breakdown and got a plain table has no way to tell it was refused.
  if (g_bSuite && (g_bPerXform || g_bLeaf)) {
    printf("%s has no effect with -suite: the built-in table reports"
           " whole-chain throughput only.\n",
           g_bPerXform ? "-perxform" : "-leaf");
    return 1;
  }

  if (g_bSuite)
    return RunSuite();

  int nInterp;
  if (nArg >= argc || !ParseIntArg(argv[nArg], 0, 1, nInterp)) {
    printf("Invalid interpolation: expected 0 (linear) or 1 (tetrahedral)\n");
    return 1;
  }
  nArg++;

  if (nArg >= argc) {
    printf("No profiles given: a chain needs at least one"
           " 'profile_path rendering_intent' pair\n");
    return 1;
  }

  // A -PCC profile is not owned by the CMM: CIccXform::Create stores it as the
  // bare m_pConnectionConditions pointer (IccCmm.cpp:1239) and ~CIccXform never
  // deletes it, so the caller both owns it and must keep it alive for as long as
  // the xforms can dereference it. Holding the profiles here does both. Declared
  // ahead of theCmm so that the CMM -- and every xform holding one of these
  // pointers -- is destroyed first. iccApplyToLink keeps the same contract with
  // its pccList (#1336); iccBenchApply was the one -PCC caller that dropped it,
  // leaking the whole profile object graph on every run that passed one (#2250).
  std::vector<std::unique_ptr<CIccProfile>> pccProfiles;

  CIccCmm theCmm(icSigUnknownData, icSigUnknownData, true);
  int nProfiles = 0;
  std::string sFirstProfile;

  while (nArg < argc) {
    const char *szProfile = argv[nArg];

    if (nArg + 1 >= argc) {
      printf("Profile '%s' has no rendering intent\n", szProfile);
      return 1;
    }

    int nEncoded;
    if (!ParseIntArg(argv[nArg + 1], INT_MIN, INT_MAX, nEncoded)) {
      printf("Invalid rendering intent '%s': expected an integer code\n",
             argv[nArg + 1]);
      return 1;
    }
    nArg += 2;

    int nIntent, nType;
    bool bUseSubProfile, bUseD2BxB2DxTags;
    if (!DecodeIntent(nEncoded, nIntent, nType,
                      bUseSubProfile, bUseD2BxB2DxTags)) {
      printf("Invalid rendering intent '%s': decoded intent out of range\n",
             argv[nArg - 1]);
      return 1;
    }

    std::unique_ptr<CIccProfile> pPccProfile;
    if (nArg + 1 < argc && !stricmp(argv[nArg], "-PCC")) {
      pPccProfile.reset(ReadIccProfile(argv[nArg + 1]));
      if (!pPccProfile) {
        printf("Unable to read PCC profile '%s'\n", argv[nArg + 1]);
        return 1;
      }
      nArg += 2;
    }

    icStatusCMM stat = theCmm.AddXform(szProfile,
                                       (icRenderingIntent)nIntent,
                                       nInterp ? icInterpTetrahedral
                                               : icInterpLinear,
                                       pPccProfile.get(),
                                       (icXformLutType)nType,
                                       bUseD2BxB2DxTags,
                                       NULL,
                                       bUseSubProfile);
    if (stat != icCmmStatOk) {
      printf("Unable to add '%s' to the chain: %s\n",
             szProfile, CIccCmm::GetStatusText(stat));
      return 1;
    }
    if (pPccProfile)
      pccProfiles.push_back(std::move(pPccProfile));
    if (sFirstProfile.empty())
      sFirstProfile = szProfile;
    nProfiles++;
  }

  icStatusCMM stat = theCmm.Begin();
  if (stat != icCmmStatOk) {
    printf("Begin() failed: %s\n", CIccCmm::GetStatusText(stat));
    return 1;
  }

  printf("chain: %d profile(s), %d source sample(s) -> %d destination sample(s)\n",
         nProfiles, (int)theCmm.GetSourceSamples(),
         (int)theCmm.GetDestSamples());

  CChainReporter reporter;
  theCmm.IterateXforms(&reporter);

  printf("resolved transforms:\n");
  for (size_t i = 0; i < reporter.m_xforms.size(); i++) {
    printf("  %2d  %-12s\n", (int)i,
           XformTypeName(reporter.m_xforms[i]->GetXformType()));
  }

  const icUInt16Number nSrc = theCmm.GetSourceSamples();
  const icUInt16Number nDst = theCmm.GetDestSamples();

  if (!nSrc || !nDst) {
    printf("Chain reports %d source and %d destination samples;"
           " nothing to measure\n", (int)nSrc, (int)nDst);
    return 1;
  }

  printf("\n  %-16s %-6s %9s %8s  %-10s %s\n",
         "case", "thr", "Mpx/s", "scale", "checksum", "status");
  const bool bSweepOk = RunThreadSweep(theCmm, g_threads, "chain");

  if (g_bPerXform) {
    printf("\n  per-xform breakdown:\n");
    printf("  %3s %-12s %9s %7s\n", "#", "type", "Mpx/s", "share");

    // Two buffers, ping-ponged: each xform reads what the previous produced.
    // Sized to the widest sample count any stage uses, so no stage can write
    // past the end of a buffer sized for a narrower neighbour.
    icUInt16Number nMax = nSrc > nDst ? nSrc : nDst;
    for (size_t i = 0; i < reporter.m_xforms.size(); i++) {
      const icUInt16Number n = reporter.m_xforms[i]->GetNumDstSamples();
      if (n > nMax)
        nMax = n;
    }

    std::vector<icFloatNumber> a((size_t)g_nPixels * nMax);
    std::vector<icFloatNumber> b((size_t)g_nPixels * nMax);
    icBenchFill(a.data(), g_nPixels, nSrc, 20260820u);

    std::vector<double> secs;
    std::vector<std::string> names;
    icFloatNumber *pIn = a.data(), *pOut = b.data();

    for (size_t i = 0; i < reporter.m_xforms.size(); i++) {
      CIccXform *pXform = reporter.m_xforms[i];
      const char *szType = XformTypeName(pXform->GetXformType());

      icStatusCMM xstat = icCmmStatOk;
      CIccApplyXform *pApply = pXform->GetNewApply(xstat);
      if (!pApply || xstat != icCmmStatOk) {
        printf("  %3d %-12s   (no apply object; skipped)\n", (int)i, szType);
        delete pApply;
        continue;
      }

      const icUInt16Number nIn  = pXform->GetNumSrcSamples();
      const icUInt16Number nOut = pXform->GetNumDstSamples();

      const BenchStats xs = icBenchRun(
        [&]() {
          const icFloatNumber *s = pIn;
          icFloatNumber *d = pOut;
          for (icUInt32Number k = 0; k < g_nPixels; k++, s += nIn, d += nOut)
            pXform->Apply(pApply, d, s);
        },
        g_nPixels, g_nRepeats);

      secs.push_back(xs.medianMpxPerSec > 0.0 ? 1.0 / xs.medianMpxPerSec : 0.0);
      names.push_back(szType);
      printf("  %3d %-12s %9.2f\n", (int)i, szType, xs.medianMpxPerSec);

      delete pApply;
      icFloatNumber *t = pIn; pIn = pOut; pOut = t;
    }

    double total = 0.0;
    for (size_t i = 0; i < secs.size(); i++)
      total += secs[i];
    if (total > 0.0) {
      printf("\n  shares: ");
      for (size_t i = 0; i < secs.size(); i++)
        printf("%s%s %.0f%%", i ? ", " : "", names[i].c_str(),
               100.0 * secs[i] / total);
      printf("\n");
    }

    printf("\n  note: per-xform timing defeats the chunked-apply cache locality\n"
           "        that the chain number measures (IccCmm.cpp:8671), so these\n"
           "        rows sum to more than it. They are for attribution, not a\n"
           "        decomposition of the chain figure.\n");
  }

  if (g_bLeaf)
    RunLeaf(sFirstProfile.c_str());

  // A checksum that moved with the thread count is a correctness failure, so it
  // has to reach the exit code -- the CTest registration asserts on nothing else.
  return bSweepOk ? 0 : 1;
}
