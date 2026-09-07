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

#import <UIKit/UIKit.h>
#include "IccCmm.h"
#include "IccIO.h"
#include "IccProfLibVer.h"
#include "IccProfile.h"
#include "IccTagLut.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {

bool Check(NSMutableArray<NSDictionary *> *results, bool ok, NSString *name)
{
  [results addObject:@{@"test": name, @"passed": @(ok)}];
  std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name.UTF8String);
  return ok;
}

bool Near(icFloatNumber value, icFloatNumber expected, double tolerance)
{
  return std::isfinite(value) && std::fabs(value - expected) <= tolerance;
}

void TestProfiles(NSMutableArray<NSDictionary *> *results, NSURL *documents)
{
  NSString *path = [[NSBundle mainBundle] pathForResource:@"sRGB_D65_MAT" ofType:@"icc"];
  if (!Check(results, path != nil, @"Bundled RGB fixture exists"))
    return;
  std::unique_ptr<CIccProfile> profile(ReadIccProfile(path.fileSystemRepresentation));
  if (!Check(results, profile && profile->m_Header.colorSpace == icSigRgbData,
             @"Read bundled RGB profile"))
    return;

  CIccMemIO memory;
  if (!Check(results, memory.Alloc(1024 * 1024, true) &&
             profile->Write(&memory, icNeverWriteID), @"Serialize profile to memory"))
    return;
  const size_t length = memory.GetLength();
  std::unique_ptr<CIccProfile> restored(
    ReadIccProfile(memory.GetData(), static_cast<icUInt32Number>(length)));
  if (!Check(results, restored != nullptr, @"Read serialized profile"))
    return;
  std::unique_ptr<CIccProfile> truncated(ReadIccProfile(memory.GetData(), 64));
  Check(results, !truncated, @"Reject truncated profile header");

  NSURL *saved = [documents URLByAppendingPathComponent:@"roundtrip.icc"];
  bool savedOk = SaveIccProfile(saved.fileSystemRepresentation, restored.get(), icNeverWriteID);
  std::unique_ptr<CIccProfile> fromFile(
    savedOk ? ReadIccProfile(saved.fileSystemRepresentation) : nullptr);
  Check(results, fromFile != nullptr, @"Write and reopen profile inside app sandbox");
  if (savedOk) {
    NSError *error = nil;
    Check(results, [[NSFileManager defaultManager] removeItemAtURL:saved error:&error],
          @"Remove sandbox round-trip file");
    if (error)
      std::fprintf(stderr, "%s\n", error.localizedDescription.UTF8String);
  }

  CIccCmm cmm(icSigRgbData, icSigRgbData, true);
  if (!Check(results,
             cmm.AddXform(*profile, icRelativeColorimetric) == icCmmStatOk &&
             cmm.AddXform(*restored, icRelativeColorimetric) == icCmmStatOk &&
             cmm.Begin() == icCmmStatOk, @"Begin original/serialized RGB round-trip CMM"))
    return;

  icFloatNumber input[81] = {}, output[81] = {};
  size_t offset = 0;
  for (int r = 0; r < 3; ++r) {
    for (int g = 0; g < 3; ++g) {
      for (int b = 0; b < 3; ++b) {
        input[offset++] = r * 0.5f;
        input[offset++] = g * 0.5f;
        input[offset++] = b * 0.5f;
      }
    }
  }
  if (!Check(results, cmm.Apply(output, input, 27) == icCmmStatOk,
             @"Apply 27 RGB pixels in one batch"))
    return;
  bool roundTrip = true, singleMatches = true;
  for (size_t pixel = 0; pixel < 27; ++pixel) {
    icFloatNumber single[3] = {};
    singleMatches &= cmm.Apply(single, input + pixel * 3) == icCmmStatOk;
    for (size_t channel = 0; channel < 3; ++channel) {
      size_t index = pixel * 3 + channel;
      roundTrip &= Near(output[index], input[index], 0.005);
      singleMatches &= Near(single[channel], output[index], 1e-6);
    }
  }
  Check(results, roundTrip, @"RGB round-trip error <= 0.005 for all 81 channels");
  Check(results, singleMatches, @"Single-pixel and batch CMM results agree");
}

void TestClut(NSMutableArray<NSDictionary *> *results)
{
  for (icUInt16Number channels : {3, 8, 15, 16}) {
    CIccCLUT clut(3, channels);
    if (!Check(results, clut.Init(static_cast<icUInt8Number>(2)),
               [NSString stringWithFormat:@"Allocate 3D CLUT with %u outputs", channels]))
      continue;
    icFloatNumber *data = clut.GetData(0);
    for (icUInt32Number point = 0; point < 8; ++point) {
      for (icUInt16Number channel = 0; channel < channels; ++channel)
        data[point * channels + channel] = point * 0.08f + channel * 0.01f;
    }
    if (!Check(results, clut.Begin(), @"Initialize CLUT interpolation"))
      continue;
    bool matches = true;
    const icFloatNumber probes[][3] = {
      {0, 0, 0}, {1, 1, 1}, {0.5f, 0.25f, 0.75f}, {0.1f, 0.9f, 0.3f}
    };
    for (const auto &input : probes) {
      icFloatNumber tri[16], tetra[16];
      for (int i = 0; i < 16; ++i)
        tri[i] = tetra[i] = -100;
      clut.Interp3d(tri, input);
      clut.Interp3dTetra(tetra, input);
      for (icUInt16Number channel = 0; channel < channels; ++channel) {
        const icFloatNumber expected =
          input[0] * 0.32f + input[1] * 0.16f + input[2] * 0.08f + channel * 0.01f;
        matches &= Near(tri[channel], expected, 1e-6) &&
                   Near(tetra[channel], expected, 1e-6);
      }
    }
    Check(results, matches,
          [NSString stringWithFormat:@"Trilinear/tetrahedral %u-output CLUT matches analytic ramp",
                                     channels]);
  }
}

} // namespace

@interface IccDevAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation IccDevAppDelegate
- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
  (void)application;
  (void)launchOptions;
  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
  UIViewController *controller = [[UIViewController alloc] init];
  UITextView *text = [[UITextView alloc] initWithFrame:self.window.bounds];
  text.editable = NO;
  text.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  text.font = [UIFont monospacedSystemFontOfSize:14 weight:UIFontWeightRegular];
  text.text = @"iccDEV core device tests running...";
  controller.view = text;
  self.window.rootViewController = controller;
  [self.window makeKeyAndVisible];

  dispatch_async(dispatch_get_main_queue(), ^{
    NSMutableArray<NSDictionary *> *results = [NSMutableArray array];
    NSURL *documents = [[[NSFileManager defaultManager]
      URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
    if (Check(results, documents != nil, @"Locate app sandbox"))
      TestProfiles(results, documents);
    TestClut(results);
    bool passed = results.count > 0;
    for (NSDictionary *result in results)
      passed &= [result[@"passed"] boolValue];
    NSDictionary *report = @{
      @"passed": @(passed), @"tests": results,
      @"libraryVersion": @ICCPROFLIBVER,
      @"osVersion": [UIDevice currentDevice].systemVersion
    };
    NSError *error = nil;
    NSData *json = [NSJSONSerialization dataWithJSONObject:report
                      options:NSJSONWritingPrettyPrinted error:&error];
    bool written = json && documents &&
      [json writeToURL:[documents URLByAppendingPathComponent:@"results.json"]
              options:NSDataWritingAtomic error:&error];
    if (!written) {
      passed = false;
      std::fprintf(stderr, "Cannot persist device results: %s\n",
                   error ? error.localizedDescription.UTF8String : "no output location");
    }
    NSMutableString *summary = [NSMutableString stringWithFormat:
      @"iccDEV %s\n%@\n\n", ICCPROFLIBVER, passed ? @"PASS" : @"FAIL"];
    for (NSDictionary *result in results)
      [summary appendFormat:@"%@ %@\n", [result[@"passed"] boolValue] ? @"PASS" : @"FAIL",
                            result[@"test"]];
    if (!written)
      [summary appendString:@"FAIL Persist device results\n"];
    text.text = summary;
    std::printf("ICCDEV_DEVICE_TESTS %s (%lu checks)\n", passed ? "PASS" : "FAIL",
                static_cast<unsigned long>(results.count));
    std::fflush(stdout);
    std::fflush(stderr);
    // Explicit opt-in for devicectl automation; a normal launch keeps the report visible.
    if ([[NSProcessInfo processInfo].arguments containsObject:@"--exit-after-tests"])
      std::exit(passed ? 0 : 1);
  });
  return YES;
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([IccDevAppDelegate class]));
  }
}
