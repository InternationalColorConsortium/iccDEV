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
#include "../IccCmdLineUtil.h"
#if defined(_WIN32)
  #include <winsock2.h>
#else
  #include <arpa/inet.h>
#endif

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
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

static
FILE* icOpenWriteBinaryFile(const char* szFname)
{
  return icOpenRegularWriteBinaryFile(szFname);
}

static bool ReadBinaryStream(std::istream& input, std::vector<unsigned char>& data)
{
    data.clear();
    unsigned char buffer[4096];

    while (input) {
        input.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        std::streamsize count = input.gcount();
        if (count > 0) {
            data.insert(data.end(), buffer, buffer + static_cast<size_t>(count));
        }
    }

    return input.eof();
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
    std::ifstream file(jpegPath, std::ios::binary);
    if (!file) safe_exit("Cannot open input JPEG file.");

    std::vector<unsigned char> data;
    if (!ReadBinaryStream(file, data)) safe_exit("Failed to read input JPEG file.");

    const size_t n = data.size();
    if (n < 2 || data[0] != 0xFF || data[1] != 0xD8) {
        LOG_ERROR("Not a valid JPEG file (missing SOI).");
        return false;
    }

    // ICC profiles are carried in one or more APP2 segments, each beginning with
    // the 12-byte "ICC_PROFILE\0" signature followed by a 1-based chunk number and
    // a chunk count; the profile is those chunks concatenated in order (ITU-T T.871
    // / ICC Annex B).  Walk the JPEG marker structure and reassemble them.  The old
    // code stopped at the first APP2 and otherwise scanned raw bytes for 'acsp',
    // which truncated multi-segment profiles and matched arbitrary data (#1382).
    static const unsigned char kSig[12] = {
        'I','C','C','_','P','R','O','F','I','L','E','\0' };

    std::vector<std::vector<unsigned char> > chunks;
    std::vector<bool> seen;
    unsigned int totalChunks = 0;
    size_t pos = 2;  // past SOI

    while (pos < n) {
        if (data[pos] != 0xFF) { pos++; continue; }        // resync to next marker
        while (pos < n && data[pos] == 0xFF) pos++;         // skip marker 0xFF + fill
        if (pos >= n) break;
        unsigned char m = data[pos++];

        if (m == 0xD9 || m == 0xDA) break;                  // EOI, or SOS (entropy data follows)
        if (m == 0x01 || (m >= 0xD0 && m <= 0xD7)) continue; // standalone markers (no length)

        if (n - pos < 2) break;
        unsigned int segLen =
            (static_cast<unsigned int>(data[pos]) << 8) |
            static_cast<unsigned int>(data[pos + 1]);
        pos += 2;
        if (segLen < 2) break;
        size_t payloadLen = segLen - 2;
        if (payloadLen > n - pos) break;
        const unsigned char* seg = data.data() + pos;

        if (m == 0xE2 && payloadLen >= 14 && memcmp(seg, kSig, 12) == 0) {
            unsigned int seq = seg[12];
            unsigned int total = seg[13];
            if (seq == 0 || total == 0 || seq > total) {
                LOG_ERROR("Invalid ICC_PROFILE APP2 chunk sequence.");
                return false;
            }
            if (totalChunks == 0) {
                totalChunks = total;
                chunks.resize(total);
                seen.assign(total, false);
            }
            else if (total != totalChunks) {
                LOG_ERROR("Inconsistent ICC_PROFILE APP2 chunk count.");
                return false;
            }
            if (seen[seq - 1]) {
                LOG_ERROR("Duplicate ICC_PROFILE APP2 chunk.");
                return false;
            }
            seen[seq - 1] = true;
            chunks[seq - 1].assign(seg + 14, seg + payloadLen);
            printf("[INFO] ICC_PROFILE APP2 chunk %u/%u (%zu bytes).\n",
                   seq, total, payloadLen - 14);
        }
        pos += payloadLen;
    }

    if (totalChunks == 0) {
        LOG_ERROR("No ICC_PROFILE APP2 marker found.");
        return false;
    }

    std::vector<unsigned char> iccData;
    for (unsigned int i = 0; i < totalChunks; ++i) {
        if (!seen[i]) {
            LOG_ERROR("Incomplete ICC_PROFILE APP2 sequence (missing chunk).");
            return false;
        }
        iccData.insert(iccData.end(), chunks[i].begin(), chunks[i].end());
    }

    FILE* out = icOpenWriteBinaryFile(iccOutPath);
    if (!out) safe_exit("Failed to open output ICC file.");
    bool failed = fwrite(iccData.data(), 1, iccData.size(), out) != iccData.size();
    if (failed) safe_exit("Failed to write output ICC file.");
    if (!icFlushAndClose(out)) safe_exit("Failed to close output ICC file.");

    printf("[INFO] ICC profile extracted to: %s (%zu bytes, %u chunk(s))\n",
           iccOutPath, iccData.size(), totalChunks);
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
    FILE* out = icOpenWriteBinaryFile(outputPath);
    std::ifstream icc(iccPath, std::ios::binary);

    if (!in || !out || !icc.is_open())
        safe_exit("Cannot open input/output/ICC file.");

    // ------------------------------------------------------------------------
    // Read ICC profile into memory
    // ------------------------------------------------------------------------
    std::vector<unsigned char> iccData;
    if (!ReadBinaryStream(icc, iccData)) {
        safe_exit("Failed to read ICC profile.");
    }

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
    if (fwrite(soi, 1, 2, out) != 2)
        safe_exit("Failed to write JPEG SOI marker.");

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
    // Split the profile across as many APP2 segments as needed.  Each segment's
    // length is a 2-byte field, so a profile larger than ~64 KB MUST be chunked --
    // the previous single-segment write overflowed that field for large profiles.
    // Each segment carries "ICC_PROFILE\0" + a 1-based chunk number + the total
    // chunk count, matching the reassembly in ExtractIccFromJpeg (#1382).
    static const unsigned char kSig[12] = {
        'I','C','C','_','P','R','O','F','I','L','E','\0' };
    const size_t kMaxChunk = 65535 - 2 - 12 - 2;   // length field - len - sig - seq/count
    size_t totalChunks = (iccData.size() + kMaxChunk - 1) / kMaxChunk;
    if (totalChunks == 0)
        totalChunks = 1;
    if (totalChunks > 255)
        safe_exit("ICC profile too large to embed in a JPEG (>255 APP2 chunks).");

    for (size_t i = 0; i < totalChunks; ++i) {
        size_t off = i * kMaxChunk;
        size_t remaining = iccData.size() - off;
        size_t chunkLen = remaining < kMaxChunk ? remaining : kMaxChunk;
        unsigned short segLen = (unsigned short)(2 + 12 + 2 + chunkLen);
        unsigned short segLenBE = htons(segLen);

        if (fputc(0xFF, out) == EOF || fputc(0xE2, out) == EOF ||
            fwrite(&segLenBE, 2, 1, out) != 1 ||
            fwrite(kSig, 1, 12, out) != 12 ||
            fputc((int)(i + 1), out) == EOF ||       // chunk number (1-based)
            fputc((int)totalChunks, out) == EOF ||   // chunk count
            fwrite(iccData.data() + off, 1, chunkLen, out) != chunkLen) {
            safe_exit("Failed to write ICC APP2 segment.");
        }
    }

    // ------------------------------------------------------------------------
    // Append remainder of JPEG file unmodified
    // ------------------------------------------------------------------------
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n)
            safe_exit("Failed to write JPEG output.");
    }

    fclose(in);
    if (!icFlushAndClose(out))
        safe_exit("Failed to close output JPEG file.");
    icc.close();
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
        return 0;
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
