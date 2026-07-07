/*
    File:       xform-create-tag-ownership.cpp

    Contains:   CTest helper for CIccXform::Create(CIccProfile*, CIccTag*, ...)
                profile ownership (issue #1308).

    Post-#1327 every CIccCmm::AddXform overload owns the profile it is given: it
    stores the profile on success or frees it on failure.  AddXform(CIccProfile*,
    CIccTag*) forwards the profile to the tag-based CIccXform::Create overload and
    relies on Create to free it when no xform can be built (the "CIccXform::Create
    has already deleted the profile" comment at the AddXform call site).  Before
    #1308 that tag-based Create returned NULL on its failure paths WITHOUT freeing
    the profile, so the owned profile leaked.

    This helper hands a valid profile plus a NULL transform tag to that overload.
    A NULL tag makes Create take its "unsupported element" failure path and return
    NULL, which must now free the profile.  The profile is opened and passed inside
    a helper function, so once AddXform takes ownership nothing in main() holds a
    pointer to it; a profile Create failed to free is therefore unreachable and is
    reported by the at-exit LeakSanitizer scan (the iccDEV ASan CI job).  The
    status check guards that the intended Create-failure path was exercised.

    Usage:
      xform-create-tag-ownership <profile.icc>

    Exit codes:
      0 - Create failed as expected and (under ASan at exit) the profile was freed
      1 - unexpected status
      2 - usage / profile-open error
      (non-zero is also forced by LeakSanitizer at exit if the profile leaks)
*/

#include "IccCmm.h"
#include "IccProfile.h"

#include <cstdio>

// Kept in its own function so AddXform is the only thing that ever owns the
// opened profile; nothing in main() can act as a LeakSanitizer root for it.
static icStatusCMM add_profile_with_null_tag(const char* profilePath, bool& opened)
{
  CIccProfile* pProfile = OpenIccProfile(profilePath);
  opened = (pProfile != NULL);
  if (!pProfile)
    return icCmmStatBad;

  // Hand the owned profile to the tag-based AddXform overload with a NULL tag.
  // Create cannot build an xform for a NULL tag, so it returns NULL and -- post
  // #1308 -- frees the profile we just gave it.  The pointer is not retained
  // here, so a leaked profile becomes unreachable for LeakSanitizer.
  CIccCmm cmm;
  return cmm.AddXform(pProfile, (CIccTag*)NULL);
}

int main(int argc, char** argv)
{
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <profile.icc>\n",
                 argv[0] ? argv[0] : "xform-create-tag-ownership");
    return 2;
  }

  bool opened = false;
  icStatusCMM rv = add_profile_with_null_tag(argv[1], opened);

  if (!opened) {
    std::fprintf(stderr, "xform-create-tag-ownership: could not open '%s'\n", argv[1]);
    return 2;
  }

  // A NULL tag yields no xform, so AddXform must report BadXform.
  if (rv != icCmmStatBadXform) {
    std::fprintf(stderr,
      "xform-create-tag-ownership: expected BadXform (%d), got %d (%s)\n",
      (int)icCmmStatBadXform, (int)rv, CIccCmm::GetStatusText(rv));
    return 1;
  }

  std::fprintf(stdout, "xform-create-tag-ownership: status=%d (%s)\n",
               (int)rv, CIccCmm::GetStatusText(rv));
  // Returns to the runtime; under ASan the at-exit LeakSanitizer check fails the
  // process if the tag-based Create left the owned profile unfreed (#1308).
  return 0;
}
