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

// Recursively sort JSON object keys for deterministic output.
// Works regardless of whether IccJson is ordered_json or json.
static nlohmann::ordered_json sortJsonKeys(const IccJson &j)
{
  if (j.is_object()) {
    // Collect keys and sort alphabetically
    std::vector<std::string> keys;
    for (auto it = j.begin(); it != j.end(); ++it)
      keys.push_back(it.key());
    std::sort(keys.begin(), keys.end());

    nlohmann::ordered_json sorted = nlohmann::ordered_json::object();
    for (const auto &k : keys)
      sorted[k] = sortJsonKeys(j[k]);
    return sorted;
  }
  if (j.is_array()) {
    nlohmann::ordered_json arr = nlohmann::ordered_json::array();
    for (const auto &elem : j)
      arr.push_back(sortJsonKeys(elem));
    return arr;
  }
  // Primitives: convert via dump/parse to cross nlohmann type boundary
  return nlohmann::ordered_json::parse(j.dump());
}

int main(int argc, char* argv[])
{
  if (argc <= 2) {
    printf("IccToJson built with IccProfLib Version " ICCPROFLIBVER ", IccLibJSON Version " ICCLIBJSONVER "\n\n");
    printf("Usage: IccToJson src_icc_profile dest_json_file {options}\n");
    printf("  -indent=N   pretty-print with N spaces of indentation (default: 2)\n");
    printf("  -sort        sort JSON keys alphabetically (deterministic output)\n");
#ifdef ICC_JSON_ORDERED
    printf("\n  Built with ICC_JSON_ORDERED: keys preserve insertion order by default.\n");
#else
    printf("\n  Built with unordered JSON: use -sort for reproducible key order.\n");
#endif
    return 0;
  }

  CIccTagCreator::PushFactory(new CIccTagJsonFactory());
  CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

  int indent = 2;
  bool bSort = false;
  for (int i = 3; i < argc; i++) {
    if (!strncmp(argv[i], "-indent=", 8))
      indent = atoi(argv[i] + 8);
    else if (!strcmp(argv[i], "-sort"))
      bSort = true;
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
    if (bSort) {
      IccJson j;
      if (!profile.ToJson(j)) {
        printf("Unable to convert '%s' to JSON\n", argv[1]);
        return -1;
      }
      IccJson wrapper;
      wrapper["IccProfile"] = j;
      nlohmann::ordered_json sorted = sortJsonKeys(wrapper);
      jsonStr = sorted.dump(indent);
    }
    else {
      if (!profile.ToJson(jsonStr, indent)) {
        printf("Unable to convert '%s' to JSON\n", argv[1]);
        return -1;
      }
    }
  }
  catch (const std::exception &e) {
    printf("JSON serialization error for '%s': %s\n", argv[1], e.what());
    return -1;
  }

  CIccFileIO outFile;
  // IccToJson intentionally writes to a caller-selected output path after CIccFileIO regular-file validation.

  // codeql[cpp/path-injection]
  if (!outFile.Open(argv[2], "wb")) {
    printf("Unable to open '%s' for writing\n", argv[2]);
    return -1;
  }

  if (outFile.Write8((void*)jsonStr.c_str(), jsonStr.size()) != jsonStr.size() ||
      !outFile.Flush() ||
      !outFile.CloseFile()) {
    printf("Unable to write '%s'\n", argv[2]);
    return -1;
  }

  printf("JSON successfully created\n");
  return 0;
}
