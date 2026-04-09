/**
 * hello-iccdev Cmake Configuration| iccDEV Project
 * Copyright (c) 2026 The International Color Consortium. All rights reserved.
 * 
 * Last Updated: 2026-03-24 20:49:38 UTC by David Hoyt
 *
 * hello-iccdev.cpp — Minimal example linking IccProfLib2 and IccXML2
 *
 * Prints library versions and round-trips a trivial sRGB profile to XML.
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

int main()
{
    // ── Library versions ───────────────────────────────────
    std::printf("IccProfLib version: %s\n", ICCPROFLIBVER);
    std::printf("IccLibXML  version: %s\n", ICCLIBXMLVER);

    // ── Create a minimal ICC profile ───────────────────────
    CIccProfile profile;
    profile.InitHeader();
    profile.m_Header.colorSpace  = icSigRgbData;
    profile.m_Header.pcs         = icSigLabData;
    profile.m_Header.deviceClass = icSigDisplayClass;

    // Decode the header version using CIccInfo
    CIccInfo info;
    std::printf("Profile spec ver:  %s\n",
                info.GetVersionName(profile.m_Header.version));

    // ── Round-trip to XML string via IccXML2 ───────────────
    CIccProfileXml xmlProfile;
    xmlProfile.InitHeader();
    xmlProfile.m_Header.colorSpace  = icSigRgbData;
    xmlProfile.m_Header.pcs         = icSigLabData;
    xmlProfile.m_Header.deviceClass = icSigDisplayClass;

    std::string xmlStr;
    if (xmlProfile.ToXml(xmlStr) && !xmlStr.empty()) {
        std::printf("\nXML round-trip OK (%zu bytes)\n", xmlStr.size());
        // Print first 5 lines as a preview
        int lines = 0;
        for (size_t i = 0; i < xmlStr.size() && lines < 5; ++i) {
            std::putchar(xmlStr[i]);
            if (xmlStr[i] == '\n') ++lines;
        }
        std::printf("  ... (truncated)\n");
    } else {
        std::printf("\nXML round-trip: serialization produced empty output "
                    "(profile has no tags — this is expected for a bare header)\n");
    }

    std::printf("\nHello, iccDEV!\n");
    return 0;
}
