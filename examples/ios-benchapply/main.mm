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
#import "BenchApplyHost.h"

@interface IccBenchApplyAppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation IccBenchApplyAppDelegate
- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
  (void)application;
  (void)launchOptions;
  // initWithFrame:[UIScreen mainScreen].bounds is deprecated starting iOS 26
  // in favour of a windowScene-based initializer, but this POC intentionally
  // keeps the app-delegate-owns-the-window pattern the Build/AppleMobile core
  // smoke app also uses (Testing/AppleMobile/main.mm), rather than adding a
  // full UIScene lifecycle for a proof of concept. Silenced narrowly, once,
  // rather than disabling -Wdeprecated-declarations project-wide.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
#pragma clang diagnostic pop
  UIViewController *controller = [[UIViewController alloc] init];
  controller.view.backgroundColor = [UIColor systemBackgroundColor];
  NSString *logoPath = [[NSBundle mainBundle] pathForResource:@"icc-logo" ofType:@"png"];
  UIImageView *logo = [[UIImageView alloc]
    initWithImage:logoPath ? [UIImage imageWithContentsOfFile:logoPath] : nil];
  logo.contentMode = UIViewContentModeScaleAspectFit;
  logo.translatesAutoresizingMaskIntoConstraints = NO;
  UIButton *iccLink = [UIButton buttonWithType:UIButtonTypeSystem];
  [iccLink setTitle:@"International Color Consortium - color.org" forState:UIControlStateNormal];
  iccLink.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  iccLink.translatesAutoresizingMaskIntoConstraints = NO;
  [iccLink addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    NSURL *url = [NSURL URLWithString:@"https://www.color.org/"];
    [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
  }] forControlEvents:UIControlEventTouchUpInside];
  UIButton *repoLink = [UIButton buttonWithType:UIButtonTypeSystem];
  [repoLink setTitle:@"iccDEV Repository" forState:UIControlStateNormal];
  repoLink.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  repoLink.translatesAutoresizingMaskIntoConstraints = NO;
  [repoLink addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    NSURL *url = [NSURL URLWithString:@"https://github.com/InternationalColorConsortium/iccDEV"];
    [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
  }] forControlEvents:UIControlEventTouchUpInside];
  UITextView *text = [[UITextView alloc] initWithFrame:CGRectMake(0.0, 0.0, 0.0, 0.0)];
  text.editable = NO;
  text.selectable = YES;
  text.scrollEnabled = YES;
  text.translatesAutoresizingMaskIntoConstraints = NO;
  text.backgroundColor = [UIColor systemBackgroundColor];
  text.textColor = [UIColor labelColor];
  text.textContainerInset = UIEdgeInsetsMake(16, 16, 16, 16);
  text.font = [UIFont monospacedSystemFontOfSize:20 weight:UIFontWeightRegular];
  text.text = @"iccBenchApply mobile POC running...";
  UISegmentedControl *workload =
    [[UISegmentedControl alloc] initWithItems:@[@"Quick", @"Standard", @"Large"]];
  workload.selectedSegmentIndex = 1;
  workload.translatesAutoresizingMaskIntoConstraints = NO;
  UIButton *run = [UIButton buttonWithType:UIButtonTypeSystem];
  [run setTitle:@"Run Benchmark" forState:UIControlStateNormal];
  run.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  run.translatesAutoresizingMaskIntoConstraints = NO;
  UIButton *share = [UIButton buttonWithType:UIButtonTypeSystem];
  [share setTitle:@"Share Results" forState:UIControlStateNormal];
  share.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  share.translatesAutoresizingMaskIntoConstraints = NO;
  __weak UISegmentedControl *weakWorkload = workload;
  __weak UITextView *weakText = text;
  __weak UIButton *weakRun = run;
  __weak UIButton *weakShare = share;
  __weak UIViewController *weakController = controller;
  void (^runBenchmark)(void) = ^{
    UISegmentedControl *strongWorkload = weakWorkload;
    UITextView *strongText = weakText;
    UIButton *strongRun = weakRun;
    if (!strongWorkload || !strongText || !strongRun)
      return;
    NSInteger index = strongWorkload.selectedSegmentIndex;
    if (index < 0 || index > 2)
      index = 1;
    icUInt32Number nPixels = 65536;
    int nRepeats = 3;
    if (index == 0) {
      nPixels = 16384;
      nRepeats = 2;
    }
    else if (index == 2) {
      nPixels = 262144;
      nRepeats = 5;
    }
    strongText.text = @"iccBenchApply mobile POC running...";
    strongRun.enabled = NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      NSString *result = IccDevRunBenchApply(nPixels, nRepeats);
      dispatch_async(dispatch_get_main_queue(), ^{
        UITextView *nestedText = weakText;
        UIButton *nestedRun = weakRun;
        if (!nestedText || !nestedRun)
          return;
        nestedText.text = result;
        nestedRun.enabled = YES;
      });
    });
  };
  [run addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    runBenchmark();
  }] forControlEvents:UIControlEventTouchUpInside];
  [share addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    UITextView *strongText = weakText;
    UIButton *strongShare = weakShare;
    UIViewController *strongController = weakController;
    if (!strongText || !strongShare || !strongController)
      return;
    NSString *body = strongText.text ?: @"";
    UIActivityViewController *activity =
      [[UIActivityViewController alloc] initWithActivityItems:@[body]
                                        applicationActivities:nil];
    activity.popoverPresentationController.sourceView = strongShare;
    activity.popoverPresentationController.sourceRect = strongShare.bounds;
    [strongController presentViewController:activity animated:YES completion:nil];
  }] forControlEvents:UIControlEventTouchUpInside];

  UIStackView *stack = [[UIStackView alloc]
    initWithArrangedSubviews:@[logo, iccLink, repoLink, workload, run, share, text]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.alignment = UIStackViewAlignmentFill;
  stack.distribution = UIStackViewDistributionFill;
  stack.spacing = 12;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [controller.view addSubview:stack];
  UILayoutGuide *safeArea = controller.view.safeAreaLayoutGuide;
  NSLayoutConstraint *logoHeight =
    [logo.heightAnchor constraintEqualToAnchor:controller.view.heightAnchor multiplier:0.126];
  logoHeight.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    [stack.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor constant:16],
    [stack.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor constant:-16],
    [stack.topAnchor constraintEqualToAnchor:safeArea.topAnchor constant:16],
    [stack.bottomAnchor constraintEqualToAnchor:safeArea.bottomAnchor constant:-16],
    [iccLink.heightAnchor constraintGreaterThanOrEqualToConstant:32],
    [repoLink.heightAnchor constraintGreaterThanOrEqualToConstant:32],
    [workload.heightAnchor constraintGreaterThanOrEqualToConstant:36],
    [run.heightAnchor constraintGreaterThanOrEqualToConstant:44],
    [share.heightAnchor constraintGreaterThanOrEqualToConstant:44],
    [logo.heightAnchor constraintLessThanOrEqualToConstant:112],
    logoHeight,
  ]];
  self.window.rootViewController = controller;
  [self.window makeKeyAndVisible];

  runBenchmark();
  return YES;
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([IccBenchApplyAppDelegate class]));
  }
}
