/*
 * Copyright (c) 2026 International Color Consortium.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR ITS CONTRIBUTING
 * MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @file In-process libFuzzer target for IccXML and instrumented libxml2. */

#include "IccMpeXmlFactory.h"
#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccUtilXml.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>

#include <stddef.h>
#include <stdint.h>

#include <new>
#include <string>

static constexpr size_t kMaxTextInputSize = 1024 * 1024;

static void discardXmlDiagnostic(void *, const char *, ...)
{
}

#if LIBXML_VERSION >= 21400
static void discardXmlStructuredDiagnostic(void *, const xmlError *)
#else
static void discardXmlStructuredDiagnostic(void *, xmlErrorPtr)
#endif
{
}

static void initializeXmlFactories()
{
  static const bool initialized = []() {
    // Mutated XML is expected to be malformed. Keep routine parser rejection
    // diagnostics out of sanitizer logs while leaving sanitizer reports and
    // the harness validation report untouched.
    xmlSetGenericErrorFunc(nullptr, discardXmlDiagnostic);
    xmlSetStructuredErrorFunc(nullptr, discardXmlStructuredDiagnostic);
    IccXmlSetAllowFileIncludes(false);
    CIccTagCreator::PushFactory(new (std::nothrow) CIccTagXmlFactory());
    CIccMpeCreator::PushFactory(new (std::nothrow) CIccMpeXmlFactory());
    return true;
  }();
  (void)initialized;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (!data || !size || size > kMaxTextInputSize)
    return 0;

  initializeXmlFactories();
  xmlDocPtr document = xmlReadMemory(
      reinterpret_cast<const char *>(data), static_cast<int>(size),
      "fuzz.xml", nullptr,
      XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING |
          XML_PARSE_COMPACT);
  if (!document)
    return 0;

  CIccProfileXml profile;
  std::string report;
  xmlNodePtr root = xmlDocGetRootElement(document);
  const bool parsed = root && profile.ParseXml(root, report);
  if (parsed)
    profile.Validate(report);

  volatile size_t result_sink = report.size();
  (void)result_sink;
  xmlFreeDoc(document);
  return 0;
}
