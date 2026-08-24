function result = run_docker_qa(image)
%RUN_DOCKER_QA Validate the published iccDEV image from MATLAB.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  matlab_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(matlab_dir);
  profile_path = fullfile(repo_root, 'Testing', ...
    'sRGB_v4_ICC_preference.icc');
  fixture_path = fullfile(matlab_dir, 'tests', 'fixtures', ...
    'docker_expected.txt');

  if nargin < 1
    result = iccdev.docker_validate(profile_path);
  else
    result = iccdev.docker_validate(profile_path, 'Image', image);
  end
  expected = regexp(strtrim(fileread(fixture_path)), '\r?\n', 'split');
  combined_output = [result.dumpOutput char(10) result.roundTripOutput];
  for i = 1:numel(expected)
    assert(~isempty(strfind(combined_output, expected{i})), ...
      'Missing Docker output marker: %s', expected{i});
  end

  fprintf('Docker QA passed: reference=%s image-id=%s\n', ...
    result.image, result.imageId);
end
