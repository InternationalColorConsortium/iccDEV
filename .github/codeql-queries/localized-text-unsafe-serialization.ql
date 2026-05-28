/**
 * @name Localized ICC text serialized without a safe fallback
 * @description Localized ICC text is profile-controlled UTF-16 data. XML and
 *              JSON serializers should route converted UTF-8 through helpers
 *              that preserve unsafe control bytes and invalid text as hex
 *              instead of emitting raw CDATA or JSON strings.
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id iccdev/localized-text-unsafe-serialization
 * @tags security
 *       correctness
 *       external/cwe/cwe-116
 *       external/cwe/cwe-20
 */

import cpp

predicate isLocalizedSerializationFunction(Function f) {
  (
    f.getName().matches("%ToXml%") or
    f.getName().matches("%ToJson%") or
    f.getName() = "dictLocalizedToJson"
  ) and
  (
    f.getFile().getRelativePath().matches("%IccXML/IccLibXML/IccTagXml.cpp") or
    f.getFile().getRelativePath().matches("%IccJSON/IccLibJSON/IccTagJson.cpp")
  ) and
  exists(FunctionCall convert |
    convert.getEnclosingFunction() = f and
    convert.getTarget().getName() = "icUtf16ToUtf8"
  ) and
  exists(Expr e |
    e.getEnclosingFunction() = f and
    e.toString().regexpMatch(".*(LocalizedText|LocalizedName|LocalizedValue|localizedStrings).*")
  )
}

predicate usesLocalizedTextSafeSink(Function f) {
  exists(FunctionCall safe |
    safe.getEnclosingFunction() = f and
    (
      safe.getTarget().getName() = "icXmlDumpLocalizedText" or
      safe.getTarget().getName() = "icJsonSetLocalizedText"
    )
  )
}

from Function f
where
  isLocalizedSerializationFunction(f) and
  not usesLocalizedTextSafeSink(f)
select f,
  "Localized ICC text converted with icUtf16ToUtf8() is serialized without a " +
  "safe localized text sink. Route output through icXmlDumpLocalizedText() or " +
  "icJsonSetLocalizedText() so control bytes and invalid text are preserved as " +
  "hex instead of raw XML/JSON text."
