function results = run_gamma_qa()
%RUN_GAMMA_QA Verify curveType u8Fixed8 gamma decoding for issue #815.
%
% Decodes the raw u8Fixed8Number out of the checked-in fixture without going
% through IccProfLib, so the arithmetic is an independent second opinion rather
% than a restatement of the library. The reconstruction is modelled in single
% precision because that is what IccProfLib stores; see the note at the bound
% below and issue #2044. The native iccdev.curve-gamma-u8fixed8 CTest is
% authoritative for what the compiled library actually prints and emits.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  matlab_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(matlab_dir);
  profile_path = fullfile(repo_root, '.github', 'ci', 'regression', ...
    'gamma-2.20703125.icc');
  expected_raw = 565;
  expected_gamma = 2.20703125;
  tag_signatures = {'rTRC', 'gTRC', 'bTRC'};

  bytes = read_profile_bytes(profile_path);
  results = struct('signature', {}, 'raw', {}, 'normalized', {}, ...
    'stored', {}, 'gamma', {}, 'reconstructed', {}, 'reconstructionError', {});

  % IccProfLib keeps the normalized value in an icFloatNumber, which is a 32-bit
  % float, so the reconstruction it performs is bounded by that type's rounding
  % and not by double precision. Modelling it in double would make the check
  % below pass with an error of exactly zero no matter what the library did, so
  % the storage step has to be spelled out. The bound is gamma * 2^-24: the
  % relative error of rounding raw/65535 into a 24-bit significand is at most
  % 2^-24, and multiplying by the exact ratio 65535/256 carries it straight
  % through. eps('single') is 2^-23, hence the halving.
  reconstruction_bound = expected_gamma * eps('single') / 2;

  fprintf('=== iccdev MATLAB QA: curveType Gamma ===\n\n');
  fprintf('Profile: %s\n', profile_path);

  for i = 1:numel(tag_signatures)
    raw = read_curve_gamma(bytes, tag_signatures{i});
    normalized = double(raw) / 65535.0;
    gamma = double(raw) / 256.0;
    stored = single(normalized);
    reconstructed = double(stored) * 65535.0 / 256.0;
    reconstruction_error = abs(reconstructed - expected_gamma);

    assert(raw == expected_raw, ...
      '%s raw u8Fixed8 value should be %d, got %d.', ...
      tag_signatures{i}, expected_raw, raw);

    % The encoded value is exact: raw/256 is a dyadic rational and 565/256 is
    % representable with no error at all, so this one is an equality.
    assert(gamma == expected_gamma, ...
      '%s gamma should be %.8f, got %.17g.', ...
      tag_signatures{i}, expected_gamma, gamma);

    assert(reconstruction_error <= reconstruction_bound, ...
      ['%s single-precision reconstruction should be within %.3g of %.8f, ' ...
       'got %.17g (error %.3g).'], ...
      tag_signatures{i}, reconstruction_bound, expected_gamma, ...
      reconstructed, reconstruction_error);

    % What iccFromJson relies on: however the reconstructed decimal is printed,
    % it must still land back on the same u8Fixed8 integer.
    assert(round(reconstructed * 256.0) == raw, ...
      '%s reconstruction should round-trip to raw u8Fixed8 value %d.', ...
      tag_signatures{i}, raw);
    assert(round(gamma * 256.0) == raw, ...
      '%s gamma should round-trip to raw u8Fixed8 value %d.', ...
      tag_signatures{i}, raw);

    results(i).signature = tag_signatures{i};
    results(i).raw = raw;
    results(i).normalized = normalized;
    results(i).stored = stored;
    results(i).gamma = gamma;
    results(i).reconstructed = reconstructed;
    results(i).reconstructionError = reconstruction_error;

    fprintf('  %s: raw=%d normalized=%.15f gamma=%.8f\n', ...
      tag_signatures{i}, raw, normalized, gamma);
    fprintf('        single-precision reconstruction=%.12f error=%.3g ', ...
      reconstructed, reconstruction_error);
    fprintf('(bound %.3g)\n', reconstruction_bound);
  end

  fprintf('\nPASS: 565 / 256 = 2.20703125 exactly, and the single-precision\n');
  fprintf('      reconstruction stays within %.3g and rounds back to 565.\n', ...
    reconstruction_bound);
end

function bytes = read_profile_bytes(profile_path)
  file_id = fopen(profile_path, 'rb');
  if file_id < 0
    error('iccdev:gammaProfileMissing', ...
      'Unable to open gamma regression profile: %s', profile_path);
  end
  cleanup = onCleanup(@() fclose(file_id));
  bytes = fread(file_id, Inf, '*uint8');
  clear cleanup;

  if numel(bytes) < 132
    error('iccdev:gammaProfileTruncated', ...
      'Gamma regression profile is too short: %d bytes.', numel(bytes));
  end
end

function raw = read_curve_gamma(bytes, tag_signature)
  tag_count = read_u32_be(bytes, 129);
  table_end = 132 + double(tag_count) * 12;
  if table_end > numel(bytes)
    error('iccdev:gammaTagTableTruncated', ...
      'ICC tag table exceeds the profile size.');
  end

  for i = 0:double(tag_count) - 1
    entry = 133 + i * 12;
    signature = char(bytes(entry:entry + 3).');
    if strcmp(signature, tag_signature)
      tag_offset = read_u32_be(bytes, entry + 4);
      tag_size = read_u32_be(bytes, entry + 8);
      data_start = double(tag_offset) + 1;
      data_end = double(tag_offset) + double(tag_size);

      if tag_size < 14 || data_start < 1 || data_end > numel(bytes)
        error('iccdev:gammaTagTruncated', ...
          '%s curveType data exceeds the profile size.', tag_signature);
      end
      if ~strcmp(char(bytes(data_start:data_start + 3).'), 'curv')
        error('iccdev:gammaTagType', ...
          '%s is not encoded as curveType.', tag_signature);
      end
      if read_u32_be(bytes, data_start + 8) ~= 1
        error('iccdev:gammaTagCount', ...
          '%s curveType must contain one gamma value.', tag_signature);
      end

      raw = read_u16_be(bytes, data_start + 12);
      return;
    end
  end

  error('iccdev:gammaTagMissing', ...
    'Gamma regression profile does not contain %s.', tag_signature);
end

function value = read_u16_be(bytes, index)
  value = double(bytes(index)) * 256.0 + double(bytes(index + 1));
end

function value = read_u32_be(bytes, index)
  value = double(bytes(index)) * 16777216.0 + ...
    double(bytes(index + 1)) * 65536.0 + ...
    double(bytes(index + 2)) * 256.0 + ...
    double(bytes(index + 3));
end
