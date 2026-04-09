/**
 * @name snprintf buffer size mismatch
 * @description snprintf calls where the size argument does not match the
 *              actual buffer size, or where the format string could produce
 *              output exceeding the buffer. Common in ICC profile dump tools
 *              that format tag names, offsets, and sizes into fixed buffers.
 * @kind problem
 * @problem.severity warning
 * @precision high
 * @id iccdev/snprintf-size-mismatch
 * @tags security
 *       correctness
 *       external/cwe/cwe-120
 */

import cpp

from FunctionCall snpf, Variable buf
where
  snpf.getTarget().getName() = "snprintf" and
  // First argument is a stack buffer
  buf.getAnAccess() = snpf.getArgument(0) and
  buf.getType() instanceof ArrayType and
  // The size argument is a literal that does not match the array size
  exists(Literal sizeLit |
    sizeLit = snpf.getArgument(1) and
    sizeLit.getValue().toInt() != buf.getType().(ArrayType).getArraySize() and
    // Exclude cases where size comes from a named constant (likely correct)
    not exists(Variable sizeVar |
      sizeVar.getAnAccess() = snpf.getArgument(1)
    )
  )
select snpf,
  "snprintf size argument (" + snpf.getArgument(1).toString() +
  ") does not match buffer size (" +
  buf.getType().(ArrayType).getArraySize().toString() +
  "). This can lead to truncation or buffer overflow (CWE-120)."
