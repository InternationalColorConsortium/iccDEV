function test_json_bindings()
%TEST_JSON_BINDINGS Test MATLAB IccJSON conversion helpers.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  profiles = find_test_profiles();
  for i = 1:size(profiles, 1)
    exercise_round_trip(profiles{i, 1}, profiles{i, 2});
  end

  failed = false;
  try
    iccdev.from_json('{"IccProfile":');
  catch e
    failed = strcmp(e.identifier, 'iccdev:jsonCommandFailed') && ...
      ~isempty(regexp(e.message, 'Unable to Parse', 'once'));
  end
  assert(failed, ['Malformed JSON must fail through iccdev:jsonCommandFailed ' ...
    'with the native parser diagnostic.']);
end

function exercise_round_trip(profile_path, expected_major)
  source_bytes = read_binary_file(profile_path);
  json_text = iccdev.to_json(profile_path);
  assert(~isempty(json_text), 'iccdev.to_json returned no JSON text.');
  assert(~isempty(regexp(json_text, '"IccProfile"', 'once')), ...
    'iccdev.to_json did not return an IccProfile document.');

  profile_bytes = iccdev.from_json(json_text);
  assert(isequal(profile_bytes(:), source_bytes(:)), ...
    'IccJSON round trip changed the checked-in ICC.%d profile bytes.', ...
    expected_major - 3);
  assert_icc_profile_structure(profile_bytes, expected_major);

  json_path = [tempname '.json'];
  write_utf8(json_path, json_text);
  cleanup = onCleanup(@() delete_if_exists(json_path));
  profile_from_path = iccdev.from_json(json_path);
  assert(isequal(profile_from_path, profile_bytes), ...
    'JSON text and JSON path conversion returned different ICC bytes.');
  clear cleanup;
end

function profiles = find_test_profiles()
  this_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(fileparts(this_dir));
  profiles = {
    fullfile(repo_root, 'Testing', 'sRGB_v4_ICC_preference.icc'), 4
    fullfile(repo_root, 'Testing', 'ApplyDataFiles', 'test-profiles', ...
      'sRGB_D65_MAT.icc'), 5
  };
  for i = 1:size(profiles, 1)
    assert(exist(profiles{i, 1}, 'file') == 2, ...
      'Required ICC.%d JSON fixture is missing: %s', ...
      profiles{i, 2} - 3, profiles{i, 1});
  end
end

function assert_icc_profile_structure(profile_bytes, expected_major)
  profile_bytes = uint8(profile_bytes(:));
  assert(numel(profile_bytes) >= 132, ...
    'ICC profile must contain a 128-byte header and tag count.');

  profile_size = read_u32_be(profile_bytes, 0);
  assert(profile_size == numel(profile_bytes), ...
    'ICC profile size field does not match the returned byte count.');
  assert(isequal(profile_bytes(37:40).', uint8('acsp')), ...
    'ICC profile file signature is not acsp.');

  major = double(profile_bytes(9));
  minor_bugfix = double(profile_bytes(10));
  assert(major == expected_major, ...
    'Expected ICC major version %d, found %d.', expected_major, major);
  assert(bitshift(minor_bugfix, -4) <= 9 && bitand(minor_bugfix, 15) <= 9, ...
    'ICC profile minor or bug-fix version is not BCD encoded.');

  if major == 4
    assert(all(profile_bytes(11:12) == 0), ...
      'ICC.1 profile version reserved bytes must be zero.');
    assert(all(profile_bytes(101:128) == 0), ...
      'ICC.1 profile header reserved bytes must be zero.');
  elseif major == 5
    assert(all(profile_bytes(125:128) == 0), ...
      'ICC.2 profile header reserved bytes must be zero.');
    if all(profile_bytes(121:124) == 0)
      assert(all(profile_bytes(11:12) == 0), ...
        'ICC.2 profile without a sub-class must use a zero sub-version.');
    end
  else
    error('iccdev:jsonUnexpectedProfileVersion', ...
      'JSON specification QA supports ICC major versions 4 and 5.');
  end

  date_values = zeros(1, 6);
  for i = 1:6
    date_values(i) = read_u16_be(profile_bytes, 24 + 2 * (i - 1));
  end
  assert(date_values(2) >= 1 && date_values(2) <= 12, ...
    'ICC profile creation month is outside 1..12.');
  assert(date_values(3) >= 1 && date_values(3) <= 31, ...
    'ICC profile creation day is outside 1..31.');
  assert(date_values(3) <= days_in_month(date_values(1), date_values(2)), ...
    'ICC profile creation date is not a valid calendar date.');
  assert(date_values(4) <= 23 && date_values(5) <= 59 && ...
    date_values(6) <= 59, 'ICC profile creation time is invalid.');

  tag_count = read_u32_be(profile_bytes, 128);
  assert(tag_count <= floor((profile_size - 132) / 12), ...
    'ICC tag count exceeds the profile bounds.');
  table_end = 132 + 12 * tag_count;
  signatures = cell(tag_count, 1);
  offsets = zeros(tag_count, 1);
  sizes = zeros(tag_count, 1);
  for i = 1:tag_count
    entry_offset = 132 + 12 * (i - 1);
    signature_bytes = profile_bytes(entry_offset + (1:4));
    signatures{i} = sprintf('%02X', signature_bytes);
    offsets(i) = read_u32_be(profile_bytes, entry_offset + 4);
    sizes(i) = read_u32_be(profile_bytes, entry_offset + 8);
    assert(mod(offsets(i), 4) == 0, ...
      'ICC tag data offset is not aligned to a four-byte boundary.');
    assert(offsets(i) >= table_end && sizes(i) <= profile_size - offsets(i), ...
      'ICC tag data range exceeds the profile bounds.');
  end
  assert(numel(unique(signatures)) == tag_count, ...
    'ICC tag table contains duplicate tag signatures.');

  for i = 1:tag_count
    for j = (i + 1):tag_count
      if offsets(i) == offsets(j)
        assert(sizes(i) == sizes(j), ...
          'Shared ICC tag offsets must use the same data size.');
      else
        assert(offsets(i) + sizes(i) <= offsets(j) || ...
          offsets(j) + sizes(j) <= offsets(i), ...
          'ICC tag data elements partially overlap.');
      end
    end
  end

  [unique_offsets, first_indices] = unique(offsets);
  unique_sizes = sizes(first_indices);
  assert(~isempty(unique_offsets) && unique_offsets(1) == table_end, ...
    'The first ICC tag data element must immediately follow the tag table.');
  for i = 1:numel(unique_offsets)
    data_end = unique_offsets(i) + unique_sizes(i);
    padded_end = align4(data_end);
    if data_end < padded_end
      assert(all(profile_bytes((data_end + 1):padded_end) == 0), ...
        'ICC tag data padding bytes must be zero.');
    end
    if i < numel(unique_offsets)
      assert(unique_offsets(i + 1) == padded_end, ...
        'ICC tag data elements must form a contiguous padded sequence.');
    else
      assert(padded_end == profile_size, ...
        'ICC profile size must include the final tag padding.');
    end
  end

  stored_id = profile_bytes(85:100);
  if any(stored_id ~= 0)
    digest_bytes = profile_bytes;
    digest_bytes(45:48) = 0;
    digest_bytes(65:68) = 0;
    digest_bytes(85:100) = 0;
    digest = javaMethod('getInstance', ...
      'java.security.MessageDigest', 'MD5');
    digest.update(typecast(digest_bytes, 'int8'));
    calculated_id = typecast(digest.digest(), 'uint8');
    assert(isequal(stored_id(:), calculated_id(:)), ...
      'ICC profile ID does not match the required MD5 calculation.');
  end
end

function value = read_u32_be(bytes, offset)
  value = sum(double(bytes(offset + (1:4))) .* ...
    [16777216; 65536; 256; 1]);
end

function value = read_u16_be(bytes, offset)
  value = sum(double(bytes(offset + (1:2))) .* [256; 1]);
end

function value = align4(value)
  value = 4 * ceil(value / 4);
end

function count = days_in_month(year, month)
  counts = [31 28 31 30 31 30 31 31 30 31 30 31];
  count = counts(month);
  if month == 2 && (mod(year, 400) == 0 || ...
      (mod(year, 4) == 0 && mod(year, 100) ~= 0))
    count = 29;
  end
end

function bytes = read_binary_file(path)
  file_id = fopen(path, 'rb');
  assert(file_id >= 0, 'Unable to open ICC fixture: %s', path);
  cleanup = onCleanup(@() fclose(file_id));
  bytes = fread(file_id, Inf, '*uint8');
  clear cleanup;
end

function write_utf8(path, text)
  file_id = fopen(path, 'wb');
  assert(file_id >= 0, 'Unable to create JSON fixture: %s', path);
  cleanup = onCleanup(@() fclose(file_id));
  fwrite(file_id, unicode2native(text, 'UTF-8'), 'uint8');
  clear cleanup;
end

function delete_if_exists(path)
  if exist(path, 'file') == 2
    delete(path);
  end
end
