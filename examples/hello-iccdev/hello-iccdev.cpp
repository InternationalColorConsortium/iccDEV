/**
 * hello-iccdev Cmake Configuration| iccDEV Project
 * Copyright (c) 2026 The International Color Consortium. All rights reserved.
 *
 * Last Updated: 2026-04-10 05:19:00 UTC by David Hoyt
 *
 * hello-iccdev.cpp -- Minimal example linking IccProfLib2, IccXML2,
 *                     and optionally IccJSON2
 *
 * Prints library versions and round-trips a trivial sRGB profile to XML
 * (and to JSON when built with USE_ICCJSON=1).
 */

#include <cstdio>

// IccProfLib headers
#include "IccProfile.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"

// IccXML headers
#include "IccTagXml.h"
#include "IccProfileXml.h"
#include "IccLibXMLVer.h"

// IccJSON headers (optional)
#ifdef USE_ICCJSON
#include "IccProfileJson.h"
#include "IccTagJson.h"
#include "IccMpeJson.h"
#include "IccTagJsonFactory.h"
#include "IccMpeJsonFactory.h"
#include "IccLibJSONVer.h"
#endif

int main()
{
    // -- Library versions -------------------------------------------
    std::printf("IccProfLib version: %s\n", ICCPROFLIBVER);
    std::printf("IccLibXML  version: %s\n", ICCLIBXMLVER);
#ifdef USE_ICCJSON
    std::printf("IccLibJSON version: %s\n", ICCLIBJSONVER);
#else
    std::printf("IccLibJSON version: (not linked)\n");
#endif

    // -- Create a minimal ICC profile -------------------------------
    CIccProfile profile;
    profile.InitHeader();
    profile.m_Header.colorSpace  = icSigRgbData;
    profile.m_Header.pcs         = icSigLabData;
    profile.m_Header.deviceClass = icSigDisplayClass;

    // Decode the header version using CIccInfo
    CIccInfo info;
    std::printf("Profile spec ver:  %s\n",
                info.GetVersionName(profile.m_Header.version));

    // -- Round-trip to XML string via IccXML2 -----------------------
    CIccProfileXml xmlProfile;
    xmlProfile.InitHeader();
    xmlProfile.m_Header.colorSpace  = icSigRgbData;
    xmlProfile.m_Header.pcs         = icSigLabData;
    xmlProfile.m_Header.deviceClass = icSigDisplayClass;

    std::string xmlStr;
    if (xmlProfile.ToXml(xmlStr) && !xmlStr.empty()) {
        std::printf("\nXML round-trip OK (%zu bytes)\n", xmlStr.size());
        int lines = 0;
        for (size_t i = 0; i < xmlStr.size() && lines < 5; ++i) {
            std::putchar(xmlStr[i]);
            if (xmlStr[i] == '\n') ++lines;
        }
        std::printf("  ... (truncated)\n");
    } else {
        std::printf("\nXML round-trip: serialization produced empty output\n"
                    "(profile has no tags -- this is expected for a bare header)\n");
    }

    // -- Round-trip to JSON string via IccJSON2 (optional) ----------
#ifdef USE_ICCJSON
    CIccTagCreator::PushFactory(new CIccTagJsonFactory());
    CIccMpeCreator::PushFactory(new CIccMpeJsonFactory());

    CIccProfileJson jsonProfile;
    jsonProfile.InitHeader();
    jsonProfile.m_Header.colorSpace  = icSigRgbData;
    jsonProfile.m_Header.pcs         = icSigLabData;
    jsonProfile.m_Header.deviceClass = icSigDisplayClass;

    std::string jsonStr;
    if (jsonProfile.ToJson(jsonStr, 2)) {
        std::printf("\nJSON round-trip OK (%zu bytes)\n", jsonStr.size());
        int lines = 0;
        for (size_t i = 0; i < jsonStr.size() && lines < 5; ++i) {
            std::putchar(jsonStr[i]);
            if (jsonStr[i] == '\n') ++lines;
        }
        std::printf("  ... (truncated)\n");
    } else {
        std::printf("\nJSON round-trip: serialization produced empty output\n"
                    "(profile has no tags -- this is expected for a bare header)\n");
    }
#endif

    std::printf("\nHello, iccDEV!\n");
    return 0;
}
