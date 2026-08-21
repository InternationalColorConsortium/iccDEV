function test_colorimetry_issue_1475()
%TEST_COLORIMETRY_ISSUE_1475 Verify the independent MATLAB reproduction.
%
% Run from the repository root after adding matlab/ and matlab/tests/ to the
% MATLAB path. The compiled companion is iccdev.colorimetry-methods.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  result = iccdev.qa.check_colorimetry_issue_1475();
  assert(abs(result.legacy_xyz(2) - 1.0) < 1e-12);
  assert(max(abs(result.legacy_weighted10_xyz - result.legacy_xyz)) < 1e-12);
  assert(max(abs(result.weighted_control_error)) < 1e-12);
  assert(abs(result.registry_xyz(2) - 1.0) < 1e-9);
  assert(max(abs(result.registry_runtime_xyz - result.registry_xyz)) < 1e-8);
  assert(result.delta_xyz(3) < 0.0);

  % The controls, restated here so a reader of the test sees what is actually
  % being claimed: a 10 nm weighting operator derived from the legacy data
  % preserves the 5 nm white, while direct 10 nm summation does not. "Closer"
  % still flips depending on which of the tree's two D50 references is adopted.
  assert(result.grid_effect < 0.0);
  assert(abs(result.grid_effect) > abs(result.delta_xyz(3)));
  assert(result.closer_to_cie);
  assert(result.closer_to_icc_pcs);

  fprintf('Issue #1475 colorimetry calculations passed.\n');
end
