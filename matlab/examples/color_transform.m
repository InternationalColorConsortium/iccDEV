%% color_transform.m - Apply a color transform between two ICC profiles
%
% Usage:
%   Adjust inputProfile and outputProfile paths, then run.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

thisDir = fileparts(mfilename('fullpath'));
matlabDir = fileparts(thisDir);
repoRoot = fileparts(matlabDir);
addpath(matlabDir);

candidates = {
  fullfile(repoRoot, 'Testing', 'Display', 'sRGB_D65_MAT.icc')
  fullfile(repoRoot, 'Testing', 'Display', 'sRGB_D65_colorimetric.icc')
  fullfile(repoRoot, 'Testing', 'Display', 'LCDDisplay.icc')
};
found = {};
for i = 1:numel(candidates)
  if exist(candidates{i}, 'file')
    found{end+1} = candidates{i}; %#ok<AGROW>
  end
end
if isempty(found)
  error('iccdev:exampleProfileNotFound', ...
    'No bundled display profile found. Run Testing/CreateAllProfiles.sh first.');
end
inputProfile = found{1};
if numel(found) >= 2
  outputProfile = found{2};
else
  outputProfile = found{1};
end

fprintf('=== iccdev MATLAB Example: Color Transform ===\n\n');

% Create CMM and attach profiles
cmm = iccdev.IccCmm();
cmm.attach(inputProfile, 'intent', iccdev.RenderingIntent.Perceptual);
cmm.attach(outputProfile);
cmm.begin();

fprintf('Pipeline: %d ch -> %d ch\n\n', cmm.SrcChannels, cmm.DstChannels);

% Single pixel transform
pixel = [0.5, 0.3, 0.1];
result = cmm.apply(pixel);
fprintf('Single pixel:\n');
fprintf('  Input:  [%.4f, %.4f, %.4f]\n', pixel);
fprintf('  Output: [%.4f, %.4f, %.4f]\n', result);

% Bulk transform (N x 3 matrix)
testPixels = [
  0.0, 0.0, 0.0;  % Black
  1.0, 1.0, 1.0;  % White
  1.0, 0.0, 0.0;  % Red
  0.0, 1.0, 0.0;  % Green
  0.0, 0.0, 1.0;  % Blue
  0.5, 0.5, 0.5;  % Gray
];

results = cmm.apply(testPixels);

fprintf('\nBulk transform (%d pixels):\n', size(testPixels, 1));
for i = 1:size(testPixels, 1)
  fprintf('  [%.2f, %.2f, %.2f] -> [%.4f, %.4f, %.4f]\n', ...
    testPixels(i,:), results(i,:));
end

% Thread-safe apply handle
ah = cmm.get_apply();
r2 = ah.apply([0.25, 0.75, 0.5]);
fprintf('\nApply handle: [0.25, 0.75, 0.50] -> [%.4f, %.4f, %.4f]\n', r2);
ah.close();

% Performance test
N = 10000;
bigPixels = rand(N, 3, 'single');
tic;
bigResults = cmm.apply(bigPixels);
elapsed = toc;
fprintf('\nBulk %d pixels: %.3f ms (%.0f pixels/sec)\n', ...
  N, elapsed*1000, N/elapsed);

cmm.close();
fprintf('\nDone.\n');
