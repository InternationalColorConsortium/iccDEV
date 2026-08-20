function result = check_colorimetry_issue_1475()
%CHECK_COLORIMETRY_ISSUE_1475 Compare legacy and registry D50 reductions.
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
% one of them requires holding the others fixed, so this check pins the two
% controls that can be computed here, and defers the third to the native CTest:
%
%   method - NOT the cause. iccdev.colorimetry-methods asserts
%            "same-grid: DirectSum == Weighting" and "== SpragueTo1nm" to
%            TOL_EXACT (1e-6), so on one grid with one data set the three
%            reduction methods agree far inside the 4.5e-4 gap. Driving the
%            calculator with the real D50 SPD and 1931 CMFs rather than that
%            test's synthetic data, the measured spread is 0.0. A weighting
%            table is not intrinsically closer to CIE than a direct sum.
%   grid   - NOT the cause either, and it does not even have the right sign.
%            Re-summing the SAME legacy data on the 10 nm subgrid moves Z by
%            about -6.2e-4, which is larger than the +4.5e-4 gap and points the
%            other way. Pinned below as grid_effect.
%   data   - what is left. The gap lives in the illuminant/observer data, not in
%            how it is integrated. That matches the #1475 finding that iccDEV's
%            5 nm D50 SPD (inherited in 889db62b, 2023-11-03) is the divergent
%            object, while all ten registry weighting tables match the published
%            registry values exactly.
%
% "Which one is right" is therefore a choice of reference, not a bug fix, and
% the answer flips depending on which white point is adopted -- see the two
% closer_to_* fields. Note also that this reads the C++ SOURCE TEXT as double.
% The library stores these tables as icFloatNumber (float, IccDefs.h:88) and
% computes in float, so the native CTest is authoritative for library behaviour;
% this check is an independent arithmetic model of the checked-in data.

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

  % The registry table is a complete operator on the CIE Y=100 scale: applied to
  % a perfect diffuser it degenerates to the column sums, so no illuminant or
  % observer is applied on top of it.
  registry_xyz = [
    sum(weights(1:41))
    sum(weights(42:82))
    sum(weights(83:123))
  ] ./ 100.0;

  % Grid control. Same SPD, same CMFs, same rectangular sum -- only every second
  % sample, giving the registry table's 380-780@10nm grid. The k = 1/sum(ybar*S)
  % normalization divides the sample spacing out, so this isolates the sampling
  % rate on its own.
  decimated = 1:2:81;
  legacy10_xyz = direct_sum(d50(decimated), x_bar(decimated), ...
    y_bar(decimated), z_bar(decimated));

  expected_legacy = [0.964245565670359; 1.0; 0.824679093854306];
  expected_registry = [0.9642408375; 1.0000000002; 0.8251281168];
  expected_legacy10 = [0.963956085519434; 1.0; 0.824057352615889];

  % Two references, deliberately both. CIE 15 is the external colorimetric truth;
  % icD50XYZ is the PCS illuminant iccDEV has always encoded (IccUtil.cpp) and is
  % what the native CTest anchors the legacy path to. They are 3.1e-4 apart in Z,
  % which is comparable to the effect under study.
  canonical_xyz = [0.96422; 1.0; 0.82521];      % CIE 15 D50/1931
  icc_pcs_xyz = [0.9642; 1.0; 0.8249];          % icD50XYZ, the ICC PCS illuminant

  assert(max(abs(legacy_xyz - expected_legacy)) < 1e-12, ...
    'Legacy D50/1931 calculation changed unexpectedly.');
  assert(max(abs(registry_xyz - expected_registry)) < 1e-10, ...
    'Registry D50/1931 weighting-table calculation changed unexpectedly.');
  assert(max(abs(legacy10_xyz - expected_legacy10)) < 1e-12, ...
    'Decimated 10 nm legacy calculation changed unexpectedly.');

  delta = legacy_xyz - registry_xyz;
  assert(delta(3) < -4.4e-4 && delta(3) > -4.6e-4, ...
    'Expected the legacy D50 Z value to be about 0.000449 below registry.');

  % The grid control, asserted rather than merely reported: halving the sampling
  % rate perturbs Z by MORE than the whole legacy-to-registry gap, and in the
  % opposite direction. If this ever stopped holding, the "sampling artifact"
  % explanation would be back on the table and this file's framing would need
  % revisiting.
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
    'legacy10_xyz', legacy10_xyz, ...
    'registry_xyz', registry_xyz, ...
    'canonical_xyz', canonical_xyz, ...
    'icc_pcs_xyz', icc_pcs_xyz, ...
    'delta_xyz', delta, ...
    'grid_effect', grid_effect, ...
    'legacy_error', legacy_error, ...
    'registry_error', registry_error, ...
    'legacy_pcs_error', legacy_pcs_error, ...
    'registry_pcs_error', registry_pcs_error, ...
    'closer_to_cie', closer_to_cie, ...
    'closer_to_icc_pcs', closer_to_icc_pcs);
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
