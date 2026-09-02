function test_lut_type_range()
%TEST_LUT_TYPE_RANGE Check the full IccProfLib transform LUT enum boundary.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  profile_path = find_test_profile();
  call_mex = @iccdev.IccCmm.call_mex_for_test;
  warning_state = warning('query', 'iccdev:attachWarning');
  warning_cleanup = onCleanup(@() warning(warning_state.state, ...
    'iccdev:attachWarning'));
  warning('off', 'iccdev:attachWarning');

  for lut_type = 11:13
    cmm = call_mex('cmm_create');
    handle_cleanup = onCleanup(@() call_mex('cmm_free', cmm));
    call_mex('cmm_attach', cmm, profile_path, int32(0), int32(0), ...
      int32(lut_type), int32(1), int32(0));
    clear handle_cleanup;
  end

  cmm = call_mex('cmm_create');
  handle_cleanup = onCleanup(@() call_mex('cmm_free', cmm));
  failed = false;
  try
    call_mex('cmm_attach', cmm, profile_path, int32(0), int32(0), ...
      int32(14), int32(1), int32(0));
  catch e
    if strcmp(e.identifier, 'iccdev:badArgs')
      failed = true;
    else
      rethrow(e);
    end
  end
  clear handle_cleanup warning_cleanup;
  assert(failed, 'lutType 14 must be rejected by the MEX gateway.');
end

function profile_path = find_test_profile()
  this_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(fileparts(this_dir));
  profile_path = fullfile(repo_root, 'Testing', ...
    'sRGB_v4_ICC_preference.icc');
  assert(exist(profile_path, 'file') == 2, ...
    'Required LUT type fixture is missing: %s', profile_path);
end
