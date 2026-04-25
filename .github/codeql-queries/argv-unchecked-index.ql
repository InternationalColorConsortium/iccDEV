/**
 * @name CmdLine tool indexes argv without bound check
 * @description Tools under Tools/CmdLine/ frequently access argv[N] from
 *              ParseXxx / main without first verifying argc>N, leading to
 *              out-of-bounds reads when invoked with too few arguments
 *              (CWE-125, CWE-787). Pattern recurs: PR #688 (printing
 *              filename), #688 (#50816df), #464 (iccV5DspObsToV4Dsp),
 *              #09ab6be argument-count test.
 * @kind problem
 * @problem.severity warning
 * @security-severity 6.0
 * @precision medium
 * @id iccdev/argv-unchecked-index
 * @tags security
 *       external/cwe/cwe-125
 *       external/cwe/cwe-787
 *       reliability
 */

import cpp

/** A `main`-style argv parameter (char** or char*[] named argv). */
predicate isArgv(Parameter p) {
  p.getName() = "argv" and
  p.getType().getUnspecifiedType() instanceof PointerType
}

/** Function on the argc/argv path (main, or any function taking argc+argv). */
predicate takesArgcArgv(Function f) {
  exists(Parameter argc, Parameter argv |
    f.getAParameter() = argc and
    f.getAParameter() = argv and
    argc.getName() = "argc" and
    isArgv(argv)
  )
}

/**
 * An array access of the form `argv[idx]` where idx is an integer literal
 * or simple variable, inside a function that took argc+argv.
 */
predicate argvAccess(ArrayExpr ae, Function f, Expr idx) {
  ae.getArrayBase().(VariableAccess).getTarget().(Parameter).getName() = "argv" and
  ae.getEnclosingFunction() = f and
  takesArgcArgv(f) and
  idx = ae.getArrayOffset()
}

/**
 * The function (or its enclosing if-branch) guards on argc > k for some k
 * before the access. Approximated by: a comparison expression involving
 * the parameter named "argc" exists earlier in the same function.
 */
predicate hasArgcGuard(Function f, ArrayExpr access) {
  exists(ComparisonOperation cmp |
    cmp.getEnclosingFunction() = f and
    cmp.getAnOperand().(VariableAccess).getTarget().(Parameter).getName() = "argc" and
    cmp.getLocation().getStartLine() < access.getLocation().getStartLine()
  )
  or
  // assert/return-on-bad-argc earlier
  exists(MacroInvocation mi |
    mi.getEnclosingFunction() = f and
    mi.getMacroName().regexpMatch("(?i)assert|require|check") and
    mi.getLocation().getStartLine() < access.getLocation().getStartLine()
  )
}

from ArrayExpr ae, Function f, Expr idx
where
  argvAccess(ae, f, idx) and
  not hasArgcGuard(f, ae) and
  // exclude argv[0] (program name, always valid)
  not idx.getValue().toInt() = 0 and
  // restrict to Tools/ to avoid noise from examples
  f.getFile().getRelativePath().matches("Tools/%")
select ae,
  "Access argv[" + idx.toString() + "] in '" + f.getName() +
  "' without a prior comparison against argc. Add `if (argc <= N) usage();` before this read."
