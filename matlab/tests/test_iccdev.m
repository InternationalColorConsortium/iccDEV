function summary = test_iccdev()
%TEST_ICCDEV Comprehensive test suite for iccdev MATLAB/Octave bindings.
%
%   test_iccdev()
%
% Runs all tests and reports pass/fail/skip. Requires icc_mex on the path.
% Groups that cannot run (missing fixture, no second profile, no Docker image)
% are counted and listed rather than passing silently.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  fprintf('=== iccdev MATLAB Test Suite ===\n\n');

  nPass = 0;
  nFail = 0;
  % Skips are counted, not just printed. Several groups below stand down when a
  % fixture, a second profile or the Docker image is unavailable, and until this
  % counter existed the summary line reported only passes and failures -- so a run
  % that silently skipped a third of the suite still ended in "N passed, 0 failed"
  % and a green CI leg. #2043 and #2044 are both examples: each reported a clean
  % result while "Docker interoperability" had not run at all.
  nSkip = 0;
  skipped = {};

  % --- Enum tests (always work) ---
  [nPass, nFail] = run_test(@test_color_space_values, 'ColorSpace values', nPass, nFail);
  [nPass, nFail] = run_test(@test_rendering_intent_values, 'RenderingIntent values', nPass, nFail);
  [nPass, nFail] = run_test(@test_interpolation_values, 'Interpolation values', nPass, nFail);
  [nPass, nFail] = run_test(@test_sig_to_str, 'sig_to_str', nPass, nFail);
  [nPass, nFail] = run_test(@test_curve_gamma_fixture, 'curveType gamma math', nPass, nFail);
  [nPass, nFail] = run_test(@iccdev.qa.check_colorimetry_issue_1475, ...
    'issue #1475 colorimetry math', nPass, nFail);
  [nPass, nFail] = run_test(@test_build_mex_dependency_paths, ...
    'build dependency path selection', nPass, nFail);

  % --- Profile tests (need test profiles) ---
  profilePath = find_test_profile();
  if ~isempty(profilePath)
    [nPass, nFail] = run_test(@() test_profile_open(profilePath), 'Profile open', nPass, nFail);
    [nPass, nFail] = run_test(@() test_profile_header(profilePath), 'Profile header', nPass, nFail);
    [nPass, nFail] = run_test(@() test_profile_read_vs_open(profilePath), 'Read vs Open', nPass, nFail);
    [nPass, nFail] = run_test(@() test_profile_header_fields(profilePath), 'Header fields', nPass, nFail);
    [nPass, nFail] = run_test(@() test_profile_double_close(profilePath), 'Double close safety', nPass, nFail);
  else
    [nSkip, skipped] = note_skip('Profile open/header/read/fields/double-close', ...
      'no test profile found', nSkip, skipped);
  end

  % --- CMM tests (basic, no profiles needed) ---
  [nPass, nFail] = run_test(@test_cmm_create, 'CMM create', nPass, nFail);
  [nPass, nFail] = run_test(@test_cmm_double_close, 'CMM double close', nPass, nFail);
  [nPass, nFail] = run_test(@test_profile_not_found, 'Profile not found error', nPass, nFail);
  if ~isempty(profilePath)
    [nPass, nFail] = run_test(@() test_docker_input_validation(profilePath), ...
      'Docker input validation', nPass, nFail);
  else
    [nSkip, skipped] = note_skip('Docker input validation', ...
      'no test profile found', nSkip, skipped);
  end

  [dockerAvailable, dockerDetails] = iccdev.docker_available();
  if dockerAvailable && ~isempty(profilePath)
    [nPass, nFail] = run_test(@test_docker_interop, ...
      'Docker interoperability', nPass, nFail);
  else
    [nSkip, skipped] = note_skip('Docker interoperability', dockerDetails, ...
      nSkip, skipped);
  end

  % --- CMM pipeline tests (need two compatible profiles for transform) ---
  [srcProf, dstProf] = find_two_profiles();
  if ~isempty(srcProf) && ~isempty(dstProf) && try_cmm_pipeline(srcProf, dstProf)
    [nPass, nFail] = run_test(@() test_cmm_roundtrip(srcProf, dstProf), 'CMM pipeline apply', nPass, nFail);
    [nPass, nFail] = run_test(@() test_cmm_bulk_apply(srcProf, dstProf), 'CMM bulk apply', nPass, nFail);
    [nPass, nFail] = run_test(@() test_apply_handle(srcProf, dstProf), 'Apply handle', nPass, nFail);
    [nPass, nFail] = run_test(@() test_apply_handle_parent_close(srcProf, dstProf), 'Apply handle parent close', nPass, nFail);
    [nPass, nFail] = run_test(@() test_mex_apply_parent_close(srcProf, dstProf), 'MEX apply parent close', nPass, nFail);
    [nPass, nFail] = run_test(@() test_cmm_single_precision(srcProf, dstProf), 'Single precision input', nPass, nFail);
  else
    [nSkip, skipped] = note_skip('CMM pipeline/bulk/apply-handle/single-precision', ...
      ['no compatible profile pair; from Testing run .\CreateAllProfiles.bat ' ...
       'in PowerShell on Windows, or ./CreateAllProfiles.sh on Unix'], ...
      nSkip, skipped);
  end

  fprintf('\n=== Results: %d passed, %d failed, %d skipped ===\n', ...
    nPass, nFail, nSkip);
  summary = struct('passed', nPass, 'failed', nFail, 'skipped', nSkip, ...
    'skippedGroups', {skipped});
  if nSkip > 0
    % Name them again at the end. A skip scrolls past in the middle of a long
    % run, and the count alone does not say what stopped running.
    fprintf('Skipped groups (coverage was reduced):\n');
    for i = 1:numel(skipped)
      fprintf('  - %s\n', skipped{i});
    end
  end
  if nFail > 0
    error('iccdev:testFailed', '%d test(s) failed.', nFail);
  end
end

% Records a skipped group so it reaches the summary as well as the log. Returns
% the updated counter and list rather than using a global, matching the
% pass/fail threading already used by run_test.
function [nSkip, skipped] = note_skip(what, why, nSkip, skipped)
  if isempty(why)
    why = 'unavailable';
  end
  fprintf('  SKIP: %s - %s\n', what, why);
  nSkip = nSkip + 1;
  skipped{end+1} = sprintf('%s (%s)', what, why);
end

function [nPass, nFail] = run_test(fn, name, nPass, nFail)
  try
    fn();
    fprintf('  PASS: %s\n', name);
    nPass = nPass + 1;
  catch e
    fprintf('  FAIL: %s - %s\n', name, e.message);
    nFail = nFail + 1;
  end
end

function path = find_test_profile()
  thisDir = fileparts(mfilename('fullpath'));
  repoRoot = fileparts(fileparts(thisDir));
  testDir = fullfile(repoRoot, 'Testing');

  candidates = {
    fullfile(testDir, 'Display', 'sRGB_v4_ICC_preference.icc')
    fullfile(testDir, 'Display', 'sRGB2014.icc')
  };

  path = '';
  for i = 1:numel(candidates)
    if exist(candidates{i}, 'file')
      path = candidates{i};
      return;
    end
  end

  % Search for any .icc file
  files = dir(fullfile(testDir, '**', '*.icc'));
  if ~isempty(files)
    path = fullfile(files(1).folder, files(1).name);
  end
end

function [src, dst] = find_two_profiles()
  thisDir = fileparts(mfilename('fullpath'));
  repoRoot = fileparts(fileparts(thisDir));
  testDir = fullfile(repoRoot, 'Testing');

  % Look for Display profiles that support CMM pipelines
  candidates = {
    fullfile(testDir, 'Display', 'sRGB_v4_ICC_preference.icc')
    fullfile(testDir, 'Display', 'sRGB2014.icc')
    fullfile(testDir, 'Display', 'sRGB_D65_MAT.icc')
    fullfile(testDir, 'Display', 'sRGB_D65_colorimetric.icc')
    fullfile(testDir, 'sRGB_v4_ICC_preference.icc')
  };

  found = {};
  for i = 1:numel(candidates)
    if exist(candidates{i}, 'file')
      found{end+1} = candidates{i}; %#ok<AGROW>
      if numel(found) >= 2, break; end
    end
  end

  % Fall back: search for .icc files outside CalcTest (CalcTest profiles
  % are calculator element profiles that don't work in CMM pipelines)
  if numel(found) < 2
    searchDirs = {'Display', 'Encoding', 'Named', 'SpecRef', 'HDR'};
    for d = 1:numel(searchDirs)
      subdir = fullfile(testDir, searchDirs{d});
      if ~exist(subdir, 'dir'), continue; end
      files = dir(fullfile(subdir, '*.icc'));
      for i = 1:numel(files)
        p = fullfile(files(i).folder, files(i).name);
        if numel(found) < 2 && ~any(strcmp(found, p))
          found{end+1} = p; %#ok<AGROW>
        end
        if numel(found) >= 2, break; end
      end
      if numel(found) >= 2, break; end
    end
  end

  if numel(found) >= 2
    src = found{1};
    dst = found{2};
  elseif isscalar(found)
    src = found{1};
    dst = found{1};  % self round-trip
  else
    src = '';
    dst = '';
  end
end

function ok = try_cmm_pipeline(srcPath, dstPath)
  %TRY_CMM_PIPELINE Test if two profiles can form a valid CMM pipeline.
  ok = false;
  try
    cmm = iccdev.IccCmm();
    cmm.attach(srcPath);
    cmm.attach(dstPath);
    cmm.begin();
    ok = true;
    cmm.close();
  catch
    try cmm.close(); catch, end
  end
end

%% Test functions
function test_color_space_values()
  assert(iccdev.ColorSpace.RGB  == uint32(hex2dec('52474220')));
  assert(iccdev.ColorSpace.CMYK == uint32(hex2dec('434D594B')));
  assert(iccdev.ColorSpace.Lab  == uint32(hex2dec('4C616220')));
  assert(iccdev.ColorSpace.XYZ  == uint32(hex2dec('58595A20')));
end

function test_rendering_intent_values()
  assert(iccdev.RenderingIntent.Perceptual == 0);
  assert(iccdev.RenderingIntent.RelativeColorimetric == 1);
  assert(iccdev.RenderingIntent.Saturation == 2);
  assert(iccdev.RenderingIntent.AbsoluteColorimetric == 3);
end

function test_interpolation_values()
  assert(iccdev.Interpolation.Linear == 0);
  assert(iccdev.Interpolation.Tetrahedral == 1);
end

function test_sig_to_str()
  s = iccdev.sig_to_str(uint32(hex2dec('52474220')));
  assert(strcmp(s, 'RGB'), 'Expected RGB, got %s', s);
end

function test_curve_gamma_fixture()
  results = run_gamma_qa();
  assert(numel(results) == 3, 'Expected red, green, and blue TRC results');
  assert(all([results.raw] == 565), 'Expected raw u8Fixed8 value 565');
  assert(all([results.gamma] == 2.20703125), ...
    'Expected decoded gamma 2.20703125');
end

function test_profile_open(path)
  p = iccdev.IccProfile(path);
  assert(p.is_valid(), 'Profile should be valid after open');
  p.close();
  assert(~p.is_valid(), 'Profile should be invalid after close');
end

function test_profile_header(path)
  p = iccdev.IccProfile(path);
  hdr = p.header();
  assert(hdr.size > 0, 'Header size should be > 0');
  assert(hdr.magic == hex2dec('61637370'), 'Magic should be acsp');
  p.close();
end

function test_profile_read_vs_open(path)
  p1 = iccdev.IccProfile(path, 'lazy', true);
  p2 = iccdev.IccProfile(path, 'lazy', false);
  h1 = p1.header();
  h2 = p2.header();
  assert(h1.size == h2.size, 'Headers should match');
  p1.close();
  p2.close();
end

function test_profile_header_fields(path)
  p = iccdev.IccProfile(path);
  hdr = p.header();

  % Validate all expected fields exist
  assert(isfield(hdr, 'size'), 'Missing size');
  assert(isfield(hdr, 'version'), 'Missing version');
  assert(isfield(hdr, 'colorSpace'), 'Missing colorSpace');
  assert(isfield(hdr, 'pcs'), 'Missing pcs');
  assert(isfield(hdr, 'versionString'), 'Missing versionString');
  assert(isfield(hdr, 'profileId'), 'Missing profileId');
  assert(isfield(hdr, 'illuminantX'), 'Missing illuminantX');
  assert(isfield(hdr, 'illuminantY'), 'Missing illuminantY');
  assert(isfield(hdr, 'illuminantZ'), 'Missing illuminantZ');

  % D50 illuminant bounds (should be close to X=0.9642 Y=1.0 Z=0.8249)
  assert(hdr.illuminantY > 0.5 && hdr.illuminantY < 1.5, ...
    'illuminantY should be near 1.0, got %f', hdr.illuminantY);

  % Version string format check
  assert(~isempty(hdr.versionString), 'versionString should not be empty');

  % Profile ID is 16 bytes
  assert(numel(hdr.profileId) == 16, 'profileId should be 16 bytes');

  p.close();
end

function test_profile_double_close(path)
  p = iccdev.IccProfile(path);
  p.close();
  p.close();  % should not crash or error
  assert(~p.is_valid());
end

function test_cmm_create()
  cmm = iccdev.IccCmm();
  assert(cmm.is_valid(), 'CMM should be valid after create');
  cmm.close();
  assert(~cmm.is_valid(), 'CMM should be invalid after close');
end

function test_cmm_double_close()
  cmm = iccdev.IccCmm();
  cmm.close();
  cmm.close();  % should not crash or error
  assert(~cmm.is_valid());
end

function test_profile_not_found()
  threw = false;
  try
    iccdev.IccProfile('/nonexistent/path/profile.icc');
  catch
    threw = true;
  end
  assert(threw, 'Should throw for nonexistent profile');
end

function test_docker_input_validation(profilePath)
  threw = false;
  try
    iccdev.docker_validate(profilePath, 'Image', ...
      [iccdev.default_docker_image() ';invalid']);
  catch
    threw = true;
  end
  assert(threw, 'Unsafe Docker image references must be rejected');

  commaPath = [tempname ',profile.icc'];
  copyfile(profilePath, commaPath);
  cleanup = onCleanup(@() delete_if_exists(commaPath));
  identifier = '';
  try
    iccdev.docker_validate(commaPath);
  catch e
    identifier = e.identifier;
  end
  assert(strcmp(identifier, 'iccdev:unsafeDockerPath'), ...
    'Docker mount paths containing commas must be rejected');
  clear cleanup;

  if exist('string', 'builtin') || exist('string', 'class')
    identifier = '';
    try
      iccdev.docker_validate(string(profilePath), 'Image', ... %#ok<STRQUOT>
        string([iccdev.default_docker_image() ';invalid'])); %#ok<STRQUOT>
    catch e
      identifier = e.identifier;
    end
    assert(strcmp(identifier, 'iccdev:invalidDockerImage'), ...
      'String scalar inputs should pass parsing before image validation');

    identifier = '';
    try
      iccdev.docker_available( ...
        string([iccdev.default_docker_image() ';invalid'])); %#ok<STRQUOT>
    catch e
      identifier = e.identifier;
    end
    assert(strcmp(identifier, 'iccdev:invalidDockerImage'), ...
      'docker_available should accept string scalars before validation');
  end
end

function test_docker_interop()
  result = run_docker_qa();
  assert(result.dumpStatus == 0);
  assert(result.roundTripStatus == 0);
  assert(exist(result.profile, 'file') == 2);
end

function delete_if_exists(path)
  if exist(path, 'file') == 2
    delete(path);
  end
end

function test_cmm_roundtrip(srcPath, dstPath)
  cmm = iccdev.IccCmm();
  cmm.attach(srcPath);
  cmm.attach(dstPath);
  cmm.begin();

  % Transform a single pixel
  pixel = [0.5 0.3 0.1];
  result = cmm.apply(pixel);

  assert(size(result, 1) == 1, 'Should return 1 row');
  assert(size(result, 2) > 0, 'Should return at least 1 channel');
  assert(all(isfinite(result(:))), 'Results should be finite');

  cmm.close();
end

function test_cmm_bulk_apply(srcPath, dstPath)
  cmm = iccdev.IccCmm();
  cmm.attach(srcPath);
  cmm.attach(dstPath);
  cmm.begin();

  % Bulk transform: 100 pixels
  pixels = rand(100, 3);
  results = cmm.apply(pixels);

  assert(size(results, 1) == 100, 'Should return 100 rows, got %d', size(results, 1));
  assert(all(isfinite(results(:))), 'All results should be finite');

  % Verify consistency: same input gives same output
  result1 = cmm.apply(pixels(1,:));
  assert(max(abs(result1 - results(1,:))) < 1e-6, 'Repeated apply should be consistent');

  cmm.close();
end

function test_apply_handle(srcPath, dstPath)
  cmm = iccdev.IccCmm();
  cmm.attach(srcPath);
  cmm.attach(dstPath);
  cmm.begin();

  % Create thread-safe apply handle
  ah = cmm.get_apply();

  pixel = [0.5 0.3 0.1];
  result_cmm = cmm.apply(pixel);
  result_ah  = ah.apply(pixel);

  % Both should produce same result
  assert(max(abs(result_cmm - result_ah)) < 1e-6, ...
    'Apply handle should match CMM apply');

  ah.close();
  cmm.close();
end

function test_apply_handle_parent_close(srcPath, dstPath)
  cmm = iccdev.IccCmm();
  cmm.attach(srcPath);
  cmm.attach(dstPath);
  cmm.begin();

  ah = cmm.get_apply();
  cmm.close();

  failed = false;
  try
    ah.apply([0.5 0.3 0.1]);
  catch
    failed = true;
  end
  assert(failed, 'Apply handle should fail after parent CMM is closed');
  ah.close();
end

function test_mex_apply_parent_close(srcPath, dstPath)
  call_mex = @iccdev.IccCmm.call_mex_for_test;
  cmm = call_mex('cmm_create');
  call_mex('cmm_attach', cmm, srcPath);
  call_mex('cmm_attach', cmm, dstPath);
  call_mex('cmm_begin', cmm);
  ah = call_mex('apply_create', cmm);
  call_mex('cmm_free', cmm);

  failed = false;
  try
    call_mex('apply_apply', ah, [0.5 0.3 0.1], int32(3), int32(3));
  catch
    failed = true;
  end
  assert(failed, 'Native apply handle should fail after parent CMM is closed');
  call_mex('apply_free', ah);
end

function test_cmm_single_precision(srcPath, dstPath)
  cmm = iccdev.IccCmm();
  cmm.attach(srcPath);
  cmm.attach(dstPath);
  cmm.begin();

  pixels_double = [0.5 0.3 0.1; 0.9 0.1 0.5];
  pixels_single = single(pixels_double);

  result_d = cmm.apply(pixels_double);
  result_s = cmm.apply(pixels_single);

  % Results should be very close (float vs double precision difference)
  assert(max(abs(result_d(:) - result_s(:))) < 1e-4, ...
    'Single and double precision should produce similar results');

  cmm.close();
end
