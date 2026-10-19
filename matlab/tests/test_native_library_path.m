function test_native_library_path()
%TEST_NATIVE_LIBRARY_PATH Verify native tools do not inherit MATLAB libraries.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  root = tempname;
  matlab_root = fullfile(root, 'matlab');
  runtime_dir = fullfile(matlab_root, 'sys', 'os', 'glnxa64');
  mkdir(runtime_dir);
  remove_root = onCleanup(@() rmdir(root, 's'));
  stage_dir = fullfile(root, 'stage');
  host_dir = fullfile(root, 'host');
  sibling_dir = [matlab_root '-extras'];

  inherited = strjoin({stage_dir, runtime_dir, '', ...
    fullfile(matlab_root, 'bin'), matlab_root, host_dir, sibling_dir, ...
    stage_dir}, pathsep);
  actual = iccdev.qa.native_library_path(inherited, stage_dir, ...
    [matlab_root filesep]);
  expected = strjoin({stage_dir, host_dir, sibling_dir}, pathsep);
  assert(strcmp(actual, expected), ...
    'Expected MATLAB library entries to be removed, got: %s', actual);

  assert(strcmp(iccdev.qa.native_library_path('', stage_dir, matlab_root), ...
    stage_dir), 'An empty inherited path must yield only the tool directory.');
  assert(isempty(iccdev.qa.native_library_path(runtime_dir, '', matlab_root)), ...
    'A MATLAB-only inherited path must be removed completely.');

  try
    iccdev.qa.native_library_path({'a', 'b'}, stage_dir, matlab_root);
    error('iccdev:test:expectedError', 'Expected invalid input to fail.');
  catch e
    assert(strcmp(e.identifier, 'iccdev:invalidLibraryPath'), ...
      'Unexpected error: %s', e.identifier);
  end
end
