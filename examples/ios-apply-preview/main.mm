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
#import "ApplyPreviewHost.h"

@interface IccApplyPreviewAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

static UIColor *IccDevBrandBlue(void)
{
  if (@available(iOS 13.0, *)) {
    return [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *traits) {
      if (traits.userInterfaceStyle == UIUserInterfaceStyleDark) {
        return [UIColor colorWithRed:91.0 / 255.0
                               green:173.0 / 255.0
                                blue:217.0 / 255.0
                               alpha:1.0];
      }
      return [UIColor colorWithRed:3.0 / 255.0
                             green:102.0 / 255.0
                              blue:153.0 / 255.0
                             alpha:1.0];
    }];
  }
  return [UIColor colorWithRed:3.0 / 255.0
                         green:102.0 / 255.0
                          blue:153.0 / 255.0
                         alpha:1.0];
}

@implementation IccApplyPreviewAppDelegate
- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
  (void)application;
  (void)launchOptions;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
#pragma clang diagnostic pop
  UIViewController *controller = [[UIViewController alloc] init];
  controller.view.backgroundColor = [UIColor systemBackgroundColor];

  UIImageView *logo = [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"ICCLogo"]];
  logo.contentMode = UIViewContentModeScaleAspectFit;
  logo.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel *title = [[UILabel alloc] init];
  title.text = @"International Color Consortium";
  title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  title.textAlignment = NSTextAlignmentCenter;
  title.adjustsFontForContentSizeCategory = YES;
  title.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel *subtitle = [[UILabel alloc] init];
  subtitle.text = @"Making color seamless between devices and documents";
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  subtitle.textAlignment = NSTextAlignmentCenter;
  subtitle.textColor = [UIColor secondaryLabelColor];
  subtitle.numberOfLines = 0;
  subtitle.adjustsFontForContentSizeCategory = YES;
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton *iccLink = [UIButton buttonWithType:UIButtonTypeSystem];
  [iccLink setTitle:@"International Color Consortium - color.org"
           forState:UIControlStateNormal];
  iccLink.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  iccLink.translatesAutoresizingMaskIntoConstraints = NO;
  [iccLink addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    NSURL *url = [NSURL URLWithString:@"https://www.color.org/"];
    [[UIApplication sharedApplication] openURL:url options:@{}
                             completionHandler:nil];
  }] forControlEvents:UIControlEventTouchUpInside];

  UIButton *repoLink = [UIButton buttonWithType:UIButtonTypeSystem];
  [repoLink setTitle:@"iccDEV Repository" forState:UIControlStateNormal];
  repoLink.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  repoLink.translatesAutoresizingMaskIntoConstraints = NO;
  [repoLink addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    NSURL *url =
      [NSURL URLWithString:@"https://github.com/InternationalColorConsortium/iccDEV"];
    [[UIApplication sharedApplication] openURL:url options:@{}
                             completionHandler:nil];
  }] forControlEvents:UIControlEventTouchUpInside];

  UISegmentedControl *size =
    [[UISegmentedControl alloc] initWithItems:@[@"128", @"256", @"384"]];
  size.selectedSegmentIndex = 1;
  size.translatesAutoresizingMaskIntoConstraints = NO;

  UISegmentedControl *interpolation =
    [[UISegmentedControl alloc] initWithItems:@[@"Linear", @"Tetra"]];
  interpolation.selectedSegmentIndex = 0;
  interpolation.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton *run = [UIButton buttonWithType:UIButtonTypeSystem];
  UIButtonConfiguration *runConfig = [UIButtonConfiguration filledButtonConfiguration];
  runConfig.title = @"Run Apply Preview";
  runConfig.baseBackgroundColor = IccDevBrandBlue();
  runConfig.baseForegroundColor = [UIColor whiteColor];
  runConfig.contentInsets = NSDirectionalEdgeInsetsMake(10, 16, 10, 16);
  run.configuration = runConfig;
  run.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  run.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton *share = [UIButton buttonWithType:UIButtonTypeSystem];
  UIButtonConfiguration *shareConfig =
    [UIButtonConfiguration borderedButtonConfiguration];
  shareConfig.title = @"Share Report";
  shareConfig.baseForegroundColor = IccDevBrandBlue();
  shareConfig.contentInsets = NSDirectionalEdgeInsetsMake(10, 16, 10, 16);
  share.configuration = shareConfig;
  share.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  share.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageView *source = [[UIImageView alloc] init];
  UIImageView *applied = [[UIImageView alloc] init];
  UIImageView *delta = [[UIImageView alloc] init];
  NSArray<UIImageView *> *images = @[source, applied, delta];
  for (UIImageView *image in images) {
    image.contentMode = UIViewContentModeScaleAspectFit;
    image.backgroundColor = [UIColor secondarySystemBackgroundColor];
    image.layer.borderColor = [UIColor separatorColor].CGColor;
    image.layer.borderWidth = 1.0;
    image.translatesAutoresizingMaskIntoConstraints = NO;
  }

  UILabel *(^label)(NSString *) = ^UILabel *(NSString *text) {
    UILabel *view = [[UILabel alloc] init];
    view.text = text;
    view.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    view.numberOfLines = 0;
    view.adjustsFontForContentSizeCategory = YES;
    view.translatesAutoresizingMaskIntoConstraints = NO;
    return view;
  };

  UILabel *deltaNote = [[UILabel alloc] init];
  deltaNote.text =
    @"Expected: the delta image is now colorful. It shows per-channel "
     "differences amplified 12x from sRGB_v4_ICC_preference.icc into "
     "sRGB_D65_MAT.icc.";
  deltaNote.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  deltaNote.textColor = [UIColor secondaryLabelColor];
  deltaNote.numberOfLines = 0;
  deltaNote.adjustsFontForContentSizeCategory = YES;
  deltaNote.translatesAutoresizingMaskIntoConstraints = NO;

  UITextView *report = [[UITextView alloc] initWithFrame:CGRectZero];
  report.editable = NO;
  report.selectable = YES;
  report.scrollEnabled = NO;
  report.backgroundColor = [UIColor secondarySystemBackgroundColor];
  report.textColor = [UIColor labelColor];
  report.textContainerInset = UIEdgeInsetsMake(12, 12, 12, 12);
  report.font = [UIFont monospacedSystemFontOfSize:14 weight:UIFontWeightRegular];
  report.text = @"Apply preview ready.";
  report.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:@[
    logo,
    title,
    subtitle,
    iccLink,
    repoLink,
    label(@"Image size, edge pixels"),
    size,
    label(@"Interpolation"),
    interpolation,
    run,
    share,
    label(@"Source image"),
    source,
    label(@"Applied image"),
    applied,
    label(@"Color delta, amplified 12x"),
    delta,
    deltaNote,
    report
  ]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.alignment = UIStackViewAlignmentFill;
  stack.distribution = UIStackViewDistributionFill;
  stack.spacing = 12;
  stack.translatesAutoresizingMaskIntoConstraints = NO;

  UIScrollView *scroll = [[UIScrollView alloc] init];
  scroll.translatesAutoresizingMaskIntoConstraints = NO;
  [scroll addSubview:stack];
  [controller.view addSubview:scroll];

  __weak UISegmentedControl *weakSize = size;
  __weak UISegmentedControl *weakInterpolation = interpolation;
  __weak UIImageView *weakSource = source;
  __weak UIImageView *weakApplied = applied;
  __weak UIImageView *weakDelta = delta;
  __weak UITextView *weakReport = report;
  __weak UIButton *weakRun = run;
  __weak UIButton *weakShare = share;
  __weak UIViewController *weakController = controller;

  void (^runPreview)(void) = ^{
    UISegmentedControl *strongSize = weakSize;
    UISegmentedControl *strongInterpolation = weakInterpolation;
    UITextView *strongReport = weakReport;
    UIButton *strongRun = weakRun;
    if (!strongSize || !strongInterpolation || !strongReport || !strongRun)
      return;
    NSUInteger edge = 256u;
    if (strongSize.selectedSegmentIndex == 0)
      edge = 128u;
    else if (strongSize.selectedSegmentIndex == 2)
      edge = 384u;
    const BOOL useTetrahedral = strongInterpolation.selectedSegmentIndex == 1;
    strongReport.text = @"Apply preview running...";
    strongRun.enabled = NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      IccApplyPreviewResult *result =
        IccDevRunApplyPreview(edge, useTetrahedral);
      dispatch_async(dispatch_get_main_queue(), ^{
        UIImageView *nestedSource = weakSource;
        UIImageView *nestedApplied = weakApplied;
        UIImageView *nestedDelta = weakDelta;
        UITextView *nestedReport = weakReport;
        UIButton *nestedRun = weakRun;
        if (!nestedSource || !nestedApplied || !nestedDelta ||
            !nestedReport || !nestedRun)
          return;
        nestedSource.image = result.sourceImage;
        nestedApplied.image = result.appliedImage;
        nestedDelta.image = result.deltaImage;
        nestedReport.text = result.report;
        nestedRun.enabled = YES;
      });
    });
  };

  [run addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    runPreview();
  }] forControlEvents:UIControlEventTouchUpInside];

  [share addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    UITextView *strongReport = weakReport;
    UIButton *strongShare = weakShare;
    UIViewController *strongController = weakController;
    if (!strongReport || !strongShare || !strongController)
      return;
    UIActivityViewController *activity =
      [[UIActivityViewController alloc]
        initWithActivityItems:@[strongReport.text ?: @""]
        applicationActivities:nil];
    activity.popoverPresentationController.sourceView = strongShare;
    activity.popoverPresentationController.sourceRect = strongShare.bounds;
    [strongController presentViewController:activity animated:YES
                                 completion:nil];
  }] forControlEvents:UIControlEventTouchUpInside];

  UILayoutGuide *safeArea = controller.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [scroll.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor],
    [scroll.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor],
    [scroll.topAnchor constraintEqualToAnchor:safeArea.topAnchor],
    [scroll.bottomAnchor constraintEqualToAnchor:safeArea.bottomAnchor],
    [logo.heightAnchor constraintLessThanOrEqualToConstant:120],
    [logo.heightAnchor constraintGreaterThanOrEqualToConstant:72],
    [stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor
                                        constant:16],
    [stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor
                                         constant:-16],
    [stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor
                                    constant:16],
    [stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor
                                       constant:-16],
    [stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor
                                      constant:-32],
    [iccLink.heightAnchor constraintGreaterThanOrEqualToConstant:32],
    [repoLink.heightAnchor constraintGreaterThanOrEqualToConstant:32],
    [size.heightAnchor constraintGreaterThanOrEqualToConstant:36],
    [interpolation.heightAnchor constraintGreaterThanOrEqualToConstant:36],
    [run.heightAnchor constraintGreaterThanOrEqualToConstant:44],
    [share.heightAnchor constraintGreaterThanOrEqualToConstant:44],
    [source.heightAnchor constraintEqualToAnchor:source.widthAnchor],
    [applied.heightAnchor constraintEqualToAnchor:applied.widthAnchor],
    [delta.heightAnchor constraintEqualToAnchor:delta.widthAnchor]
  ]];

  self.window.rootViewController = controller;
  [self.window makeKeyAndVisible];
  runPreview();
  return YES;
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil,
                             NSStringFromClass([IccApplyPreviewAppDelegate class]));
  }
}
