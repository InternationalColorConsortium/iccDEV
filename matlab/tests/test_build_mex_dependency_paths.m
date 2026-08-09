function test_build_mex_dependency_paths()
%TEST_BUILD_MEX_DEPENDENCY_PATHS Verify vcpkg installed-root selection.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  root = tempname();
  mkdir(root);
  cleanup = onCleanup(@() rmdir(root, 's')); %#ok<NASGU>

  buildDir = fullfile(root, 'build');
  mkdir(buildDir);
  triplet = 'x64-windows';
  defaultRoot = fullfile(buildDir, 'vcpkg_installed');
  mkdir(fullfile(defaultRoot, triplet));

  selected = iccdev.qa.select_vcpkg_installed_root( ...
    buildDir, '', triplet);
  assert(strcmp(selected, defaultRoot));

  selected = iccdev.qa.select_vcpkg_installed_root( ...
    buildDir, fullfile(root, 'stale'), triplet);
  assert(strcmp(selected, defaultRoot));

  relativeRoot = 'relative-vcpkg';
  resolvedRelativeRoot = fullfile(buildDir, relativeRoot);
  mkdir(fullfile(resolvedRelativeRoot, triplet));
  selected = iccdev.qa.select_vcpkg_installed_root( ...
    buildDir, relativeRoot, triplet);
  assert(strcmp(selected, resolvedRelativeRoot));

  preferredRoot = fullfile(root, 'preferred-vcpkg');
  mkdir(fullfile(preferredRoot, triplet));
  selected = iccdev.qa.select_vcpkg_installed_root( ...
    buildDir, preferredRoot, triplet);
  assert(strcmp(selected, preferredRoot));
end
