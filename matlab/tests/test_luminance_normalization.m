function test_luminance_normalization()
%TEST_LUMINANCE_NORMALIZATION Reproduce issue #1811 luminance calculations.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  results = iccdev.qa.check_luminance_normalization();

  expected_normalized = [0.9504, 1.0, 1.0889];
  expected_rows = repmat(expected_normalized, 3, 1);
  assert(max(abs(results.normalizedXYZ(:) - expected_rows(:))) < 1e-12, ...
    'Normalized fixtures must reproduce the D65 XYZ triple.');
  assert(max(results.normalizationError) < 1e-12, ...
    'Fixture normalization error exceeds tolerance.');
  assert(abs(results.bt2100SurroundXYZ(2) - 5.0) < 1e-12, ...
    'The BT.2100 physical surround should remain at 5 cd/m^2.');

  warn_y = results.windowY(results.windowWarns);
  clean_y = results.windowY(~results.windowWarns);
  assert(isequal(single(warn_y), ...
    single([0.99; 0.995; 1; 1.005; 1.01])), ...
    'Unexpected warning-window values.');
  assert(isequal(single(clean_y), single([0.98; 1.02; 5; 300; 500])), ...
    'Unexpected accepted luminance values.');

  fprintf('Luminance normalization calculations passed.\n');
end
