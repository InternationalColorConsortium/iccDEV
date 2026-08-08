function result = run_docker_qa(image)
%RUN_DOCKER_QA Validate the published iccDEV image from MATLAB.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    image = 'ghcr.io/internationalcolorconsortium/iccdev:latest';
  end

  matlab_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(matlab_dir);
  profile_path = fullfile(repo_root, 'Testing', ...
    'sRGB_v4_ICC_preference.icc');
  fixture_path = fullfile(matlab_dir, 'tests', 'fixtures', ...
    'docker_expected.txt');

  result = iccdev.docker_validate(profile_path, 'Image', image);
  expected = regexp(strtrim(fileread(fixture_path)), '\r?\n', 'split');
  combined_output = [result.dumpOutput char(10) result.roundTripOutput];
  for i = 1:numel(expected)
    assert(~isempty(strfind(combined_output, expected{i})), ...
      'Missing Docker output marker: %s', expected{i});
  end

  fprintf('Docker QA passed with %s\n', result.imageId);
end
