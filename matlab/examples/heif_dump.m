%% heif_dump.m - Interactively inspect embedded HEIF ICC properties
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

this_dir = fileparts(mfilename('fullpath'));
matlab_dir = fileparts(this_dir);
addpath(matlab_dir);

[filename, directory] = uigetfile( ...
  {'*.heif;*.heic;*.avif', 'HEIF, HEIC, and AVIF images'}, ...
  'Select a HEIF, HEIC, or AVIF image');
assert(~isequal(filename, 0), 'iccdev:heifSelectionCancelled', ...
  'No HEIF, HEIC, or AVIF image was selected.');

image_path = fullfile(directory, filename);
report = iccdev.heif_dump(image_path);
fprintf('%s\n', report);
