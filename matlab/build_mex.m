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
  addParameter(p, 'BuildDir', '', @ischar);
  addParameter(p, 'Debug', false, @islogical);
  parse(p, varargin{:});

  thisDir  = fileparts(mfilename('fullpath'));
  repoRoot = fileparts(thisDir);
  mexSrc   = fullfile(thisDir, 'mex', 'icc_mex.cpp');
  inclDir  = fullfile(repoRoot, 'IccProfLib');

  % Find build directory
  buildDir = p.Results.BuildDir;
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

  % Build MEX arguments
  cxxFlags = {};
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

  outDir = thisDir;
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
    {['-l' libName]}
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

  fprintf('Build complete. Add %s to your MATLAB path.\n', thisDir);
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
