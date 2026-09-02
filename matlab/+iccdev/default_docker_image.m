function image = default_docker_image()
%DEFAULT_DOCKER_IMAGE Return the pinned published iccDEV Docker image.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  image = ['ghcr.io/internationalcolorconsortium/iccdev@sha256:' ...
    '0a54b8ad1ca73e294ecf9c71323e6385c8812945c6ca3b40ba98d9f82b89c0fc'];
end
