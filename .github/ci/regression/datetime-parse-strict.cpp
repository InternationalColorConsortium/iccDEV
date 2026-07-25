// Regression for #1831: icGetDateTimeValue must not narrow an out-of-range field
// into the icUInt16Number members of icDateTimeNumber.
//
// The parser was a bare sscanf("%u-%02u-%02uT%02u:%02u:%02u") whose return value was
// discarded. Three things went wrong, all reachable from iccFromXml/iccFromJson on
// attacker-supplied input:
//
//   1. The conversion count was ignored, so a partially-matching string left the
//      remaining fields at whatever they already held.
//   2. "%u" accepts a leading '-' and negates the result into a large unsigned
//      value, so "-1" parsed as 4294967295.
//   3. Each field was assigned into an icUInt16Number before any range check, so an
//      out-of-range value was silently truncated. UBSan reported exactly this for
//      the year: "implicit conversion from ... 590359881 ... changed the value to
//      11593" (IccUtil.cpp, #1831).
//
// The parser is now explicit: every field must be a run of decimal digits that fits
// in icUInt16Number, every separator must be present, and the whole string must be
// consumed apart from surrounding whitespace. A malformed string yields an all-zero
// icDateTimeNumber -- the conventional ICC "date unknown" value, which
// CIccInfo::CheckData already reports on, so a bad date still surfaces to the user
// instead of becoming a plausible-looking wrong one.
//
// Two properties matter beyond "rejects garbage", and both are pinned here:
//
//   * Structurally valid but calendar-nonsensical dates are still passed through
//     (month 59, day 0) so CheckData can name the offending field. The repository's
//     own #1343 fuzz fixture contains 59881-59-00T00:00:00, and the existing
//     iccdev-issue-1342-1343 regression asserts that file still round-trips, so
//     over-tightening here would break it.
//   * The all-zero sentinel 0-00-00T00:00:00 must keep parsing as all zeros.
//
// Red-green: reverting to the sscanf form makes the truncation cases fail (year
// 590359881 arrives as 11593 rather than 0, and "-1" arrives as 65535).
//
// Returns 0 on success; the number of failed assertions otherwise (each printed).

#include "IccUtil.h"
#include "IccDefs.h"

#include <cstdio>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

int g_fail = 0;

void check(bool ok, const char *what)
{
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr, "[datetime-parse] FAIL: %s\n", what);
  }
}

bool isZero(const icDateTimeNumber &d)
{
  return !d.year && !d.month && !d.day && !d.hours && !d.minutes && !d.seconds;
}

// Compare against expected field values; prints the mismatch so a failure says what
// was actually parsed rather than just which case failed.
void checkFields(const char *str, unsigned y, unsigned mo, unsigned d,
                 unsigned h, unsigned mi, unsigned s, const char *what)
{
  icDateTimeNumber v = icGetDateTimeValue(str);
  bool ok = v.year == y && v.month == mo && v.day == d &&
            v.hours == h && v.minutes == mi && v.seconds == s;
  if (!ok) {
    ++g_fail;
    std::fprintf(stderr,
                 "[datetime-parse] FAIL: %s -- \"%s\" gave %u-%02u-%02uT%02u:%02u:%02u,"
                 " expected %u-%02u-%02uT%02u:%02u:%02u\n",
                 what, str, v.year, v.month, v.day, v.hours, v.minutes, v.seconds,
                 y, mo, d, h, mi, s);
  }
}

} // namespace

int main()
{
  // --- well-formed input is unchanged -----------------------------------------

  checkFields("2026-07-25T12:34:56", 2026, 7, 25, 12, 34, 56, "canonical date parses");
  checkFields("0-00-00T00:00:00", 0, 0, 0, 0, 0, 0, "all-zero sentinel parses as zero");

  // The #1343 fuzz fixture. Structurally valid and every field fits icUInt16Number,
  // so it must survive intact for CheckData to flag month 59 -- see the file header.
  checkFields("59881-59-00T00:00:00", 59881, 59, 0, 0, 0, 0,
              "#1343 fixture (5-digit year, month 59) passes through");

  // Boundary: 65535 is representable, so it is accepted rather than zeroed.
  checkFields("65535-12-31T23:59:59", 65535, 12, 31, 23, 59, 59,
              "icUInt16Number-max year accepted");

  // Writers emit text nodes that can carry surrounding whitespace.
  checkFields("  2026-07-25T12:34:56  ", 2026, 7, 25, 12, 34, 56,
              "surrounding whitespace tolerated");

  // A bare ISO-8601 date is a complete instant (midnight that day) and is what the
  // old sscanf already produced for this input, so it must keep working -- these are
  // authoring tools and a hand-written date without a time is reasonable input.
  checkFields("2026-07-25", 2026, 7, 25, 0, 0, 0,
              "date without a time means midnight that day");

  // --- the #1831 truncation cases ---------------------------------------------

  // The exact value from the UBSan report: it must not arrive as 11593.
  icDateTimeNumber v = icGetDateTimeValue("590359881-07-25T00:00:00");
  check(v.year != 11593, "year 590359881 must not be truncated to 11593 (#1831)");
  check(isZero(v), "out-of-range year yields the all-zero date (#1831)");

  // "%u" negated this into 4294967295, which truncated to 65535.
  v = icGetDateTimeValue("-1-07-25T00:00:00");
  check(v.year != 65535, "negative year must not become 65535 (#1831)");
  check(isZero(v), "negative year yields the all-zero date (#1831)");

  // Just past the icUInt16Number bound.
  check(isZero(icGetDateTimeValue("65536-01-01T00:00:00")),
        "year 65536 is rejected rather than wrapping to 0");

  // --- structural rejection ----------------------------------------------------
  // The discarded sscanf return count meant these left fields partly assigned.

  check(isZero(icGetDateTimeValue("")), "empty string is rejected");
  check(isZero(icGetDateTimeValue("garbage")), "non-numeric string is rejected");
  check(isZero(icGetDateTimeValue("2026/07/25T00:00:00")), "wrong separators rejected");
  check(isZero(icGetDateTimeValue("2026-07-25T00:00:00trailing")),
        "trailing garbage is rejected");

  // A 'T' promises a time, so an incomplete one after it is rejected rather than
  // zero-filled -- the old sscanf turned this into 12:34:00.
  check(isZero(icGetDateTimeValue("2026-07-25T12:34")),
        "incomplete time after 'T' is rejected");

  // Reduced-precision dates are not accepted: a zero day is not a calendar date,
  // and the old code turned these into dates that never existed (2026-07-00).
  check(isZero(icGetDateTimeValue("2026-07")), "year-month without a day rejected");
  check(isZero(icGetDateTimeValue("2026")), "year alone rejected");
  check(isZero(icGetDateTimeValue(NULL)), "NULL is rejected without dereferencing");

  // "now" is the documented alias for the current local time; it must still produce
  // a populated date rather than being caught by the stricter parse.
  v = icGetDateTimeValue("now");
  check(v.year >= 2020 && v.month >= 1 && v.month <= 12,
        "\"now\" still resolves to the current date");

  if (g_fail)
    std::fprintf(stderr, "[datetime-parse] %d assertion(s) failed\n", g_fail);
  else
    std::fprintf(stdout, "[datetime-parse] all assertions passed\n");

  return g_fail;
}
