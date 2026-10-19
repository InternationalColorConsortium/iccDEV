function summary = run_ci_qa()
%RUN_CI_QA Run focused native MATLAB QA without release packaging.
%   Set ICCDEV_BUILD_DIR to the Release native build before calling.
%   Docker interoperability has a separate CI job and is the only allowed
%   unavailable group in the comprehensive suite.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  matlab_dir = fileparts(mfilename('fullpath'));
  build_dir = getenv('ICCDEV_BUILD_DIR');
  assert(~isempty(build_dir) && exist(build_dir, 'dir') == 7, ...
    'iccdev:ciBuildMissing', 'Set ICCDEV_BUILD_DIR to the native build.');
  assert(usejava('jvm'), 'iccdev:ciJavaRequired', ...
    'Native MATLAB tool wrappers require Java.');
  assert(exist(fullfile(matlab_dir, '+iccdev', 'private', ...
    ['icc_mex.' mexext()]), 'file') == 3, ...
    'iccdev:ciMexMissing', 'Build the platform-native private MEX gateway.');
  addpath(matlab_dir);
  addpath(fullfile(matlab_dir, 'tests'));

  test_heif_dump_native(build_dir);
  test_pawg_q1();
  test_luminance_normalization();
  test_colorimetry_issue_1475();
  test_add_docker_path();
  test_plot();
  summary = test_iccdev();
  assert(summary.failed == 0, 'iccdev:ciTestFailed', ...
    'Native MATLAB QA must report zero failures.');
  if summary.skipped > 0
    assert(summary.skipped == 1 && isscalar(summary.skippedGroups) && ...
      strncmp(summary.skippedGroups{1}, 'Docker interoperability (', ...
      numel('Docker interoperability (')), ...
      'iccdev:ciUnexpectedSkip', ...
      'Only the separately tested Docker interoperability group may skip.');
  end
  run_local_qa();
  run_gamma_qa();

  examples = {'read_profile.m', 'color_transform.m', 'gamma_curve.m', ...
    'ciede2000.m', 'luminance_normalization.m', 'colorimetry_issue_1475.m'};
  for i = 1:numel(examples)
    run_example(fullfile(matlab_dir, 'examples', examples{i}));
  end
  fprintf('Native MATLAB CI QA passed on %s with %s.\n', computer(), mexext());
end

function run_example(path)
  run(path);
end
