function plots = plot(profile_path, varargin)
%PLOT Plot graph visualizations available in an ICC profile.
%
%   plots = iccdev.plot(profile_path)
%   plots = iccdev.plot(profile_path, 'Visible', 'off')
%   plots = iccdev.plot(profile_path, 'BuildDir', build_dir)
%   plots = iccdev.plot(profile_path, 'PlotTool', tool_path)
%
% Uses the iccProfilePlot command-line tool to enumerate and render the
% profile's data-first graph visualizations. Raster visualizations are not
% displayed by this function. Requires MATLAB R2016b+ or GNU Octave 7.1+
% for jsondecode support.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    error('iccdev:plotProfileRequired', ...
      ['Provide an ICC profile path. Example: ' ...
       'plots = iccdev.plot(profile_path, ''Visible'', ''off'');']);
  end

  p = inputParser;
  addRequired(p, 'profile_path', @is_text_scalar);
  addParameter(p, 'Visible', 'on', @is_on_off);
  addParameter(p, 'BuildDir', '', @is_text_scalar);
  addParameter(p, 'PlotTool', '', @is_text_scalar);
  addParameter(p, 'IncludeHints', true, ...
    @(value) islogical(value) && isscalar(value));
  parse(p, profile_path, varargin{:});

  profile_path = canonical_file(char(p.Results.profile_path), ...
    'iccdev:plotProfileNotFound', 'Profile');
  tool_path = char(p.Results.PlotTool);
  if isempty(tool_path)
    tool_path = find_plot_tool(char(p.Results.BuildDir));
  else
    tool_path = canonical_file(tool_path, ...
      'iccdev:plotToolNotFound', 'iccProfilePlot executable');
  end

  descriptors = run_json(tool_path, {profile_path, 'list'});
  if isempty(descriptors)
    error('iccdev:noProfilePlots', ...
      'The profile does not expose any visualizations.');
  end
  if ~isstruct(descriptors)
    error('iccdev:invalidPlotData', ...
      'iccProfilePlot returned an invalid descriptor list.');
  end

  graph_mask = strcmp({descriptors.output}, 'graph');
  descriptors = descriptors(graph_mask);
  if isempty(descriptors)
    error('iccdev:noProfileGraphs', ...
      'The profile exposes no graph visualizations.');
  end

  plots = repmat(struct( ...
    'id', '', ...
    'title', '', ...
    'figure', [], ...
    'axes', [], ...
    'data', struct()), 1, numel(descriptors));
  created_figures = {};

  try
    for i = 1:numel(descriptors)
      graph = run_json(tool_path, ...
        {profile_path, 'graph', descriptors(i).id});
      [fig, ax] = render_graph(graph, char(p.Results.Visible), ...
        p.Results.IncludeHints);
      created_figures{end+1} = fig; %#ok<AGROW>
      plots(i).id = descriptors(i).id;
      plots(i).title = graph.title;
      plots(i).figure = fig;
      plots(i).axes = ax;
      plots(i).data = graph;
    end
  catch e
    if ~isempty(created_figures)
      handles = [created_figures{:}];
      close(handles(ishandle(handles)));
    end
    rethrow(e);
  end
end

function [fig, ax] = render_graph(graph, visible, include_hints)
  validate_graph(graph);

  fig = figure('Name', graph.title, 'NumberTitle', 'off', ...
    'Visible', visible);
  ax = axes('Parent', fig);
  hold(ax, 'on');

  legend_handles = {};
  for i = 1:numel(graph.series)
    series = graph.series(i);
    if ~include_hints && strcmp(series.role, 'hint')
      continue;
    end

    points = double(series.points(:));
    if mod(numel(points), 2) ~= 0
      error('iccdev:invalidPlotData', ...
        'Series %s has an odd number of point coordinates.', series.id);
    end
    points = reshape(points, 2, []).';
    if isempty(points)
      continue;
    end

    if strcmp(series.shape, 'closedPath')
      points(end+1, :) = points(1, :); %#ok<AGROW>
    end

    [color, line_style, marker] = series_style(series, i);
    if strcmp(series.shape, 'scatter')
      handle = scatter(ax, points(:, 1), points(:, 2), 24, color, ...
        'filled', 'DisplayName', series.name);
    else
      handle = line(ax, points(:, 1), points(:, 2), ...
        'Color', color, ...
        'LineStyle', line_style, ...
        'Marker', marker, ...
        'DisplayName', series.name);
    end
    legend_handles{end+1} = handle; %#ok<AGROW>
    add_labels(ax, series, points, color);
  end

  title(ax, graph.title, 'Interpreter', 'none');
  xlabel(ax, graph.xAxis.label, 'Interpreter', 'none');
  ylabel(ax, graph.yAxis.label, 'Interpreter', 'none');
  apply_axis_hint(ax, graph.xAxis, 'x');
  apply_axis_hint(ax, graph.yAxis, 'y');
  if logical(graph.xAxis.equalAspect) || logical(graph.yAxis.equalAspect)
    axis(ax, 'equal');
  end
  grid(ax, 'on');
  box(ax, 'on');
  if ~isempty(legend_handles)
    legend(ax, [legend_handles{:}], ...
      'Location', 'best', 'Interpreter', 'none');
  end
end

function validate_graph(graph)
  required = {'title', 'xAxis', 'yAxis', 'series'};
  if ~isstruct(graph) || ~all(isfield(graph, required))
    error('iccdev:invalidPlotData', ...
      'iccProfilePlot returned incomplete graph data.');
  end
end

function apply_axis_hint(ax, axis_data, dimension)
  limits = double([axis_data.min, axis_data.max]);
  if all(isfinite(limits)) && limits(1) ~= limits(2)
    if dimension == 'x'
      xlim(ax, sort(limits));
      if limits(1) > limits(2)
        set(ax, 'XDir', 'reverse');
      end
    else
      ylim(ax, sort(limits));
      if limits(1) > limits(2)
        set(ax, 'YDir', 'reverse');
      end
    end
  end
end

function [color, line_style, marker] = series_style(series, series_index)
  color = color_hint(series.colorHint, series_index);
  marker = 'none';
  if strcmp(series.role, 'hint')
    line_style = '--';
  else
    line_style = '-';
  end
  if strcmp(series.shape, 'closedPath')
    line_style = '-';
  elseif strcmp(series.shape, 'scatter')
    marker = 'o';
  end
end

function color = color_hint(hint, series_index)
  switch lower(char(hint))
    case {'r', 'red'}
      color = [0.85, 0.15, 0.15];
    case {'g', 'green'}
      color = [0.15, 0.65, 0.20];
    case {'b', 'blue'}
      color = [0.15, 0.30, 0.85];
    case {'white'}
      color = [0.35, 0.35, 0.35];
    case {'locus'}
      color = [0.35, 0.35, 0.35];
    case {'planckian'}
      color = [0.85, 0.45, 0.10];
    otherwise
      lab = parse_lab_hint(hint);
      if isempty(lab)
        palette = [
          0.0000, 0.4470, 0.7410
          0.8500, 0.3250, 0.0980
          0.9290, 0.6940, 0.1250
          0.4940, 0.1840, 0.5560
          0.4660, 0.6740, 0.1880
          0.3010, 0.7450, 0.9330
          0.6350, 0.0780, 0.1840
        ];
        index = mod(series_index - 1, size(palette, 1)) + 1;
        color = palette(index, :);
      else
        color = lab_to_srgb(lab);
      end
  end
end

function lab = parse_lab_hint(hint)
  values = regexp(char(hint), ...
    '^\s*(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?),\s*(-?\d+(?:\.\d+)?)\s*$', ...
    'tokens', 'once');
  if isempty(values)
    lab = [];
  else
    lab = [str2double(values{1}), str2double(values{2}), ...
      str2double(values{3})];
  end
end

function rgb = lab_to_srgb(lab)
  fy = (lab(1) + 16.0) / 116.0;
  fx = fy + lab(2) / 500.0;
  fz = fy - lab(3) / 200.0;
  xyz_d50 = [inverse_lab_f(fx) * 0.9642; ...
             inverse_lab_f(fy); ...
             inverse_lab_f(fz) * 0.8249];
  xyz_d65 = [
     0.9555766, -0.0230393,  0.0631636
    -0.0282895,  1.0099416,  0.0210077
     0.0122982, -0.0204830,  1.3299098
  ] * xyz_d50;
  linear = [
     3.2404542, -1.5371385, -0.4985314
    -0.9692660,  1.8760108,  0.0415560
     0.0556434, -0.2040259,  1.0572252
  ] * xyz_d65;
  rgb = zeros(3, 1);
  low = linear <= 0.0031308;
  rgb(low) = 12.92 * linear(low);
  rgb(~low) = 1.055 * max(linear(~low), 0).^(1 / 2.4) - 0.055;
  rgb = min(max(rgb, 0), 1).';
end

function value = inverse_lab_f(value)
  delta = 6 / 29;
  if value > delta
    value = value^3;
  else
    value = 3 * delta^2 * (value - 4 / 29);
  end
end

function add_labels(ax, series, points, color)
  if ~isfield(series, 'labels') || isempty(series.labels) || ...
      ~isstruct(series.labels)
    return;
  end

  for i = 1:numel(series.labels)
    label = series.labels(i);
    if ~isfield(label, 'i') || ~isfield(label, 't') || isempty(label.t)
      continue;
    end
    index = double(label.i) + 1;
    if index < 1 || index > size(points, 1)
      continue;
    end
    text(ax, points(index, 1), points(index, 2), char(label.t), ...
      'Color', color, ...
      'FontSize', 8, ...
      'Interpreter', 'none');
  end
end

function data = run_json(tool_path, arguments)
  [status, output, error_output] = run_process( ...
    [{tool_path}; arguments(:)]);
  if status ~= 0
    error('iccdev:plotCommandFailed', ...
      'iccProfilePlot failed with status %d:\n%s', ...
      status, strtrim(error_output));
  end
  if exist('jsondecode', 'builtin') ~= 5 && exist('jsondecode', 'file') ~= 2
    error('iccdev:jsonDecodeUnavailable', ...
      'iccdev.plot requires MATLAB or Octave with jsondecode support.');
  end
  try
    data = jsondecode(strtrim(output));
  catch e
    error('iccdev:invalidPlotJson', ...
      'Unable to decode iccProfilePlot output: %s', e.message);
  end
end

function tool_path = find_plot_tool(build_dir)
  this_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(fileparts(this_dir));

  build_dirs = {};
  if ~isempty(build_dir)
    build_dirs{end+1} = build_dir;
  end
  env_build_dir = getenv('ICCDEV_BUILD_DIR');
  if ~isempty(env_build_dir)
    build_dirs{end+1} = env_build_dir;
  end
  build_dirs = [build_dirs, {
    fullfile(repo_root, 'msvc')
    fullfile(repo_root, 'out', 'matlab')
    fullfile(repo_root, 'Build', 'Cmake', 'build')
    fullfile(repo_root, 'Build')
  }.'];

  if ispc()
    names = {
      fullfile('bin', 'Release', 'iccProfilePlot.exe')
      fullfile('bin', 'iccProfilePlot.exe')
      fullfile('Tools', 'IccProfilePlot', 'Release', 'iccProfilePlot.exe')
      fullfile('Tools', 'IccProfilePlot', 'iccProfilePlot.exe')
    };
  else
    names = {
      fullfile('bin', 'iccProfilePlot')
      fullfile('Tools', 'IccProfilePlot', 'iccProfilePlot')
    };
  end

  for i = 1:numel(build_dirs)
    for j = 1:numel(names)
      candidate = fullfile(build_dirs{i}, names{j});
      if exist(candidate, 'file') == 2
        tool_path = canonical_file(candidate, ...
          'iccdev:plotToolNotFound', 'iccProfilePlot executable');
        return;
      end
    end
  end

  path_dirs = iccdev.qa.path_entries(getenv('PATH'));
  if ispc()
    path_name = 'iccProfilePlot.exe';
  else
    path_name = 'iccProfilePlot';
  end
  for i = 1:numel(path_dirs)
    candidate = fullfile(path_dirs{i}, path_name);
    if exist(candidate, 'file') == 2
      tool_path = canonical_file(candidate, ...
        'iccdev:plotToolNotFound', 'iccProfilePlot executable');
      return
    end
  end

  error('iccdev:plotToolNotFound', ...
    ['iccProfilePlot was not found. Build the iccProfilePlot target, set ' ...
     'ICCDEV_BUILD_DIR, or pass the PlotTool option.']);
end

function path = canonical_file(path, error_id, description)
  [exists, attributes] = fileattrib(path); %#ok<FILEATTRIB>
  if ~exists || attributes.directory
    error(error_id, '%s not found: %s', description, path);
  end
  path = attributes.Name;
end

function [status, output, error_output] = run_process(arguments)
  if exist('usejava', 'builtin') ~= 5 && exist('usejava', 'file') ~= 2
    error('iccdev:plotJvmUnavailable', ...
      'iccdev.plot requires a Java-enabled MATLAB or Octave runtime.');
  end
  if ~usejava('jvm')
    error('iccdev:plotJvmUnavailable', ...
      'iccdev.plot requires a Java-enabled MATLAB or Octave runtime.');
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
    error('iccdev:plotProcessStartFailed', ...
      'Unable to start iccProfilePlot: %s', e.message);
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
    lines{end+1} = char(line); %#ok<AGROW>
    line = reader.readLine();
  end
  reader.close();
  output = strjoin(lines, newline);
end

function delete_if_exists(path)
  if exist(path, 'file') == 2
    delete(path);
  end
end

function tf = is_text_scalar(value)
  tf = ischar(value) && (isempty(value) || size(value, 1) == 1);
  if ~tf && (exist('isstring', 'builtin') == 5 || ...
      exist('isstring', 'file') == 2)
    tf = isstring(value) && isscalar(value);
  end
end

function tf = is_on_off(value)
  tf = is_text_scalar(value);
  if tf
    value = char(value);
    tf = strcmp(value, 'on') || strcmp(value, 'off');
  end
end
