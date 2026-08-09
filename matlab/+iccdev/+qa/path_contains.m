function found = path_contains(entries, candidate, case_insensitive)
%PATH_CONTAINS Test whether a PATH entry is already present.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 3
    case_insensitive = ispc();
  end
  if ~iscell(entries) || ...
      ~(ischar(candidate) && (isempty(candidate) || size(candidate, 1) == 1)) || ...
      ~(islogical(case_insensitive) && isscalar(case_insensitive))
    error('iccdev:invalidPathEntry', ...
      'Invalid PATH entry comparison arguments.');
  end

  if case_insensitive
    found = any(strcmpi(entries, candidate));
  else
    found = any(strcmp(entries, candidate));
  end
end
