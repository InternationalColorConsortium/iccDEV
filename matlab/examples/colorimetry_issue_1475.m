%% colorimetry_issue_1475.m - Compare legacy and registry D50 reductions
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

thisDir = fileparts(mfilename('fullpath'));
matlabDir = fileparts(thisDir);
addpath(matlabDir);

result = iccdev.qa.check_colorimetry_issue_1475();

fprintf('=== iccdev MATLAB QA: Issue #1475 Colorimetry ===\n\n');
fprintf('Perfect diffuser, D50 / CIE 1931 2-degree observer:\n');
fprintf('  Legacy 5 nm direct sum: X=%.9f Y=%.9f Z=%.9f\n', ...
  result.legacy_xyz);
fprintf('  Legacy-derived 10 nm weights: X=%.9f Y=%.9f Z=%.9f\n', ...
  result.legacy_weighted10_xyz);
fprintf('  Registry source literals: X=%.9f Y=%.9f Z=%.9f\n', ...
  result.registry_xyz);
fprintf('  Registry compiled float:  X=%.9f Y=%.9f Z=%.9f\n', ...
  result.registry_runtime_xyz);
fprintf('  Float - source literals: dX=%+.3e dY=%+.3e dZ=%+.3e\n', ...
  result.registry_runtime_xyz - result.registry_xyz);
fprintf('  Legacy - registry:      dX=%+.9f dY=%+.9f dZ=%+.9f\n', ...
  result.delta_xyz);

% The complete 10 nm operator is the decisive control: it changes the
% representation while holding the source data fixed, and preserves a constant
% perfect diffuser exactly. Directly decimating the color-stimulus products is
% intentionally shown separately because it is not the weighting method TN-06
% recommends for coarse measurements.
fprintf('\nControls using the same legacy SPD and CMFs:\n');
fprintf('  Weighted 10 nm - 5 nm: dX=%+.3e dY=%+.3e dZ=%+.3e\n', ...
  result.weighted_control_error);
fprintf('  Legacy at 10 nm:        X=%.9f Y=%.9f Z=%.9f\n', ...
  result.legacy10_xyz);
fprintf('  Grid effect on Z:       %+.9f  (gap to explain: %+.9f)\n', ...
  result.grid_effect, -result.delta_xyz(3));
fprintf('  Direct decimation moves Z by %.1f%% of the gap, opposite in sign.\n', ...
  100 * abs(result.grid_effect) / abs(result.delta_xyz(3)));
fprintf('  The weighted control preserves white, so this white-point difference\n');
fprintf('  lives in the source tables, not the 10 nm weighting representation.\n');
fprintf('  This does not generalize to non-flat spectra, where TN-06 weighting\n');
fprintf('  guidance remains material.\n');

% Which path is "closer" is a choice of reference, not a measure of accuracy.
fprintf('\nAgainst the two references in the tree:\n');
fprintf('  CIE 15   %.5f:  legacy dZ=%+.9f  registry dZ=%+.9f\n', ...
  result.canonical_xyz(3), result.legacy_error(3), result.registry_error(3));
fprintf('  ICC PCS  %.5f:  legacy dZ=%+.9f  registry dZ=%+.9f\n', ...
  result.icc_pcs_xyz(3), result.legacy_pcs_error(3), ...
  result.registry_pcs_error(3));
% Plain if/else rather than a helper: every other example in matlab/examples is a
% flat script with no local functions, and a script-local function is a construct
% no CI lane here has ever executed.
if result.closer_to_cie
  cieWinner = 'registry';
else
  cieWinner = 'legacy';
end
if result.closer_to_icc_pcs
  pcsWinner = 'legacy';
else
  pcsWinner = 'registry';
end
fprintf('  Closer in Z to CIE 15:            %s\n', cieWinner);
fprintf('  Closer in Z to the ICC PCS white: %s\n', pcsWinner);

fprintf('\nThe registry table is reachable through IccColorimetry, but no CMM or\n');
fprintf('tool path consumes it yet - CIccColorimetricCalculator is referenced\n');
fprintf('only by the colorimetry-methods regression test, so this is a parallel\n');
fprintf('API rather than a change to what the library currently computes.\n');
fprintf('iccPawgReport issue #1451 still requires spectral-column ingestion.\n');
fprintf('\nDone.\n');
