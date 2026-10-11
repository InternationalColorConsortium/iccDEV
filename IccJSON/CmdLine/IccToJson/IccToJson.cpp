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
// IccToJson.cpp : Convert a binary ICC profile to a JSON file.
//

#include <cstdio>
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccProfileJson.h"
#include "IccIO.h"
#include "IccProfLibVer.h"
#include "IccLibJSONVer.h"
#include <cstdlib>
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
    // Element count is known up front, so size the backing vector once instead of
    // letting push_back reallocate and copy its way up. CLUT-bearing tags dominate
    // the document, and those arrays are where nearly all of the elements live.
    arr.get_ref<nlohmann::ordered_json::array_t &>().reserve(j.size());
    for (const auto &elem : j)
      arr.push_back(sortJsonKeys(elem));
    return arr;
  }
  // Primitives: nlohmann converts directly between two basic_json instantiations.
  // Its converting constructor dispatches on the source's value_t and moves the
  // scalar across by type, so there is no need to serialize to text and re-parse.
  // When IccJson already IS ordered_json that constructor is excluded (same type)
  // and the ordinary copy constructor runs, so this is correct in both builds.
  return nlohmann::ordered_json(j);
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
    if (!strncmp(argv[i], "-indent=", 8)) {
      indent = atoi(argv[i] + 8);
      if (indent < 0)
        indent = 0;
      if (indent > 20)
        indent = 20;
    }
    else if (!strcmp(argv[i], "-sort"))
      bSort = true;
  }

  CIccProfileJson profile;
  CIccFileIO srcIO;

  if (!srcIO.Open(argv[1], "r")) {
    printf("Unable to open '%s'\n", argv[1]);
    return EXIT_FAILURE;
  }

  if (!profile.Read(&srcIO)) {
    printf("Unable to read '%s'\n", argv[1]);
    return EXIT_FAILURE;
  }

  std::string jsonStr;
  try {
    if (bSort) {
      IccJson j;
      if (!profile.ToJson(j)) {
        printf("Unable to convert '%s' to JSON\n", argv[1]);
        return EXIT_FAILURE;
      }
      IccJson wrapper;
      // j is the whole document and is dead after this hand-off; copying it here
      // doubled peak memory before the sort pass even started.
      wrapper["IccProfile"] = std::move(j);
      nlohmann::ordered_json sorted = sortJsonKeys(wrapper);
      jsonStr = sorted.dump(indent);
    }
    else {
      if (!profile.ToJson(jsonStr, indent)) {
        printf("Unable to convert '%s' to JSON\n", argv[1]);
        return EXIT_FAILURE;
      }
    }
  }
  catch (const std::exception &e) {
    printf("JSON serialization error for '%s': %s\n", argv[1], e.what());
    return EXIT_FAILURE;
  }

  CIccFileIO outFile;
  // IccToJson intentionally writes to a caller-selected output path after CIccFileIO regular-file validation.

  // codeql[cpp/path-injection]
  if (!outFile.Open(argv[2], "wb")) {
    printf("Unable to open '%s' for writing\n", argv[2]);
    return EXIT_FAILURE;
  }

  if (outFile.Write8((void*)jsonStr.c_str(), jsonStr.size()) != jsonStr.size() ||
      !outFile.Flush() ||
      !outFile.CloseFile()) {
    printf("Unable to write '%s'\n", argv[2]);
    return EXIT_FAILURE;
  }

  printf("JSON successfully created\n");
  return EXIT_SUCCESS;
}
