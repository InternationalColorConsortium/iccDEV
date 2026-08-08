function [available, details] = docker_available(image)
%DOCKER_AVAILABLE Check whether Docker and an iccDEV image are available.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    image = iccdev.default_docker_image();
  end
  if ~docker_is_text_scalar(image)
    error('iccdev:invalidDockerImage', ...
      'Docker image reference must be a character vector or string scalar.');
  end
  image = docker_validate_image(char(image));

  [daemon_status, daemon_output] = system( ...
    'docker version --format "{{.Server.Version}}"');
  if daemon_status ~= 0
    available = false;
    details = strtrim(daemon_output);
    return;
  end

  command = docker_command({'docker', 'image', 'inspect', image, ...
    '--format={{.Id}}'});
  [image_status, image_output] = system(command);
  available = image_status == 0;
  details = strtrim(image_output);
end
