function result = docker_validate(profile_path, varargin)
%DOCKER_VALIDATE Validate a profile with the published iccDEV Docker image.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  p = inputParser;
  addRequired(p, 'profile_path', @docker_is_text_scalar);
  addParameter(p, 'Image', ...
    'ghcr.io/internationalcolorconsortium/iccdev:latest', ...
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

  [docker_ready, docker_details] = iccdev.docker_available(image);
  if ~docker_ready && p.Results.Pull
    pull_command = docker_command({'docker', 'pull', image});
    [pull_status, pull_output] = system(pull_command);
    if pull_status ~= 0
      error('iccdev:dockerPullFailed', ...
        'Unable to pull %s:\n%s', image, pull_output);
    end
    [docker_ready, docker_details] = iccdev.docker_available(image);
  end
  if ~docker_ready
    error('iccdev:dockerUnavailable', ...
      'Docker or image %s is unavailable:\n%s', image, docker_details);
  end

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
    image
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
    'image', image, ...
    'imageId', docker_details, ...
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
