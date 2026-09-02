function results = check_luminance_normalization(fixture_root)
%CHECK_LUMINANCE_NORMALIZATION Verify spectral-viewing luminance calculations.
%
%   results = iccdev.qa.check_luminance_normalization()
%   results = iccdev.qa.check_luminance_normalization(fixture_root)
%
% Reads the sRGB D65 fixtures authored at Y=1, 300, and 500 cd/m^2,
% confirms that scaling each XYZ triple by its Y produces the same D65
% chromaticity, and emulates CIccInfo::CheckLuminance using single precision.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    matlab_dir = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    repo_root = fileparts(matlab_dir);
    fixture_root = fullfile(repo_root, 'Testing');
  end

  display_dir = fullfile(fixture_root, 'Display');
  fixture_names = {
    'sRGB_D65_MAT.xml'
    'sRGB_D65_MAT-300cdm2.xml'
    'sRGB_D65_MAT-500cdm2.xml'
  };
  expected_y = [1; 300; 500];
  illuminant = zeros(3, 3);
  surround = zeros(3, 3);

  for i = 1:numel(fixture_names)
    fixture_path = fullfile(display_dir, fixture_names{i});
    xml = fileread(fixture_path);
    illuminant(i, :) = read_xyz(xml, 'IlluminantXYZ');
    surround(i, :) = read_xyz(xml, 'SurroundXYZ');
  end

  tolerance = 1e-12;
  assert(max(abs(illuminant(:, 2) - expected_y)) < tolerance, ...
    'iccdev:luminanceFixtureY', ...
    'Fixture Y values do not match 1, 300, and 500 cd/m^2.');
  assert(max(abs(illuminant(:) - surround(:))) < tolerance, ...
    'iccdev:luminanceFixtureMismatch', ...
    'IlluminantXYZ and SurroundXYZ differ in the sRGB D65 fixtures.');

  normalized = bsxfun(@rdivide, illuminant, illuminant(:, 2));
  normalized_error = max(abs(normalized - normalized(1, :)), [], 2);
  assert(max(normalized_error) < tolerance, ...
    'iccdev:luminanceNormalizationMismatch', ...
    'The 300 and 500 cd/m^2 fixtures do not normalize to the Y=1 fixture.');

  bt2100_path = fullfile(fixture_root, 'HDR', 'BT2100PQFullDisplay.xml');
  bt2100_xml = fileread(bt2100_path);
  bt2100_surround = read_xyz(bt2100_xml, 'SurroundXYZ');
  assert(abs(bt2100_surround(2) - 5.0) < tolerance, ...
    'iccdev:luminanceBt2100Mismatch', ...
    'The BT.2100 fixture surround is not 5 cd/m^2.');

  case_y = single([0.98; 0.99; 0.995; 1.0; 1.005; 1.01; 1.02; 5; 300; 500]);
  distance = abs(double(case_y) - 1.0);
  warns = distance < 0.01;
  expected_warns = logical([0; 1; 1; 1; 1; 1; 0; 0; 0; 0]);
  assert(isequal(warns, expected_warns), ...
    'iccdev:luminanceWindowMismatch', ...
    'Single-precision warning-window behavior changed.');

  results.fixtureNames = fixture_names;
  results.illuminantXYZ = illuminant;
  results.surroundXYZ = surround;
  results.normalizedXYZ = normalized;
  results.normalizationError = normalized_error;
  results.bt2100SurroundXYZ = bt2100_surround;
  results.windowY = double(case_y);
  results.windowDistance = distance;
  results.windowWarns = warns;
end

function xyz = read_xyz(xml, tag_name)
  tag_match = regexp(xml, ['<' tag_name '\s+[^>]*>'], 'match', 'once');
  assert(~isempty(tag_match), 'iccdev:luminanceTagMissing', ...
    'Missing %s element.', tag_name);

  xyz = zeros(1, 3);
  attributes = {'X', 'Y', 'Z'};
  for i = 1:numel(attributes)
    token = regexp(tag_match, ...
      [attributes{i} '="([^"]+)"'], 'tokens', 'once');
    assert(~isempty(token), 'iccdev:luminanceAttributeMissing', ...
      'Missing %s attribute on %s.', attributes{i}, tag_name);
    xyz(i) = str2double(token{1});
    assert(isfinite(xyz(i)), 'iccdev:luminanceAttributeInvalid', ...
      'Invalid %s attribute on %s.', attributes{i}, tag_name);
  end
end
