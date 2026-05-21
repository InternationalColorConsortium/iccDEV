classdef RenderingIntent
  %RENDERINGINTENT ICC rendering intent constants.
  %
  %   iccdev.RenderingIntent.Perceptual           % 0
  %   iccdev.RenderingIntent.RelativeColorimetric  % 1
  %   iccdev.RenderingIntent.Saturation            % 2
  %   iccdev.RenderingIntent.AbsoluteColorimetric  % 3
  %
  % Copyright (c) International Color Consortium.
  % BSD 3-Clause License. See LICENSE.md for details.

  properties (Constant)
    Perceptual           = int32(0)
    RelativeColorimetric = int32(1)
    Saturation           = int32(2)
    AbsoluteColorimetric = int32(3)
  end
end
