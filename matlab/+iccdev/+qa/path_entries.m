function entries = path_entries(path_value)
%PATH_ENTRIES Split PATH while excluding empty and relative entries.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if ~(ischar(path_value) && (isempty(path_value) || size(path_value, 1) == 1))
    error('iccdev:invalidPathValue', ...
      'PATH must be a single character row.');
  end
  entries = regexp(path_value, pathsep, 'split');
  entries = entries(~cellfun('isempty', entries));
  entries = entries(cellfun(@is_absolute_path, entries));
end

function valid = is_absolute_path(value)
  if ispc()
    valid = ~isempty(regexp(value, '^(?:[A-Za-z]:[\\/]|\\\\)', 'once'));
  else
    valid = value(1) == filesep;
  end
end
