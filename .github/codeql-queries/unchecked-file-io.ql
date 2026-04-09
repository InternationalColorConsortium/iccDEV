/**
 * @name Unchecked file I/O return value
 * @description fopen, fseek, fread, fwrite return values must be checked when
 *              processing untrusted ICC profiles. A failed open returns NULL;
 *              failed seek/read can silently produce garbage data.
 * @kind problem
 * @problem.severity warning
 * @precision high
 * @id iccdev/unchecked-file-io
 * @tags security
 *       correctness
 *       external/cwe/cwe-252
 */

import cpp

/**
 * Calls to C stdio functions whose return value must be checked.
 */
class FileIOCall extends FunctionCall {
  FileIOCall() {
    this.getTarget().getName() in [
      "fopen", "fseek", "fread", "fwrite", "fgets", "fgetc",
      "ftell", "fclose", "fflush"
    ]
  }
}

from FileIOCall call
where
  // The return value is discarded (the call is a standalone expression statement)
  exists(ExprStmt es | es.getExpr() = call) and
  call.getTarget().getName() != "fclose" and
  call.getTarget().getName() != "fflush" and
  // Exclude cases in test code
  not call.getFile().getRelativePath().matches("%test%")
select call,
  "Return value of " + call.getTarget().getName() +
  "() is not checked. This can lead to silent data corruption when processing malformed profiles (CWE-252)."
