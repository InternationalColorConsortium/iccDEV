function image = docker_validate_image(image)
%DOCKER_VALIDATE_IMAGE Validate an official iccDEV Docker image reference.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  pattern = [ ...
    '^ghcr\.io/internationalcolorconsortium/iccdev' ...
    '(:[A-Za-z0-9._-]+|@sha256:[A-Fa-f0-9]{64})$' ...
  ];
  if isempty(regexp(image, pattern, 'once'))
    error('iccdev:invalidDockerImage', ...
      'Unsupported iccDEV Docker image reference: %s', image);
  end
end
