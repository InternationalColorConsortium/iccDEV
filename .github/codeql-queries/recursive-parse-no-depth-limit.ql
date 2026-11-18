/**
 * @name Recursive parse or composite validation lacks a depth guard
 * @description Parse/import functions that call themselves, and composite
 *              Validate methods that dispatch to nested Validate methods,
 *              need an explicit depth or visited-set guard. Untrusted input
 *              can otherwise drive stack exhaustion (CWE-674). This includes
 *              the XML import shape from #3021 and nested array validation
 *              from #3086.
 * @kind problem
 * @problem.severity warning
 * @security-severity 6.5
 * @precision medium
 * @id iccdev/recursive-parse-no-depth-limit
 * @tags security
 *       external/cwe/cwe-674
 *       reliability
 */

import cpp

private predicate isProductSource(File file) {
  file.getRelativePath().regexpMatch("(IccProfLib|IccXML|IccJSON|IccConnect)/.*") or
  file.getBaseName() = "case.cpp"
}

/** A function whose name suggests it processes untrusted nested input. */
predicate isParseLike(MemberFunction f) {
  f.getName().regexpMatch("(?i)(ParseXml|ParseTag|ParseImport|Read|Apply|ApplySequence|Flatten|SetElem|Iterate|Validate)") and
  f.fromSource()
}

/** A self-recursive call: f calls itself directly. */
predicate selfRecursive(MemberFunction f) {
  exists(FunctionCall c |
    c.getEnclosingFunction() = f and
    c.getTarget() = f
  )
}

/**
 * A composite Validate method that dispatches to a nested object's Validate.
 * The static target can be a base-class virtual method, so direct self-call
 * matching alone misses the nested tag-array path from #3086.
 */
predicate nestedCompositeValidation(MemberFunction f) {
  f.getName() = "Validate" and
  f.getDeclaringType().getName().regexpMatch(
    "CIcc(TagArray|TagStruct|ArrayUnknown|StructUnknown)"
  ) and
  exists(FunctionCall c |
    c.getEnclosingFunction() = f and
    c.getTarget().getName() = "Validate" and
    c.getTarget() != f
  )
}

/**
 * Heuristic: the function (or its class) carries a depth/level counter
 * indicating an explicit guard exists.
 */
predicate hasDepthGuard(MemberFunction f) {
  exists(Variable v |
    (v.getName().regexpMatch("(?i).*(depth|level|recurs|nest|stack|visit).*")) and
    (
      v.getDeclaringType() = f.getDeclaringType() or
      v.(LocalVariable).getFunction() = f or
      f.getAParameter() = v
    )
  )
  or
  // Defensive limit constants used inside the function body.
  exists(MacroInvocation mi |
    mi.getEnclosingFunction() = f and
    mi.getMacroName().regexpMatch("(?i).*(MAX|LIMIT).*(DEPTH|RECURS|NEST|LEVEL).*")
  )
}

from MemberFunction f
where
  isParseLike(f) and
  (selfRecursive(f) or nestedCompositeValidation(f)) and
  not hasDepthGuard(f) and
  isProductSource(f.getFile())
select f,
  "Nested '" + f.getDeclaringType().getName() + "::" + f.getName() +
  "' processing has no visible depth or visited-set guard. Untrusted nested input may exhaust the stack."
