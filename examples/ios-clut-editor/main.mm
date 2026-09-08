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
#import <TargetConditionals.h>
#import "ClutEditorHost.h"

@interface IccClutEditorViewController :
  UIViewController <UIImagePickerControllerDelegate, UINavigationControllerDelegate>
@property(nonatomic, strong) UIImageView *sourceView;
@property(nonatomic, strong) UIImageView *managedView;
@property(nonatomic, strong) UIImageView *editedView;
@property(nonatomic, strong) UIImageView *deltaView;
@property(nonatomic, strong) UITextView *reportView;
@property(nonatomic, strong) UIButton *pickButton;
@property(nonatomic, strong) UIButton *runButton;
@property(nonatomic, strong) UIButton *shareButton;
@property(nonatomic, strong) UIButton *saveImageButton;
@property(nonatomic, strong) UILabel *sliderSummary;
@property(nonatomic, strong) UISegmentedControl *sizeControl;
@property(nonatomic, strong) UISegmentedControl *gridControl;
@property(nonatomic, strong) UISegmentedControl *interpolationControl;
@property(nonatomic, strong) UISegmentedControl *appearanceControl;
@property(nonatomic, strong) UISlider *exposureSlider;
@property(nonatomic, strong) UISlider *contrastSlider;
@property(nonatomic, strong) UISlider *saturationSlider;
@property(nonatomic, strong) UISlider *warmSlider;
@property(nonatomic, strong) UIImage *selectedImage;
@property(nonatomic, strong) UIImage *lastSourceImage;
@property(nonatomic, strong) UIImage *lastManagedImage;
@property(nonatomic, strong) UIImage *lastEditedImage;
@property(nonatomic, strong) UIImage *lastDeltaImage;
@property(nonatomic, copy) NSString *lastReport;
@property(nonatomic, assign) NSInteger renderSerial;
@property(nonatomic, assign) BOOL wideLayout;
@property(nonatomic, assign) BOOL renderInFlight;
@end

@interface IccClutEditorAppDelegate : UIResponder <UIApplicationDelegate>
@end

@interface IccClutEditorSceneDelegate : UIResponder <UIWindowSceneDelegate>
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

static UIColor *IccDevPrimaryBlue(void)
{
  return [UIColor colorWithRed:3.0 / 255.0
                         green:102.0 / 255.0
                          blue:153.0 / 255.0
                         alpha:1.0];
}

static UILabel *IccDevLabel(NSString *text, UIFontTextStyle style)
{
  UILabel *view = [[UILabel alloc] init];
  view.text = text;
  view.font = [UIFont preferredFontForTextStyle:style];
  view.numberOfLines = 0;
  view.adjustsFontForContentSizeCategory = YES;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  return view;
}

static UIButton *IccDevLinkButton(NSString *title, NSString *urlText)
{
  UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:title forState:UIControlStateNormal];
  button.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    NSURL *url = [NSURL URLWithString:urlText];
    [[UIApplication sharedApplication] openURL:url options:@{}
                             completionHandler:nil];
  }] forControlEvents:UIControlEventTouchUpInside];
  return button;
}

static UIStackView *IccDevControlPanel(NSString *text, UIControl *control)
{
  UIStackView *panel = [[UIStackView alloc]
    initWithArrangedSubviews:@[IccDevLabel(text, UIFontTextStyleHeadline),
                               control]];
  panel.axis = UILayoutConstraintAxisVertical;
  panel.alignment = UIStackViewAlignmentFill;
  panel.distribution = UIStackViewDistributionFill;
  panel.spacing = 4;
  panel.translatesAutoresizingMaskIntoConstraints = NO;
  return panel;
}

static UIStackView *IccDevImagePanel(NSString *text, UIImageView *image)
{
  UIStackView *panel = [[UIStackView alloc]
    initWithArrangedSubviews:@[IccDevLabel(text, UIFontTextStyleHeadline),
                               image]];
  panel.axis = UILayoutConstraintAxisVertical;
  panel.alignment = UIStackViewAlignmentFill;
  panel.distribution = UIStackViewDistributionFill;
  panel.spacing = 6;
  panel.translatesAutoresizingMaskIntoConstraints = NO;
  return panel;
}

static void IccDevConfigureImageView(UIImageView *image)
{
  image.contentMode = UIViewContentModeScaleAspectFit;
  image.backgroundColor = [UIColor secondarySystemBackgroundColor];
  image.layer.borderColor = [UIColor separatorColor].CGColor;
  image.layer.borderWidth = 1.0;
  image.translatesAutoresizingMaskIntoConstraints = NO;
}

@implementation IccClutEditorViewController

- (BOOL)useWideLayoutForSize:(CGSize)size
{
  const BOOL isPhone =
    [UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPhone;
  CGFloat layoutWidth = size.width;
  if (layoutWidth <= 0.0)
    layoutWidth = [UIScreen mainScreen].bounds.size.width;
  BOOL wide = !isPhone &&
    self.traitCollection.horizontalSizeClass != UIUserInterfaceSizeClassCompact &&
    layoutWidth >= 700.0;
#if TARGET_OS_MACCATALYST
  wide =
    self.traitCollection.horizontalSizeClass != UIUserInterfaceSizeClassCompact &&
    layoutWidth >= 700.0;
#endif
  return wide;
}

- (void)viewDidLoad
{
  [super viewDidLoad];
  [self rebuildInterfaceForSize:self.view.bounds.size];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  BOOL wide = [self useWideLayoutForSize:size];
  if (wide == self.wideLayout)
    return;
  [coordinator animateAlongsideTransition:nil completion:
    ^(id<UIViewControllerTransitionCoordinatorContext> context) {
      (void)context;
      [self rebuildInterfaceForSize:size];
    }];
}

- (void)rebuildInterfaceForSize:(CGSize)size
{
  const BOOL hadInterface = self.reportView != nil;
  NSInteger sizeIndex = self.sizeControl ? self.sizeControl.selectedSegmentIndex : 1;
  NSInteger gridIndex = self.gridControl ? self.gridControl.selectedSegmentIndex : 1;
  NSInteger interpIndex =
    self.interpolationControl ? self.interpolationControl.selectedSegmentIndex : 0;
  NSInteger appearanceIndex =
    self.appearanceControl ? self.appearanceControl.selectedSegmentIndex : 0;
  float exposureValue = self.exposureSlider ? self.exposureSlider.value : 0.0f;
  float contrastValue = self.contrastSlider ? self.contrastSlider.value : 1.0f;
  float saturationValue = self.saturationSlider ? self.saturationSlider.value : 1.0f;
  float warmValue = self.warmSlider ? self.warmSlider.value : 0.0f;

  for (UIView *subview in self.view.subviews)
    [subview removeFromSuperview];

  self.view.backgroundColor = [UIColor systemBackgroundColor];
  const BOOL isPad = [self useWideLayoutForSize:size];
  self.wideLayout = isPad;
  const BOOL compactPhone = !isPad;

  UIImageView *logo =
    [[UIImageView alloc] initWithImage:[UIImage imageNamed:@"ICCLogo"]];
  logo.contentMode = UIViewContentModeScaleAspectFit;
  logo.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel *title =
    IccDevLabel(compactPhone ? @"iccCLUT Editor" :
                              @"iccCLUT Editor Proof of Concept",
                compactPhone ? UIFontTextStyleTitle3 : UIFontTextStyleTitle1);
  title.textAlignment = NSTextAlignmentCenter;
  UILabel *subtitle = IccDevLabel(
    compactPhone ? @"Live ICC profile chain plus editable 3D LUT preview." :
      @"Edit a live 3D LUT, apply it after a bundled ICC profile chain, "
       "and preview the result on a selected or default image.",
    UIFontTextStyleSubheadline);
  subtitle.textAlignment = NSTextAlignmentCenter;
  subtitle.textColor = [UIColor secondaryLabelColor];

  UIButton *iccLink =
    IccDevLinkButton(@"International Color Consortium - color.org",
                     @"https://www.color.org/");
  UIButton *repoLink =
    IccDevLinkButton(@"iccDEV Repository",
                     @"https://github.com/InternationalColorConsortium/iccDEV");

  self.sizeControl =
    [[UISegmentedControl alloc] initWithItems:@[@"160", @"256", @"384"]];
  self.sizeControl.selectedSegmentIndex = sizeIndex;
  self.sizeControl.translatesAutoresizingMaskIntoConstraints = NO;
  self.gridControl =
    [[UISegmentedControl alloc] initWithItems:@[@"5", @"9", @"17"]];
  self.gridControl.selectedSegmentIndex = gridIndex;
  self.gridControl.translatesAutoresizingMaskIntoConstraints = NO;
  self.interpolationControl =
    [[UISegmentedControl alloc] initWithItems:@[@"Linear", @"Tetra"]];
  self.interpolationControl.selectedSegmentIndex = interpIndex;
  self.interpolationControl.translatesAutoresizingMaskIntoConstraints = NO;
  self.appearanceControl =
    [[UISegmentedControl alloc] initWithItems:@[@"System", @"Light", @"Dark"]];
  self.appearanceControl.selectedSegmentIndex = appearanceIndex;
  self.appearanceControl.translatesAutoresizingMaskIntoConstraints = NO;

  self.exposureSlider = [[UISlider alloc] init];
  self.exposureSlider.minimumValue = -1.0f;
  self.exposureSlider.maximumValue = 1.0f;
  self.exposureSlider.value = exposureValue;
  self.exposureSlider.translatesAutoresizingMaskIntoConstraints = NO;
  self.contrastSlider = [[UISlider alloc] init];
  self.contrastSlider.minimumValue = 0.50f;
  self.contrastSlider.maximumValue = 1.50f;
  self.contrastSlider.value = contrastValue;
  self.contrastSlider.translatesAutoresizingMaskIntoConstraints = NO;
  self.saturationSlider = [[UISlider alloc] init];
  self.saturationSlider.minimumValue = 0.0f;
  self.saturationSlider.maximumValue = 2.0f;
  self.saturationSlider.value = saturationValue;
  self.saturationSlider.translatesAutoresizingMaskIntoConstraints = NO;
  self.warmSlider = [[UISlider alloc] init];
  self.warmSlider.minimumValue = -1.0f;
  self.warmSlider.maximumValue = 1.0f;
  self.warmSlider.value = warmValue;
  self.warmSlider.translatesAutoresizingMaskIntoConstraints = NO;

  self.sliderSummary = IccDevLabel(@"", UIFontTextStyleFootnote);
  self.sliderSummary.textColor = [UIColor secondaryLabelColor];

  UIButton *pick = [UIButton buttonWithType:UIButtonTypeSystem];
  self.pickButton = pick;
  UIButtonConfiguration *pickConfig =
    [UIButtonConfiguration borderedButtonConfiguration];
  pickConfig.title = @"Select Image";
  pickConfig.baseForegroundColor = IccDevBrandBlue();
  pickConfig.contentInsets = NSDirectionalEdgeInsetsMake(compactPhone ? 8 : 10,
                                                        16,
                                                        compactPhone ? 8 : 10,
                                                        16);
  pick.configuration = pickConfig;
  pick.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton *reset = [UIButton buttonWithType:UIButtonTypeSystem];
  UIButtonConfiguration *resetConfig =
    [UIButtonConfiguration borderedButtonConfiguration];
  resetConfig.title = @"Use Default";
  resetConfig.baseForegroundColor = IccDevBrandBlue();
  resetConfig.contentInsets = NSDirectionalEdgeInsetsMake(compactPhone ? 8 : 10,
                                                         16,
                                                         compactPhone ? 8 : 10,
                                                         16);
  reset.configuration = resetConfig;
  reset.translatesAutoresizingMaskIntoConstraints = NO;

  self.runButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIButtonConfiguration *runConfig =
    [UIButtonConfiguration filledButtonConfiguration];
  runConfig.title = compactPhone ? @"Apply" : @"Apply Live Edit";
  runConfig.baseBackgroundColor = IccDevPrimaryBlue();
  runConfig.baseForegroundColor = [UIColor whiteColor];
  runConfig.contentInsets = NSDirectionalEdgeInsetsMake(compactPhone ? 8 : 10,
                                                       compactPhone ? 10 : 16,
                                                       compactPhone ? 8 : 10,
                                                       compactPhone ? 10 : 16);
  self.runButton.configuration = runConfig;
  self.runButton.enabled = !self.renderInFlight;
  self.runButton.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  self.runButton.translatesAutoresizingMaskIntoConstraints = NO;

  self.shareButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIButtonConfiguration *shareConfig =
    [UIButtonConfiguration borderedButtonConfiguration];
  shareConfig.title = compactPhone ? @"Report" : @"Share Report";
  shareConfig.baseForegroundColor = IccDevBrandBlue();
  shareConfig.contentInsets = NSDirectionalEdgeInsetsMake(compactPhone ? 8 : 10,
                                                         compactPhone ? 10 : 16,
                                                         compactPhone ? 8 : 10,
                                                         compactPhone ? 10 : 16);
  self.shareButton.configuration = shareConfig;
  self.shareButton.enabled = !self.renderInFlight && self.lastReport.length > 0;
  self.shareButton.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  self.shareButton.translatesAutoresizingMaskIntoConstraints = NO;

  self.saveImageButton = [UIButton buttonWithType:UIButtonTypeSystem];
  UIButtonConfiguration *saveConfig =
    [UIButtonConfiguration borderedButtonConfiguration];
  saveConfig.title = compactPhone ? @"Image" : @"Share Edited Image";
  saveConfig.baseForegroundColor = IccDevBrandBlue();
  saveConfig.contentInsets = NSDirectionalEdgeInsetsMake(compactPhone ? 8 : 10,
                                                        compactPhone ? 10 : 16,
                                                        compactPhone ? 8 : 10,
                                                        compactPhone ? 10 : 16);
  self.saveImageButton.configuration = saveConfig;
  self.saveImageButton.enabled = !self.renderInFlight && self.lastEditedImage != nil;
  self.saveImageButton.titleLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  self.saveImageButton.translatesAutoresizingMaskIntoConstraints = NO;

  self.sourceView = [[UIImageView alloc] init];
  self.managedView = [[UIImageView alloc] init];
  self.editedView = [[UIImageView alloc] init];
  self.deltaView = [[UIImageView alloc] init];
  for (UIImageView *image in @[self.sourceView, self.managedView,
                               self.editedView, self.deltaView]) {
    IccDevConfigureImageView(image);
  }
  self.sourceView.image = self.lastSourceImage;
  self.managedView.image = self.lastManagedImage;
  self.editedView.image = self.lastEditedImage;
  self.deltaView.image = self.lastDeltaImage;

  self.reportView = [[UITextView alloc] initWithFrame:CGRectZero];
  self.reportView.editable = NO;
  self.reportView.selectable = YES;
  self.reportView.scrollEnabled = YES;
  self.reportView.showsVerticalScrollIndicator = YES;
  self.reportView.backgroundColor = [UIColor secondarySystemBackgroundColor];
  self.reportView.textColor = [UIColor labelColor];
  self.reportView.textContainerInset = UIEdgeInsetsMake(isPad ? 8 : 12,
                                                        isPad ? 10 : 12,
                                                        isPad ? 8 : 12,
                                                        isPad ? 10 : 12);
  self.reportView.font =
    [UIFont monospacedSystemFontOfSize:isPad ? 13 : 14
                                weight:UIFontWeightRegular];
  self.reportView.text = self.lastReport ?: (self.renderInFlight ?
    @"Applying ICC profile chain and live CLUT edit..." : @"CLUT editor ready.");
  self.reportView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView *links =
    [[UIStackView alloc] initWithArrangedSubviews:@[iccLink, repoLink]];
  links.axis = isPad ? UILayoutConstraintAxisHorizontal :
                       UILayoutConstraintAxisVertical;
  links.alignment = UIStackViewAlignmentFill;
  links.distribution = UIStackViewDistributionFillEqually;
  links.spacing = 8;
  links.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView *sizePanel = IccDevControlPanel(@"Image size", self.sizeControl);
  UIStackView *gridPanel = IccDevControlPanel(@"CLUT grid", self.gridControl);
  UIStackView *interpolationPanel =
    IccDevControlPanel(@"Interpolation", self.interpolationControl);
  UIStackView *appearancePanel =
    IccDevControlPanel(@"Appearance", self.appearanceControl);
  UIStackView *topControls = nil;
  if (isPad) {
    topControls = [[UIStackView alloc] initWithArrangedSubviews:@[
      sizePanel, gridPanel, interpolationPanel, appearancePanel
    ]];
    topControls.axis = UILayoutConstraintAxisHorizontal;
  }
  else {
    UIStackView *topControlsUpper =
      [[UIStackView alloc] initWithArrangedSubviews:@[sizePanel, gridPanel]];
    topControlsUpper.axis = UILayoutConstraintAxisHorizontal;
    topControlsUpper.alignment = UIStackViewAlignmentFill;
    topControlsUpper.distribution = UIStackViewDistributionFillEqually;
    topControlsUpper.spacing = 8;
    topControlsUpper.translatesAutoresizingMaskIntoConstraints = NO;
    UIStackView *topControlsLower =
      [[UIStackView alloc] initWithArrangedSubviews:@[interpolationPanel,
                                                      appearancePanel]];
    topControlsLower.axis = UILayoutConstraintAxisHorizontal;
    topControlsLower.alignment = UIStackViewAlignmentFill;
    topControlsLower.distribution = UIStackViewDistributionFillEqually;
    topControlsLower.spacing = 8;
    topControlsLower.translatesAutoresizingMaskIntoConstraints = NO;
    topControls = [[UIStackView alloc] initWithArrangedSubviews:@[
      topControlsUpper, topControlsLower
    ]];
    topControls.axis = UILayoutConstraintAxisVertical;
  }
  topControls.alignment = UIStackViewAlignmentFill;
  topControls.distribution = UIStackViewDistributionFillEqually;
  topControls.spacing = compactPhone ? 8 : 10;
  topControls.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView *sliders =
    [[UIStackView alloc] initWithArrangedSubviews:@[
      IccDevControlPanel(@"Exposure", self.exposureSlider),
      IccDevControlPanel(@"Contrast", self.contrastSlider),
      IccDevControlPanel(@"Saturation", self.saturationSlider),
      IccDevControlPanel(@"Warm/cool bias", self.warmSlider),
      self.sliderSummary
    ]];
  sliders.axis = UILayoutConstraintAxisVertical;
  sliders.alignment = UIStackViewAlignmentFill;
  sliders.distribution = UIStackViewDistributionFill;
  sliders.spacing = compactPhone ? 4 : 6;
  sliders.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView *imageButtons =
    [[UIStackView alloc] initWithArrangedSubviews:@[pick, reset]];
  imageButtons.axis = UILayoutConstraintAxisHorizontal;
  imageButtons.alignment = UIStackViewAlignmentFill;
  imageButtons.distribution = UIStackViewDistributionFillEqually;
  imageButtons.spacing = 10;
  imageButtons.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView *actions =
    [[UIStackView alloc] initWithArrangedSubviews:@[self.runButton,
                                                    self.shareButton,
                                                    self.saveImageButton]];
  actions.axis = UILayoutConstraintAxisHorizontal;
  actions.alignment = UIStackViewAlignmentFill;
  actions.distribution = UIStackViewDistributionFillEqually;
  actions.spacing = compactPhone ? 8 : 10;
  actions.translatesAutoresizingMaskIntoConstraints = NO;

  UIView *sourcePanel = IccDevImagePanel(@"Source", self.sourceView);
  UIView *managedPanel = IccDevImagePanel(@"Managed", self.managedView);
  UIView *editedPanel = IccDevImagePanel(@"Edited", self.editedView);
  UIView *deltaPanel = IccDevImagePanel(@"Delta, amplified 8x", self.deltaView);
  NSArray<UIView *> *previewPanels = @[sourcePanel, managedPanel,
                                       editedPanel, deltaPanel];
  UIStackView *previews = nil;
  if (isPad) {
    previews = [[UIStackView alloc] initWithArrangedSubviews:previewPanels];
    previews.axis = UILayoutConstraintAxisHorizontal;
  }
  else {
    UIStackView *previewTop =
      [[UIStackView alloc] initWithArrangedSubviews:@[sourcePanel,
                                                      managedPanel]];
    previewTop.axis = UILayoutConstraintAxisHorizontal;
    previewTop.alignment = UIStackViewAlignmentFill;
    previewTop.distribution = UIStackViewDistributionFillEqually;
    previewTop.spacing = 8;
    previewTop.translatesAutoresizingMaskIntoConstraints = NO;
    UIStackView *previewBottom =
      [[UIStackView alloc] initWithArrangedSubviews:@[editedPanel,
                                                      deltaPanel]];
    previewBottom.axis = UILayoutConstraintAxisHorizontal;
    previewBottom.alignment = UIStackViewAlignmentFill;
    previewBottom.distribution = UIStackViewDistributionFillEqually;
    previewBottom.spacing = 8;
    previewBottom.translatesAutoresizingMaskIntoConstraints = NO;
    previews = [[UIStackView alloc] initWithArrangedSubviews:@[previewTop,
                                                               previewBottom]];
    previews.axis = UILayoutConstraintAxisVertical;
  }
  previews.alignment = UIStackViewAlignmentFill;
  previews.distribution = UIStackViewDistributionFillEqually;
  previews.spacing = compactPhone ? 8 : 10;
  previews.translatesAutoresizingMaskIntoConstraints = NO;

  NSArray<UIView *> *arranged = isPad ? @[
    title, subtitle, links, topControls, sliders, imageButtons, actions,
    previews, self.reportView
  ] : @[
    logo, title, subtitle, links, topControls, sliders, imageButtons, actions,
    previews, self.reportView
  ];
  UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:arranged];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.alignment = UIStackViewAlignmentFill;
  stack.distribution = UIStackViewDistributionFill;
  stack.spacing = compactPhone ? 8 : 12;
  stack.translatesAutoresizingMaskIntoConstraints = NO;

  UIScrollView *scroll = [[UIScrollView alloc] init];
  scroll.translatesAutoresizingMaskIntoConstraints = NO;
  UILayoutGuide *safeArea = self.view.safeAreaLayoutGuide;
  [scroll addSubview:stack];
  [self.view addSubview:scroll];
  CGFloat margin = isPad ? 18.0 : 16.0;
  NSMutableArray<NSLayoutConstraint *> *constraints = [NSMutableArray arrayWithArray:@[
    [scroll.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor],
    [scroll.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor],
    [scroll.topAnchor constraintEqualToAnchor:safeArea.topAnchor],
    [scroll.bottomAnchor constraintEqualToAnchor:safeArea.bottomAnchor],
    [stack.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor
                                        constant:margin],
    [stack.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor
                                         constant:-margin],
    [stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor
                                    constant:isPad ? 12 : 16],
    [stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor
                                       constant:isPad ? -12 : -16],
    [stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor
                                      constant:-(margin * 2.0)]
  ]];
  if (isPad) {
    [constraints addObjectsFromArray:@[
      [stack.heightAnchor constraintGreaterThanOrEqualToAnchor:scroll.frameLayoutGuide.heightAnchor
                                                      constant:-24],
      [actions.heightAnchor constraintEqualToConstant:50],
      [imageButtons.heightAnchor constraintEqualToConstant:50],
      [self.sourceView.heightAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.heightAnchor
                                                 multiplier:0.20],
      [self.managedView.heightAnchor constraintEqualToAnchor:self.sourceView.heightAnchor],
      [self.editedView.heightAnchor constraintEqualToAnchor:self.sourceView.heightAnchor],
      [self.deltaView.heightAnchor constraintEqualToAnchor:self.sourceView.heightAnchor],
      [self.reportView.heightAnchor constraintGreaterThanOrEqualToAnchor:scroll.frameLayoutGuide.heightAnchor
                                                              multiplier:0.24]
    ]];
  }
  else {
    [constraints addObjectsFromArray:@[
      [logo.heightAnchor constraintLessThanOrEqualToConstant:55],
      [logo.heightAnchor constraintGreaterThanOrEqualToConstant:34],
      [self.runButton.heightAnchor constraintGreaterThanOrEqualToConstant:44],
      [self.shareButton.heightAnchor constraintGreaterThanOrEqualToConstant:44],
      [self.saveImageButton.heightAnchor constraintGreaterThanOrEqualToConstant:44],
      [self.sourceView.heightAnchor constraintEqualToAnchor:self.sourceView.widthAnchor
                                                 multiplier:0.55],
      [self.managedView.heightAnchor constraintEqualToAnchor:self.sourceView.heightAnchor],
      [self.editedView.heightAnchor constraintEqualToAnchor:self.sourceView.heightAnchor],
      [self.deltaView.heightAnchor constraintEqualToAnchor:self.sourceView.heightAnchor],
      [self.reportView.heightAnchor constraintGreaterThanOrEqualToConstant:160]
    ]];
  }
  [NSLayoutConstraint activateConstraints:constraints];

  [pick addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    [self pickImage];
  }] forControlEvents:UIControlEventTouchUpInside];
  [reset addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    self.selectedImage = nil;
    [self runEditor];
  }] forControlEvents:UIControlEventTouchUpInside];
  [self.runButton addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    [self runEditor];
  }] forControlEvents:UIControlEventTouchUpInside];
  [self.shareButton addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    [self shareReport];
  }] forControlEvents:UIControlEventTouchUpInside];
  [self.saveImageButton addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    [self shareEditedImage];
  }] forControlEvents:UIControlEventTouchUpInside];
  [self.appearanceControl addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
    (void)action;
    [self applyAppearanceStyle];
  }] forControlEvents:UIControlEventValueChanged];

  for (UIControl *control in @[self.sizeControl, self.gridControl,
                               self.interpolationControl,
                               self.exposureSlider, self.contrastSlider,
                               self.saturationSlider, self.warmSlider]) {
    [control addAction:[UIAction actionWithHandler:^(__kindof UIAction *action) {
      (void)action;
      [self scheduleEditorRun];
    }] forControlEvents:UIControlEventValueChanged];
  }

  [self updateSliderSummary];
  [self applyAppearanceStyle];
  if (!hadInterface)
    [self runEditor];
}

- (void)applyAppearanceStyle
{
  UIUserInterfaceStyle style = UIUserInterfaceStyleUnspecified;
  if (self.appearanceControl.selectedSegmentIndex == 1) {
    style = UIUserInterfaceStyleLight;
  }
  else if (self.appearanceControl.selectedSegmentIndex == 2) {
    style = UIUserInterfaceStyleDark;
  }
  self.overrideUserInterfaceStyle = style;
  self.view.window.overrideUserInterfaceStyle = style;
}

- (NSUInteger)selectedEdgePixels
{
  if (self.sizeControl.selectedSegmentIndex == 0)
    return 160u;
  if (self.sizeControl.selectedSegmentIndex == 2)
    return 384u;
  return 256u;
}

- (NSUInteger)selectedGridPoints
{
  if (self.gridControl.selectedSegmentIndex == 0)
    return 5u;
  if (self.gridControl.selectedSegmentIndex == 2)
    return 17u;
  return 9u;
}

- (void)updateSliderSummary
{
  self.sliderSummary.text = [NSString stringWithFormat:
    @"Exposure %.2f stops, contrast %.2f, saturation %.2f, warm bias %.2f",
    self.exposureSlider.value, self.contrastSlider.value,
    self.saturationSlider.value, self.warmSlider.value];
}

- (void)scheduleEditorRun
{
  [self updateSliderSummary];
  ++self.renderSerial;
  self.renderInFlight = YES;
  self.reportView.text = @"Applying ICC profile chain and live CLUT edit...";
  self.shareButton.enabled = NO;
  self.saveImageButton.enabled = NO;
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(runEditor)
                                             object:nil];
  [self performSelector:@selector(runEditor) withObject:nil afterDelay:0.20];
}

- (void)runEditor
{
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(runEditor)
                                             object:nil];
  [self updateSliderSummary];
  const NSInteger serial = ++self.renderSerial;
  UIImage *input = self.selectedImage;
  const NSUInteger edge = [self selectedEdgePixels];
  const NSUInteger grid = [self selectedGridPoints];
  const double exposure = self.exposureSlider.value;
  const double contrast = self.contrastSlider.value;
  const double saturation = self.saturationSlider.value;
  const double warm = self.warmSlider.value;
  const BOOL tetra = self.interpolationControl.selectedSegmentIndex == 1;
  self.renderInFlight = YES;
  self.reportView.text = @"Applying ICC profile chain and live CLUT edit...";
  self.runButton.enabled = NO;
  self.shareButton.enabled = NO;
  self.saveImageButton.enabled = NO;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    IccClutEditorResult *result =
      IccDevRunClutEditor(input, edge, grid, exposure, contrast, saturation,
                          warm, tetra);
    dispatch_async(dispatch_get_main_queue(), ^{
      if (serial != self.renderSerial)
        return;
      NSString *persistFailure = nil;
      if (!IccDevPersistClutEditorReport(result, &persistFailure)) {
        result.passed = NO;
        result.report = [result.report stringByAppendingFormat:
          @"FAIL Persist device results: %@\n",
          persistFailure ?: @"write failed"];
      }
      self.sourceView.image = result.sourceImage;
      self.managedView.image = result.managedImage;
      self.editedView.image = result.editedImage;
      self.deltaView.image = result.deltaImage;
      self.reportView.text = result.report;
      self.lastSourceImage = result.sourceImage;
      self.lastManagedImage = result.managedImage;
      self.lastReport = result.report;
      self.lastEditedImage = result.editedImage;
      self.lastDeltaImage = result.deltaImage;
      self.renderInFlight = NO;
      self.runButton.enabled = YES;
      self.shareButton.enabled = result.report.length > 0;
      self.saveImageButton.enabled = result.editedImage != nil;
      IccDevFinishClutEditor(result);
    });
  });
}

- (void)pickImage
{
  if (![UIImagePickerController isSourceTypeAvailable:
          UIImagePickerControllerSourceTypePhotoLibrary]) {
    self.reportView.text = @"Photo library is not available; using default image.";
    self.selectedImage = nil;
    [self runEditor];
    return;
  }
  UIImagePickerController *picker = [[UIImagePickerController alloc] init];
  picker.sourceType = UIImagePickerControllerSourceTypePhotoLibrary;
  picker.delegate = self;
  picker.modalPresentationStyle = UIModalPresentationPopover;
  picker.popoverPresentationController.sourceView = self.pickButton ?: self.view;
  picker.popoverPresentationController.sourceRect =
    self.pickButton ? self.pickButton.bounds : self.view.bounds;
  [self presentViewController:picker animated:YES completion:nil];
}

- (void)shareReport
{
  NSString *report = self.lastReport ?: self.reportView.text ?: @"";
  UIActivityViewController *activity =
    [[UIActivityViewController alloc] initWithActivityItems:@[report]
                                      applicationActivities:nil];
  activity.popoverPresentationController.sourceView = self.shareButton;
  activity.popoverPresentationController.sourceRect = self.shareButton.bounds;
  [self presentViewController:activity animated:YES completion:nil];
}

- (void)shareEditedImage
{
  UIImage *image = self.lastEditedImage ?: self.editedView.image;
  if (!image) {
    self.reportView.text =
      [self.reportView.text stringByAppendingString:
        @"\nNo edited image is available to share.\n"];
    return;
  }
  NSData *png = UIImagePNGRepresentation(image);
  if (!png) {
    self.reportView.text =
      [self.reportView.text stringByAppendingString:
        @"\nFAIL Encode edited image as PNG.\n"];
    return;
  }
  NSURL *url = [NSURL fileURLWithPath:
    [NSTemporaryDirectory() stringByAppendingPathComponent:
      @"icc-clut-editor-edited.png"]];
  NSError *error = nil;
  if (![png writeToURL:url options:NSDataWritingAtomic error:&error]) {
    self.reportView.text = [self.reportView.text stringByAppendingFormat:
      @"\nFAIL Write edited image export: %@\n",
      error ? error.localizedDescription : @"write failed"];
    return;
  }
  UIActivityViewController *activity =
    [[UIActivityViewController alloc] initWithActivityItems:@[image, url]
                                      applicationActivities:nil];
  activity.popoverPresentationController.sourceView = self.saveImageButton;
  activity.popoverPresentationController.sourceRect = self.saveImageButton.bounds;
  [self presentViewController:activity animated:YES completion:nil];
}

- (void)imagePickerController:(UIImagePickerController *)picker
didFinishPickingMediaWithInfo:(NSDictionary<UIImagePickerControllerInfoKey, id> *)info
{
  UIImage *image = info[UIImagePickerControllerOriginalImage];
  self.selectedImage = image;
  [picker dismissViewControllerAnimated:YES completion:^{
    [self runEditor];
  }];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController *)picker
{
  [picker dismissViewControllerAnimated:YES completion:nil];
}

@end

@implementation IccClutEditorSceneDelegate
- (void)scene:(UIScene *)scene
willConnectToSession:(UISceneSession *)session
      options:(UISceneConnectionOptions *)connectionOptions
{
  (void)session;
  (void)connectionOptions;
  if (![scene isKindOfClass:[UIWindowScene class]])
    return;
  UIWindowScene *windowScene = (UIWindowScene *)scene;
  self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
  self.window.rootViewController = [[IccClutEditorViewController alloc] init];
  [self.window makeKeyAndVisible];
}
@end

@implementation IccClutEditorAppDelegate
- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
  (void)application;
  (void)launchOptions;
  return YES;
}

- (UISceneConfiguration *)application:(UIApplication *)application
configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
                               options:(UISceneConnectionOptions *)options
{
  (void)application;
  (void)options;
  UISceneConfiguration *configuration =
    [[UISceneConfiguration alloc] initWithName:@"Default Configuration"
                                   sessionRole:connectingSceneSession.role];
  configuration.delegateClass = [IccClutEditorSceneDelegate class];
  return configuration;
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil,
                             NSStringFromClass([IccClutEditorAppDelegate class]));
  }
}
