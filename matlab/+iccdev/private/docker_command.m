function command = docker_command(arguments)
%DOCKER_COMMAND Quote validated arguments for a Docker shell command.
%
% Copyright (c) International Color Consortium.
% BSD 3-Clause License. See LICENSE.md for details.

  quoted = cell(size(arguments));
  for i = 1:numel(arguments)
    argument = char(arguments{i});
    if ispc()
      if ~isempty(regexp(argument, '["%%&|<>^!\r\n]', 'once'))
        error('iccdev:unsafeDockerArgument', ...
          'Docker argument contains unsupported shell characters.');
      end
      quoted{i} = ['"' argument '"'];
    else
      if ~isempty(regexp(argument, '[''\r\n]', 'once'))
        error('iccdev:unsafeDockerArgument', ...
          'Docker argument contains unsupported shell characters.');
      end
      quoted{i} = ['''' argument ''''];
    end
  end
  command = strjoin(quoted, ' ');
end
