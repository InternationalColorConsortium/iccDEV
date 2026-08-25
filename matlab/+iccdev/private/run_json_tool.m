function run_json_tool(tool_name, input_path, output_path, build_dir, explicit_tool)
%RUN_JSON_TOOL Run an ICC JSON conversion tool without invoking a shell.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  tool_path = find_tool(tool_name, build_dir, explicit_tool);
  input_path = canonical_file(input_path, 'iccdev:jsonInputNotFound', ...
    'JSON conversion input');

  if exist('usejava', 'builtin') ~= 5 && exist('usejava', 'file') ~= 2
    error('iccdev:jsonJvmUnavailable', ...
      'ICC JSON conversion requires a Java-enabled MATLAB or Octave runtime.');
  end
  if ~usejava('jvm')
    error('iccdev:jsonJvmUnavailable', ...
      'ICC JSON conversion requires a Java-enabled MATLAB or Octave runtime.');
  end

  command = javaObject('java.util.ArrayList');
  command.add(javaObject('java.lang.String', tool_path));
  command.add(javaObject('java.lang.String', input_path));
  command.add(javaObject('java.lang.String', output_path));

  error_path = [tempname '.txt'];
  cleanup = onCleanup(@() delete_if_exists(error_path));
  try
    builder = javaObject('java.lang.ProcessBuilder', command);
    builder.redirectError(javaObject('java.io.File', error_path));
    process = builder.start();
  catch e
    error('iccdev:jsonProcessStartFailed', ...
      'Unable to start %s: %s', tool_name, e.message);
  end

  standard_output = read_stream(process.getInputStream());
  status = process.waitFor();
  if exist(error_path, 'file') == 2
    error_output = fileread(error_path);
  else
    error_output = '';
  end
  clear cleanup;

  if status ~= 0
    diagnostics = strtrim(sprintf('%s\n%s', standard_output, error_output));
    error('iccdev:jsonCommandFailed', '%s failed with status %d:\n%s', ...
      tool_name, status, diagnostics);
  end
  if exist(output_path, 'file') ~= 2
    error('iccdev:jsonOutputMissing', ...
      '%s succeeded but did not create: %s', tool_name, output_path);
  end
end

function tool_path = find_tool(tool_name, build_dir, explicit_tool)
  if ~isempty(explicit_tool)
    tool_path = canonical_file(explicit_tool, 'iccdev:jsonToolNotFound', ...
      [tool_name ' executable']);
    return;
  end

  this_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(fileparts(fileparts(this_dir)));
  build_dirs = {};
  if ~isempty(build_dir)
    build_dirs{end+1} = build_dir;
  end
  env_build_dir = getenv('ICCDEV_BUILD_DIR');
  if ~isempty(env_build_dir)
    build_dirs{end+1} = env_build_dir;
  end
  build_dirs = [build_dirs(:); {
    fullfile(repo_root, 'msvc')
    fullfile(repo_root, 'out', 'matlab')
    fullfile(repo_root, 'Build', 'Cmake', 'build')
    fullfile(repo_root, 'Build')
  }];

  if ispc()
    executable = [tool_name '.exe'];
    names = {
      fullfile('bin', 'Release', executable)
      fullfile('bin', executable)
      fullfile('Tools', tool_target(tool_name), 'Release', executable)
      fullfile('Tools', tool_target(tool_name), executable)
    };
  else
    executable = tool_name;
    names = {
      fullfile('bin', executable)
      fullfile('Tools', tool_target(tool_name), executable)
    };
  end

  for i = 1:numel(build_dirs)
    for j = 1:numel(names)
      candidate = fullfile(build_dirs{i}, names{j});
      if exist(candidate, 'file') == 2
        tool_path = canonical_file(candidate, 'iccdev:jsonToolNotFound', ...
          [tool_name ' executable']);
        return;
      end
    end
  end

  path_dirs = iccdev.qa.path_entries(getenv('PATH'));
  for i = 1:numel(path_dirs)
    candidate = fullfile(path_dirs{i}, executable);
    if exist(candidate, 'file') == 2
      tool_path = canonical_file(candidate, 'iccdev:jsonToolNotFound', ...
        [tool_name ' executable']);
      return;
    end
  end

  error('iccdev:jsonToolNotFound', ...
    ['%s was not found. Build %s, set ICCDEV_BUILD_DIR, or pass the ' ...
     'corresponding Tool option.'], tool_name, tool_name);
end

function target = tool_target(tool_name)
  switch tool_name
    case 'iccToJson'
      target = 'IccToJson';
    case 'iccFromJson'
      target = 'IccFromJson';
    otherwise
      error('iccdev:jsonUnknownTool', 'Unsupported JSON tool: %s', tool_name);
  end
end

function path = canonical_file(path, error_id, description)
  [exists, attributes] = fileattrib(path); %#ok<FILEATTRIB>
  if ~exists || attributes.directory
    error(error_id, '%s not found: %s', description, path);
  end
  path = attributes.Name;
end

function output = read_stream(stream)
  reader = javaObject('java.io.BufferedReader', ...
    javaObject('java.io.InputStreamReader', stream));
  lines = {};
  line = reader.readLine();
  while ~isempty(line)
    lines{end+1} = char(line);
    line = reader.readLine();
  end
  reader.close();
  if isempty(lines)
    output = '';
  else
    output = sprintf('%s\n', lines{:});
  end
end

function delete_if_exists(path)
  if exist(path, 'file') == 2
    delete(path);
  end
end
