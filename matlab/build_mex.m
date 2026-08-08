function build_mex(varargin)
%BUILD_MEX Build the icc_mex MEX gateway for MATLAB/Octave.
%
%   build_mex()                         % auto-detect build dir
%   build_mex('BuildDir', '/path/to')   % explicit build directory
%   build_mex('Debug', true)            % debug build
%
% Requirements:
%   - MATLAB with MEX compiler configured, or GNU Octave with mkoctfile
%   - IccProfLib2 built (static or shared library)
%   - C++17 compiler
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  p = inputParser;
  addParameter(p, 'BuildDir', '', @is_text_scalar);
  addParameter(p, 'Debug', false, ...
    @(value) islogical(value) && isscalar(value));
  parse(p, varargin{:});

  thisDir  = fileparts(mfilename('fullpath'));
  repoRoot = fileparts(thisDir);
  mexSrc   = fullfile(thisDir, 'mex', 'icc_mex.cpp');
  inclDir  = fullfile(repoRoot, 'IccProfLib');

  % Find build directory
  buildDir = char(p.Results.BuildDir);
  if isempty(buildDir)
    buildDir = getenv('ICCDEV_BUILD_DIR');
  end
  if isempty(buildDir)
    buildDir = fullfile(repoRoot, 'Build');
  end

  % Search for library. Normal MATLAB/Octave builds must not link debug CRT
  % artifacts; pass 'Debug', true when intentionally using debug libraries.
  releaseLibNames = {
    'IccProfLib2-static'
    'IccProfLib2'
  };
  debugLibNames = {
    'IccProfLib2-staticd'
    'IccProfLib2d'
  };
  releaseSearchDirs = {
    buildDir
    fullfile(buildDir, 'IccProfLib')
    fullfile(buildDir, 'lib')
    fullfile(buildDir, 'Release')
    fullfile(buildDir, 'IccProfLib', 'Release')
  };
  debugSearchDirs = {
    fullfile(buildDir, 'Debug')
    fullfile(buildDir, 'IccProfLib', 'Debug')
    buildDir
    fullfile(buildDir, 'IccProfLib')
  };
  if p.Results.Debug
    libNames = [debugLibNames; releaseLibNames];
    searchDirs = [debugSearchDirs; releaseSearchDirs];
  else
    libNames = releaseLibNames;
    searchDirs = releaseSearchDirs;
  end

  libDir = '';
  libName = '';
  for i = 1:numel(searchDirs)
    d = searchDirs{i};
    for n = 1:numel(libNames)
      candidateLibName = libNames{n};
      if ispc()
        candidates = {fullfile(d, [candidateLibName '.lib']), ...
                      fullfile(d, ['lib' candidateLibName '.a'])};
      else
        candidates = {fullfile(d, ['lib' candidateLibName '.a']), ...
                      fullfile(d, [candidateLibName '.a']), ...
                      fullfile(d, ['lib' candidateLibName '.so']), ...
                      fullfile(d, ['lib' candidateLibName '.dylib'])};
      end
      for j = 1:numel(candidates)
        if exist(candidates{j}, 'file')
          libDir = d;
          libName = candidateLibName;
          break;
        end
      end
      if ~isempty(libDir), break; end
    end
    if ~isempty(libDir), break; end
  end

  if isempty(libDir)
    if ~p.Results.Debug && has_library(debugSearchDirs, debugLibNames)
      error('iccdev:debugLibForReleaseMex', ...
        ['Found only Debug IccProfLib2 libraries. Build a Release iccDEV tree, ' ...
         'or call build_mex(''Debug'', true) with a compatible debug MATLAB/Octave runtime.']);
    end
    error('iccdev:libNotFound', ...
      'Cannot find IccProfLib2 static/shared library. Set ICCDEV_BUILD_DIR or pass BuildDir.');
  end

  fprintf('Building icc_mex:\n');
  fprintf('  Source:  %s\n', mexSrc);
  fprintf('  Include: %s\n', inclDir);
  fprintf('  Library: %s in %s\n', libName, libDir);

  [dependencyArgs, runtimeFiles] = find_optional_dependencies( ...
    buildDir, libName, p.Results.Debug);
  for i = 1:numel(dependencyArgs)
    fprintf('  Dependency: %s\n', dependencyArgs{i});
  end

  % Build MEX arguments
  if exist('OCTAVE_VERSION', 'builtin')
    cxxFlags = {'-std=c++17'};
  elseif ispc()
    cxxFlags = {'COMPFLAGS=$COMPFLAGS /std:c++17 /EHsc'};
  else
    cxxFlags = {'CXXFLAGS=$CXXFLAGS -std=c++17'};
  end

  if p.Results.Debug
    cxxFlags{end+1} = '-g';
  end

  if exist('OCTAVE_VERSION', 'builtin')
    % Octave: place MEX alongside +iccdev/ directory (private/ not resolved)
    outDir = thisDir;
  else
    % MATLAB: place in +iccdev/private for proper namespace scoping
    outDir = fullfile(thisDir, '+iccdev', 'private');
  end
  if ~exist(outDir, 'dir')
    mkdir(outDir);
  end

  commonArgs = [
    cxxFlags, ...
    {['-I' inclDir]}, ...
    {['-L' libDir]}, ...
    {['-l' libName]}, ...
    dependencyArgs
  ];

  fprintf('  Output:  %s\n', outDir);

  if exist('OCTAVE_VERSION', 'builtin')
    outFile = fullfile(outDir, ['icc_mex.' mexext()]);
    args = [commonArgs, {'-o', outFile}, {mexSrc}];
    fprintf('  mkoctfile --mex %s\n', strjoin(args, ' '));
    mkoctfile('--mex', args{:});
  else
    args = [commonArgs, {'-outdir', outDir}, {mexSrc}];
    fprintf('  mex %s\n', strjoin(args, ' '));
    mex(args{:});
  end

  for i = 1:numel(runtimeFiles)
    copyfile(runtimeFiles{i}, outDir, 'f');
    fprintf('  Runtime: %s\n', runtimeFiles{i});
  end

  fprintf('Build complete. Add %s to your MATLAB path.\n', thisDir);
end

function [linkArgs, runtimeFiles] = find_optional_dependencies( ...
    buildDir, libName, debugBuild)
  linkArgs = {};
  runtimeFiles = {};

  if isempty(strfind(libName, '-static')) %#ok<STREMP>
    return;
  end

  cachePath = fullfile(buildDir, 'CMakeCache.txt');
  if ~exist(cachePath, 'file')
    return;
  end

  cacheText = fileread(cachePath);
  if isempty(regexp(cacheText, '(?m)^ICC_USE_ZLIB:BOOL=ON\s*$', 'once'))
    return;
  end

  if ~ispc()
    linkArgs = {'-lz'};
    return;
  end

  triplet = 'x64-windows';
  tripletMatch = regexp(cacheText, ...
    '(?m)^VCPKG_TARGET_TRIPLET:[^=]*=([^\r\n]+)\s*$', 'tokens', 'once');
  if ~isempty(tripletMatch)
    triplet = strtrim(tripletMatch{1});
  end
  tripletRoot = fullfile(buildDir, 'vcpkg_installed', triplet);
  staticTriplet = ~isempty(strfind(triplet, '-static')); %#ok<STREMP>
  if debugBuild
    libraryCandidates = {
      fullfile(tripletRoot, 'debug', 'lib', 'zd.lib')
      fullfile(tripletRoot, 'debug', 'lib', 'zlibd.lib')
      fullfile(tripletRoot, 'debug', 'lib', 'zlibstaticd.lib')
    };
    runtimeCandidates = {
      fullfile(tripletRoot, 'debug', 'bin', 'zd.dll')
      fullfile(tripletRoot, 'debug', 'bin', 'z.dll')
      fullfile(tripletRoot, 'debug', 'bin', 'zlibd1.dll')
    };
  else
    libraryCandidates = {
      fullfile(tripletRoot, 'lib', 'z.lib')
      fullfile(tripletRoot, 'lib', 'zlib.lib')
      fullfile(tripletRoot, 'lib', 'zlibstatic.lib')
    };
    runtimeCandidates = {
      fullfile(tripletRoot, 'bin', 'z.dll')
      fullfile(tripletRoot, 'bin', 'zlib1.dll')
    };
  end

  zlibLibrary = first_existing_file(libraryCandidates);
  if isempty(zlibLibrary)
    error('iccdev:zlibNotFound', ...
      ['IccProfLib2-static was built with ICC_USE_ZLIB=ON, but its zlib ' ...
       'import library was not found under %s.'], tripletRoot);
  end
  linkArgs = {zlibLibrary};

  zlibRuntime = first_existing_file(runtimeCandidates);
  if ~isempty(zlibRuntime)
    runtimeFiles = {zlibRuntime};
  elseif ~staticTriplet
    error('iccdev:zlibRuntimeNotFound', ...
      ['The dynamic vcpkg triplet %s requires a zlib runtime DLL, but no ' ...
       'supported DLL name was found under %s.'], triplet, tripletRoot);
  end
end

function path = first_existing_file(candidates)
  path = '';
  for i = 1:numel(candidates)
    if exist(candidates{i}, 'file')
      path = candidates{i};
      return;
    end
  end
end

function found = has_library(searchDirs, libNames)
  found = false;
  for i = 1:numel(searchDirs)
    d = searchDirs{i};
    for n = 1:numel(libNames)
      candidateLibName = libNames{n};
      if ispc()
        candidates = {fullfile(d, [candidateLibName '.lib']), ...
                      fullfile(d, ['lib' candidateLibName '.a'])};
      else
        candidates = {fullfile(d, ['lib' candidateLibName '.a']), ...
                      fullfile(d, [candidateLibName '.a']), ...
                      fullfile(d, ['lib' candidateLibName '.so']), ...
                      fullfile(d, ['lib' candidateLibName '.dylib'])};
      end
      for j = 1:numel(candidates)
        if exist(candidates{j}, 'file')
          found = true;
          return;
        end
      end
    end
  end
end

function valid = is_text_scalar(value)
  valid = (ischar(value) && (isempty(value) || size(value, 1) == 1)) || ...
    (isa(value, 'string') && isscalar(value));
end
