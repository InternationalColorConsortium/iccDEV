function lab = pcs_to_lab(values, pcs)
%PCS_TO_LAB Decode normalized internal PCS samples to CIELAB.
%
% Copyright (c) 2026 International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  values = double(values);
  if size(values, 2) < 3
    error('iccdev:pawgQ1InvalidPcsSamples', ...
      'PCS samples must have at least three columns.');
  end
  values = values(:, 1:3);

  if pcs == iccdev.ColorSpace.Lab
    lab = [values(:, 1) * 100, ...
      values(:, 2) * 255 - 128, values(:, 3) * 255 - 128];
    return;
  end
  if pcs ~= iccdev.ColorSpace.XYZ
    error('iccdev:pawgQ1UnsupportedPcs', ...
      'PCS samples must use Lab or XYZ encoding.');
  end

  xyz = values * (65535 / 32768);
  xyz = xyz ./ [0.9642, 1.0, 0.8249];
  epsilon = 216 / 24389;
  kappa = 24389 / 27;
  f = (kappa * xyz + 16) / 116;
  nonlinear = xyz > epsilon;
  f(nonlinear) = xyz(nonlinear) .^ (1 / 3);
  lab = [116 * f(:, 2) - 16, ...
    500 * (f(:, 1) - f(:, 2)), 200 * (f(:, 2) - f(:, 3))];
end
