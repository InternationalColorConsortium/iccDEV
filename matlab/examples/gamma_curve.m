%% gamma_curve.m - Decode and apply an ICC curveType gamma value
%
% ICC.1:2022 sections 4.9 and 10.6 define a one-entry curveType as a
% u8Fixed8Number exponent in y = x^gamma, not as an inverse exponent.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

thisDir = fileparts(mfilename('fullpath'));
matlabDir = fileparts(thisDir);
addpath(matlabDir);

results = run_gamma_qa();
gamma = results(1).gamma;
inputValues = [0.0 0.18 0.25 0.5 0.75 1.0];
outputValues = inputValues .^ gamma;

fprintf('\n=== iccdev MATLAB Example: curveType Gamma ===\n\n');
fprintf('ICC equation: y = x^gamma, gamma = %.8f\n\n', gamma);
for i = 1:numel(inputValues)
  fprintf('  %.2f -> %.8f\n', inputValues(i), outputValues(i));
end
fprintf('\nDone.\n');
