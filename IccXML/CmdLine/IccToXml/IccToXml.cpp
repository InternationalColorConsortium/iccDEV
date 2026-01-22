// IccToXml.cpp : Defines the entry point for the console application.
//

#include <cstdio>
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccProfileXml.h"
#include "IccIO.h"
#include "IccProfLibVer.h"
#include "IccLibXMLVer.h"

int main(int argc, char* argv[])
{
  if (argc<=2) {
    printf("IccToXml built with IccProfLib Version " ICCPROFLIBVER ", IccLibXML Version " ICCLIBXMLVER "\n\nUsage: IccToXml src_icc_profile dest_xml_file\n");
    return -1;
  }
  
  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  CIccProfileXml profile;
  CIccFileIO srcIO, dstIO;

  if (!srcIO.Open(argv[1], "r")) {
    printf("Unable to open '%s'\n", argv[1]);
    return -1;
  }

  if (!profile.Read(&srcIO)) {
    printf("Unable to read '%s'\n", argv[1]);
    return -1;
  }

  std::string xml;
  xml.reserve(40000000);

  if (!profile.ToXml(xml)) {
    printf("Unable to convert '%s' to xml\n", argv[1]);
    return -1;
  }

  if (!dstIO.Open(argv[2], "wb")) {
    printf("unable to open '%s'\n", argv[2]);
    return -1;
  }

  if (dstIO.Write8((char*)xml.c_str(), xml.size())== xml.size()) {
    printf("XML successfully created\n");
  }
  else {
    printf("Unable to write '%s'\n", argv[2]);
    return -1;
  }

  dstIO.Close();

  return 0;
}

