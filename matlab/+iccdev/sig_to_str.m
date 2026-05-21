function s = sig_to_str(sig)
  %SIG_TO_STR Convert a 4-byte ICC signature to a readable ASCII string.
  %
  %   s = iccdev.sig_to_str(uint32(hex2dec('52474220')))  % 'RGB'
  %
  % Copyright (c) International Color Consortium.
  % BSD 3-Clause License. See LICENSE.md for details.

  sig = uint32(sig);
  bytes = [
    bitand(bitshift(sig, -24), 255), ...
    bitand(bitshift(sig, -16), 255), ...
    bitand(bitshift(sig,  -8), 255), ...
    bitand(sig, 255)
  ];
  s = strtrim(char(bytes));
end
