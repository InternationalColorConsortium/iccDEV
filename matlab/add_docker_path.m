function docker_dir = add_docker_path(docker_dir)
%ADD_DOCKER_PATH Add a user-selected Docker CLI directory to PATH.
%
%   docker_dir = add_docker_path(docker_dir)
%
% This helper only updates the current MATLAB process environment. It does not
% run Docker or start the MATLAB QA suite.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin ~= 1 || ~is_text_scalar(docker_dir)
    error('iccdev:dockerPathRequired', ...
      ['Provide one Docker CLI directory. Example: ' ...
       'add_docker_path(docker_cli_directory);']);
  end

  docker_dir = char(docker_dir);
  [exists, attributes] = fileattrib(docker_dir); %#ok<FILEATTRIB>
  if ~exists || ~attributes.directory
    error('iccdev:dockerPathNotFound', ...
      'Docker CLI directory not found: %s', docker_dir);
  end
  docker_dir = attributes.Name;

  if ispc()
    executable = fullfile(docker_dir, 'docker.exe');
  else
    executable = fullfile(docker_dir, 'docker');
  end
  if exist(executable, 'file') ~= 2
    error('iccdev:dockerExecutableNotFound', ...
      'Docker executable not found under %s.', docker_dir);
  end

  current_path = getenv('PATH');
  entries = iccdev.qa.path_entries(current_path);
  if ~iccdev.qa.path_contains(entries, docker_dir)
    setenv('PATH', [docker_dir pathsep current_path]);
  end
end

function valid = is_text_scalar(value)
  valid = (ischar(value) && (isempty(value) || size(value, 1) == 1)) || ...
    (isa(value, 'string') && isscalar(value));
end
