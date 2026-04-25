/**
 * @name Recursive Parse/Read/Apply call lacks depth guard
 * @description Functions named ParseXml, Read, Apply, ApplySequence,
 *              Flatten, SetElem on Calculator/MPE/Tag classes recursively
 *              invoke themselves (directly or via mutual recursion) without
 *              an explicit depth or visited-set guard. Untrusted input can
 *              drive unbounded recursion → stack-exhaustion crash (CWE-674).
 *              See PRs #413, #406, #684, #cce5f9b, #798be59.
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

/** A function whose name suggests it processes untrusted nested input. */
predicate isParseLike(MemberFunction f) {
  f.getName().regexpMatch("(?i)(ParseXml|ParseTag|Read|Apply|ApplySequence|Flatten|SetElem|Iterate)") and
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
  selfRecursive(f) and
  not hasDepthGuard(f) and
  f.getFile().getRelativePath().regexpMatch("(IccProfLib|IccXML|IccJSON)/.*")
select f,
  "Recursive '" + f.getDeclaringType().getName() + "::" + f.getName() +
  "' has no visible depth or visited-set guard. Untrusted nested input may exhaust the stack."
