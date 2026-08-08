function valid = docker_is_text_scalar(value)
%DOCKER_IS_TEXT_SCALAR Check for a character vector or string scalar.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  valid = (ischar(value) && (isempty(value) || size(value, 1) == 1)) || ...
    (isa(value, 'string') && isscalar(value));
end
