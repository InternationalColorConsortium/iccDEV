function test_delta_e_2000()
%TEST_DELTA_E_2000 Verify single- and multiple-pair CIEDE2000 calculations.
%
% Copyright (c) 2026 International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  lab1 = [ ...
    50.0000, 2.6772, -79.7751
    50.0000, 3.1571, -77.2803];
  lab2 = repmat([50.0000, 0.0000, -82.7485], 2, 1);
  expected = [2.0425; 2.8615];

  single_delta = iccdev.qa.delta_e_2000(lab1(1, :), lab2(1, :));
  assert(isscalar(single_delta));
  assert(abs(single_delta - expected(1)) < 5e-5, ...
    'Single-pair CIEDE2000 reference agreement failed.');

  multiple_delta = iccdev.qa.delta_e_2000(lab1, lab2);
  assert(isequal(size(multiple_delta), [2, 1]));
  assert(max(abs(multiple_delta - expected)) < 5e-5, ...
    'Multiple-pair CIEDE2000 reference agreement failed.');

  fprintf(['MATLAB CIEDE2000 examples passed: single=%.4f, ' ...
    'multiple=[%.4f %.4f].\n'], single_delta, multiple_delta);
end
