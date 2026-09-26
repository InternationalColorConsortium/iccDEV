function details = docker_resolve_image(image, pull_requested)
%DOCKER_RESOLVE_IMAGE Resolve an official image selector for execution.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  image = docker_validate_image(image);
  is_digest = ~isempty(regexp(image, '@sha256:[A-Fa-f0-9]{64}$', 'once'));
  [available, availability_details] = iccdev.docker_available(image);

  % Mutable tags must be refreshed before automated QA. An immutable digest
  % needs a pull only when it is not already present (hosted CI pre-pulls it).
  if pull_requested && (~is_digest || ~available)
    pull_command = docker_command({'docker', 'pull', image});
    [pull_status, pull_output] = system(pull_command);
    if pull_status ~= 0
      error('iccdev:dockerPullFailed', ...
        'Unable to pull %s:\n%s', image, pull_output);
    end
    [available, availability_details] = iccdev.docker_available(image);
  end
  if ~available
    error('iccdev:dockerUnavailable', ...
      'Docker or image %s is unavailable:\n%s', image, availability_details);
  end

  if is_digest
    resolved_image = image;
  else
    digest_command = docker_command({ ...
      'docker', 'image', 'inspect', image, ...
      '--format={{index .RepoDigests 0}}'});
    [digest_status, digest_output] = system(digest_command);
    resolved_image = strtrim(digest_output);
    if digest_status ~= 0 || isempty(resolved_image) || ...
        strcmp(resolved_image, '<no value>')
      error('iccdev:dockerDigestUnavailable', ...
        ['Unable to resolve %s to an immutable repository digest. ' ...
         'Pull the published image and try again.\n%s'], image, digest_output);
    end
    resolved_image = docker_validate_image(resolved_image);
  end

  [resolved_available, image_id] = iccdev.docker_available(resolved_image);
  if ~resolved_available
    error('iccdev:dockerDigestUnavailable', ...
      'Resolved Docker image %s is unavailable:\n%s', ...
      resolved_image, image_id);
  end

  revision_command = docker_command({ ...
    'docker', 'image', 'inspect', resolved_image, ...
    ['--format={{index .Config.Labels ' ...
     '`org.opencontainers.image.revision`}}']});
  [revision_status, revision_output] = system(revision_command);
  source_revision = strtrim(revision_output);
  if revision_status ~= 0 || isempty(source_revision) || ...
      strcmp(source_revision, '<no value>')
    error('iccdev:dockerRevisionUnavailable', ...
      ['Docker image %s does not record the required ' ...
       'org.opencontainers.image.revision label.\n%s'], ...
      resolved_image, revision_output);
  end

  details = struct( ...
    'selector', image, ...
    'resolvedImage', resolved_image, ...
    'imageId', strtrim(image_id), ...
    'sourceRevision', source_revision);
end
