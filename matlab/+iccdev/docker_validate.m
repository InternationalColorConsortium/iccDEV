function result = docker_validate(profile_path, varargin)
%DOCKER_VALIDATE Validate a profile with the published iccDEV Docker image.
%   RESULT = iccdev.docker_validate(PROFILE_PATH) resolves the locally
%   available default image to an immutable digest before validation.
%
%   RESULT = iccdev.docker_validate(PROFILE_PATH, 'Image', IMAGE, ...
%   'Pull', true) refreshes a mutable tag (or obtains a missing digest), then
%   executes the resolved digest. RESULT records the requested selector in
%   image, the executed reference in resolvedImage, and the OCI source label
%   in sourceRevision.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    error('iccdev:dockerProfileRequired', ...
      ['Provide an ICC profile path. Example: ' ...
       'result = iccdev.docker_validate(profile_path);']);
  end

  p = inputParser;
  addRequired(p, 'profile_path', @docker_is_text_scalar);
  addParameter(p, 'Image', ...
    iccdev.default_docker_image(), ...
    @docker_is_text_scalar);
  addParameter(p, 'Pull', false, ...
    @(value) islogical(value) && isscalar(value));
  parse(p, profile_path, varargin{:});

  profile_path = char(p.Results.profile_path);
  image = docker_validate_image(char(p.Results.Image));
  [exists, attributes] = fileattrib(profile_path); %#ok<FILEATTRIB>
  if ~exists || attributes.directory
    error('iccdev:dockerProfileNotFound', ...
      'Docker input profile not found: %s', profile_path);
  end

  profile_path = attributes.Name;
  validate_host_path(profile_path);

  image_details = docker_resolve_image(image, p.Results.Pull);

  mount_spec = ['type=bind,source=' profile_path ...
    ',target=/profile.icc,readonly'];
  container_profile = '/profile.icc';
  common = {
    'docker'
    'run'
    '--rm'
    '--network'
    'none'
    '--read-only'
    '--security-opt'
    'no-new-privileges'
    '--cap-drop'
    'ALL'
    '--pids-limit'
    '128'
    '--memory'
    '1g'
    '--cpus'
    '2'
    '--mount'
    mount_spec
    image_details.resolvedImage
  };

  dump_command = docker_command([common; {
    'iccDumpProfile'
    '-v'
    container_profile
  }]);
  [dump_status, dump_output] = system(dump_command);
  if dump_status ~= 0
    error('iccdev:dockerDumpFailed', ...
      'iccDumpProfile failed with status %d:\n%s', ...
      dump_status, dump_output);
  end

  roundtrip_command = docker_command([common; {
    'iccRoundTrip'
    container_profile
  }]);
  [roundtrip_status, roundtrip_output] = system(roundtrip_command);

  result = struct( ...
    'image', image_details.selector, ...
    'resolvedImage', image_details.resolvedImage, ...
    'imageId', image_details.imageId, ...
    'sourceRevision', image_details.sourceRevision, ...
    'profile', profile_path, ...
    'dumpStatus', dump_status, ...
    'dumpOutput', dump_output, ...
    'roundTripStatus', roundtrip_status, ...
    'roundTripOutput', roundtrip_output);

  if roundtrip_status ~= 0
    error('iccdev:dockerRoundTripFailed', ...
      'iccRoundTrip failed with status %d:\n%s', ...
      roundtrip_status, roundtrip_output);
  end
end

function validate_host_path(path)
  if ispc()
    unsafe = '[,"%%&|<>^!\r\n]';
  else
    unsafe = '[,''\r\n]';
  end
  if ~isempty(regexp(path, unsafe, 'once'))
    error('iccdev:unsafeDockerPath', ...
      'Docker profile path contains unsupported mount or shell characters.');
  end
end
