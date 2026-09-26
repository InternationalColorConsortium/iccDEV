function result = run_docker_qa(image)
%RUN_DOCKER_QA Validate the published iccDEV image from MATLAB.
%   RESULT = run_docker_qa() refreshes the default published tag, resolves it
%   to an immutable repository digest, and validates the Docker output.
%
%   RESULT = run_docker_qa(IMAGE) performs the same checks for an official
%   iccDEV tag or digest. RESULT.resolvedImage identifies the image executed.
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
    result = iccdev.docker_validate(profile_path, 'Pull', true);
  else
    result = iccdev.docker_validate(profile_path, ...
      'Image', image, 'Pull', true);
  end
  expected = regexp(strtrim(fileread(fixture_path)), '\r?\n', 'split');
  combined_output = [result.dumpOutput char(10) result.roundTripOutput];
  for i = 1:numel(expected)
    assert(~isempty(strfind(combined_output, expected{i})), ...
      'Missing Docker output marker: %s', expected{i});
  end

  fprintf(['Docker QA passed: selector=%s digest=%s ' ...
    'source-revision=%s image-id=%s\n'], ...
    result.image, result.resolvedImage, result.sourceRevision, result.imageId);
end
