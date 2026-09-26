%% ciede2000.m - Compare one or more pairs of CIELAB colours
%
% Copyright (c) 2026 International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

thisDir = fileparts(mfilename('fullpath'));
matlabDir = fileparts(thisDir);
addpath(matlabDir);

% Each CIELAB colour is one row in the order [L* a* b*].
referenceLab = [50.0000, 2.6772, -79.7751];
sampleLab = [50.0000, 0.0000, -82.7485];
singleDelta = iccdev.qa.delta_e_2000(referenceLab, sampleLab);

fprintf('Single CIELAB pair: Delta E 2000 = %.4f\n', singleDelta);

% For multiple pairs, use equal-sized N-by-3 arrays. Row i in lab1 is
% compared with row i in lab2, and deltaE(i) is the corresponding result.
lab1 = [ ...
  50.0000, 2.6772, -79.7751
  50.0000, 3.1571, -77.2803];
lab2 = repmat([50.0000, 0.0000, -82.7485], 2, 1);
deltaE = iccdev.qa.delta_e_2000(lab1, lab2);

fprintf('\nMultiple CIELAB pairs:\n');
fprintf('  Pair 1: Delta E 2000 = %.4f\n', deltaE(1));
fprintf('  Pair 2: Delta E 2000 = %.4f\n', deltaE(2));

expected = [2.0425; 2.8615];
assert(max(abs(deltaE - expected)) < 5e-5, ...
  'Published CIEDE2000 reference agreement failed.');
fprintf('\nPublished CIEDE2000 reference values verified.\n');
