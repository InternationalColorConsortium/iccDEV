// IccToJson.cpp : Convert a binary ICC profile to a JSON file.
//

#include <cstdio>
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccProfileJson.h"
#include "IccIO.h"
#include "IccProfLibVer.h"
#include "IccLibJSONVer.h"
#include <fstream>

int main(int argc, char* argv[])
{
  if (argc <= 2) {
    printf("IccToJson built with IccProfLib Version " ICCPROFLIBVER ", IccLibJSON Version " ICCLIBJSONVER "\n\n");
    printf("Usage: IccToJson src_icc_profile dest_json_file {-indent=N}\n");
    printf("         -indent=N  pretty-print with N spaces of indentation (default: 2)\n");
    return 0;
  }

  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  int indent = 2;
  for (int i = 3; i < argc; i++) {
    if (!strncmp(argv[i], "-indent=", 8))
      indent = atoi(argv[i] + 8);
  }

  CIccProfileJson profile;
  CIccFileIO srcIO;

  if (!srcIO.Open(argv[1], "r")) {
    printf("Unable to open '%s'\n", argv[1]);
    return -1;
  }

  if (!profile.Read(&srcIO)) {
    printf("Unable to read '%s'\n", argv[1]);
    return -1;
  }

  std::string jsonStr;
  try {
    if (!profile.ToJson(jsonStr, indent)) {
      printf("Unable to convert '%s' to JSON\n", argv[1]);
      return -1;
    }
  }
  catch (const std::exception &e) {
    printf("JSON serialization error for '%s': %s\n", argv[1], e.what());
    return -1;
  }

  std::ofstream outFile(argv[2]);
  if (!outFile.is_open()) {
    printf("Unable to open '%s' for writing\n", argv[2]);
    return -1;
  }

  outFile << jsonStr;
  if (outFile.fail()) {
    printf("Unable to write '%s'\n", argv[2]);
    return -1;
  }
  outFile.close();

  printf("JSON successfully created\n");
  return 0;
}
