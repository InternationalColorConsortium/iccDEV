function profile_bytes = from_json(json_input, varargin)
%FROM_JSON Convert IccJSON text or a JSON file to ICC profile bytes.
%
%   profile_bytes = iccdev.from_json(json_text)
%   profile_bytes = iccdev.from_json(json_path)
%   profile_bytes = iccdev.from_json(json_input, 'BuildDir', build_dir)
%   profile_bytes = iccdev.from_json(json_input, 'FromJsonTool', tool_path)
%
% A scalar string that names an existing file is read as JSON input; all other
% scalar text is encoded as UTF-8 JSON content. The returned column vector is
% suitable for fwrite(output_path, profile_bytes, 'uint8').
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    error('iccdev:jsonInputRequired', ...
      ['Provide IccJSON text or a JSON file path. Example: ' ...
       'profile_bytes = iccdev.from_json(json_text);']);
  end

  p = inputParser;
  addRequired(p, 'json_input', @is_text_scalar);
  addParameter(p, 'BuildDir', '', @is_text_scalar);
  addParameter(p, 'FromJsonTool', '', @is_text_scalar);
  parse(p, json_input, varargin{:});

  json_input = char(p.Results.json_input);
  input_path = '';
  temporary_input = false;
  if exist(json_input, 'file') == 2
    input_path = json_input;
  else
    input_path = [tempname '.json'];
    temporary_input = true;
    write_utf8(input_path, json_input);
  end

  output_path = [tempname '.icc'];
  cleanup = onCleanup(@() cleanup_files(input_path, temporary_input, output_path));
  run_json_tool('iccFromJson', input_path, output_path, ...
    char(p.Results.BuildDir), char(p.Results.FromJsonTool));

  file_id = fopen(output_path, 'rb');
  if file_id < 0
    error('iccdev:jsonOutputReadFailed', ...
      'Unable to read ICC output: %s', output_path);
  end
  close_file = onCleanup(@() fclose(file_id));
  profile_bytes = fread(file_id, Inf, '*uint8');
  clear close_file cleanup;
end

function write_utf8(path, text)
  file_id = fopen(path, 'wb');
  if file_id < 0
    error('iccdev:jsonInputWriteFailed', 'Unable to write JSON input: %s', path);
  end
  cleanup = onCleanup(@() fclose(file_id));
  bytes = unicode2native(text, 'UTF-8');
  fwrite(file_id, bytes, 'uint8');
  clear cleanup;
end

function tf = is_text_scalar(value)
  tf = ischar(value) && (isempty(value) || size(value, 1) == 1);
  if ~tf && (exist('isstring', 'builtin') == 5 || ...
      exist('isstring', 'file') == 2)
    tf = isstring(value) && isscalar(value);
  end
end

function cleanup_files(input_path, remove_input, output_path)
  if remove_input && exist(input_path, 'file') == 2
    delete(input_path);
  end
  if exist(output_path, 'file') == 2
    delete(output_path);
  end
end
