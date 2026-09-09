/*
 * Copyright (c) International Color Consortium.
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

#import "BenchApplyHost.h"
#include "BenchTimer.h"
#include "IccCmm.h"
#include "IccProfLibVer.h"
#include "IccProfile.h"

#include <cstdlib>
#include <cstdio>
#include <vector>

// Proof-of-concept only: a fixed single-profile round-trip chain
// (bundled RGB matrix/curve profile -> itself, relative colorimetric),
// timed and checksummed with the exact primitives
// iccBenchApply's desktop CLI uses (BenchTimer.h). This intentionally
// does not reuse RunSuite()/DecodeIntent() from iccBenchApply.cpp: those
// resolve a Testing/ tree and a multi-case chain table from argv, neither
// of which exists in a sandboxed mobile app with one bundled profile.
// A faithful "-suite" port is future work, not this proof of concept.
static NSString *IccDevFinishBenchApply(NSMutableString *summary,
                                        bool bPassed,
                                        int nChecks)
{
  std::printf("ICCDEV_BENCHAPPLY_TESTS %s (%d checks)\n",
              bPassed ? "PASS" : "FAIL", nChecks);
  std::fflush(stdout);
  if ([[NSProcessInfo processInfo].arguments containsObject:@"--exit-after-tests"])
    std::exit(bPassed ? 0 : 1);
  return summary;
}

NSString *IccDevRunBenchApply(icUInt32Number nPixels, int nRepeats)
{
  @autoreleasepool {
    NSOperatingSystemVersion os = [NSProcessInfo processInfo].operatingSystemVersion;
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    formatter.dateStyle = NSDateFormatterMediumStyle;
    formatter.timeStyle = NSDateFormatterMediumStyle;
    NSString *runDate = [formatter stringFromDate:[NSDate date]];
    NSDictionary *info = [[NSBundle mainBundle] infoDictionary];
    NSString *shortVersion = info[@"CFBundleShortVersionString"] ?: @"unknown";
    NSString *buildVersion = info[@"CFBundleVersion"] ?: @"unknown";
    NSMutableString *summary = [NSMutableString stringWithFormat:
      @"ICCDEV BENCHMARK\n"
       "iccBenchApply Mobile POC\n"
       "========================\n\n"
       "Run: %@\n"
       "App: %@ (%@)\n"
       "ICC: https://www.color.org/\n"
       "Repo: https://github.com/InternationalColorConsortium/iccDEV\n"
       "Library: IccProfLib %s\n"
       "Platform: iOS %ld.%ld.%ld\n\n",
      runDate, shortVersion, buildVersion, ICCPROFLIBVER,
      static_cast<long>(os.majorVersion), static_cast<long>(os.minorVersion),
      static_cast<long>(os.patchVersion)];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *documents = [[fileManager
      URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
    NSURL *reportURL = documents
      ? [documents URLByAppendingPathComponent:@"bench-results.json"] : nil;
    if (!reportURL) {
      [summary appendString:@"FAIL\n\nResolve Documents/bench-results.json\n"];
      return IccDevFinishBenchApply(summary, false, 0);
    }
    NSError *error = nil;
    if ([fileManager fileExistsAtPath:reportURL.path] &&
        ![fileManager removeItemAtURL:reportURL error:&error]) {
      [summary appendFormat:@"FAIL\n\nRemove stale device results: %s\n",
        error ? error.localizedDescription.UTF8String : "remove failed"];
      return IccDevFinishBenchApply(summary, false, 0);
    }

    NSString *path = [[NSBundle mainBundle] pathForResource:@"sRGB_D65_MAT" ofType:@"icc"];
    if (!path) {
      [summary appendString:@"FAIL\n\nBundled RGB fixture not found\n"];
      return IccDevFinishBenchApply(summary, false, 0);
    }

    CIccCmm cmm(icSigRgbData, icSigRgbData, true);
    const icStatusCMM addStat1 =
      cmm.AddXform(path.fileSystemRepresentation, icRelativeColorimetric,
                   icInterpLinear);
    const icStatusCMM addStat2 =
      cmm.AddXform(path.fileSystemRepresentation, icRelativeColorimetric,
                   icInterpLinear);
    icStatusCMM buildStat = addStat1;
    if (buildStat == icCmmStatOk)
      buildStat = addStat2;
    if (buildStat == icCmmStatOk)
      buildStat = cmm.Begin();
    if (buildStat != icCmmStatOk) {
      [summary appendFormat:@"FAIL\n\nBuild RGB round-trip CMM: %s\n",
        CIccCmm::GetStatusText(buildStat)];
      return IccDevFinishBenchApply(summary, false, 0);
    }

    const icUInt16Number nSamples = cmm.GetSourceSamples();

    std::vector<icFloatNumber> src((size_t)nPixels * nSamples);
    std::vector<icFloatNumber> dst((size_t)nPixels * nSamples);
    icBenchFill(src.data(), nPixels, nSamples, 20260907u);

    icStatusCMM applyStat = cmm.Apply(dst.data(), src.data(), nPixels);
    if (applyStat != icCmmStatOk) {
      [summary appendFormat:@"FAIL\n\nApply RGB round-trip CMM: %s\n",
        CIccCmm::GetStatusText(applyStat)];
      return IccDevFinishBenchApply(summary, false, 0);
    }

    icStatusCMM timedApplyStat = icCmmStatOk;
    const BenchStats stats = icBenchRun(
      [&]() {
        if (timedApplyStat == icCmmStatOk)
          timedApplyStat = cmm.Apply(dst.data(), src.data(), nPixels);
      },
      nPixels, nRepeats);
    if (timedApplyStat != icCmmStatOk) {
      [summary appendFormat:@"FAIL\n\nTimed RGB round-trip CMM: %s\n",
        CIccCmm::GetStatusText(timedApplyStat)];
      return IccDevFinishBenchApply(summary, false, 0);
    }
    const icUInt32Number checksum = icBenchChecksum(dst.data(), dst.size());

    [summary appendFormat:
      @"PASS\n\n"
       "Chain: sRGB_D65_MAT.icc -> sRGB_D65_MAT.icc\n"
       "Intent: relative colorimetric\n"
       "Profile path: matrix/curve, no CLUT interpolation\n"
       "Pixels: %u\n"
       "Repeats: %d\n\n"
       "Median: %.3f Mpx/s\n"
       "Min:    %.3f Mpx/s\n"
       "Max:    %.3f Mpx/s\n"
       "Hash:   0x%08x\n\n"
       "Report: Documents/bench-results.json\n",
      nPixels, nRepeats,
      stats.medianMpxPerSec, stats.minMpxPerSec, stats.maxMpxPerSec, checksum];

    NSDictionary *report = @{
      @"passed": @YES,
      @"pixels": @(nPixels),
      @"repeats": @(nRepeats),
      @"profilePath": @"matrix/curve",
      @"medianMpxPerSec": @(stats.medianMpxPerSec),
      @"minMpxPerSec": @(stats.minMpxPerSec),
      @"maxMpxPerSec": @(stats.maxMpxPerSec),
      @"checksum": [NSString stringWithFormat:@"0x%08x", checksum],
      @"libraryVersion": @ICCPROFLIBVER
    };
    NSData *json = [NSJSONSerialization dataWithJSONObject:report
                      options:NSJSONWritingPrettyPrinted error:&error];
    bool written = json &&
      [json writeToURL:reportURL options:NSDataWritingAtomic error:&error];
    if (!written) {
      NSRange status = [summary rangeOfString:@"PASS\n\n"];
      if (status.location != NSNotFound)
        [summary replaceCharactersInRange:status withString:@"FAIL\n\n"];
      [summary appendFormat:@"Persist device results: %s\n",
        error ? error.localizedDescription.UTF8String : "no output location"];
    }

    return IccDevFinishBenchApply(summary, written, 1);
  }
}
