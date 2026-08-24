function values = bounded_grid(channels, grid_size)
%BOUNDED_GRID Build a device grid within the PAWG quality sample budget.
%
% Copyright (c) 2026 International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  max_samples = 2000000;
  if ~isscalar(channels) || ~isfinite(channels) || channels ~= fix(channels) || ...
      channels < 1 || channels > 16
    error('iccdev:pawgQ1InvalidChannels', ...
      'Channel count must be an integer between 1 and 16.');
  end
  if ~isscalar(grid_size) || ~isfinite(grid_size) || ...
      grid_size ~= fix(grid_size) || grid_size < 2
    error('iccdev:pawgQ1InvalidGridSize', ...
      'Grid size must be an integer of at least 2.');
  end

  count = 1;
  for i = 1:channels
    if count > floor(max_samples / grid_size)
      error('iccdev:pawgQ1SampleBudgetExceeded', ...
        'The %d-channel %d-point grid exceeds the %d-sample quality budget.', ...
        channels, grid_size, max_samples);
    end
    count = count * grid_size;
  end

  values = zeros(count, channels, 'single');
  index = (0:count - 1).';
  for channel = channels:-1:1
    values(:, channel) = single(mod(index, grid_size) / (grid_size - 1));
    index = floor(index / grid_size);
  end
end
