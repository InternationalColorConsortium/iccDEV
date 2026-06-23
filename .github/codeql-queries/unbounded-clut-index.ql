/**
 * @name Unbounded index into CLUT/LUT sample data
 * @description A CmdLine tool reads a buffer returned by CIccCLUT/CIccMBB::GetData()
 *              with a computed index but never bounds that index against the CLUT's
 *              actual sample count (NumPoints() x channels). A malformed or
 *              non-square CLUT can drive the index past the end of the sample array,
 *              giving an out-of-bounds read (CWE-125). This is the pattern behind the
 *              iccProfilePlot buildClutRaster() heap-buffer-overflow (issue #1548):
 *              the tile-packing geometry was derived from the grid arrangement, not
 *              from the real array length.
 * @kind problem
 * @problem.severity warning
 * @security-severity 6.5
 * @precision medium
 * @id iccdev/unbounded-clut-index
 * @tags security
 *       correctness
 *       external/cwe/cwe-125
 */

import cpp

// Scope to the CmdLine tools.  The IccProfLib core interpolation paths
// (CIccCLUT::Interp*, MPE elements) also index GetData() results, but they are
// bounded by their own input-clamping invariants and are intentionally out of
// scope here -- this query targets the tool-side report/render code where the
// index is reconstructed from profile geometry (the #1548 class).
private predicate isToolSource(File f) {
  f.getRelativePath().regexpMatch("Tools/.*") and
  not f.getRelativePath().regexpMatch("(?i).*(^|/)(test|tests|Testing)(/|$).*")
}

// A `…GetData(…)` call whose receiver is a CLUT / multi-dimensional-LUT object,
// i.e. the flat icFloatNumber sample array a CLUT raster walk indexes into.
private predicate isClutGetDataCall(FunctionCall call) {
  call.getTarget().getName() = "GetData" and
  exists(string t | t = call.getTarget().getDeclaringType().getName() |
    t.regexpMatch("(?i)CIcc.*(CLUT|MBB)")
  )
}

// A LOCAL pointer initialised from such a call -- e.g. `clutData = clut->GetData(0)`.
// Requiring a local (not a member) keeps this to the in-function "fetch then index"
// shape of #1548 and excludes the member-pointer caches used elsewhere.
private predicate isClutDataLocal(LocalVariable v) {
  exists(FunctionCall call |
    isClutGetDataCall(call) and
    call = v.getInitializer().getExpr().getAChild*()
  )
}

// Names that denote the CLUT's own sample/node length -- a comparison or
// min()/clamp against one of these is what actually bounds a CLUT index. Kept
// deliberately CLUT-specific (NumPoints/NumOffset/SampleCount/node counts) rather
// than the broad *Size/*Count families: an unrelated `nSize`/`vector.size()`
// comparison elsewhere in a large function must NOT be credited as a bound on the
// CLUT array (that loose form let the original iccProfileVisualize index slip
// through). The #1548 fix's `clutSampleCount` (= NumPoints() x channels) matches.
bindingset[name]
private predicate isLengthName(string name) {
  name.regexpMatch(
    "(?i)^(.*NumPoints.*|.*NumOffset.*|.*SampleCount.*|.*ClutSize.*|" +
    ".*ClutSampleCount.*|.*nNodes.*|.*MaxNodes.*|.*NodeCount.*)$"
  )
}

// True when an earlier statement in the same function bounds something against a
// length-named symbol: either a comparison (`idx > clutSampleCount`,
// `i < NumPoints()*nout`) or a std::min/clamp, referencing a count/size name.
// This is what the #1548 fix added (`in + outputChannels > clutSampleCount`), and
// what already-correct loop-bounded indexing (`for (...; i < NumPoints()*n; ...)`)
// satisfies -- so both clear the alert.
private predicate hasLengthBoundBefore(ArrayExpr access) {
  exists(Expr guard |
    guard.getEnclosingFunction() = access.getEnclosingFunction() and
    guard.getLocation().getStartLine() < access.getLocation().getStartLine() and
    (
      guard instanceof ComparisonOperation
      or
      guard.(FunctionCall).getTarget().getName().regexpMatch("(?i)^(min|clamp|clip)$")
    ) and
    exists(Expr ref |
      ref = guard.getAChild*() and
      (
        isLengthName(ref.(VariableAccess).getTarget().getName())
        or
        isLengthName(ref.(FunctionCall).getTarget().getName())
      )
    )
  )
}

from ArrayExpr ae, LocalVariable v
where
  ae.getArrayBase().(VariableAccess).getTarget() = v and
  isClutDataLocal(v) and
  not hasLengthBoundBefore(ae) and
  isToolSource(ae.getFile())
select ae,
  "Index into CLUT sample buffer '" + v.getName() +
  "' (from GetData()) without a prior bound check against the CLUT element count " +
  "(e.g. NumPoints() x channels). A malformed/non-square CLUT can read past the " +
  "array end; add an explicit length guard before this access (CWE-125, see #1548)."
