function root = select_vcpkg_installed_root(buildDir, cachedRoot, triplet)
%SELECT_VCPKG_INSTALLED_ROOT Choose a usable vcpkg installed tree.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  defaultRoot = fullfile(buildDir, 'vcpkg_installed');
  candidates = {defaultRoot};

  if ~isempty(cachedRoot)
    if ~is_absolute_path(cachedRoot)
      cachedRoot = fullfile(buildDir, cachedRoot);
    end
    candidates = [{cachedRoot}; candidates];
  end

  root = candidates{1};
  for i = 1:numel(candidates)
    candidate = candidates{i};
    if exist(fullfile(candidate, triplet), 'dir') == 7
      root = candidate;
      return;
    end
  end
end

function absolute = is_absolute_path(path)
  if ispc()
    absolute = ~isempty(regexp(path, ...
      '^(?:[A-Za-z]:[\\/]|\\\\)', 'once'));
  else
    absolute = strncmp(path, filesep, 1);
  end
end
