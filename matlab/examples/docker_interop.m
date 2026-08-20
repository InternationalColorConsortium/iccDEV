%% docker_interop.m - Compare MATLAB and container profile validation
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

thisDir = fileparts(mfilename('fullpath'));
matlabDir = fileparts(thisDir);
repoRoot = fileparts(matlabDir);
addpath(matlabDir);

profilePath = fullfile(repoRoot, 'Testing', ...
  'sRGB_v4_ICC_preference.icc');
image = iccdev.default_docker_image();

profile = iccdev.IccProfile(profilePath);
header = profile.header();
profile.close();

result = run_docker_qa(image);

fprintf('MATLAB profile version: %s\n', header.versionString);
fprintf('MATLAB color space: %s\n', ...
  iccdev.sig_to_str(uint32(header.colorSpace)));
fprintf('Docker image ID: %s\n', result.imageId);
fprintf('Docker dump status: %d\n', result.dumpStatus);
fprintf('Docker round-trip status: %d\n', result.roundTripStatus);

assert(~isempty(strfind(result.dumpOutput, 'Data Color Space:   RgbData')));
assert(~isempty(strfind(result.roundTripOutput, 'Max DeltaE:')));
fprintf('Docker interoperability checks passed.\n');
