function run_local_qa()
%RUN_LOCAL_QA Run the iccDEV MATLAB regression suite and local smoke tests.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  matlab_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(matlab_dir);
  tests_dir = fullfile(matlab_dir, 'tests');
  profile_candidates = {
    fullfile(repo_root, 'Testing', 'Display', 'sRGB_D65_MAT.icc')
    fullfile(repo_root, 'Testing', 'sRGB_v4_ICC_preference.icc')
  };
  profile_path = '';
  for i = 1:numel(profile_candidates)
    if exist(profile_candidates{i}, 'file') == 2
      profile_path = profile_candidates{i};
      break;
    end
  end

  addpath(matlab_dir);
  addpath(tests_dir);

  if isempty(profile_path)
    error('iccdev:qaProfileMissing', ...
      'No compatible QA profile was found under %s.', ...
      fullfile(repo_root, 'Testing'));
  end

  test_iccdev();

  profile = iccdev.IccProfile(profile_path);
  header = profile.header();
  assert(header.size > 0);
  assert(strcmp(iccdev.sig_to_str(uint32(header.colorSpace)), 'RGB'));
  profile.close();

  cmm = iccdev.IccCmm();
  cmm.attach(profile_path);
  cmm.attach(profile_path);
  cmm.begin();

  pixels = rand(100000, 3, 'single');
  output = cmm.apply(pixels);
  assert(isequal(size(output), size(pixels)));
  assert(all(isfinite(output(:))));
  repeat_output = cmm.apply(pixels(1:100, :));
  expected_output = output(1:100, :);
  assert(max(abs(repeat_output(:) - expected_output(:))) < 1e-6);

  apply_handle = cmm.get_apply();
  apply_output = apply_handle.apply(pixels(1:100, :));
  assert(max(abs(apply_output(:) - expected_output(:))) < 1e-6);
  apply_handle.close();
  cmm.close();

  missing_profile_failed = false;
  try
    iccdev.IccProfile(fullfile(repo_root, 'missing-profile.icc'));
  catch
    missing_profile_failed = true;
  end
  assert(missing_profile_failed, ...
    'Opening a missing profile should raise an error.');

  fprintf('Local MATLAB QA passed for %s\n', repo_root);
end
