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

/**
 * A `main`-style argv parameter (char** or char*[] named argv).
 *
 * Both spellings have to be accepted, as this comment has always said.  A
 * parameter written `char* argv[]` decays to a pointer at the call, but the
 * extractor keeps the *declared* type, so its unspecified type is an
 * `ArrayType` -- `char *[]` -- and never a `PointerType`.  Testing only for
 * `PointerType` therefore matched `char** argv` alone.  Measured on a database
 * of this tree, 19 functions under Tools/ take an (argc, argv) pair and 17 of
 * them spell argv as an array, so `takesArgcArgv` was false for those 17 and
 * the query reported clean over almost its entire stated scope
 * (code-scanning alert 2366).
 */
predicate isArgv(Parameter p) {
  p.getName() = "argv" and
  exists(Type t |
    t = p.getType().getUnspecifiedType() and
    (t instanceof PointerType or t instanceof ArrayType)
  )
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
    // The guard and the access sit on the *same* line in the short-circuit
    // idiom every CmdLine tool spells its help screen with --
    //   if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    // -- where `argv[1]` is read only because `argc == 2` already held.  A
    // strict `<` on lines could not see that guard and reported the read twice,
    // once per `argv[1]`: that is code-scanning alerts 2366 and 2367.
    //
    // Same line is not on its own enough, though.  Relaxing this to `<=` also
    // swallows the case the line test exists to catch,
    //   if (!strcmp(argv[1], "-x") && argc > 2)
    // where argv[1] is read *before* argc is compared and is out of bounds at
    // argc == 1.  So on a shared line, compare columns: the comparison has to
    // start left of the access, which the help idiom satisfies and this one
    // does not.
    //
    // Columns are still only an ordering approximation.  A guard written
    // entirely on one line but inverted relative to the access -- `if (argc > 5)
    // { ... } else { ... argv[3] ... }` -- reads left to right and is missed.
    // `GuardCondition.controls()` is the sound way to do this and would replace
    // the whole predicate; that is a much larger change than these alerts
    // need.
    (
      cmp.getLocation().getStartLine() < access.getLocation().getStartLine()
      or
      cmp.getLocation().getStartLine() = access.getLocation().getStartLine() and
      cmp.getLocation().getStartColumn() < access.getLocation().getStartColumn()
    )
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
