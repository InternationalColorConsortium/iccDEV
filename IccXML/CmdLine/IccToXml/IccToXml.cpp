/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// IccToXml.cpp : Defines the entry point for the console application.
//

#include <cstdio>
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccProfileXml.h"
#include "IccIO.h"
#include "IccProfLibVer.h"
#include "IccLibXMLVer.h"
#include "IccFileUtil.h"
#include <cstdlib>

int main(int argc, char* argv[])
{
  if (argc<=2) {
    printf("IccToXml built with IccProfLib Version " ICCPROFLIBVER ", IccLibXML Version " ICCLIBXMLVER "\n\n");
    printf("Usage: IccToXml src_icc_profile dest_xml_file\n");
    return 0;
  }
  
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  CIccProfileXml profile;
  CIccFileIO srcIO, dstIO;

  // #2414: both operands are echoed by every failure path below, so a path carrying
  // terminal control sequences reached the console verbatim (#2406).  Sanitized once
  // here rather than at each printf so a message added later cannot reintroduce the
  // raw form; the paths handed to Open() stay untouched.
  std::string srcName = icSanitizeConsoleText(argv[1]);
  std::string dstName = icSanitizeConsoleText(argv[2]);

  if (!srcIO.Open(argv[1], "r")) {
    printf("Unable to open '%s'\n", srcName.c_str());
    return EXIT_FAILURE;
  }

  if (!profile.Read(&srcIO)) {
    printf("Unable to read '%s'\n", srcName.c_str());
    return EXIT_FAILURE;
  }

  std::string xml;
  xml.reserve(40000000);

  if (!profile.ToXml(xml)) {
    printf("Unable to convert '%s' to xml\n", srcName.c_str());
    return EXIT_FAILURE;
  }

  if (!dstIO.Open(argv[2], "wb")) {
    printf("unable to open '%s'\n", dstName.c_str());
    return EXIT_FAILURE;
  }

  if (dstIO.Write8((char*)xml.c_str(), xml.size()) != xml.size() ||
      !dstIO.Flush() ||
      !dstIO.CloseFile()) {
    printf("Unable to write '%s'\n", dstName.c_str());
    return EXIT_FAILURE;
  }

  printf("XML successfully created\n");

  return EXIT_SUCCESS;
}
