function test_colorimetry_issue_1475()
%TEST_COLORIMETRY_ISSUE_1475 Verify the independent MATLAB reproduction.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  result = iccdev.qa.check_colorimetry_issue_1475();
  assert(abs(result.legacy_xyz(2) - 1.0) < 1e-12);
  assert(abs(result.registry_xyz(2) - 1.0) < 1e-9);
  assert(result.delta_xyz(3) < 0.0);

  % The controls, restated here so a reader of the test sees what is actually
  % being claimed: the legacy/registry difference survives holding the sampling
  % rate fixed (grid_effect is larger and opposite in sign), and "closer" flips
  % depending on which of the tree's two D50 references is adopted.
  assert(result.grid_effect < 0.0);
  assert(abs(result.grid_effect) > abs(result.delta_xyz(3)));
  assert(result.closer_to_cie);
  assert(result.closer_to_icc_pcs);

  fprintf('Issue #1475 colorimetry calculations passed.\n');
end
