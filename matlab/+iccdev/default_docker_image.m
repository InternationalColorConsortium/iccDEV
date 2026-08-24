function image = default_docker_image()
%DEFAULT_DOCKER_IMAGE Return the pinned published iccDEV Docker image.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  image = ['ghcr.io/internationalcolorconsortium/iccdev@sha256:' ...
    '2f4230308320b60106c2675b2c50aa7c22e0b50fe56045080ce480c6232b2672'];
end
