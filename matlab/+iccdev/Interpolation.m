classdef Interpolation
  %INTERPOLATION CMM interpolation method constants.
  %
  %   iccdev.Interpolation.Linear       % 0
  %   iccdev.Interpolation.Tetrahedral  % 1
  %
  % Copyright (c) International Color Consortium.
  % BSD 3-Clause License. See LICENSE.md for details.

  properties (Constant)
    Linear      = int32(0)
    Tetrahedral = int32(1)
  end
end
