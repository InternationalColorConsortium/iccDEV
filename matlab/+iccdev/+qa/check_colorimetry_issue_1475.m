function result = check_colorimetry_issue_1475()
%CHECK_COLORIMETRY_ISSUE_1475 Compare legacy and registry D50 reductions.
%
%   result = iccdev.qa.check_colorimetry_issue_1475()
%
% This dependency-free source-table audit is the MATLAB half of the TN-06
% perfect-diffuser QA. Run the compiled half with the
% iccdev.colorimetry-methods CTest from a build configured with ENABLE_TESTS.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.
%
% Issue #1475 concerns the spectral-to-colorimetry path. Two D50/1931 white
% points can be computed from data already in the tree, and they disagree:
%
%   legacy   0.964245566 / 1 / 0.824679094  - icKnownIllums D50 SPD times the
%            icKnownObservers 1931 CMFs, summed on the built-in 380-780@5nm grid
%   registry 0.964240837 / 1 / 0.825128117  - the column sums of the baked-in
%            registry LWL weighting table kWtsObs1931D50, 380-780@10nm
%
% The two paths differ in THREE ways at once: reduction method (plain product
% versus a weighting table), sample grid (5 nm versus 10 nm), and the underlying
% illuminant data (iccDEV's inherited D50 SPD versus the CIE SPD that the
% registry table was generated from). Attributing the 4.5e-4 Z difference to any
% one of them requires holding the others fixed, so this check constructs both
% direct-sum and weighting controls from the same legacy tables:
%
%   weighted control - a 10 nm linear-reconstruction weighting operator built
%            from the SAME 5 nm legacy SPD and CMFs reproduces the 5 nm white
%            exactly. A perfect diffuser is constant, so the reconstruction
%            basis is a partition of unity and no sampling information is lost.
%   direct control - re-summing the same tables directly on the 10 nm subgrid
%            moves Z by about -6.2e-4. This is expected: direct coarse-grid
%            summation and a weighting operator are different algorithms.
%   data   - what is left. The gap lives in the illuminant/observer data, not in
%            the 10 nm weighting representation. That matches the #1475 finding
%            that iccDEV's 5 nm D50 SPD (inherited in 889db62b, 2023-11-03) is
%            the divergent object, while all ten registry weighting tables match
%            the published registry values exactly.
%
% This conclusion is deliberately narrow: it explains this perfect-diffuser
% white-point gap. TN-06 still prefers suitable weighting functions for coarse,
% non-1/5 nm measured spectra because direct summation can differ materially for
% non-constant stimuli.
%
% "Which one is right" is therefore a choice of reference, not a bug fix, and
% the answer flips depending on which white point is adopted -- see the two
% closer_to_* fields. Note also that this reads the C++ SOURCE TEXT as double.
% The library stores table literals as icFloatNumber (float, IccDefs.h:88).
% registry_runtime_xyz below rounds the literals to single before accumulating
% them in double, matching icApplyWeightingTable. The native CTest remains
% authoritative for compiled library behaviour.

  qa_dir = fileparts(mfilename('fullpath'));
  matlab_dir = fileparts(fileparts(qa_dir));
  repo_root = fileparts(matlab_dir);

  tag_basic = fileread(fullfile(repo_root, 'IccProfLib', 'IccTagBasic.cpp'));
  colorimetry = fileread(fullfile(repo_root, 'IccProfLib', ...
    'IccColorimetry.cpp'));

  d50 = extract_values(tag_basic, ...
    '\{ icIlluminantD50,\s*\{(.*?)\}\s*\}', 81);
  observer = extract_values(tag_basic, ...
    '\{ icStdObs1931TwoDegrees,\s*\{(.*?)\}\s*\}', 243);
  weights = extract_values(colorimetry, ...
    'kWtsObs1931D50\[123\]\s*=\s*\{(.*?)\};', 123);

  x_bar = observer(1:81);
  y_bar = observer(82:162);
  z_bar = observer(163:243);

  legacy_xyz = direct_sum(d50, x_bar, y_bar, z_bar);
  legacy_weighted10_xyz = linear_weighted_white(d50, x_bar, y_bar, z_bar);

  % The registry table is a complete operator on the CIE Y=100 scale: applied to
  % a perfect diffuser it degenerates to the column sums, so no illuminant or
  % observer is applied on top of it.
  registry_xyz = [
    sum(weights(1:41))
    sum(weights(42:82))
    sum(weights(83:123))
  ] ./ 100.0;
  runtime_weights = single(weights);
  registry_runtime_xyz = [
    sum(double(runtime_weights(1:41)))
    sum(double(runtime_weights(42:82)))
    sum(double(runtime_weights(83:123)))
  ] ./ 100.0;

  % Grid control. Same SPD, same CMFs, same rectangular sum -- only every second
  % sample, giving the registry table's 380-780@10nm grid. The k = 1/sum(ybar*S)
  % normalization divides the sample spacing out, so this isolates the sampling
  % rate on its own.
  decimated = 1:2:81;
  legacy10_xyz = direct_sum(d50(decimated), x_bar(decimated), ...
    y_bar(decimated), z_bar(decimated));

  expected_legacy = [0.964245565670359; 1.0; 0.824679093854306];
  expected_legacy_weighted10 = expected_legacy;
  expected_registry = [0.9642408375; 1.0000000002; 0.8251281168];
  expected_registry_runtime = [
    0.96424083708705444
    0.99999999357314662
    0.82512811001751218
  ];
  expected_legacy10 = [0.963956085519434; 1.0; 0.824057352615889];

  % Two references, deliberately both. CIE 15 is the external colorimetric truth;
  % icD50XYZ is the PCS illuminant iccDEV has always encoded (IccUtil.cpp) and is
  % what the native CTest anchors the legacy path to. They are 3.1e-4 apart in Z,
  % which is comparable to the effect under study.
  canonical_xyz = [0.96422; 1.0; 0.82521];      % CIE 15 D50/1931
  icc_pcs_xyz = [0.9642; 1.0; 0.8249];          % icD50XYZ, the ICC PCS illuminant

  assert(max(abs(legacy_xyz - expected_legacy)) < 1e-12, ...
    'Legacy D50/1931 calculation changed unexpectedly.');
  assert(max(abs(legacy_weighted10_xyz - expected_legacy_weighted10)) < 1e-12, ...
    'Legacy-derived 10 nm weighting control changed unexpectedly.');
  assert(max(abs(registry_xyz - expected_registry)) < 1e-10, ...
    'Registry D50/1931 weighting-table calculation changed unexpectedly.');
  assert(max(abs(registry_runtime_xyz - expected_registry_runtime)) < 1e-12, ...
    'Registry D50/1931 compiled-float model changed unexpectedly.');
  assert(max(abs(registry_runtime_xyz - registry_xyz)) < 1e-8, ...
    'Registry source-literal rounding exceeded the expected float32 scale.');
  assert(max(abs(legacy10_xyz - expected_legacy10)) < 1e-12, ...
    'Decimated 10 nm legacy calculation changed unexpectedly.');

  delta = legacy_xyz - registry_xyz;
  assert(delta(3) < -4.4e-4 && delta(3) > -4.6e-4, ...
    'Expected the legacy D50 Z value to be about 0.000449 below registry.');

  % The weighting control isolates the table data while preserving the registry
  % representation: same legacy SPD, same legacy CMFs, same 10 nm measurement
  % grid, but a complete operator instead of direct coarse-grid summation.
  weighted_control_error = legacy_weighted10_xyz - legacy_xyz;
  assert(max(abs(weighted_control_error)) < 1e-12, ...
    'Expected the legacy-derived 10 nm weighting operator to preserve white.');

  % The direct-grid control is retained because it demonstrates why the weighted
  % control is necessary. Decimating the products is not equivalent to deriving
  % a weighting operator, and its effect is larger than the gap with the opposite
  % sign.
  grid_effect = legacy10_xyz(3) - legacy_xyz(3);
  assert(grid_effect < 0.0 && abs(grid_effect) > abs(delta(3)), ...
    'Expected the 5 nm -> 10 nm grid effect to exceed the legacy/registry gap.');

  legacy_error = legacy_xyz - canonical_xyz;
  registry_error = registry_xyz - canonical_xyz;
  legacy_pcs_error = legacy_xyz - icc_pcs_xyz;
  registry_pcs_error = registry_xyz - icc_pcs_xyz;

  % The reference-dependence, pinned in both directions. Against CIE 15 the
  % registry table wins; against the ICC PCS illuminant the legacy path wins.
  % Asserting only the first would read as "registry is more accurate", which
  % the second half of this pair shows is not a statement the data supports on
  % its own.
  % These two are deliberately a tripwire for a CHANGE THAT IS EXPECTED: CIE
  % TC1-102 has revised weighting tables pending, and #1475 records that the ICC
  % registry will be regenerated once they publish. When that lands, the margin
  % here (2.21e-4 versus 2.28e-4 against the PCS white) is small enough to flip.
  % That is the point -- it should not regenerate silently -- so the messages say
  % what to do rather than just reporting a failed comparison.
  closer_to_cie = abs(registry_error(3)) < abs(legacy_error(3));
  closer_to_icc_pcs = abs(legacy_pcs_error(3)) < abs(registry_pcs_error(3));
  assert(closer_to_cie, ...
    ['Registry D50 Z is no longer closer to CIE 15 than the legacy path. ' ...
     'If the registry weighting tables were regenerated, re-derive the ' ...
     'expected values above and revisit the #1475 framing in ' ...
     'docs/matlab-bindings.md before re-pinning.']);
  assert(closer_to_icc_pcs, ...
    ['Legacy D50 Z is no longer closer to the ICC PCS illuminant than the ' ...
     'registry table. Same follow-up as above: this pair records which ' ...
     'reference each path favours, so a flip is a decision to make, not a ' ...
     'number to update.']);

  result = struct( ...
    'legacy_xyz', legacy_xyz, ...
    'legacy_weighted10_xyz', legacy_weighted10_xyz, ...
    'legacy10_xyz', legacy10_xyz, ...
    'registry_xyz', registry_xyz, ...
    'registry_runtime_xyz', registry_runtime_xyz, ...
    'canonical_xyz', canonical_xyz, ...
    'icc_pcs_xyz', icc_pcs_xyz, ...
    'delta_xyz', delta, ...
    'weighted_control_error', weighted_control_error, ...
    'grid_effect', grid_effect, ...
    'legacy_error', legacy_error, ...
    'registry_error', registry_error, ...
    'legacy_pcs_error', legacy_pcs_error, ...
    'registry_pcs_error', registry_pcs_error, ...
    'closer_to_cie', closer_to_cie, ...
    'closer_to_icc_pcs', closer_to_icc_pcs);
end

% Builds the complete 10 nm weighting operator implied by linear reconstruction
% onto the checked-in 5 nm observer grid, then applies it to a perfect diffuser.
% This mirrors the basis-function construction in icComputeWeightingTable.
function xyz = linear_weighted_white(spd, x_bar, y_bar, z_bar)
  fine_count = numel(spd);
  assert(fine_count == 81 && mod(fine_count, 2) == 1);
  coarse_count = (fine_count + 1) / 2;
  basis = zeros(fine_count, coarse_count);

  for coarse = 1:coarse_count
    fine = 2 * coarse - 1;
    basis(fine, coarse) = 1.0;
  end
  for coarse = 1:(coarse_count - 1)
    fine = 2 * coarse;
    basis(fine, coarse) = 0.5;
    basis(fine, coarse + 1) = 0.5;
  end

  normalization = 1.0 / sum(y_bar .* spd);
  products = [
    (x_bar .* spd).'
    (y_bar .* spd).'
    (z_bar .* spd).'
  ];
  weights = normalization .* products * basis;
  xyz = weights * ones(coarse_count, 1);
end

% Reduces a perfect diffuser the way the legacy spectral path does: a plain
% rectangular sum of SPD times CMF, normalized by k = 1/sum(ybar*SPD) so that
% Y == 1 by construction. Mirrors CIccPcc::getEmissiveObserver and the
% icXYZCalcDirectSum branch of CIccColorimetricCalculator::Prepare.
function xyz = direct_sum(spd, x_bar, y_bar, z_bar)
  normalization = 1.0 / sum(y_bar .* spd);
  xyz = normalization .* [
    sum(x_bar .* spd)
    sum(y_bar .* spd)
    sum(z_bar .* spd)
  ];
end

function values = extract_values(source, pattern, expected_count)
  token = regexp(source, ['(?s)' pattern], 'tokens', 'once');
  if isempty(token)
    error('iccdev:colorimetrySourceMissing', ...
      'Unable to locate a required colorimetry table in the C++ source.');
  end

  block = regexprep(token{1}, '//[^\r\n]*', '');
  text_values = regexp(block, ...
    '[-+]?(?:[0-9]+\.[0-9]+|[0-9]+)(?:[eE][-+]?[0-9]+)?', ...
    'match');
  values = str2double(text_values(:));
  if numel(values) ~= expected_count || any(~isfinite(values))
    error('iccdev:colorimetrySourceMalformed', ...
      'Expected %d finite table values, found %d.', ...
      expected_count, numel(values));
  end
end
