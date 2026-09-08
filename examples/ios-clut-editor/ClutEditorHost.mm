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

#import "ClutEditorHost.h"
#include "IccCmm.h"
#include "IccProfLibVer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

@implementation IccClutEditorResult
@end

static double IccDevClampUnit(double value)
{
  if (std::isnan(value))
    return 0.0;
  if (value < 0.0)
    return 0.0;
  if (value > 1.0)
    return 1.0;
  return value;
}

static icUInt8Number IccDevUnitToByte(double value)
{
  return static_cast<icUInt8Number>(IccDevClampUnit(value) * 255.0 + 0.5);
}

static CGColorSpaceRef IccDevCreateSrgbColorSpace()
{
  CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  if (!colorSpace)
    colorSpace = CGColorSpaceCreateDeviceRGB();
  return colorSpace;
}

static size_t IccDevClutIndex(NSUInteger grid,
                              NSUInteger r,
                              NSUInteger g,
                              NSUInteger b,
                              NSUInteger c)
{
  return (((static_cast<size_t>(b) * grid + g) * grid + r) * 3u) + c;
}

static void IccDevFillDefaultSource(std::vector<icFloatNumber>& src,
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
      const bool checker = (((x / 24u) + (y / 24u)) & 1u) != 0u;
      const icFloatNumber blue = checker ? (1.0f - red) : (1.0f - green);
      const size_t index = (static_cast<size_t>(y) * width + x) * 3u;
      src[index + 0u] = red;
      src[index + 1u] = green;
      src[index + 2u] = blue;
    }
  }
}

static BOOL IccDevDecodeImage(UIImage *image,
                              NSUInteger width,
                              NSUInteger height,
                              std::vector<icFloatNumber>& src)
{
  if (!image || !image.CGImage)
    return NO;

  std::vector<icUInt8Number> rgba(static_cast<size_t>(width) * height * 4u);
  CGColorSpaceRef colorSpace = IccDevCreateSrgbColorSpace();
  CGContextRef context = colorSpace ?
    CGBitmapContextCreate(rgba.data(), width, height, 8, width * 4u,
                          colorSpace,
                          kCGImageAlphaPremultipliedLast |
                            kCGBitmapByteOrder32Big) : NULL;
  if (!context) {
    if (colorSpace)
      CGColorSpaceRelease(colorSpace);
    return NO;
  }

  CGContextSetBlendMode(context, kCGBlendModeCopy);
  UIGraphicsPushContext(context);
  [image drawInRect:CGRectMake(0.0, 0.0, width, height)];
  UIGraphicsPopContext();
  CGContextRelease(context);
  CGColorSpaceRelease(colorSpace);

  const size_t nPixels = static_cast<size_t>(width) * height;
  for (size_t i = 0; i < nPixels; ++i) {
    const size_t p = i * 4u;
    const size_t f = i * 3u;
    src[f + 0u] = static_cast<icFloatNumber>(rgba[p + 0u] / 255.0);
    src[f + 1u] = static_cast<icFloatNumber>(rgba[p + 1u] / 255.0);
    src[f + 2u] = static_cast<icFloatNumber>(rgba[p + 2u] / 255.0);
  }
  return YES;
}

static void IccDevEditRgb(double rgb[3],
                          double exposureStops,
                          double contrast,
                          double saturation,
                          double warmBias)
{
  const double exposure = std::pow(2.0, exposureStops);
  for (size_t c = 0; c < 3u; ++c)
    rgb[c] = (rgb[c] * exposure - 0.5) * contrast + 0.5;

  const double luma =
    rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722;
  for (size_t c = 0; c < 3u; ++c)
    rgb[c] = luma + (rgb[c] - luma) * saturation;

  rgb[0] += warmBias * 0.075;
  rgb[1] += warmBias * 0.015;
  rgb[2] -= warmBias * 0.075;
  for (size_t c = 0; c < 3u; ++c)
    rgb[c] = IccDevClampUnit(rgb[c]);
}

static void IccDevBuildClut(std::vector<icFloatNumber>& clut,
                            NSUInteger grid,
                            double exposureStops,
                            double contrast,
                            double saturation,
                            double warmBias)
{
  for (NSUInteger b = 0; b < grid; ++b) {
    for (NSUInteger g = 0; g < grid; ++g) {
      for (NSUInteger r = 0; r < grid; ++r) {
        double rgb[3] = {
          grid > 1 ? static_cast<double>(r) / static_cast<double>(grid - 1) : 0.0,
          grid > 1 ? static_cast<double>(g) / static_cast<double>(grid - 1) : 0.0,
          grid > 1 ? static_cast<double>(b) / static_cast<double>(grid - 1) : 0.0
        };
        IccDevEditRgb(rgb, exposureStops, contrast, saturation, warmBias);
        for (NSUInteger c = 0; c < 3u; ++c)
          clut[IccDevClutIndex(grid, r, g, b, c)] =
            static_cast<icFloatNumber>(rgb[c]);
      }
    }
  }
}

static void IccDevCorner(const std::vector<icFloatNumber>& clut,
                         NSUInteger grid,
                         NSUInteger r,
                         NSUInteger g,
                         NSUInteger b,
                         double out[3])
{
  for (NSUInteger c = 0; c < 3u; ++c)
    out[c] = clut[IccDevClutIndex(grid, r, g, b, c)];
}

static void IccDevBlend(double out[3],
                        const double a[3],
                        const double b[3],
                        double t)
{
  for (NSUInteger c = 0; c < 3u; ++c)
    out[c] = a[c] + (b[c] - a[c]) * t;
}

static void IccDevEvalLinearClut(const std::vector<icFloatNumber>& clut,
                                 NSUInteger grid,
                                 const icFloatNumber in[3],
                                 icFloatNumber out[3])
{
  const double scale = static_cast<double>(grid - 1);
  const double pr = IccDevClampUnit(in[0]) * scale;
  const double pg = IccDevClampUnit(in[1]) * scale;
  const double pb = IccDevClampUnit(in[2]) * scale;
  const NSUInteger r0 = static_cast<NSUInteger>(std::floor(pr));
  const NSUInteger g0 = static_cast<NSUInteger>(std::floor(pg));
  const NSUInteger b0 = static_cast<NSUInteger>(std::floor(pb));
  const NSUInteger r1 = std::min(r0 + 1u, grid - 1u);
  const NSUInteger g1 = std::min(g0 + 1u, grid - 1u);
  const NSUInteger b1 = std::min(b0 + 1u, grid - 1u);
  const double fr = pr - static_cast<double>(r0);
  const double fg = pg - static_cast<double>(g0);
  const double fb = pb - static_cast<double>(b0);
  double c000[3], c100[3], c010[3], c110[3];
  double c001[3], c101[3], c011[3], c111[3];
  double c00[3], c10[3], c01[3], c11[3], c0[3], c1[3];
  IccDevCorner(clut, grid, r0, g0, b0, c000);
  IccDevCorner(clut, grid, r1, g0, b0, c100);
  IccDevCorner(clut, grid, r0, g1, b0, c010);
  IccDevCorner(clut, grid, r1, g1, b0, c110);
  IccDevCorner(clut, grid, r0, g0, b1, c001);
  IccDevCorner(clut, grid, r1, g0, b1, c101);
  IccDevCorner(clut, grid, r0, g1, b1, c011);
  IccDevCorner(clut, grid, r1, g1, b1, c111);
  IccDevBlend(c00, c000, c100, fr);
  IccDevBlend(c10, c010, c110, fr);
  IccDevBlend(c01, c001, c101, fr);
  IccDevBlend(c11, c011, c111, fr);
  IccDevBlend(c0, c00, c10, fg);
  IccDevBlend(c1, c01, c11, fg);
  IccDevBlend(c0, c0, c1, fb);
  for (NSUInteger c = 0; c < 3u; ++c)
    out[c] = static_cast<icFloatNumber>(IccDevClampUnit(c0[c]));
}

static void IccDevEvalTetraClut(const std::vector<icFloatNumber>& clut,
                                NSUInteger grid,
                                const icFloatNumber in[3],
                                icFloatNumber out[3])
{
  const double scale = static_cast<double>(grid - 1);
  const double pr = IccDevClampUnit(in[0]) * scale;
  const double pg = IccDevClampUnit(in[1]) * scale;
  const double pb = IccDevClampUnit(in[2]) * scale;
  const NSUInteger r0 = static_cast<NSUInteger>(std::floor(pr));
  const NSUInteger g0 = static_cast<NSUInteger>(std::floor(pg));
  const NSUInteger b0 = static_cast<NSUInteger>(std::floor(pb));
  const NSUInteger r1 = std::min(r0 + 1u, grid - 1u);
  const NSUInteger g1 = std::min(g0 + 1u, grid - 1u);
  const NSUInteger b1 = std::min(b0 + 1u, grid - 1u);
  const double fr = pr - static_cast<double>(r0);
  const double fg = pg - static_cast<double>(g0);
  const double fb = pb - static_cast<double>(b0);
  double c000[3], c100[3], c010[3], c110[3];
  double c001[3], c101[3], c011[3], c111[3];
  IccDevCorner(clut, grid, r0, g0, b0, c000);
  IccDevCorner(clut, grid, r1, g0, b0, c100);
  IccDevCorner(clut, grid, r0, g1, b0, c010);
  IccDevCorner(clut, grid, r1, g1, b0, c110);
  IccDevCorner(clut, grid, r0, g0, b1, c001);
  IccDevCorner(clut, grid, r1, g0, b1, c101);
  IccDevCorner(clut, grid, r0, g1, b1, c011);
  IccDevCorner(clut, grid, r1, g1, b1, c111);

  for (NSUInteger c = 0; c < 3u; ++c) {
    double value = c000[c];
    if (fr >= fg) {
      if (fg >= fb) {
        value += fr * (c100[c] - c000[c]);
        value += fg * (c110[c] - c100[c]);
        value += fb * (c111[c] - c110[c]);
      }
      else if (fr >= fb) {
        value += fr * (c100[c] - c000[c]);
        value += fb * (c101[c] - c100[c]);
        value += fg * (c111[c] - c101[c]);
      }
      else {
        value += fb * (c001[c] - c000[c]);
        value += fr * (c101[c] - c001[c]);
        value += fg * (c111[c] - c101[c]);
      }
    }
    else {
      if (fr >= fb) {
        value += fg * (c010[c] - c000[c]);
        value += fr * (c110[c] - c010[c]);
        value += fb * (c111[c] - c110[c]);
      }
      else if (fg >= fb) {
        value += fg * (c010[c] - c000[c]);
        value += fb * (c011[c] - c010[c]);
        value += fr * (c111[c] - c011[c]);
      }
      else {
        value += fb * (c001[c] - c000[c]);
        value += fg * (c011[c] - c001[c]);
        value += fr * (c111[c] - c011[c]);
      }
    }
    out[c] = static_cast<icFloatNumber>(IccDevClampUnit(value));
  }
}

static UIImage *IccDevMakeImage(const std::vector<icFloatNumber>& rgb,
                                NSUInteger width,
                                NSUInteger height)
{
  std::vector<icUInt8Number> rgba(static_cast<size_t>(width) * height * 4u);
  const size_t nPixels = static_cast<size_t>(width) * height;
  for (size_t i = 0; i < nPixels; ++i) {
    const size_t f = i * 3u;
    const size_t p = i * 4u;
    rgba[p + 0u] = IccDevUnitToByte(rgb[f + 0u]);
    rgba[p + 1u] = IccDevUnitToByte(rgb[f + 1u]);
    rgba[p + 2u] = IccDevUnitToByte(rgb[f + 2u]);
    rgba[p + 3u] = 255u;
  }

  NSData *data = [NSData dataWithBytes:rgba.data() length:rgba.size()];
  CGDataProviderRef provider =
    CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
  CGColorSpaceRef colorSpace = IccDevCreateSrgbColorSpace();
  if (!provider || !colorSpace) {
    if (colorSpace)
      CGColorSpaceRelease(colorSpace);
    if (provider)
      CGDataProviderRelease(provider);
    return nil;
  }
  CGImageRef image = CGImageCreate(width, height, 8, 32, width * 4u,
                                   colorSpace,
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

static void IccDevApplyClut(const std::vector<icFloatNumber>& clut,
                            NSUInteger grid,
                            BOOL useTetrahedral,
                            const std::vector<icFloatNumber>& managed,
                            std::vector<icFloatNumber>& edited)
{
  const size_t nPixels = managed.size() / 3u;
  for (size_t i = 0; i < nPixels; ++i) {
    const size_t f = i * 3u;
    if (useTetrahedral)
      IccDevEvalTetraClut(clut, grid, &managed[f], &edited[f]);
    else
      IccDevEvalLinearClut(clut, grid, &managed[f], &edited[f]);
  }
}

static void IccDevMeasureEdit(const std::vector<icFloatNumber>& managed,
                              const std::vector<icFloatNumber>& edited,
                              std::vector<icFloatNumber>& delta,
                              double& meanDelta,
                              double& maxDelta,
                              icUInt32Number& checksum)
{
  checksum = 2166136261u;
  double sumChannelDelta = 0.0;
  maxDelta = 0.0;
  const size_t nPixels = managed.size() / 3u;
  for (size_t i = 0; i < nPixels; ++i) {
    const size_t f = i * 3u;
    double pixelDelta = 0.0;
    for (size_t c = 0; c < 3u; ++c) {
      const double channelDelta =
        std::fabs(static_cast<double>(edited[f + c] - managed[f + c]));
      sumChannelDelta += channelDelta;
      pixelDelta = std::max(pixelDelta, channelDelta);
      delta[f + c] = static_cast<icFloatNumber>(
        IccDevClampUnit(channelDelta * 8.0));
      checksum ^= IccDevUnitToByte(edited[f + c]);
      checksum *= 16777619u;
    }
    maxDelta = std::max(maxDelta, pixelDelta);
  }
  meanDelta = nPixels ?
    sumChannelDelta / (static_cast<double>(nPixels) * 3.0) : 0.0;
}

void IccDevFinishClutEditor(IccClutEditorResult *result)
{
  std::printf("ICCDEV_CLUTEDITOR_TESTS %s\n",
              result.passed ? "PASS" : "FAIL");
  std::fflush(stdout);
  if ([[NSProcessInfo processInfo].arguments containsObject:@"--exit-after-tests"])
    std::exit(result.passed ? 0 : 1);
}

BOOL IccDevPersistClutEditorReport(IccClutEditorResult *result,
                                   NSString **failure)
{
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSURL *documents = [[fileManager
    URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] firstObject];
  NSURL *reportURL = documents
    ? [documents URLByAppendingPathComponent:@"clut-editor-report.json"] : nil;
  NSError *error = nil;
  if (!reportURL) {
    if (failure)
      *failure = @"resolve Documents/clut-editor-report.json";
    return NO;
  }
  if ([fileManager fileExistsAtPath:reportURL.path] &&
      ![fileManager removeItemAtURL:reportURL error:&error]) {
    if (failure) {
      *failure = [NSString stringWithFormat:@"remove stale device results: %@",
        error ? error.localizedDescription : @"remove failed"];
    }
    return NO;
  }
  if (!result.jsonReport)
    return YES;

  NSData *json = [NSJSONSerialization dataWithJSONObject:result.jsonReport
                                                 options:NSJSONWritingPrettyPrinted
                                                   error:&error];
  if (!json) {
    if (failure) {
      *failure = [NSString stringWithFormat:@"serialize device results: %@",
        error ? error.localizedDescription : @"serialize failed"];
    }
    return NO;
  }
  if (![json writeToURL:reportURL options:NSDataWritingAtomic error:&error]) {
    if (failure) {
      *failure = [NSString stringWithFormat:@"write device results: %@",
        error ? error.localizedDescription : @"write failed"];
    }
    return NO;
  }
  return YES;
}

IccClutEditorResult *IccDevRunClutEditor(UIImage *selectedImage,
                                         NSUInteger edgePixels,
                                         NSUInteger gridPoints,
                                         double exposureStops,
                                         double contrast,
                                         double saturation,
                                         double warmBias,
                                         BOOL useTetrahedral)
{
  @autoreleasepool {
    IccClutEditorResult *result = [[IccClutEditorResult alloc] init];
    result.passed = NO;
    if (edgePixels < 96u)
      edgePixels = 96u;
    if (edgePixels > 512u)
      edgePixels = 512u;
    if (gridPoints < 3u)
      gridPoints = 3u;
    if (gridPoints > 33u)
      gridPoints = 33u;

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
      @"ICCDEV CLUT EDITOR\n"
       "Profile and 3D LUT iOS POC\n"
       "==========================\n\n"
       "Run: %@\n"
       "App: %@ (%@)\n"
       "ICC: https://www.color.org/\n"
       "Repo: https://github.com/InternationalColorConsortium/iccDEV\n"
       "Library: IccProfLib %s\n"
       "Platform: iOS %ld.%ld.%ld\n\n",
      runDate, shortVersion, buildVersion, ICCPROFLIBVER,
      static_cast<long>(os.majorVersion), static_cast<long>(os.minorVersion),
      static_cast<long>(os.patchVersion)];

    const NSUInteger width = edgePixels;
    const NSUInteger height = edgePixels;
    const size_t nPixels = static_cast<size_t>(width) * height;
    std::vector<icFloatNumber> src(nPixels * 3u);
    BOOL usedPickedImage = IccDevDecodeImage(selectedImage, width, height, src);
    if (!usedPickedImage)
      IccDevFillDefaultSource(src, width, height);

    NSString *srcPath =
      [[NSBundle mainBundle] pathForResource:@"sRGB_v4_ICC_preference"
                                      ofType:@"icc"];
    NSString *dstPath =
      [[NSBundle mainBundle] pathForResource:@"sRGB_D65_MAT" ofType:@"icc"];
    if (!srcPath || !dstPath) {
      [report appendString:@"FAIL\n\nBundled RGB fixtures not found.\n"];
      result.report = report;
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
      [report appendFormat:@"FAIL\n\nBuild RGB editor CMM: %s\n",
        CIccCmm::GetStatusText(status)];
      result.report = report;
      return result;
    }

    std::vector<icFloatNumber> managed(nPixels * 3u);
    status = cmm.Apply(managed.data(), src.data(),
                       static_cast<icUInt32Number>(nPixels));
    if (status != icCmmStatOk) {
      [report appendFormat:@"FAIL\n\nApply RGB editor CMM: %s\n",
        CIccCmm::GetStatusText(status)];
      result.report = report;
      return result;
    }

    std::vector<icFloatNumber> clut(
      static_cast<size_t>(gridPoints) * gridPoints * gridPoints * 3u);
    std::vector<icFloatNumber> edited(nPixels * 3u);
    std::vector<icFloatNumber> delta(nPixels * 3u);
    IccDevBuildClut(clut, gridPoints, exposureStops, contrast, saturation,
                    warmBias);
    IccDevApplyClut(clut, gridPoints, useTetrahedral, managed, edited);

    double meanDelta = 0.0;
    double maxDelta = 0.0;
    icUInt32Number checksum = 0;
    IccDevMeasureEdit(managed, edited, delta, meanDelta, maxDelta, checksum);
    result.sourceImage = IccDevMakeImage(src, width, height);
    result.managedImage = IccDevMakeImage(managed, width, height);
    result.editedImage = IccDevMakeImage(edited, width, height);
    result.deltaImage = IccDevMakeImage(delta, width, height);
    result.passed =
      result.sourceImage != nil && result.managedImage != nil &&
      result.editedImage != nil && result.deltaImage != nil;

    NSDictionary *jsonReport = @{
      @"passed": @(result.passed),
      @"edgePixels": @(edgePixels),
      @"selectedImage": @(usedPickedImage),
      @"profileChain": @"sRGB_v4_ICC_preference.icc -> sRGB_D65_MAT.icc",
      @"renderingIntent": @"relative colorimetric",
      @"clutGridPoints": @(gridPoints),
      @"clutInterpolation": interpName,
      @"exposureStops": @(exposureStops),
      @"contrast": @(contrast),
      @"saturation": @(saturation),
      @"warmBias": @(warmBias),
      @"meanChannelDelta": @(meanDelta),
      @"maxChannelDelta": @(maxDelta),
      @"checksum": [NSString stringWithFormat:@"0x%08x", checksum],
      @"libraryVersion": @ICCPROFLIBVER
    };
    result.jsonReport = jsonReport;

    [report appendFormat:
      @"%@\n\n"
       "Input image: %@\n"
       "Profile chain: sRGB_v4_ICC_preference.icc -> sRGB_D65_MAT.icc\n"
       "Intent: relative colorimetric\n"
       "CLUT grid: %lu points per axis\n"
       "CLUT interpolation: %@\n"
       "Exposure: %.3f stops\n"
       "Contrast: %.3f\n"
       "Saturation: %.3f\n"
       "Warm bias: %.3f\n"
       "Preview image: %lux%lu RGB\n\n"
       "Mean channel delta: %.9f\n"
       "Max channel delta:  %.9f\n"
       "Edited checksum:    0x%08x\n\n"
       "Report: Documents/clut-editor-report.json\n",
      result.passed ? @"PASS" : @"FAIL",
      usedPickedImage ? @"photo library selection" : @"bundled generated default",
      static_cast<unsigned long>(gridPoints), interpName, exposureStops,
      contrast, saturation, warmBias, static_cast<unsigned long>(width),
      static_cast<unsigned long>(height), meanDelta, maxDelta, checksum];
    result.report = report;
    return result;
  }
}
