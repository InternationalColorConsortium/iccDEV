/**
 * @name JSON .get<T>() without type guard
 * @description nlohmann::json .get<T>() throws nlohmann::json::type_error
 *              when the JSON value does not match the requested type. In ICC
 *              profile parsing, untrusted JSON input can contain any type.
 *              Calls to .get<T>() should be preceded by .is_number(),
 *              .is_string(), .is_array(), etc., or wrapped in try-catch.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/json-unchecked-type-access
 * @tags security
 *       correctness
 *       external/cwe/cwe-704
 *       external/cwe/cwe-20
 */

import cpp

/**
 * A call to nlohmann::json::get<T>() template method.
 */
class JsonGetCall extends FunctionCall {
  JsonGetCall() {
    this.getTarget().getName().matches("get<%") or
    (this.getTarget().getName() = "get" and
     this.getTarget().getDeclaringType().getName().matches("%json%"))
  }
}

/**
 * A call to a nlohmann::json type-check method (is_number, is_string, etc.).
 */
class JsonTypeCheck extends FunctionCall {
  JsonTypeCheck() {
    exists(string name | name = this.getTarget().getName() |
      name = "is_number" or
      name = "is_number_integer" or
      name = "is_number_unsigned" or
      name = "is_number_float" or
      name = "is_string" or
      name = "is_boolean" or
      name = "is_array" or
      name = "is_object" or
      name = "is_null" or
      name = "contains" or
      name = "count"
    )
  }
}

/**
 * Holds when the get call is inside a try block.
 */
predicate isInTryBlock(JsonGetCall getCall) {
  exists(TryStmt tryStmt |
    tryStmt.getStmt().getAChild*() = getCall.getEnclosingStmt()
  )
}

/**
 * Holds when a type check guards the same variable before the get call.
 */
predicate hasTypeGuard(JsonGetCall getCall) {
  exists(JsonTypeCheck check, IfStmt ifStmt |
    ifStmt.getCondition().getAChild*() = check and
    ifStmt.getThen().getAChild*() = getCall.getEnclosingStmt()
  )
}

from JsonGetCall getCall
where
  not isInTryBlock(getCall) and
  not hasTypeGuard(getCall) and
  (
    getCall.getFile().getRelativePath().matches("%IccJSON%") or
    getCall.getFile().getRelativePath().matches("%IccConnect%")
  )
select getCall,
  ".get<T>() called without a prior type guard (.is_number(), .is_string(), etc.) " +
  "or try-catch. Untrusted JSON input may cause nlohmann::json::type_error " +
  "exception (CWE-704, CWE-20)."
