function test_heif_dump_native(build_dir)
%TEST_HEIF_DUMP_NATIVE Exercise the native HEIF tool through MATLAB.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  this_dir = fileparts(mfilename('fullpath'));
  repo_root = fileparts(fileparts(this_dir));
  corpus = fullfile(repo_root, '.github', 'ci', 'tooling', 'heif', 'img');

  report = iccdev.heif_dump(fullfile(corpus, 'valid-prof.heic'), ...
    'BuildDir', build_dir);
  assert(~isempty(strfind(report, 'item=1 property=1')));
  assert(~isempty(strfind(report, 'profile_bytes=540')));
  assert(~isempty(strfind(report, 'proflib_parse=ok')));
  assert(~isempty(strfind(report, 'icc_profiles=1')));

  empty_report = iccdev.heif_dump(fullfile(corpus, 'nclx-only.heif'), ...
    'BuildDir', build_dir);
  assert(~isempty(strfind(empty_report, 'icc_profiles=0')));

  failed = false;
  try
    iccdev.heif_dump(fullfile(corpus, 'invalid-ipma-index.heic'), ...
      'BuildDir', build_dir);
  catch e
    failed = strcmp(e.identifier, 'iccdev:heifCommandFailed') && ...
      ~isempty(strfind(e.message, 'unable to read HEIF colour properties'));
  end
  assert(failed, 'Malformed HEIF input must retain the native error.');
end
