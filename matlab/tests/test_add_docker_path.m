function test_add_docker_path()
%TEST_ADD_DOCKER_PATH Verify opt-in Docker PATH configuration.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  original_path = getenv('PATH');
  restore_path = onCleanup(@() setenv('PATH', original_path));
  docker_dir = tempname;
  mkdir(docker_dir);
  remove_dir = onCleanup(@() rmdir(docker_dir, 's'));

  if ispc()
    executable = fullfile(docker_dir, 'docker.exe');
  else
    executable = fullfile(docker_dir, 'docker');
  end
  file = fopen(executable, 'wb');
  assert(file >= 0, 'Unable to create the Docker executable fixture.');
  fclose(file);

  selected = add_docker_path(docker_dir);
  assert(strcmp(selected, docker_dir), ...
    'The helper should return the selected Docker directory.');
  entries = regexp(getenv('PATH'), pathsep, 'split');
  assert(strcmpi(entries{1}, docker_dir), ...
    'The selected Docker directory should be first on PATH.');

  add_docker_path(docker_dir);
  entries = regexp(getenv('PATH'), pathsep, 'split');
  assert(sum(strcmpi(entries, docker_dir)) == 1, ...
    'Repeated calls must not duplicate the Docker directory.');

  invalid_failed = false;
  try
    add_docker_path(char('first', 'second'));
  catch e
    invalid_failed = strcmp(e.identifier, 'iccdev:dockerPathRequired');
  end
  assert(invalid_failed, ...
    'Multi-row character arrays must be rejected.');

  first_dir = fullfile(tempdir, 'iccdev-path-first');
  second_dir = fullfile(tempdir, 'iccdev-path-second');
  entries = iccdev.qa.path_entries([pathsep first_dir pathsep '.' pathsep ...
    'relative' pathsep pathsep second_dir pathsep]);
  assert(isequal(entries, {first_dir, second_dir}), ...
    'Empty and relative PATH entries must be removed before tool discovery.');
  assert(~iccdev.qa.path_contains({'CaseSensitive'}, 'casesensitive', false), ...
    'Case-sensitive filesystems must preserve distinct PATH entries.');
  assert(iccdev.qa.path_contains({'CaseInsensitive'}, 'caseinsensitive', true), ...
    'Windows PATH comparisons should ignore case.');

  clear remove_dir restore_path;
  fprintf('Docker PATH helper tests passed.\n');
end
