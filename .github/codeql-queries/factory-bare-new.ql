/**
 * @name Factory new without std::nothrow
 * @description Factory methods (CreateElement, CreateTag) that use bare `new`
 *              will throw std::bad_alloc on allocation failure rather than
 *              returning nullptr. Callers that check for nullptr get a false
 *              sense of safety. In profile parsing, malformed data can trigger
 *              large or numerous allocations. Use new(std::nothrow) so callers
 *              can detect and handle failure gracefully.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/factory-bare-new
 * @tags security
 *       correctness
 *       external/cwe/cwe-252
 *       external/cwe/cwe-755
 */

import cpp

/**
 * A new expression that does not use std::nothrow.
 */
class BareNewExpr extends NewExpr {
  BareNewExpr() {
    not exists(Expr arg |
      arg = this.getAllocatorCall().getAnArgument() and
      arg.getType().getName().matches("%nothrow%")
    )
  }
}

/**
 * A function that looks like a factory method (Create*, make*, clone).
 */
class FactoryFunction extends Function {
  FactoryFunction() {
    this.getName().matches("Create%") or
    this.getName().matches("create%") or
    this.getName().matches("Make%") or
    this.getName().matches("Clone%")
  }
}

/**
 * A function in a Json factory file.
 */
class JsonFactoryFunction extends Function {
  JsonFactoryFunction() {
    this.getFile().getRelativePath().matches("%JsonFactory%") or
    this.getFile().getRelativePath().matches("%IccIoJson%")
  }
}

/**
 * A call whose enclosing try statement has a catch-all handler. This is the
 * repository's established boundary for factories that intentionally throw on
 * allocation failure.
 */
predicate callProtectedByCatchAll(FunctionCall call) {
  exists(TryStmt tryStatement |
    tryStatement.getStmt().getAChild*() = call and
    tryStatement.getACatchClause() instanceof CatchAnyBlock
  )
}

/**
 * A factory for which every statically resolved call is protected by a
 * catch-all handler. Requiring at least one call keeps unreferenced factories
 * visible for review.
 */
predicate allKnownCallsCatchAllocationFailure(Function factory) {
  exists(FunctionCall call | call.getTarget() = factory) and
  forall(FunctionCall call |
    call.getTarget() = factory
  |
    callProtectedByCatchAll(call)
  )
}

from BareNewExpr newExpr
where
  (
    newExpr.getEnclosingFunction() instanceof FactoryFunction or
    newExpr.getEnclosingFunction() instanceof JsonFactoryFunction
  ) and
  not allKnownCallsCatchAllocationFailure(newExpr.getEnclosingFunction()) and
  newExpr.getFile().getRelativePath().matches("%Icc%")
select newExpr,
  "Factory method '" + newExpr.getEnclosingFunction().getName() +
  "' uses bare 'new' which throws std::bad_alloc on failure. " +
  "Callers checking for nullptr will not catch the exception. " +
  "Use new(std::nothrow) for graceful failure handling (CWE-252, CWE-755)."
