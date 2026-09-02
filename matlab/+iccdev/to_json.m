function json_text = to_json(profile_path, varargin)
%TO_JSON Convert an ICC profile to IccJSON text.
%
%   json_text = iccdev.to_json(profile_path)
%   json_text = iccdev.to_json(profile_path, 'BuildDir', build_dir)
%   json_text = iccdev.to_json(profile_path, 'ToJsonTool', tool_path)
%
% Uses iccToJson and returns its UTF-8 JSON document. The profile path must
% name an existing regular file.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    error('iccdev:jsonProfileRequired', ...
      ['Provide an ICC profile path. Example: ' ...
       'json_text = iccdev.to_json(profile_path);']);
  end

  p = inputParser;
  addRequired(p, 'profile_path', @is_text_scalar);
  addParameter(p, 'BuildDir', '', @is_text_scalar);
  addParameter(p, 'ToJsonTool', '', @is_text_scalar);
  parse(p, profile_path, varargin{:});

  output_path = [tempname '.json'];
  cleanup = onCleanup(@() delete_if_exists(output_path));
  run_json_tool('iccToJson', char(p.Results.profile_path), ...
    output_path, char(p.Results.BuildDir), char(p.Results.ToJsonTool));
  json_text = read_utf8(output_path);
  clear cleanup;
end

function text = read_utf8(path)
  file_id = fopen(path, 'rb');
  if file_id < 0
    error('iccdev:jsonOutputReadFailed', ...
      'Unable to read JSON output: %s', path);
  end
  cleanup = onCleanup(@() fclose(file_id));
  bytes = fread(file_id, Inf, '*uint8');
  text = native2unicode(bytes.', 'UTF-8');
  clear cleanup;
end

function tf = is_text_scalar(value)
  tf = ischar(value) && (isempty(value) || size(value, 1) == 1);
  if ~tf && (exist('isstring', 'builtin') == 5 || ...
      exist('isstring', 'file') == 2)
    tf = isstring(value) && isscalar(value);
  end
end

function delete_if_exists(path)
  if exist(path, 'file') == 2
    delete(path);
  end
end
