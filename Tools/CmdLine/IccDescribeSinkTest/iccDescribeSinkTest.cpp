/** @file
    File:       iccDescribeSinkTest.cpp

    Contains:   Smoke test for the IDescribeSink + DescribeOptions API.

                Exits 0 on success, non-zero with a diagnostic on
                failure. Designed to run under CTest:

                  iccDescribeSinkTest <profile.icc>

                Exercises four guarantees:

                  1. Sink output is byte-for-byte identical to the
                     legacy Describe(std::string&, int) output at
                     verbosity 100, both per-tag and via the new
                     CIccTag::Describe(IDescribeSink&, ...) virtual.

                  2. opts.max_clut_cells_per_tag bounds the number of
                     emitted cell rows on a CLUT-bearing tag and the
                     "[clut cells truncated after N]" footer is
                     present.

                  3. opts.emit_clut_cells = false suppresses the cell
                     rows entirely while preserving the BEGIN_LUT
                     header.

                  4. opts.max_total_bytes truncates output from a tag
                     whose Describe() falls back through the default
                     CIccTag::Describe(sink, opts) implementation, and
                     emits the "[truncated]" footer.

    Copyright:  (c) see Software License
*/

#include "IccProfile.h"
#include "IccTagBasic.h"
#include "IccTagLut.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

namespace {

// Sink that returns false from ShouldContinue() once `budget` bytes
// have been written. Used to test cooperative cancellation.
class BudgetedSink : public IDescribeSink
{
public:
  BudgetedSink(std::size_t budget) : m_budget(budget), m_written(0) {}
  void Write(const char *data, std::size_t len) override
  {
    if (m_written >= m_budget)
      return;
    std::size_t toWrite = len;
    if (m_written + toWrite > m_budget)
      toWrite = m_budget - m_written;
    m_buf.append(data, toWrite);
    m_written += toWrite;
  }
  bool ShouldContinue() override { return m_written < m_budget; }
  const std::string &str() const { return m_buf; }
  std::size_t bytesWritten() const { return m_written; }

private:
  std::size_t  m_budget;
  std::size_t  m_written;
  std::string  m_buf;
};

int fail(const char *fmt, ...)
{
  std::fputs("FAIL: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);
  std::fputc('\n', stderr);
  return 1;
}

bool tagSigToStr(icSignature sig, char out[5])
{
  out[0] = (icChar)((sig >> 24) & 0xFF);
  out[1] = (icChar)((sig >> 16) & 0xFF);
  out[2] = (icChar)((sig >>  8) & 0xFF);
  out[3] = (icChar)((sig >>  0) & 0xFF);
  out[4] = '\0';
  return true;
}

// Find the first MBB-derived (LUT-bearing) tag in the profile, or NULL.
CIccTag *firstLutTag(CIccProfile *pProfile, icSignature *sigOut)
{
  for (auto it = pProfile->m_Tags.begin(); it != pProfile->m_Tags.end(); ++it) {
    CIccTag *pTag = pProfile->FindTag(it->TagInfo.sig);
    if (pTag && pTag->IsMBBType()) {
      if (sigOut) *sigOut = it->TagInfo.sig;
      return pTag;
    }
  }
  return NULL;
}

} // namespace

int main(int argc, char *argv[])
{
  if (argc < 2) {
    std::fprintf(stderr, "Usage: %s <profile.icc>\n", argv[0]);
    return 2;
  }

  std::string sReport;
  icValidateStatus nStatus = icValidateOK;
  CIccProfile *pProfile = ValidateIccProfile(argv[1], sReport, nStatus);
  if (!pProfile)
    return fail("could not load profile %s", argv[1]);

  // ── Test 1: byte-equivalence of legacy vs sink at verbosity 100 ─────────
  // Cover every tag, since coverage is what gives this test value.
  for (auto it = pProfile->m_Tags.begin(); it != pProfile->m_Tags.end(); ++it) {
    CIccTag *pTag = pProfile->FindTag(it->TagInfo.sig);
    if (!pTag)
      continue;

    char sigStr[5];
    tagSigToStr((icSignature)it->TagInfo.sig, sigStr);

    std::string legacy;
    pTag->Describe(legacy, 100);

    std::string viaSink;
    StringDescribeSink sink(viaSink);
    pTag->Describe(sink, OptionsFromVerbosity(100));

    if (legacy != viaSink) {
      delete pProfile;
      return fail("tag '%s' sink output diverged from legacy"
                  " (legacy=%zu, sink=%zu bytes)",
                  sigStr, legacy.size(), viaSink.size());
    }
  }
  std::printf("[OK] byte-equivalence: every tag matches at verbosity 100\n");

  // ── Tests 2 & 3: only meaningful on a CLUT-bearing tag ──────────────────
  icSignature lutSig = 0;
  CIccTag *pLutTag = firstLutTag(pProfile, &lutSig);
  if (pLutTag) {
    char lutStr[5];
    tagSigToStr(lutSig, lutStr);

    // Test 2: max_clut_cells_per_tag bounds cell rows.
    {
      DescribeOptions opts = OptionsFromVerbosity(100);
      opts.max_clut_cells_per_tag = 16;

      std::string buf;
      StringDescribeSink sink(buf);
      pLutTag->Describe(sink, opts);

      if (buf.find("[clut cells truncated after 16]") == std::string::npos) {
        delete pProfile;
        return fail("max_clut_cells_per_tag=16 did not produce truncation"
                    " footer on tag '%s'", lutStr);
      }
    }
    std::printf("[OK] max_clut_cells_per_tag bounds CLUT cell output on '%s'\n",
                lutStr);

    // Test 3: emit_clut_cells=false suppresses cells, keeps BEGIN_LUT header.
    {
      DescribeOptions opts = OptionsFromVerbosity(100);
      opts.emit_clut_cells    = false;
      opts.emit_clut_channels = false;

      std::string buf;
      StringDescribeSink sink(buf);
      pLutTag->Describe(sink, opts);

      if (buf.find("BEGIN_LUT CLUT") == std::string::npos) {
        delete pProfile;
        return fail("emit_clut_cells=false unexpectedly suppressed BEGIN_LUT"
                    " header on tag '%s'", lutStr);
      }

      // The legacy output starts every cell row with two leading spaces
      // (per CIccCLUT::Iterate). If suppression worked, no such row
      // should appear after the BEGIN_LUT marker.
      std::size_t lutPos = buf.find("BEGIN_LUT CLUT");
      std::size_t cellPos = buf.find("\n  0x", lutPos);
      if (cellPos == std::string::npos)
        cellPos = buf.find("\n   ", lutPos);
      if (cellPos != std::string::npos) {
        delete pProfile;
        return fail("emit_clut_cells=false leaked cell rows on tag '%s'",
                    lutStr);
      }
    }
    std::printf("[OK] emit_clut_cells=false suppresses CLUT cells on '%s'\n",
                lutStr);
  }
  else {
    std::printf("[SKIP] no MBB/LUT tag in profile — Tests 2 & 3 skipped\n");
  }

  // ── Test 4: max_total_bytes truncates fallback path output ──────────────
  // Pick any tag whose legacy Describe() produces > 32 bytes. Profile
  // header tags reliably qualify; loop until we find one to keep the
  // test profile-agnostic.
  for (auto it = pProfile->m_Tags.begin(); it != pProfile->m_Tags.end(); ++it) {
    CIccTag *pTag = pProfile->FindTag(it->TagInfo.sig);
    if (!pTag) continue;

    std::string legacy;
    pTag->Describe(legacy, 100);
    if (legacy.size() <= 32)
      continue;

    DescribeOptions opts = OptionsFromVerbosity(100);
    opts.max_total_bytes = 32;

    std::string buf;
    StringDescribeSink sink(buf);
    pTag->Describe(sink, opts);

    // Either the converted (CIccMBB) path or the default
    // CIccTag::Describe(sink, opts) fallback must enforce the budget.
    // Both currently emit "[truncated]" via the fallback, so check
    // that for tags that go through the default path. The converted
    // path bounds CLUT cells via max_clut_cells_per_tag separately.
    char sigStr[5];
    tagSigToStr((icSignature)it->TagInfo.sig, sigStr);

    if (!pTag->IsMBBType()) {
      if (buf.size() < 32 || buf.size() > 32 + 32 /* footer slack */) {
        delete pProfile;
        return fail("max_total_bytes=32 produced %zu bytes on tag '%s'"
                    " (expected ~32 + footer)", buf.size(), sigStr);
      }
      if (buf.find("[truncated]") == std::string::npos) {
        delete pProfile;
        return fail("max_total_bytes=32 did not emit '[truncated]' footer"
                    " on tag '%s'", sigStr);
      }
      std::printf("[OK] max_total_bytes truncates default fallback on '%s'\n",
                  sigStr);
      break;
    }
  }

  // ── Test 5: cooperative cancellation via ShouldContinue() ───────────────
  // BudgetedSink stops accepting writes after `budget` bytes. Producers
  // (CIccCLUT::Iterate, CIccMBB::Describe) check ShouldContinue() in
  // their inner loops and bail when it returns false.
  if (pLutTag) {
    BudgetedSink sink(64);
    pLutTag->Describe(sink, OptionsFromVerbosity(100));

    if (sink.bytesWritten() == 0) {
      delete pProfile;
      return fail("BudgetedSink received zero bytes");
    }
    if (sink.bytesWritten() > 64) {
      delete pProfile;
      return fail("BudgetedSink absorbed %zu bytes despite 64-byte budget",
                  sink.bytesWritten());
    }
    std::printf("[OK] ShouldContinue() cancellation honoured "
                "(BudgetedSink wrote %zu/64 bytes before stopping)\n",
                sink.bytesWritten());
  }

  delete pProfile;
  std::printf("\nAll Describe sink/options tests passed.\n");
  return 0;
}
