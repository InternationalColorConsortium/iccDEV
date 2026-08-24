%% read_profile.m - Read and inspect an ICC profile header
%
% Usage:
%   run this script after building icc_mex and adding matlab/ to path.
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
  fullfile(repoRoot, 'Testing', 'sRGB_v4_ICC_preference.icc')
};
profilePath = '';
for i = 1:numel(candidates)
  if exist(candidates{i}, 'file')
    profilePath = candidates{i};
    break;
  end
end
if isempty(profilePath)
  error('iccdev:exampleProfileNotFound', ...
    ['No bundled display profile found. On Windows, run ' ...
     './CreateAllProfiles.bat from Testing in PowerShell; on Unix, run ' ...
     './CreateAllProfiles.sh from Testing.']);
end

fprintf('=== iccdev MATLAB Example: Read Profile ===\n\n');

p = iccdev.IccProfile(profilePath);
hdr = p.header();

fprintf('Profile:      %s\n', profilePath);
fprintf('Version:      %s\n', hdr.versionString);
fprintf('Size:         %d bytes\n', hdr.size);
fprintf('Color Space:  %s (0x%08X)\n', ...
  iccdev.sig_to_str(uint32(hdr.colorSpace)), uint32(hdr.colorSpace));
fprintf('PCS:          %s (0x%08X)\n', ...
  iccdev.sig_to_str(uint32(hdr.pcs)), uint32(hdr.pcs));
fprintf('Device Class: 0x%08X\n', uint32(hdr.deviceClass));
fprintf('Platform:     %s\n', iccdev.sig_to_str(uint32(hdr.platform)));
fprintf('Intent:       %d\n', hdr.renderingIntent);
fprintf('Illuminant:   X=%.4f Y=%.4f Z=%.4f\n', ...
  hdr.illuminantX, hdr.illuminantY, hdr.illuminantZ);
fprintf('Date:         %04d-%02d-%02d %02d:%02d:%02d\n', ...
  hdr.dateYear, hdr.dateMonth, hdr.dateDay, ...
  hdr.dateHours, hdr.dateMinutes, hdr.dateSeconds);
fprintf('Profile ID:   %s\n', sprintf('%02x', hdr.profileId));

p.close();
fprintf('\nDone.\n');
