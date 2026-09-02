function de = delta_e_2000(lab1, lab2)
%DELTA_E_2000 Calculate CIEDE2000 colour differences.
%
% Copyright (c) 2026 International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  if ~isequal(size(lab1), size(lab2)) || size(lab1, 2) ~= 3
    error('iccdev:pawgQ1InvalidLabSamples', ...
      'CIELAB inputs must be equal-sized N-by-3 arrays.');
  end

  lab1 = double(lab1);
  lab2 = double(lab2);
  l1 = lab1(:, 1); a1 = lab1(:, 2); b1 = lab1(:, 3);
  l2 = lab2(:, 1); a2 = lab2(:, 2); b2 = lab2(:, 3);
  c1 = hypot(a1, b1);
  c2 = hypot(a2, b2);
  cbar = (c1 + c2) / 2;
  g = 0.5 * (1 - sqrt(cbar .^ 7 ./ (cbar .^ 7 + 25 ^ 7)));
  a1p = (1 + g) .* a1;
  a2p = (1 + g) .* a2;
  c1p = hypot(a1p, b1);
  c2p = hypot(a2p, b2);
  h1p = mod(atan2d(b1, a1p), 360);
  h2p = mod(atan2d(b2, a2p), 360);
  h1p(c1p < 1e-12) = 0;
  h2p(c2p < 1e-12) = 0;

  dlp = l2 - l1;
  dcp = c2p - c1p;
  dhp = h2p - h1p;
  dhp(dhp > 180) = dhp(dhp > 180) - 360;
  dhp(dhp < -180) = dhp(dhp < -180) + 360;
  dhp(c1p < 1e-12 | c2p < 1e-12) = 0;
  big_dhp = 2 * sqrt(c1p .* c2p) .* sind(dhp / 2);

  lbarp = (l1 + l2) / 2;
  cbarp = (c1p + c2p) / 2;
  hbarp = (h1p + h2p) / 2;
  achromatic = c1p < 1e-12 | c2p < 1e-12;
  hbarp(achromatic) = h1p(achromatic) + h2p(achromatic);
  wrap = ~achromatic & abs(h1p - h2p) > 180;
  low = wrap & (h1p + h2p < 360);
  high = wrap & ~low;
  hbarp(low) = (h1p(low) + h2p(low) + 360) / 2;
  hbarp(high) = (h1p(high) + h2p(high) - 360) / 2;

  t = 1 - 0.17 * cosd(hbarp - 30) + 0.24 * cosd(2 * hbarp) + ...
    0.32 * cosd(3 * hbarp + 6) - 0.20 * cosd(4 * hbarp - 63);
  delta_theta = 30 * exp(-((hbarp - 275) / 25) .^ 2);
  rc = 2 * sqrt(cbarp .^ 7 ./ (cbarp .^ 7 + 25 ^ 7));
  sl = 1 + 0.015 * (lbarp - 50) .^ 2 ./ ...
    sqrt(20 + (lbarp - 50) .^ 2);
  sc = 1 + 0.045 * cbarp;
  sh = 1 + 0.015 * cbarp .* t;
  rt = -sind(2 * delta_theta) .* rc;
  term_l = dlp ./ sl;
  term_c = dcp ./ sc;
  term_h = big_dhp ./ sh;
  de = sqrt(max(0, term_l .^ 2 + term_c .^ 2 + term_h .^ 2 + ...
    rt .* term_c .* term_h));
end
