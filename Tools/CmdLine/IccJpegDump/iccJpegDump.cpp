/*
    File:       iccJpegDump.cpp

    Contains:   Console app display info about JPEG file and its ICC profile

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2010 The International Color Consortium. All rights 
 * reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium. 
 *
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes. 
 *
 *  
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 *  
 * 
 */

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 5-15-2003
// - CopyPasta by David Hoyt 18-APRIL-2025 to Write ICC -> JPG
//
//////////////////////////////////////////////////////////////////////

/*!
* @brief Core and external libraries necessary for the fuzzer functionality.
*
* @details This section includes the necessary headers for the Foundation framework, UIKit, Core Graphics,
* standard input/output, standard library, memory management, mathematical functions,
* Boolean type, floating-point limits, and string functions. These libraries support
* image processing, UI interaction, and basic C operations essential for the application.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#if defined(_WIN32)
  #include <winsock2.h>
#else
  #include <arpa/inet.h>
#endif


// ============================================================================
// Platform Trap Macro
// ----------------------------------------------------------------------------
// Purpose : Trigger architecture-specific traps (for fuzzing/debug asserts)
// Notes   : x86_64 = 'ud2', ARM64 = 'brk #0', fallback = abort()
// ============================================================================
#ifdef __x86_64__
    #define TRAP() asm volatile ("ud2")
#elif defined(__aarch64__)
    #define TRAP() asm volatile ("brk #0")
#else
    #define TRAP() abort()
#endif

// ============================================================================
// JPEG ICC Profile Pointer Typedef
// ----------------------------------------------------------------------------
// Purpose : Abstract ICC pointer type with platform-const enforcement
// ============================================================================
#if defined(__APPLE__) && defined(__clang__)
    typedef const unsigned char* jpeg_icc_profilep;
#else
    typedef unsigned char* jpeg_icc_profilep;
#endif

// ============================================================================
// Logging & Exit Macros
// ----------------------------------------------------------------------------
// LOG_ERROR     : Print standardized error message to stderr
// safe_exit(msg): Log and exit with failure (fatal path)
// ============================================================================
#define LOG_ERROR(msg) fprintf(stderr, "[ERROR] %s\n", msg)
#define safe_exit(reason) do { LOG_ERROR(reason); exit(EXIT_FAILURE); } while (0)

// ============================================================================
// Function: Usage
// ----------------------------------------------------------------------------
// Description : Prints command-line usage summary for the tool.
// ============================================================================
void Usage() {
    printf("Usage:\n");
    printf("  iccJpegDump <input.jpg> [output.icc]\n");
    printf("    - Extract ICC profile from JPEG (APP2 or EXIF-based) if present.\n");
    printf("  iccJpegDump <input.jpg> --write-icc <profile.icc> --output <output.jpg>\n");
    printf("    - Inject ICC profile into JPEG image.\n");
}

// ============================================================================
// Function: ExtractIccFromJpeg
// ----------------------------------------------------------------------------
// Description:
//   Attempts to extract an embedded ICC profile from a JPEG file.
//
// Search Order:
//   1. APP2 marker with "ICC_PROFILE\0" signature (preferred spec)
//   2. APP0-APPF markers with embedded "acsp" magic (EXIF/ad hoc embed)
//   3. Full binary scan fallback for "acsp" if header parsing fails
//
// Parameters:
//   - jpegPath:     Input JPEG file path
//   - iccOutPath:   Output ICC binary file to write
//
// Returns:
//   true if a valid ICC profile was found and written to disk, false otherwise.
// ============================================================================
bool ExtractIccFromJpeg(const char* jpegPath, const char* iccOutPath) {
    FILE* fp = fopen(jpegPath, "rb");
    if (!fp) safe_exit("Cannot open input JPEG file.");

    std::vector<unsigned char> iccData;
    unsigned char marker[2];
    bool found = false;

    // ------------------------------------------------------------
    // Iterate JPEG markers and scan for APPx segments
    // ------------------------------------------------------------
    while (fread(marker, 1, 2, fp) == 2) {
        if (marker[0] != 0xFF) break;
        if (marker[1] == 0xD9) break;  // EOI

        unsigned short len = 0;
        if (fread(&len, 2, 1, fp) != 1) break;
        len = ntohs(len);

        if (len < 2) break;
        std::vector<unsigned char> segment(len - 2);
        if (fread(segment.data(), 1, segment.size(), fp) != segment.size())
            break;

        printf("[INFO] Scanning segment 0x%02X, length: %u bytes\n", marker[1], len);

        // --------------------------------------------------------
        // Check for APP2 ICC_PROFILE header (spec-compliant)
        // --------------------------------------------------------
        if (marker[1] == 0xE2 && len > 16) {
            if (memcmp(segment.data(), "ICC_PROFILE\0", 12) == 0) {
                unsigned char* dataStart = segment.data() + 14;
                size_t dataLen = segment.size() - 14;
                iccData.insert(iccData.end(), dataStart, dataStart + dataLen);
                printf("[INFO] Found ICC_PROFILE in APP2 segment.\n");
                found = true;
                break;
            }
        }

        // --------------------------------------------------------
        // Fallback: Search all APPx for "acsp" ICC signature
        // --------------------------------------------------------
        if (marker[1] >= 0xE0 && marker[1] <= 0xEF) {
            for (size_t i = 0; i + 4 < segment.size(); ++i) {
                if (memcmp(&segment[i], "acsp", 4) == 0) {
                    iccData.insert(iccData.end(), segment.begin() + i, segment.end());
                    printf("[INFO] Found 'acsp' ICC magic in APP segment 0x%02X at offset %zu.\n", marker[1], i);
                    goto done;
                }
            }
        }
    }

    // ------------------------------------------------------------
    // Raw Fallback: Scan entire file for "acsp"
    // ------------------------------------------------------------
    if (!found) {
        std::ifstream file(jpegPath, std::ios::binary);
        if (!file) {
            LOG_ERROR("Fallback open failed.");
            return false;
        }

        std::vector<unsigned char> raw((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());

        for (size_t i = 0; i + 4 < raw.size(); ++i) {
            if (memcmp(&raw[i], "acsp", 4) == 0) {
                iccData.insert(iccData.end(), raw.begin() + i, raw.end());
                printf("[INFO] Fallback: Found 'acsp' in raw file at offset %zu.\n", i);
                break;
            }
        }
    }

done:
    fclose(fp);

    if (iccData.empty()) {
        LOG_ERROR("No ICC profile found.");
        return false;
    }

    FILE* out = fopen(iccOutPath, "wb");
    if (!out) safe_exit("Failed to open output ICC file.");
    fwrite(iccData.data(), 1, iccData.size(), out);
    fclose(out);

    printf("[INFO] ICC profile extracted to: %s\n", iccOutPath);
    return true;
}

// ============================================================================
// Function: InjectIccIntoJpeg
// ----------------------------------------------------------------------------
// Description:
//   Embeds an ICC profile into a JPEG image by inserting an APP2 segment
//   with the `ICC_PROFILE` signature immediately after the SOI marker.
//
//   This method does NOT strip or validate existing ICC markers—it simply
//   prepends a new APP2 segment and appends the remainder of the original image.
//
// Parameters:
//   - inputPath   : Source JPEG file (no ICC required)
//   - iccPath     : Binary ICC profile file to inject
//   - outputPath  : Output JPEG with injected ICC segment
//
// Returns:
//   true on success, false on fatal I/O error
//
// Notes:
//   - ICC profiles >64KB should be split into segments (not yet supported)
//   - The image stream is blindly copied from input to output post-SOI
// ============================================================================
bool InjectIccIntoJpeg(const char* inputPath, const char* iccPath, const char* outputPath) {
    FILE* in = fopen(inputPath, "rb");
    FILE* out = fopen(outputPath, "wb");
    std::ifstream icc(iccPath, std::ios::binary);

    if (!in || !out || !icc.is_open())
        safe_exit("Cannot open input/output/ICC file.");

    // ------------------------------------------------------------------------
    // Read ICC profile into memory
    // ------------------------------------------------------------------------
    std::vector<unsigned char> iccData((std::istreambuf_iterator<char>(icc)),
                                        std::istreambuf_iterator<char>());

    // ------------------------------------------------------------------------
    // Verify and copy SOI marker (0xFFD8)
    // ------------------------------------------------------------------------
    unsigned char soi[2];
    // Line 294: Original Code
    // fread(soi, 1, 2, in);

    // Updated Code
    size_t bytesRead = fread(soi, 1, 2, in);
    if (bytesRead != 2) {
    fprintf(stderr, "Error: Failed to read 2 bytes from the input file.\n");
    fclose(in);
    return false; // or handle the error as required
    }
    if (soi[0] != 0xFF || soi[1] != 0xD8)
        safe_exit("Not a valid JPEG file.");
    fwrite(soi, 1, 2, out);

    // ------------------------------------------------------------------------
    // Write APP2 segment with ICC header and payload
    // Format:
    //   - Marker      : 0xFFE2
    //   - Length      : 2-byte big endian (header + payload)
    //   - Signature   : "ICC_PROFILE" + '\0'
    //   - Sequence No : 1 (single-segment)
    //   - Total Segs  : 1
    //   - ICC Data    : full profile binary
    // ------------------------------------------------------------------------
    std::string sig = "ICC_PROFILE";
    unsigned short markerLen = 2 + sig.size() + 1 + 2 + iccData.size(); // length includes itself
    unsigned short markerBE = htons(markerLen);

    fputc(0xFF, out); fputc(0xE2, out);                   // APP2
    fwrite(&markerBE, 2, 1, out);                         // segment length
    fwrite(sig.c_str(), 1, sig.size(), out);              // "ICC_PROFILE"
    fputc(0x00, out);                                     // null terminator
    fputc(1, out); fputc(1, out);                         // sequence 1 of 1
    fwrite(iccData.data(), 1, iccData.size(), out);       // profile payload

    // ------------------------------------------------------------------------
    // Append remainder of JPEG file unmodified
    // ------------------------------------------------------------------------
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);

    fclose(in); fclose(out); icc.close();
    return true;
}

// ============================================================================
// Function: main
// ----------------------------------------------------------------------------
// Description:
//   Entry point for the iccJpegDump tool. Parses command-line arguments to
//   determine if the user intends to extract or inject an ICC profile.
//
//   - Extraction mode:
//       iccJpegDump <input.jpg> [output.icc]
//
//   - Injection mode:
//       iccJpegDump <input.jpg> --write-icc <profile.icc> --output <output.jpg>
//
// Returns:
//   0 on success; exits on failure with appropriate error messages.
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        Usage();
        safe_exit("Missing arguments.");
    }

    // ------------------------------------------------------------------------
    // Parse Command-Line Arguments
    // ------------------------------------------------------------------------
    const char* inputFile = nullptr;
    const char* extractIccOut = nullptr;
    const char* injectIccFile = nullptr;
    const char* outputJpegFile = nullptr;

    inputFile = argv[1];

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--write-icc") == 0 && i + 1 < argc) {
            injectIccFile = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputJpegFile = argv[++i];
        } else {
            extractIccOut = argv[i];
        }
    }

    // ------------------------------------------------------------------------
    // ICC Injection Mode: Embed ICC profile into a JPEG
    // ------------------------------------------------------------------------
    if (injectIccFile && outputJpegFile) {
        if (!InjectIccIntoJpeg(inputFile, injectIccFile, outputJpegFile)) {
            safe_exit("Failed to inject ICC profile.");
        }
        printf("[INFO] ICC profile successfully injected into: %s\n", outputJpegFile);

    // ------------------------------------------------------------------------
    // ICC Extraction Mode: Extract ICC profile from a JPEG
    // ------------------------------------------------------------------------
    } else if (extractIccOut) {
        if (!ExtractIccFromJpeg(inputFile, extractIccOut)) {
            safe_exit("Failed to extract ICC profile.");
        }

    // ------------------------------------------------------------------------
    // Invalid Argument Combinations
    // ------------------------------------------------------------------------
    } else {
        Usage();
        safe_exit("Invalid argument combination.");
    }

    return 0;
}
