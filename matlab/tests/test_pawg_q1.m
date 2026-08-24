function test_pawg_q1()
%TEST_PAWG_Q1 Compare the MATLAB and native PAWG Check Q1 implementations.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  tests_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(fileparts(tests_dir));
  test_reference_math();
  test_grid_budget();
  test_pcs_decoding();

  profile_path = fullfile(repo_root, 'Testing', ...
    'sRGB_v4_ICC_preference.icc');
  result = iccdev.qa.audit_pawg_q1(profile_path);

  assert(result.calculated.samples == 729);
  assert(strcmp(result.calculated.verdict, 'OK'));
  assert(strcmp(result.native.model, 'CIccCmm profile transform'));
  assert(result.tolerance == 1e-4);
  assert(result.sampleCountAgreement, ...
    'MATLAB and iccPawgReport Q1 sample counts differ.');
  assert(result.verdictAgreement, ...
    'MATLAB and iccPawgReport Q1 verdicts differ.');
  assert(result.metricAgreement, ...
    'MATLAB and iccPawgReport Q1 metrics differ by more than %.4g.', ...
    result.tolerance);
  assert(result.passed);

  metric_deltas = [result.deltas.firstAverage, result.deltas.firstMaximum, ...
    result.deltas.secondAverage, result.deltas.secondMaximum];
  fprintf(['PAWG Q1 audit passed: verdict=%s model=%s samples=%d ' ...
    'tolerance=%.6g max-abs-delta=%.15g\n'], ...
    result.calculated.verdict, result.native.model, ...
    result.calculated.samples, result.tolerance, max(abs(metric_deltas)));
  fprintf(['  MATLAB CIEDE2000: first avg=%.15g max=%.15g; ' ...
    'second avg=%.15g max=%.15g\n'], ...
    result.calculated.firstAverage, result.calculated.firstMaximum, ...
    result.calculated.secondAverage, result.calculated.secondMaximum);
  fprintf(['  native CIEDE2000: first avg=%.15g max=%.15g; ' ...
    'second avg=%.15g max=%.15g\n'], ...
    result.native.firstAverage, result.native.firstMaximum, ...
    result.native.secondAverage, result.native.secondMaximum);
end

function test_reference_math()
  lab1 = [ ...
    50.0000,  2.6772, -79.7751
    50.0000,  3.1571, -77.2803
    50.0000,  2.8361, -74.0200
    50.0000, -1.3802, -84.2814
    50.0000, -1.1848, -84.8006
    50.0000, -0.9009, -85.5211];
  lab2 = repmat([50.0000, 0.0000, -82.7485], 6, 1);
  expected = [2.0425; 2.8615; 3.4412; 1.0000; 1.0000; 1.0000];
  actual = iccdev.qa.delta_e_2000(lab1, lab2);
  assert(max(abs(actual - expected)) < 5e-5, ...
    'CIEDE2000 reference-vector agreement failed.');
end

function test_grid_budget()
  gray = iccdev.qa.bounded_grid(1, 9);
  rgb = iccdev.qa.bounded_grid(3, 9);
  cmyk = iccdev.qa.bounded_grid(4, 5);
  assert(isequal(size(gray), [9, 1]));
  assert(isequal(size(rgb), [729, 3]));
  assert(isequal(size(cmyk), [625, 4]));

  rejected = false;
  try
    iccdev.qa.bounded_grid(10, 5);
  catch e
    rejected = strcmp(e.identifier, 'iccdev:pawgQ1SampleBudgetExceeded');
  end
  assert(rejected, 'A grid above the quality sample budget must be rejected.');
end

function test_pcs_decoding()
  lab_white = iccdev.qa.pcs_to_lab( ...
    [1, 128 / 255, 128 / 255], iccdev.ColorSpace.Lab);
  assert(max(abs(lab_white - [100, 0, 0])) < 1e-10);

  internal_d50 = [0.9642, 1.0, 0.8249] * (32768 / 65535);
  xyz_white = iccdev.qa.pcs_to_lab(internal_d50, iccdev.ColorSpace.XYZ);
  assert(max(abs(xyz_white - [100, 0, 0])) < 5e-3);
end
