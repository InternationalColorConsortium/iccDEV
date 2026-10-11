function report = heif_dump(image_path, varargin)
%HEIF_DUMP Report embedded ICC properties in a HEIF, HEIC, or AVIF image.
%
%   report = iccdev.heif_dump(image_path)
%   report = iccdev.heif_dump(image_path, 'BuildDir', build_dir)
%   report = iccdev.heif_dump(image_path, 'HeifDumpTool', tool_path)
%
% Runs iccHeifDump through Java ProcessBuilder without a shell. The report
% retains the native tool's carrier metadata and IccProfLib parse statuses.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    error('iccdev:heifImageRequired', ...
      ['Provide a HEIF, HEIC, or AVIF image path. Example: ' ...
       'report = iccdev.heif_dump(image_path);']);
  end

  p = inputParser;
  addRequired(p, 'image_path', @is_text_scalar);
  addParameter(p, 'BuildDir', '', @is_text_scalar);
  addParameter(p, 'HeifDumpTool', '', @is_text_scalar);
  parse(p, image_path, varargin{:});

  report = run_cli_tool('iccHeifDump', char(p.Results.image_path), {}, ...
    char(p.Results.BuildDir), char(p.Results.HeifDumpTool), 'iccdev:heif');
end

function tf = is_text_scalar(value)
  tf = ischar(value) && (isempty(value) || size(value, 1) == 1);
  if ~tf && (exist('isstring', 'builtin') == 5 || ...
      exist('isstring', 'file') == 2)
    tf = isstring(value) && isscalar(value);
  end
end
