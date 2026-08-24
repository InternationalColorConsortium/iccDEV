function test_usage_guidance()
%TEST_USAGE_GUIDANCE Verify actionable errors for missing required arguments.
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

  fprintf('MATLAB default usage guidance passed for %d entry points.\n', ...
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
    case 'sig_to_str'
      iccdev.sig_to_str();
    case 'docker_validate'
      iccdev.docker_validate();
    case 'audit_pawg_q1'
      iccdev.qa.audit_pawg_q1();
    case 'add_docker_path'
      add_docker_path();
    otherwise
      error('iccdev:unknownUsageFixture', ...
        'Unknown usage guidance fixture entry: %s', name);
  end
end
