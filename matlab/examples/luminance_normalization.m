%% luminance_normalization.m - Reproduce issue #1811 calculations
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

this_dir = fileparts(mfilename('fullpath'));
matlab_dir = fileparts(this_dir);
addpath(matlab_dir);

results = iccdev.qa.check_luminance_normalization();

fprintf('Fixture normalization to Y=1:\n');
for i = 1:numel(results.fixtureNames)
  xyz = results.illuminantXYZ(i, :);
  normalized = results.normalizedXYZ(i, :);
  fprintf(['  %-26s XYZ=(%.12g, %.12g, %.12g)  ' ...
    'normalized=(%.12g, %.12g, %.12g)\n'], ...
    results.fixtureNames{i}, xyz(1), xyz(2), xyz(3), ...
    normalized(1), normalized(2), normalized(3));
end

fprintf('\nSingle-precision CheckLuminance window:\n');
for i = 1:numel(results.windowY)
  if results.windowWarns(i)
    status = 'Warning';
  else
    status = 'OK';
  end
  fprintf('  Y=%-8g distance=%-12.9g %s\n', ...
    results.windowY(i), results.windowDistance(i), status);
end

fprintf(['\nThe 300 and 500 cd/m^2 fixtures normalize to the same XYZ ' ...
  'triple as the Y=1 fixture.\n']);
fprintf('The BT.2100 physical surround remains Y=%.12g cd/m^2.\n', ...
  results.bt2100SurroundXYZ(2));
fprintf(['Nominal Y=0.99 and Y=1.01 both warn after conversion to ' ...
  'single precision.\n']);
