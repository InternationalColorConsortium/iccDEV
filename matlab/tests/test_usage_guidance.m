function test_usage_guidance()
%TEST_USAGE_GUIDANCE Verify actionable public API usage errors.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  fixture_path = fullfile(fileparts(mfilename('fullpath')), 'fixtures', ...
    'default_usage_examples.txt');
  lines = regexp(strtrim(fileread(fixture_path)), '\r?\n', 'split');

  for i = 1:numel(lines)
    fields = regexp(lines{i}, '\|', 'split');
    assert(numel(fields) == 3, ...
      'Invalid usage guidance fixture line: %s', lines{i});
    error_id = '';
    message = '';
    try
      invoke_without_arguments(fields{1});
    catch e
      error_id = e.identifier;
      message = e.message;
    end
    assert(strcmp(error_id, fields{2}), ...
      '%s returned %s instead of %s', fields{1}, error_id, fields{2});
    assert(~isempty(strfind(message, fields{3})), ... %#ok<STREMP>
      '%s did not report the expected usage marker: %s', ...
      fields{1}, fields{3});
  end

  test_plot_visibility_guidance();
  test_delta_e_shape_guidance();

  fprintf(['MATLAB usage guidance passed for %d entry points, plot ' ...
    'visibility, and the CIEDE2000 shape error.\n'], ...
    numel(lines));
end

function invoke_without_arguments(name)
  switch name
    case 'IccProfile'
      iccdev.IccProfile();
    case 'IccApply'
      iccdev.IccApply();
    case 'plot'
      iccdev.plot();
    case 'to_json'
      iccdev.to_json();
    case 'from_json'
      iccdev.from_json();
    case 'sig_to_str'
      iccdev.sig_to_str();
    case 'docker_validate'
      iccdev.docker_validate();
    case 'audit_pawg_q1'
      iccdev.qa.audit_pawg_q1();
    case 'delta_e_2000'
      iccdev.qa.delta_e_2000();
    case 'add_docker_path'
      add_docker_path();
    otherwise
      error('iccdev:unknownUsageFixture', ...
        'Unknown usage guidance fixture entry: %s', name);
  end
end

function test_plot_visibility_guidance()
  message = '';
  try
    iccdev.plot();
  catch e
    message = e.message;
  end

  expected_markers = { ...
    'Display plots interactively (Visible defaults to on):', ...
    'plots = iccdev.plot(profile_path);', ...
    'Create hidden figures for automated checks or export:', ...
    'close([plots.figure]);'};
  for i = 1:numel(expected_markers)
    assert(~isempty(strfind(message, expected_markers{i})), ... %#ok<STREMP>
      'Plot usage error omitted visibility marker: %s', ...
      expected_markers{i});
  end
end

function test_delta_e_shape_guidance()
  error_id = '';
  message = '';
  try
    iccdev.qa.delta_e_2000(1, 4);
  catch e
    error_id = e.identifier;
    message = e.message;
  end

  assert(strcmp(error_id, 'iccdev:pawgQ1InvalidLabSamples'), ...
    'Invalid CIELAB shapes returned an unexpected error: %s', error_id);
  expected_markers = { ...
    'Each row is [L* a* b*]', ...
    'Single pair:', ...
    'delta_e = iccdev.qa.delta_e_2000(', ...
    'Multiple pairs:', ...
    'lab1 = [50 2.6772 -79.7751; 60 10 20]'};
  for i = 1:numel(expected_markers)
    assert(~isempty(strfind(message, expected_markers{i})), ... %#ok<STREMP>
      'CIEDE2000 shape error omitted usage marker: %s', ...
      expected_markers{i});
  end
end
