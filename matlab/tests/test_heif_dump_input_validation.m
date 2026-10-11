function test_heif_dump_input_validation()
%TEST_HEIF_DUMP_INPUT_VALIDATION Verify HEIF wrapper input rejection.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  error_id = '';
  try
    iccdev.heif_dump(fullfile(tempdir, 'missing-input.heic'));
  catch e
    error_id = e.identifier;
  end

  assert(strcmp(error_id, 'iccdev:heifInputNotFound'), ...
    'Missing HEIF input returned an unexpected error: %s', error_id);
end
