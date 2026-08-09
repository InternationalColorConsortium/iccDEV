function test_plot()
%TEST_PLOT Verify MATLAB rendering of iccProfilePlot graph data.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  matlab_dir = fileparts(fileparts(mfilename('fullpath')));
  repo_root = fileparts(matlab_dir);
  profile_path = fullfile(repo_root, 'Testing', ...
    'sRGB_v4_ICC_preference.icc');

  invalid_failed = false;
  try
    iccdev.plot(char('first', 'second'), 'Visible', 'off');
  catch
    invalid_failed = true;
  end
  assert(invalid_failed, ...
    'Multi-row character arrays must be rejected as profile paths.');

  figures = {};
  try
    plots = iccdev.plot(profile_path, 'Visible', 'off');
    figures = {plots.figure};
    assert(~isempty(plots), 'Expected at least one graph visualization.');
    assert(all(ishandle([figures{:}])), ...
      'Expected valid MATLAB figure handles.');
    assert(all(arrayfun(@(item) ~isempty(item.data.series), plots)), ...
      'Expected every rendered graph to contain a series.');
    verify_closed_paths(plots);
    verify_reversed_axes(plots);
    verify_lab_colors(plots);
  catch e
    if ~isempty(figures)
      handles = [figures{:}];
      close(handles(ishandle(handles)));
    end
    rethrow(e);
  end
  close([figures{:}]);
  fprintf('MATLAB profile plotting passed with %d graph(s).\n', numel(plots));
end

function verify_closed_paths(plots)
  checked = false;
  for i = 1:numel(plots)
    for j = 1:numel(plots(i).data.series)
      series = plots(i).data.series(j);
      if ~strcmp(series.shape, 'closedPath')
        continue;
      end
      handles = findobj(plots(i).axes, ...
        'Type', 'line', 'DisplayName', series.name);
      if isempty(handles)
        continue;
      end
      x_data = handles(1).XData;
      y_data = handles(1).YData;
      assert(x_data(1) == x_data(end) && y_data(1) == y_data(end), ...
        'Closed-path series must connect the final point to the first.');
      checked = true;
    end
  end
  assert(checked, 'Expected at least one closed-path series.');
end

function verify_reversed_axes(plots)
  checked = false;
  for i = 1:numel(plots)
    x_axis = plots(i).data.xAxis;
    if x_axis.min > x_axis.max
      assert(strcmp(plots(i).axes.XDir, 'reverse'), ...
        'A descending x-axis hint must reverse the MATLAB axis.');
      checked = true;
    end
    y_axis = plots(i).data.yAxis;
    if y_axis.min > y_axis.max
      assert(strcmp(plots(i).axes.YDir, 'reverse'), ...
        'A descending y-axis hint must reverse the MATLAB axis.');
      checked = true;
    end
  end
  assert(checked, 'Expected at least one reversed axis hint.');
end

function verify_lab_colors(plots)
  colors = [];
  for i = 1:numel(plots)
    for j = 1:numel(plots(i).data.series)
      series = plots(i).data.series(j);
      if isempty(regexp(series.colorHint, ...
          '^-?\d+(?:\.\d+)?,-?\d+(?:\.\d+)?,-?\d+(?:\.\d+)?$', 'once'))
        continue;
      end
      handles = findobj(plots(i).axes, ...
        'Type', 'line', 'DisplayName', series.name);
      if ~isempty(handles)
        colors(end+1, :) = handles(1).Color; %#ok<AGROW>
      end
    end
  end
  assert(size(unique(colors, 'rows'), 1) > 1, ...
    'Lab color hints should produce distinguishable display colors.');
end
