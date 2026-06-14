/*
    File:       cmmsearch-namedcolor-ownership.cpp

    Contains:   CTest helper for CIccCmmSearch::AddXform profile ownership
                (issue #1332).

    Post-#1327 every CIccCmm::AddXform overload owns the profile it is given:
    it stores the profile on success or deletes it on error, so the filename and
    reference overloads no longer free it themselves.  CIccCmmSearch overrides
    AddXform(CIccProfile*) and rejects a NamedColor profile (it has no search
    LUT); that rejection must also delete the profile to honor the contract.

    This helper feeds a NamedColor profile through the inherited
    AddXform(const char*) overload, which opens the profile internally and hands
    it to the override.  Because the helper never holds the resulting pointer, a
    profile that the override failed to free is unreachable and reported by the
    at-exit LeakSanitizer scan (the iccDEV ASan CI job).  The status check also
    guards that the NamedColor rejection path is the one exercised.

    Usage:
      cmmsearch-namedcolor-ownership <namedcolor_profile.icc>

    Exit codes:
      0 - profile rejected as expected and (under ASan at exit) not leaked
      1 - unexpected status
      2 - usage error
      (non-zero is also forced by LeakSanitizer at exit if the profile leaks)
*/

#include "IccCmm.h"
#include "IccCmmSearch.h"

#include <cstdio>

// Contained in its own function so the inherited AddXform(const char*) overload
// is the only thing that ever holds a pointer to the opened profile; nothing in
// main() can act as a LeakSanitizer root for it.
static icStatusCMM add_named_color(const char* profilePath)
{
  CIccCmmSearch cmm;
  // The override hides the inherited filename overload, so qualify it explicitly
  // (as CIccConnectCmm::CreateSearch does).  It opens the profile and dispatches
  // back to CIccCmmSearch::AddXform(CIccProfile*) via virtual dispatch.
  return cmm.CIccCmm::AddXform(profilePath);
}

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <namedcolor_profile.icc>\n",
                 argv[0] ? argv[0] : "cmmsearch-namedcolor-ownership");
    return 2;
  }

  icStatusCMM rv = add_named_color(argv[1]);

  // The search CMM has no LUT for a NamedColor profile, so it must reject it.
  if (rv != icCmmStatInvalidLut) {
    std::fprintf(stderr,
      "cmmsearch-namedcolor-ownership: expected InvalidLut (%d), got %d (%s)\n",
      (int)icCmmStatInvalidLut, (int)rv, CIccCmm::GetStatusText(rv));
    return 1;
  }

  std::fprintf(stdout, "cmmsearch-namedcolor-ownership: status=%d (%s)\n",
               (int)rv, CIccCmm::GetStatusText(rv));
  // Returns to the runtime; under ASan the at-exit LeakSanitizer check fails the
  // process if the override left the NamedColor profile unowned (#1332).
  return 0;
}
