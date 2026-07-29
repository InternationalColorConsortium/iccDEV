/**
 * @name XML-controlled integer narrowed without bounds check
 * @description XML attributes parsed through integer conversion helpers and
 *              then stored in narrow ICC integer fields can wrap before later
 *              validation. Malformed XML can turn values such as 360200 into
 *              truncated icUInt16Number channel or spectral counts.
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id iccdev/xml-controlled-narrowing
 * @tags security
 *       correctness
 *       external/cwe/cwe-190
 *       external/cwe/cwe-681
 */

import cpp

private predicate isIccXmlSource(File f) {
  f.getRelativePath().regexpMatch(".*IccXML/IccLibXML/.*") and
  not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
}

private predicate isNarrowIccInteger(Type t) {
  t.getUnspecifiedType().(IntegralType).getSize() <= 2
  or
  t.getUnspecifiedType().getName().regexpMatch(
    "(?i).*(icUInt8Number|icUInt16Number|icInt8Number|icInt16Number|uint8_t|uint16_t).*"
  )
}

private predicate isXmlAttributeInteger(Expr e) {
  exists(FunctionCall parseCall, FunctionCall attrCall |
    (parseCall = e or parseCall = e.getAChild*()) and
    parseCall.getTarget().getName().regexpMatch("(?i)^(atoi|atol|atoll|strtol|strtoul|strtoll|strtoull)$") and
    attrCall = parseCall.getAChild*() and
    attrCall.getTarget().getName() = "icXmlAttrValue"
  )
}

private predicate isHighSignalChannelOrStepAttrCall(FunctionCall attrCall) {
  attrCall.getTarget().getName() = "icXmlAttrValue" and
  exists(Expr attrName |
    attrName = attrCall.getArgument(1) and
    attrName.toString().regexpMatch("(?i)^(InputChannels|OutputChannels|steps)$")
  )
}

private predicate isHighSignalChannelOrStepSource(Expr e) {
  isXmlAttributeInteger(e) and
  exists(FunctionCall attrCall |
    attrCall = e.getAChild*() and
    isHighSignalChannelOrStepAttrCall(attrCall)
  )
}

private predicate hasExplicitNarrowingCast(Expr e) {
  exists(CStyleCast cast |
    (cast = e or cast = e.getAChild*()) and
    isNarrowIccInteger(cast.getType()) and
    isXmlAttributeInteger(cast.getExpr())
  )
  or
  e.toString().regexpMatch("^\\(.*\\).*")
}

private predicate mentionsXmlAttribute(Expr e) {
  exists(FunctionCall attrCall |
    (attrCall = e or attrCall = e.getAChild*()) and
    attrCall.getTarget().getName() = "icXmlAttrValue"
  )
}

private predicate hasNearbyExplicitU16Parse(Expr sink) {
  exists(FunctionCall parser |
    parser.getEnclosingFunction() = sink.getEnclosingFunction() and
    parser.getTarget().getName().regexpMatch("(?i)^icXmlParseU(8|16)$") and
    parser.getLocation().getStartLine() <= sink.getLocation().getStartLine() and
    parser.getLocation().getStartLine() >= sink.getLocation().getStartLine() - 4 and
    mentionsXmlAttribute(parser)
  )
}

private predicate isChannelSizingCall(FunctionCall call) {
  call.getTarget().getName().regexpMatch("(?i)^(SetSize|SetChannels|SetRange)$")
}

private predicate inKnownFailClosedParser(Expr sink) {
  exists(Function f |
    f = sink.getEnclosingFunction() and
    f.getDeclaringType().getName() = "CIccMpeXmlCurveSet"
  )
}

from Expr sink, Expr source, string kind
where
  isIccXmlSource(sink.getFile()) and
  (
    exists(AssignExpr assign |
      sink = assign and
      source = assign.getRValue() and
      isNarrowIccInteger(assign.getLValue().getType()) and
      kind = "assignment"
    )
    or
    exists(Variable v |
      sink = v.getInitializer().getExpr() and
      source = v.getInitializer().getExpr() and
      isNarrowIccInteger(v.getType()) and
      kind = "initializer"
    )
    or
    exists(FunctionCall call, int i |
      sink = call and
      source = call.getArgument(i) and
      isChannelSizingCall(call) and
      isXmlAttributeInteger(source) and
      kind = "channel-size call"
    )
  ) and
  isHighSignalChannelOrStepSource(source) and
  not hasExplicitNarrowingCast(source) and
  not hasNearbyExplicitU16Parse(sink) and
  not inKnownFailClosedParser(sink)
select sink,
  "XML attribute integer reaches a narrow ICC integer through this " + kind +
  " without an explicit range-checked parser. Use icXmlParseU8/icXmlParseU16 " +
  "or check the value before storing it in a narrow field (CWE-190, CWE-681)."
