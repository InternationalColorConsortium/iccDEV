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

#import "ApplyPreviewHost.h"
#include "IccCmm.h"
#include "IccProfLibVer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

@implementation IccApplyPreviewResult
@end

static icUInt8Number IccDevUnitToByte(icFloatNumber value)
{
  if (std::isnan(value))
    return 0;
  if (value < 0.0f)
    return 0;
  if (value > 1.0f)
    return 255;
  return static_cast<icUInt8Number>(value * 255.0f + 0.5f);
}

static UIImage *IccDevMakeImage(const std::vector<icUInt8Number>& rgba,
                                NSUInteger width,
                                NSUInteger height)
{
  NSData *data = [NSData dataWithBytes:rgba.data() length:rgba.size()];
  CGDataProviderRef provider =
    CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  if (!provider || !colorSpace) {
    if (colorSpace)
      CGColorSpaceRelease(colorSpace);
    if (provider)
      CGDataProviderRelease(provider);
    return nil;
  }
  CGImageRef image = CGImageCreate(width, height, 8, 32, width * 4, colorSpace,
                                   kCGBitmapByteOrder32Big |
                                     kCGImageAlphaPremultipliedLast,
                                   provider, NULL, false,
                                   kCGRenderingIntentDefault);
  UIImage *result = image ? [UIImage imageWithCGImage:image] : nil;
  if (image)
    CGImageRelease(image);
  CGColorSpaceRelease(colorSpace);
  CGDataProviderRelease(provider);
  return result;
}

static void IccDevFillPreviewSource(std::vector<icFloatNumber>& src,
                                    NSUInteger width,
                                    NSUInteger height)
{
  for (NSUInteger y = 0; y < height; ++y) {
    const icFloatNumber green =
      height > 1 ? static_cast<icFloatNumber>(y) /
                     static_cast<icFloatNumber>(height - 1) : 0.0f;
    for (NSUInteger x = 0; x < width; ++x) {
      const icFloatNumber red =
        width > 1 ? static_cast<icFloatNumber>(x) /
                      static_cast<icFloatNumber>(width - 1) : 0.0f;
      const bool checker = (((x / 32u) + (y / 32u)) & 1u) != 0u;
      const icFloatNumber blue = checker ? (1.0f - red) : (1.0f - green);
      const size_t index = (static_cast<size_t>(y) * width + x) * 3u;
      src[index + 0u] = red;
      src[index + 1u] = green;
      src[index + 2u] = blue;
    }
  }
}

static void IccDevEncodeImages(const std::vector<icFloatNumber>& src,
                               const std::vector<icFloatNumber>& dst,
                               std::vector<icUInt8Number>& srcRgba,
                               std::vector<icUInt8Number>& dstRgba,
                               std::vector<icUInt8Number>& deltaRgba,
                               double& meanDelta,
                               double& maxDelta,
                               icUInt32Number& checksum)
{
  checksum = 2166136261u;
  double sumDelta = 0.0;
  maxDelta = 0.0;
  const size_t nPixels = src.size() / 3u;

  for (size_t i = 0; i < nPixels; ++i) {
    const size_t f = i * 3u;
    const size_t p = i * 4u;
    double pixelDelta = 0.0;
    for (size_t c = 0; c < 3u; ++c) {
      const icUInt8Number srcByte = IccDevUnitToByte(src[f + c]);
      const icUInt8Number dstByte = IccDevUnitToByte(dst[f + c]);
      srcRgba[p + c] = srcByte;
      dstRgba[p + c] = dstByte;
      deltaRgba[p + c] =
        IccDevUnitToByte(static_cast<icFloatNumber>(
          std::fabs(static_cast<double>(dst[f + c] - src[f + c])) * 12.0));
      checksum ^= dstByte;
      checksum *= 16777619u;
      const double channelDelta =
        std::fabs(static_cast<double>(dst[f + c] - src[f + c]));
      pixelDelta = std::max(pixelDelta, channelDelta);
    }
    srcRgba[p + 3u] = 255;
    dstRgba[p + 3u] = 255;
    deltaRgba[p + 3u] = 255;
    sumDelta += pixelDelta;
    maxDelta = std::max(maxDelta, pixelDelta);
  }

  meanDelta = nPixels ? sumDelta / static_cast<double>(nPixels) : 0.0;
}

static void IccDevFinishApplyPreview(IccApplyPreviewResult *result)
{
  std::printf("ICCDEV_APPLYPREVIEW_TESTS %s\n",
              result.passed ? "PASS" : "FAIL");
  std::fflush(stdout);
  if ([[NSProcessInfo processInfo].arguments containsObject:@"--exit-after-tests"])
    std::exit(result.passed ? 0 : 1);
}

IccApplyPreviewResult *IccDevRunApplyPreview(NSUInteger edgePixels,
                                             BOOL useTetrahedral)
{
  @autoreleasepool {
    IccApplyPreviewResult *result = [[IccApplyPreviewResult alloc] init];
    result.passed = NO;
    if (edgePixels < 32u)
      edgePixels = 32u;
    if (edgePixels > 512u)
      edgePixels = 512u;

    NSOperatingSystemVersion os =
      [NSProcessInfo processInfo].operatingSystemVersion;
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    formatter.dateStyle = NSDateFormatterMediumStyle;
    formatter.timeStyle = NSDateFormatterMediumStyle;
    NSString *runDate = [formatter stringFromDate:[NSDate date]];
    NSDictionary *info = [[NSBundle mainBundle] infoDictionary];
    NSString *shortVersion = info[@"CFBundleShortVersionString"] ?: @"unknown";
    NSString *buildVersion = info[@"CFBundleVersion"] ?: @"unknown";
    NSString *interpName = useTetrahedral ? @"tetrahedral" : @"linear";
    NSMutableString *report = [NSMutableString stringWithFormat:
      @"ICCDEV APPLY PREVIEW\n"
       "iccApplyProfiles-inspired iOS POC\n"
       "=================================\n\n"
       "Run: %@\n"
       "App: %@ (%@)\n"
       "ICC: https://www.color.org/\n"
       "Repo: https://github.com/InternationalColorConsortium/iccDEV\n"
       "Library: IccProfLib %s\n"
       "Platform: iOS %ld.%ld.%ld\n\n",
      runDate, shortVersion, buildVersion, ICCPROFLIBVER,
      static_cast<long>(os.majorVersion), static_cast<long>(os.minorVersion),
      static_cast<long>(os.patchVersion)];

    NSString *srcPath =
      [[NSBundle mainBundle] pathForResource:@"sRGB_v4_ICC_preference"
                                      ofType:@"icc"];
    NSString *dstPath =
      [[NSBundle mainBundle] pathForResource:@"sRGB_D65_MAT" ofType:@"icc"];
    if (!srcPath || !dstPath) {
      [report appendString:@"FAIL\n\nBundled RGB fixtures not found.\n"];
      result.report = report;
      IccDevFinishApplyPreview(result);
      return result;
    }

    const icXformInterp interpolation =
      useTetrahedral ? icInterpTetrahedral : icInterpLinear;
    CIccCmm cmm(icSigRgbData, icSigRgbData, true);
    icStatusCMM status =
      cmm.AddXform(srcPath.fileSystemRepresentation, icRelativeColorimetric,
                   interpolation);
    if (status == icCmmStatOk) {
      status =
        cmm.AddXform(dstPath.fileSystemRepresentation, icRelativeColorimetric,
                     interpolation);
    }
    if (status == icCmmStatOk)
      status = cmm.Begin();
    if (status != icCmmStatOk) {
      [report appendFormat:@"FAIL\n\nBuild RGB preview CMM: %s\n",
        CIccCmm::GetStatusText(status)];
      result.report = report;
      IccDevFinishApplyPreview(result);
      return result;
    }

    const NSUInteger width = edgePixels;
    const NSUInteger height = edgePixels;
    const size_t nPixels = static_cast<size_t>(width) * height;
    std::vector<icFloatNumber> src(nPixels * 3u);
    std::vector<icFloatNumber> dst(nPixels * 3u);
    std::vector<icUInt8Number> srcRgba(nPixels * 4u);
    std::vector<icUInt8Number> dstRgba(nPixels * 4u);
    std::vector<icUInt8Number> deltaRgba(nPixels * 4u);
    IccDevFillPreviewSource(src, width, height);
    status = cmm.Apply(dst.data(), src.data(),
                       static_cast<icUInt32Number>(nPixels));
    if (status != icCmmStatOk) {
      [report appendFormat:@"FAIL\n\nApply RGB preview CMM: %s\n",
        CIccCmm::GetStatusText(status)];
      result.report = report;
      IccDevFinishApplyPreview(result);
      return result;
    }

    double meanDelta = 0.0;
    double maxDelta = 0.0;
    icUInt32Number checksum = 0;
    IccDevEncodeImages(src, dst, srcRgba, dstRgba, deltaRgba,
                       meanDelta, maxDelta, checksum);
    result.sourceImage = IccDevMakeImage(srcRgba, width, height);
    result.appliedImage = IccDevMakeImage(dstRgba, width, height);
    result.deltaImage = IccDevMakeImage(deltaRgba, width, height);
    result.passed =
      result.sourceImage != nil && result.appliedImage != nil &&
      result.deltaImage != nil;

    NSURL *documents = [[[NSFileManager defaultManager]
      URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
    NSDictionary *jsonReport = @{
      @"passed": @(result.passed),
      @"edgePixels": @(edgePixels),
      @"profileChain": @"sRGB_v4_ICC_preference.icc -> sRGB_D65_MAT.icc",
      @"renderingIntent": @"relative colorimetric",
      @"interpolation": interpName,
      @"meanChannelDelta": @(meanDelta),
      @"maxChannelDelta": @(maxDelta),
      @"checksum": [NSString stringWithFormat:@"0x%08x", checksum],
      @"libraryVersion": @ICCPROFLIBVER
    };
    NSError *error = nil;
    NSData *json = [NSJSONSerialization dataWithJSONObject:jsonReport
                                                   options:NSJSONWritingPrettyPrinted
                                                     error:&error];
    const BOOL written = json && documents &&
      [json writeToURL:[documents URLByAppendingPathComponent:
                          @"apply-preview-report.json"]
               options:NSDataWritingAtomic error:&error];
    NSString *persistFailure = nil;
    if (!written) {
      result.passed = NO;
      persistFailure =
        error ? error.localizedDescription : @"no output location";
    }
    [report appendFormat:
      @"%@\n\n"
       "Chain: sRGB_v4_ICC_preference.icc -> sRGB_D65_MAT.icc\n"
       "Intent: relative colorimetric\n"
       "Interpolation: %@\n"
       "Generated image: %lux%lu RGB ramp and swatch grid\n"
       "Preview panels: source, applied, per-channel delta amplified 12x\n\n"
       "Mean channel delta: %.9f\n"
       "Max channel delta:  %.9f\n"
       "Applied checksum:   0x%08x\n\n"
       "Report: Documents/apply-preview-report.json\n",
      result.passed ? @"PASS" : @"FAIL", interpName,
      static_cast<unsigned long>(width), static_cast<unsigned long>(height),
      meanDelta, maxDelta, checksum];
    if (persistFailure)
      [report appendFormat:@"FAIL Persist device results: %@\n",
        persistFailure];
    result.report = report;
    IccDevFinishApplyPreview(result);
    return result;
  }
}
