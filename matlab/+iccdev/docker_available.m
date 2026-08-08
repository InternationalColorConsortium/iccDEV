function [available, details] = docker_available(image)
%DOCKER_AVAILABLE Check whether Docker and an iccDEV image are available.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if nargin < 1
    image = 'ghcr.io/internationalcolorconsortium/iccdev:latest';
  end
  image = validate_image(char(image));

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

function image = validate_image(image)
  pattern = [ ...
    '^ghcr\.io/internationalcolorconsortium/iccdev' ...
    '(:[A-Za-z0-9._-]+|@sha256:[A-Fa-f0-9]{64})$' ...
  ];
  if isempty(regexp(image, pattern, 'once'))
    error('iccdev:invalidDockerImage', ...
      'Unsupported iccDEV Docker image reference: %s', image);
  end
end

function command = docker_command(arguments)
  quoted = cell(size(arguments));
  for i = 1:numel(arguments)
    argument = char(arguments{i});
    if ispc()
      if ~isempty(regexp(argument, '["%%&|<>^!\r\n]', 'once'))
        error('iccdev:unsafeDockerArgument', ...
          'Docker argument contains unsupported shell characters.');
      end
      quoted{i} = ['"' argument '"'];
    else
      if ~isempty(regexp(argument, '[''\r\n]', 'once'))
        error('iccdev:unsafeDockerArgument', ...
          'Docker argument contains unsupported shell characters.');
      end
      quoted{i} = ['''' argument ''''];
    end
  end
  command = strjoin(quoted, ' ');
end
