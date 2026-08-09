function entries = path_entries(path_value)
%PATH_ENTRIES Split a PATH value while removing empty entries.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if ~(ischar(path_value) && (isempty(path_value) || size(path_value, 1) == 1))
    error('iccdev:invalidPathValue', ...
      'PATH must be a single character row.');
  end
  entries = regexp(path_value, pathsep, 'split');
  entries = entries(~cellfun('isempty', entries));
end
