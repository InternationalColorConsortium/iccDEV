/**
 * @name Counted buffer guard permits the one-past-end index
 * @description An index parameter is rejected only when it is greater than a
 *              size/count member, then is used to index an array or form a
 *              pointer. An index equal to the count is already one past the
 *              valid range and can cause an out-of-bounds access.
 * @kind problem
 * @problem.severity error
 * @security-severity 8.8
 * @precision high
 * @id iccdev/counted-buffer-off-by-one
 * @tags security
 *       correctness
 *       external/cwe/cwe-125
 *       external/cwe/cwe-787
 */

import cpp

private predicate isProductSource(File f) {
  (
    f.getRelativePath().regexpMatch("(IccProfLib|IccXML|IccJSON|IccConnect|Tools|examples)/.*") and
    not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
  )
  or
  f.getBaseName() = "case.cpp"
}

private predicate isIndexParameter(Parameter index) {
  index.getName().regexpMatch("(?i)^(n|i)?(index|idx|offset|position)$")
}

private predicate isCountMember(Variable countMember) {
  countMember instanceof Field and
  countMember.getName().regexpMatch("(?i)^m_.*(size|count|channels|entries|elements|matrices)$")
}

private predicate isDangerousUseAfter(Function f, Parameter index,
                                      ComparisonOperation check, Expr use) {
  use.getEnclosingFunction() = f and
  use.getLocation().getStartLine() > check.getLocation().getStartLine() and
  (
    exists(ArrayExpr access |
      use = access and
      access.getArrayOffset().getAChild*() = index.getAnAccess()
    )
    or
    exists(PointerAddExpr add |
      use = add and
      add.getAChild*() = index.getAnAccess()
    )
  )
}

from Function f, Parameter index, Variable countMember, ComparisonOperation check, Expr use
where
  isIndexParameter(index) and
  isCountMember(countMember) and
  index.getFunction() = f and
  check.getEnclosingFunction() = f and
  check.getOperator() = ">" and
  check.getLeftOperand().getAChild*() = index.getAnAccess() and
  check.getRightOperand().getAChild*() = countMember.getAnAccess() and
  not check.getRightOperand().getAChild*() instanceof BinaryArithmeticOperation and
  isDangerousUseAfter(f, index, check, use) and
  isProductSource(check.getFile())
select check,
  "Guard '" + check.toString() + "' permits index == " + countMember.getName() +
  ", but that value is one past the counted buffer used on line " +
  use.getLocation().getStartLine().toString() + ". Reject equality with >= " +
  "before the access (CWE-125/CWE-787)."
