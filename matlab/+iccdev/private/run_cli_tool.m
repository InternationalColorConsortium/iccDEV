function [standard_output, error_output] = run_cli_tool(tool_name, input_path, arguments, build_dir, explicit_tool, error_prefix)
%RUN_CLI_TOOL Run an iccDEV CLI tool without invoking a shell.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  input_path = canonical_file(input_path, [error_prefix 'InputNotFound'], ...
    [tool_name ' input']);
  tool_path = find_tool(tool_name, build_dir, explicit_tool, error_prefix);

  if exist('usejava', 'builtin') ~= 5 && exist('usejava', 'file') ~= 2
    error([error_prefix 'JvmUnavailable'], ...
      '%s requires a Java-enabled MATLAB or Octave runtime.', tool_name);
  end
  if ~usejava('jvm')
    error([error_prefix 'JvmUnavailable'], ...
      '%s requires a Java-enabled MATLAB or Octave runtime.', tool_name);
  end

  command = javaObject('java.util.ArrayList');
  command.add(javaObject('java.lang.String', tool_path));
  command.add(javaObject('java.lang.String', input_path));
  for i = 1:numel(arguments)
    command.add(javaObject('java.lang.String', char(arguments{i})));
  end

  error_path = [tempname '.txt'];
  cleanup = onCleanup(@() delete_if_exists(error_path));
  try
    builder = javaObject('java.lang.ProcessBuilder', command);
    configure_tool_environment(builder, tool_path);
    builder.redirectError(javaObject('java.io.File', error_path));
    process = builder.start();
  catch e
    error([error_prefix 'ProcessStartFailed'], ...
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
    error([error_prefix 'CommandFailed'], '%s failed with status %d:\n%s', ...
      tool_name, status, diagnostics);
  end
end

function configure_tool_environment(builder, tool_path)
  if ispc() || ismac()
    return;
  end

  environment = builder.environment();
  environment.put('LD_LIBRARY_PATH', iccdev.qa.native_library_path( ...
    getenv('LD_LIBRARY_PATH'), fileparts(tool_path)));
end

function tool_path = find_tool(tool_name, build_dir, explicit_tool, error_prefix)
  if ~isempty(explicit_tool)
    tool_path = canonical_file(explicit_tool, [error_prefix 'ToolNotFound'], ...
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
        tool_path = canonical_file(candidate, [error_prefix 'ToolNotFound'], ...
          [tool_name ' executable']);
        return;
      end
    end
  end

  path_dirs = iccdev.qa.path_entries(getenv('PATH'));
  for i = 1:numel(path_dirs)
    candidate = fullfile(path_dirs{i}, executable);
    if exist(candidate, 'file') == 2
      tool_path = canonical_file(candidate, [error_prefix 'ToolNotFound'], ...
        [tool_name ' executable']);
      return;
    end
  end

  error([error_prefix 'ToolNotFound'], ...
    ['%s was not found. Build %s, set ICCDEV_BUILD_DIR, or pass the ' ...
     'corresponding Tool option.'], tool_name, tool_name);
end

function target = tool_target(tool_name)
  switch tool_name
    case 'iccToJson'
      target = 'IccToJson';
    case 'iccFromJson'
      target = 'IccFromJson';
    case 'iccHeifDump'
      target = 'IccHeifDump';
    otherwise
      error('iccdev:unsupportedTool', 'Unsupported CLI tool: %s', tool_name);
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
