function image = default_docker_image()
%DEFAULT_DOCKER_IMAGE Return the pinned published iccDEV Docker image.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  image = ['ghcr.io/internationalcolorconsortium/iccdev@sha256:' ...
    'b003dcfdc04776035b51d9cdc03f90d62cc70c745da321ae29e182075bf2845d'];
end
