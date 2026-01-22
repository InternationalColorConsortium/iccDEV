// IccToXml.cpp : Defines the entry point for the console application.
//

#include <cstdio>
#include "IccTagXmlFactory.h"
#include "IccMpeXmlFactory.h"
#include "IccProfileXml.h"
#include "IccIO.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include "IccLibXMLVer.h"
#include <cstring> /* C strings strcpy, memcpy ... */

int main(int argc, char* argv[])
{
  if (argc<=2) {
    printf("IccFromXml built with IccProfLib Version " ICCPROFLIBVER ", IccLibXML Version " ICCLIBXMLVER "\n\nUsage: IccFromXml xml_file saved_profile_file {-noid -v{=[relax_ng_schema_file - optional]}}\n");
    return -1;
  }

  CIccTagCreator::PushFactory(new CIccTagXmlFactory());
  CIccMpeCreator::PushFactory(new CIccMpeXmlFactory());

  CIccProfileXml profile;
  std::string reason;  

  std::string szRelaxNGDir;
  bool bNoId = false;

  const char* szRelaxNGFileName = "SampleIccRELAX.rng";
  int i;
  for (i=3; i<argc; i++) {
    if (!stricmp(argv[i], "-noid")) {
      bNoId = true;
    }
    else if (!strncmp(argv[i], "-v", 2) || !strncmp(argv[i], "-V", 2)) {// user specified schema validation
      if (argv[i][2]=='=') {
        szRelaxNGDir = argv[i]+3;
      }
      else  { 
        std::string path = argv[0]; // current directory where program is called

#ifdef WIN32
        if (path != "IccFromXml.exe") { //current directory is not path where	program is located  
          path = path.substr(0,path.find_last_of("\\"));
          path += "\\"; 		
        }
#else
        if (path.substr(0,1) != "./"){
          path = path.substr(0,path.find_last_of("//"));
          path += "//"; 	
        }
#endif

        path += szRelaxNGFileName; 

        FILE *f = fopen(path.c_str(), "r");

        if (f) {
          fclose(f);

          szRelaxNGDir = path; 
        }	
      }
    }
  }

    if (!profile.LoadXml(argv[1], szRelaxNGDir.c_str(), &reason)) {
    printf("%s", reason.c_str());
#ifndef WIN32
    printf("\n");
#endif
    printf("Unable to Parse '%s'\n", argv[1]);
    return -1;
  }

  std::string valid_report;

  if (profile.Validate(valid_report)<=icValidateWarning) {
    int i;

    for (i=0; i<16; i++) {
      if (profile.m_Header.profileID.ID8[i])
        break;
    }
    if (SaveIccProfile(argv[2], &profile, bNoId ? icNeverWriteID : (i<16 ? icAlwaysWriteID : icVersionBasedID))) {
      printf("Profile parsed and saved correctly\n");
    }
    else {
      printf("Unable to save profile as '%s'\n", argv[2]);
      return -1;
    }
  }
  else {
    int i;

    for (i=0; i<16; i++) {
      if (profile.m_Header.profileID.ID8[i])
        break;
    }
    if (SaveIccProfile(argv[2], &profile, bNoId ? icNeverWriteID : (i<16 ? icAlwaysWriteID : icVersionBasedID))) {
      printf("Profile parsed.  Profile is invalid, but saved correctly\n");
    }
    else {
      printf("Unable to save profile - profile is invalid!\n");
      return -1;
    }
    printf("%s", valid_report.c_str());
  }

  printf("\n");
  return 0;
}

