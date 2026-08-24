function result = audit_pawg_q1(profile_path, varargin)
%AUDIT_PAWG_Q1 Audit PAWG Check Q1 and compare with iccPawgReport.
%
%   result = iccdev.qa.audit_pawg_q1(profile_path)
%   result = iccdev.qa.audit_pawg_q1(profile_path, 'BuildDir', build_dir)
%   result = iccdev.qa.audit_pawg_q1(profile_path, 'PawgTool', tool_path)
%   The audit supports profiles whose native Q1 evaluator selects the general
%   CIccCmm profile-transform model.
%
%   Example from the repository root:
%     profile_path = fullfile('Testing', 'sRGB_v4_ICC_preference.icc');
%     setenv('ICCDEV_BUILD_DIR', fullfile(pwd, 'msvc'));
%     result = iccdev.qa.audit_pawg_q1(profile_path);
%     assert(result.passed);
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  options = parse_options(varargin{:});
  profile_path = canonical_file(profile_path, ...
    'iccdev:pawgQ1ProfileNotFound', 'ICC profile');

  profile = iccdev.IccProfile(profile_path);
  cleanup_profile = onCleanup(@() profile.close());
  header = profile.header();
  color_space = uint32(header.colorSpace);
  pcs = uint32(header.pcs);
  device_channels = color_space_channels(color_space);
  if device_channels < 1 || device_channels > 16
    error('iccdev:pawgQ1UnsupportedColorSpace', ...
      'Q1 requires between 1 and 16 device channels; profile has %d.', ...
      device_channels);
  end
  if pcs ~= iccdev.ColorSpace.Lab && pcs ~= iccdev.ColorSpace.XYZ
    error('iccdev:pawgQ1UnsupportedPcs', ...
      'Q1 requires Lab or XYZ PCS, not %s.', iccdev.sig_to_str(pcs));
  end
  clear cleanup_profile;

  tool_path = find_pawg_tool(options.BuildDir, options.PawgTool);
  native = run_native_q1(tool_path, profile_path);
  if ~strcmp(native.model, 'CIccCmm profile transform')
    error('iccdev:pawgQ1UnsupportedNativeModel', ...
      'Q1 audit requires CIccCmm profile transform; native model is %s.', ...
      native.model);
  end

  grid_size = 9;
  if device_channels >= 4
    grid_size = 5;
  end
  device0 = iccdev.qa.bounded_grid(device_channels, grid_size);

  forward = iccdev.IccCmm(color_space, pcs, true);
  reverse = iccdev.IccCmm(pcs, color_space, false);
  cleanup_cmms = onCleanup(@() close_cmms(forward, reverse));
  attach_q1_transform(forward, profile_path);
  attach_q1_transform(reverse, profile_path);

  pcs1 = forward.apply(device0);
  device1 = reverse.apply(pcs1);
  pcs2 = forward.apply(device1);
  device2 = reverse.apply(pcs2);
  pcs3 = forward.apply(device2);

  valid = all(isfinite(pcs1(:, 1:3)), 2) & ...
    all(isfinite(pcs2(:, 1:3)), 2) & all(isfinite(pcs3(:, 1:3)), 2);
  if ~any(valid)
    error('iccdev:pawgQ1NoValidSamples', ...
      'The MATLAB Q1 pipeline produced no valid samples.');
  end
  lab1 = iccdev.qa.pcs_to_lab(pcs1(valid, 1:3), pcs);
  lab2 = iccdev.qa.pcs_to_lab(pcs2(valid, 1:3), pcs);
  lab3 = iccdev.qa.pcs_to_lab(pcs3(valid, 1:3), pcs);
  first = iccdev.qa.delta_e_2000(lab1, lab2);
  second = iccdev.qa.delta_e_2000(lab2, lab3);

  calculated = struct( ...
    'model', 'CIccCmm profile transform', ...
    'samples', numel(first), ...
    'firstAverage', mean(first), ...
    'firstMaximum', max(first), ...
    'secondAverage', mean(second), ...
    'secondMaximum', max(second));
  calculated.verdict = q1_verdict(calculated);

  deltas = struct( ...
    'firstAverage', calculated.firstAverage - native.firstAverage, ...
    'firstMaximum', calculated.firstMaximum - native.firstMaximum, ...
    'secondAverage', calculated.secondAverage - native.secondAverage, ...
    'secondMaximum', calculated.secondMaximum - native.secondMaximum);
  metric_deltas = [deltas.firstAverage, deltas.firstMaximum, ...
    deltas.secondAverage, deltas.secondMaximum];

  result = struct( ...
    'profile', profile_path, ...
    'gridSize', grid_size, ...
    'calculated', calculated, ...
    'native', native, ...
    'deltas', deltas, ...
    'sampleCountAgreement', calculated.samples == native.samples, ...
    'modelAgreement', strcmp(calculated.model, native.model), ...
    'verdictAgreement', strcmp(calculated.verdict, native.verdict), ...
    'metricAgreement', all(abs(metric_deltas) <= options.Tolerance), ...
    'tolerance', options.Tolerance);
  result.passed = result.sampleCountAgreement && ...
    result.modelAgreement && result.verdictAgreement && result.metricAgreement;
  clear cleanup_cmms;
end

function options = parse_options(varargin)
  options = struct('BuildDir', '', 'PawgTool', '', 'Tolerance', 1e-4);
  if mod(numel(varargin), 2) ~= 0
    error('iccdev:pawgQ1InvalidOptions', ...
      'Options must be supplied as name-value pairs.');
  end
  for i = 1:2:numel(varargin)
    name = lower(char(varargin{i}));
    switch name
      case 'builddir'
        options.BuildDir = char(varargin{i + 1});
      case 'pawgtool'
        options.PawgTool = char(varargin{i + 1});
      case 'tolerance'
        options.Tolerance = double(varargin{i + 1});
      otherwise
        error('iccdev:pawgQ1InvalidOption', 'Unknown option: %s', name);
    end
  end
  if ~isscalar(options.Tolerance) || ~isfinite(options.Tolerance) || ...
      options.Tolerance < 0
    error('iccdev:pawgQ1InvalidTolerance', ...
      'Tolerance must be a finite, non-negative scalar.');
  end
end

function attach_q1_transform(cmm, profile_path)
  status = cmm.attach(profile_path, 'intent', 1, 'interp', 0, ...
    'luttype', 9, 'used2b', false);
  if status ~= 0
    error('iccdev:pawgQ1AttachFailed', ...
      'Unable to attach the relative-colorimetric Q1 transform (status %d).', ...
      status);
  end
  cmm.begin();
end

function channels = color_space_channels(signature)
  switch signature
    case iccdev.ColorSpace.Gray
      channels = 1;
    case {iccdev.ColorSpace.XYZ, iccdev.ColorSpace.Lab, ...
        iccdev.ColorSpace.Luv, iccdev.ColorSpace.YCbCr, ...
        iccdev.ColorSpace.Yxy, iccdev.ColorSpace.RGB, ...
        iccdev.ColorSpace.HSV, iccdev.ColorSpace.HLS, ...
        iccdev.ColorSpace.CMY}
      channels = 3;
    case iccdev.ColorSpace.CMYK
      channels = 4;
    otherwise
      text = iccdev.sig_to_str(signature);
      if numel(text) == 4 && strcmp(text(2:4), 'CLR')
        channels = hex2dec(text(1));
      else
        channels = 0;
      end
  end
end

function verdict = q1_verdict(metrics)
  max_average = max(metrics.firstAverage, metrics.secondAverage);
  max_delta = max(metrics.firstMaximum, metrics.secondMaximum);
  if max_average > 5 || max_delta > 20
    verdict = 'FAIL';
  elseif max_average > 2 || max_delta > 10
    verdict = 'WARN';
  else
    verdict = 'OK';
  end
end

function native = run_native_q1(tool_path, profile_path)
  [status, output, error_output] = run_process( ...
    {tool_path, '--json', profile_path});
  if status ~= 0
    error('iccdev:pawgQ1CommandFailed', ...
      'iccPawgReport failed with status %d:\n%s', ...
      status, strtrim(error_output));
  end
  try
    report = jsondecode(strtrim(output));
  catch e
    error('iccdev:pawgQ1InvalidJson', ...
      'Unable to decode iccPawgReport output: %s', e.message);
  end
  item = [];
  for i = 1:numel(report.items)
    if strcmp(report.items(i).id, 'Q1')
      item = report.items(i);
      break;
    end
  end
  if isempty(item)
    error('iccdev:pawgQ1MissingNativeItem', ...
      'iccPawgReport JSON did not contain Q1.');
  end
  if ~isfield(item, 'metrics') || ~isstruct(item.metrics) || ...
      ~isfield(item.metrics, 'model') || ~isfield(item.metrics, 'samples') || ...
      ~isfield(item.metrics, 'first') || ~isfield(item.metrics, 'second') || ...
      ~isfield(item.metrics.first, 'average') || ...
      ~isfield(item.metrics.first, 'maximum') || ...
      ~isfield(item.metrics.second, 'average') || ...
      ~isfield(item.metrics.second, 'maximum')
    error('iccdev:pawgQ1MissingNativeMetrics', ...
      'iccPawgReport JSON Q1 item did not contain structured metrics.');
  end
  native = struct( ...
    'model', char(item.metrics.model), ...
    'samples', double(item.metrics.samples), ...
    'firstAverage', double(item.metrics.first.average), ...
    'firstMaximum', double(item.metrics.first.maximum), ...
    'secondAverage', double(item.metrics.second.average), ...
    'secondMaximum', double(item.metrics.second.maximum), ...
    'verdict', item.verdict, ...
    'detail', item.detail);
end

function tool_path = find_pawg_tool(build_dir, explicit_path)
  if ~isempty(explicit_path)
    tool_path = canonical_file(explicit_path, ...
      'iccdev:pawgQ1ToolNotFound', 'iccPawgReport executable');
    return;
  end
  this_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(fileparts(fileparts(this_dir)));
  build_dirs = {};
  if ~isempty(build_dir)
    build_dirs{end + 1} = build_dir;
  end
  env_build_dir = getenv('ICCDEV_BUILD_DIR');
  if ~isempty(env_build_dir)
    build_dirs{end + 1} = env_build_dir;
  end
  build_dirs = [build_dirs, { ...
    fullfile(repo_root, 'msvc'), ...
    fullfile(repo_root, 'out', 'matlab'), ...
    fullfile(repo_root, 'Build', 'Cmake', 'build'), ...
    fullfile(repo_root, 'Build')}];
  if ispc()
    names = { ...
      fullfile('bin', 'Release', 'iccPawgReport.exe'), ...
      fullfile('bin', 'iccPawgReport.exe'), ...
      fullfile('Tools', 'IccPawgReport', 'Release', 'iccPawgReport.exe'), ...
      fullfile('Tools', 'IccPawgReport', 'iccPawgReport.exe')};
    path_name = 'iccPawgReport.exe';
  else
    names = { ...
      fullfile('bin', 'iccPawgReport'), ...
      fullfile('Tools', 'IccPawgReport', 'iccPawgReport')};
    path_name = 'iccPawgReport';
  end
  for i = 1:numel(build_dirs)
    for j = 1:numel(names)
      candidate = fullfile(build_dirs{i}, names{j});
      if exist(candidate, 'file') == 2
        tool_path = canonical_file(candidate, ...
          'iccdev:pawgQ1ToolNotFound', 'iccPawgReport executable');
        return;
      end
    end
  end
  path_dirs = iccdev.qa.path_entries(getenv('PATH'));
  for i = 1:numel(path_dirs)
    candidate = fullfile(path_dirs{i}, path_name);
    if exist(candidate, 'file') == 2
      tool_path = canonical_file(candidate, ...
        'iccdev:pawgQ1ToolNotFound', 'iccPawgReport executable');
      return;
    end
  end
  error('iccdev:pawgQ1ToolNotFound', ...
    ['iccPawgReport was not found. Build the iccPawgReport target, set ' ...
     'ICCDEV_BUILD_DIR, or pass the PawgTool option.']);
end

function path = canonical_file(path, error_id, description)
  [exists, attributes] = fileattrib(char(path)); %#ok<FILEATTRIB>
  if ~exists || attributes.directory
    error(error_id, '%s not found: %s', description, char(path));
  end
  path = attributes.Name;
end

function [status, output, error_output] = run_process(arguments)
  if ~usejava('jvm')
    error('iccdev:pawgQ1JvmUnavailable', ...
      'The native Q1 comparison requires a Java-enabled MATLAB runtime.');
  end
  command = javaObject('java.util.ArrayList');
  for i = 1:numel(arguments)
    command.add(javaObject('java.lang.String', char(arguments{i})));
  end
  error_path = [tempname '.txt'];
  cleanup = onCleanup(@() delete_if_exists(error_path));
  try
    builder = javaObject('java.lang.ProcessBuilder', command);
    builder.redirectError(javaObject('java.io.File', error_path));
    process = builder.start();
  catch e
    error('iccdev:pawgQ1ProcessStartFailed', ...
      'Unable to start iccPawgReport: %s', e.message);
  end
  output = read_java_stream(process.getInputStream());
  status = process.waitFor();
  if exist(error_path, 'file') == 2
    error_output = fileread(error_path);
  else
    error_output = '';
  end
  clear cleanup;
end

function output = read_java_stream(stream)
  reader = javaObject('java.io.BufferedReader', ...
    javaObject('java.io.InputStreamReader', stream));
  lines = {};
  line = reader.readLine();
  while ~isempty(line)
    lines{end + 1} = char(line); %#ok<AGROW>
    line = reader.readLine();
  end
  reader.close();
  output = strjoin(lines, newline);
end

function close_cmms(forward, reverse)
  forward.close();
  reverse.close();
end

function delete_if_exists(path)
  if exist(path, 'file') == 2
    delete(path);
  end
end
