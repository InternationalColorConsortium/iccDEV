function value = native_library_path(inherited, tool_dir, matlab_root)
%NATIVE_LIBRARY_PATH Build LD_LIBRARY_PATH for a native iccDEV tool.
%   VALUE = iccdev.qa.native_library_path(INHERITED, TOOL_DIR) returns
%   TOOL_DIR followed by the INHERITED entries that are not inside the
%   MATLAB installation. MATLAB prepends its bundled libstdc++ to
%   LD_LIBRARY_PATH; native tools must use their staged libraries and the
%   host C++ runtime instead.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 3 || isempty(matlab_root)
    matlab_root = matlabroot();
  end
  if ~(ischar(inherited) && (isempty(inherited) || size(inherited, 1) == 1))
    error('iccdev:invalidLibraryPath', ...
      'LD_LIBRARY_PATH must be a single character row.');
  end

  roots = unique({trim_separator(matlab_root), ...
    trim_separator(resolve_path(matlab_root))});
  library_dirs = {};
  if ~isempty(tool_dir)
    library_dirs{end + 1} = tool_dir;
  end
  entries = regexp(inherited, pathsep, 'split');
  for i = 1:numel(entries)
    entry = entries{i};
    if isempty(entry) || any(strcmp(library_dirs, entry)) || ...
        is_under_root(entry, roots) || ...
        is_under_root(resolve_path(entry), roots)
      continue;
    end
    library_dirs{end + 1} = entry; %#ok<AGROW>
  end
  value = strjoin(library_dirs, pathsep);
end

function inside = is_under_root(path, roots)
  path = trim_separator(path);
  inside = false;
  for i = 1:numel(roots)
    root = roots{i};
    if strcmp(path, root) || ...
        strncmp(path, [root filesep], numel(root) + 1)
      inside = true;
      return;
    end
  end
end

function path = resolve_path(path)
  [exists, attributes] = fileattrib(path); %#ok<FILEATTRIB>
  if exists
    path = attributes.Name;
  end
end

function path = trim_separator(path)
  while numel(path) > 1 && path(end) == filesep
    path(end) = [];
  end
end
