/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** @file In-process libFuzzer target for IccXML and instrumented libxml2. */

#include "IccMpeXmlFactory.h"
#include "IccProfileXml.h"
#include "IccTagXmlFactory.h"
#include "IccUtilXml.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stddef.h>
#include <stdint.h>

#include <new>
#include <string>

static constexpr size_t kMaxTextInputSize = 1024 * 1024;

static void initializeXmlFactories()
{
  static const bool initialized = []() {
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
