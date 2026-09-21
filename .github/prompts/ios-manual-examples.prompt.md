# Manual iOS Example App Helpers

Use this prompt when adding or updating local build helpers for
`examples/ios-apply-preview`, `examples/ios-benchapply`, or
`examples/ios-clut-editor`.

## Contract Checklist

- Compare the helper script, CMake project, README, Info.plist, bundle ID,
  target name, persisted report path, and launch command as one surface.
- Keep simulator and device builds linked against matching `apple-ios-*-core`
  static archives.
- Keep command-line helpers friendly for local developers with Apple developer
  accounts: support `--open`, `--run-tests`, and `--launch`; accept `TEAM_ID`,
  `DEVICE_ID`, `SIMULATOR_UDID`, `BUNDLE_ID`, `BUILD_CONFIG`, `DEPLOYMENT`,
  and `ARCH` consistently.
- Device builds must fail before building when `TEAM_ID` or `BUNDLE_ID`
  contains a placeholder value.
- Simulator builds must not be blocked by stale exported device placeholders;
  ignore a placeholder `BUNDLE_ID` and use the default app ID.
- Every app must adopt `UIScene` lifecycle (a `UIWindowSceneDelegate` that
  builds the window from the connecting `UIWindowScene`, matching
  `UIApplicationSceneManifest` in `Info.plist.in`, and
  `configurationForConnectingSceneSession:` on the app delegate). iOS 27
  physical hardware enforces this with a fatal `SIGTRAP` in
  `UIApplicationEvaluateRuntimeIssueForNoSceneLifecycleAdoption_block_invoke`;
  the locally installed iOS 18.x/26.x simulator runtimes do not, so this
  regression can pass simulator-only QA silently.
- Do not commit signing files, generated Xcode output, device identifiers, or
  build output.
- Do not add workflow or CTest coverage unless a maintainer requests it.

## Validation

Run the helper for every changed example, using the smallest meaningful target:

```bash
git diff --check
examples/ios-apply-preview/build-ios.sh simulator --run-tests
examples/ios-benchapply/build-ios.sh simulator --run-tests
examples/ios-clut-editor/build-ios.sh simulator --run-tests
doxygen .github/ci/doxygen/Doxyfile
test ! -s docs/generated/doxygen-warnings.log
```

When changing signing guards, run at least one negative placeholder check for
each affected helper before any physical-device build.
