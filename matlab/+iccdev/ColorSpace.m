classdef ColorSpace
  %COLORSPACE ICC color space signature constants.
  %
  %   iccdev.ColorSpace.RGB   % 0x52474220
  %   iccdev.ColorSpace.CMYK  % 0x434D594B
  %   iccdev.ColorSpace.name(sig)  % 'RGB'
  %
  % Copyright (c) International Color Consortium.
  % BSD 3-Clause License. See LICENSE.md for details.

  properties (Constant)
    XYZ     = uint32(hex2dec('58595A20'))
    Lab     = uint32(hex2dec('4C616220'))
    Luv     = uint32(hex2dec('4C757620'))
    YCbCr   = uint32(hex2dec('59436272'))
    Yxy     = uint32(hex2dec('59787920'))
    RGB     = uint32(hex2dec('52474220'))
    Gray    = uint32(hex2dec('47524159'))
    HSV     = uint32(hex2dec('48535620'))
    HLS     = uint32(hex2dec('484C5320'))
    CMYK    = uint32(hex2dec('434D594B'))
    CMY     = uint32(hex2dec('434D5920'))
    Named   = uint32(hex2dec('6e6d636c'))
    Unknown = uint32(hex2dec('3f3f3f3f'))
  end

  methods (Static)
    function s = name(sig)
      %NAME Convert a color space signature to a readable string.
      s = iccdev.sig_to_str(uint32(sig));
    end
  end
end
