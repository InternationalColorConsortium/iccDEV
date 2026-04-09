/**
 * @name World-writable file creation
 * @description fopen() used without setting restrictive permissions. On Unix
 *              systems, files created by ICC tools processing untrusted profiles
 *              should not be world-writable. Use umask or fchmod after creation.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/world-writable-output
 * @tags security
 *       external/cwe/cwe-732
 */

import cpp

from FunctionCall fopen
where
  fopen.getTarget().getName() = "fopen" and
  // Writing mode
  exists(StringLiteral mode |
    mode = fopen.getArgument(1) and
    mode.getValue().matches("%w%")
  ) and
  // In tool source files
  fopen.getFile().getRelativePath().matches("%Tools/%") and
  // No umask or chmod nearby
  not exists(FunctionCall perm |
    perm.getTarget().getName() in ["umask", "fchmod", "chmod"] and
    perm.getLocation().getFile() = fopen.getLocation().getFile() and
    perm.getLocation().getStartLine() >= fopen.getLocation().getStartLine() - 5 and
    perm.getLocation().getStartLine() <= fopen.getLocation().getStartLine() + 5
  )
select fopen,
  "File opened for writing without explicit permission control. " +
  "Output files from untrusted profile processing may be world-writable (CWE-732)."
