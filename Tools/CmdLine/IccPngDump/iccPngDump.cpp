/*
    File:       iccPngDump.cpp

    Contains:   Console app display info about PNG file and its ICC profile

    Version:    V1

    Copyright:  (c) see below
*/

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
////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Initial implementation by Max Derhak 5-15-2003
// - CopyPasta by David Hoyt 18-APRIL-2025 to Write ICC -> PNG
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

#include <iostream>
#include <fstream>
#include <limits>
#include <vector>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <png.h>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include "IccCmdLineUtil.h"
#include "IccSameFile.h"
#include "IccFileUtil.h"
#include <zlib.h>
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#endif

// Handle platform-specific png_get_iCCP() profile pointer types
#if defined(__APPLE__) && defined(__clang__)
    #define PNG_ICC_PROFILE_CAST (png_charpp)
    typedef png_charp png_icc_profilep;
#else
    #define PNG_ICC_PROFILE_CAST (png_bytepp)
    typedef png_bytep png_icc_profilep;
#endif

// Logging macros for error handling
#define LOG_ERROR(msg) do { fprintf(stderr, "[ERROR] %s\n", msg); } while (0)
#define BAIL_OUT(msg) safe_exit(msg)

static const png_uint_32 kMaxPngDimension = 100000;
static const png_alloc_size_t kMaxPngChunkBytes = 64U * 1024U * 1024U;
static const size_t kMaxPngRasterBytes = 256U * 1024U * 1024U;
static const size_t kMaxIccProfileBytes = 64U * 1024U * 1024U;


// Function declarations

/**
 * Displays usage information for the program.
 */

void Usage();

/**
 * Safely exits the program with a given reason.
 *
 * @param reason The reason for exiting.
 */

void safe_exit(const char *reason);

/**
 * Extracts the ICC profile from a PNG image.
 *
 * @param png_ptr   Pointer to the PNG structure.
 * @param info_ptr  Pointer to the PNG info structure.
 * @param pProfMem  Output pointer to store the ICC profile data.
 * @param nLen      Output length of the extracted ICC profile.
 * @return          1 on success, 0 on failure.
 */
int ExtractIccProfile(png_structp png_ptr, png_infop info_ptr, unsigned char **pProfMem, unsigned int *nLen);

/**
 * Injects a new ICC profile into a PNG and writes it to disk.
 *
 * @param inputPng Source PNG path.
 * @param iccFile  ICC profile file to inject.
 * @param outputPng Destination PNG path.
 * @return true on success, false otherwise.
 */
bool InjectIccProfile(const std::string& inputPng,
                      const std::string& iccFile,
                      const std::string& outputPng);

/**
 * Displays and optionally saves ICC profile information.
 *
 * @param pProfMem ICC profile memory block.
 * @param nLen     Size of ICC data.
 * @param outputFile Path to write ICC (optional).
 */
bool PrintIccProfileInfo(const unsigned char *pProfMem, unsigned int nLen, const char *outputFile);

/**
 * Prints information about a given PNG file.
 *
 * @param png_ptr  Pointer to the PNG structure.
 * @param info_ptr Pointer to the PNG info structure.
 */
void PrintPngInfo(png_structp png_ptr, png_infop info_ptr);



/**
 * Entry point of the iccPngDump program.
 *
 * This function handles command-line arguments, opens a PNG file,
 * validates its format, and either extracts or injects an ICC profile.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, non-zero on failure.
 */
int main(int argc, char* argv[]) {
    printf("[INFO] Starting iccPngDump...\n");

    if (argc < 2) {
        Usage();
        return EXIT_FAILURE;
    }

    std::string inputPath;
    std::string outputIccPath;
    std::string injectIccPath;
    std::string outputPngPath;
    bool positionalOnly = false;
    bool invalidArguments = false;
    int positionalCount = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!positionalOnly && strcmp(arg, "--") == 0) {
            positionalOnly = true;
        } else if (!positionalOnly && strcmp(arg, "--write-icc") == 0) {
            if (!injectIccPath.empty() || i + 1 >= argc ||
                strncmp(argv[i + 1], "--", 2) == 0) {
                invalidArguments = true;
                break;
            }
            injectIccPath = argv[++i];
            if (injectIccPath.empty()) {
                invalidArguments = true;
                break;
            }
        } else if (!positionalOnly && strcmp(arg, "--output") == 0) {
            if (!outputPngPath.empty() || i + 1 >= argc ||
                strncmp(argv[i + 1], "--", 2) == 0) {
                invalidArguments = true;
                break;
            }
            outputPngPath = argv[++i];
            if (outputPngPath.empty()) {
                invalidArguments = true;
                break;
            }
        } else if (!positionalOnly && arg[0] == '-') {
            invalidArguments = true;
            break;
        } else if (positionalCount == 0) {
            inputPath = arg;
            ++positionalCount;
        } else if (positionalCount == 1 && injectIccPath.empty()) {
            outputIccPath = arg;
            ++positionalCount;
        } else {
            invalidArguments = true;
            break;
        }
    }

    if (invalidArguments || inputPath.empty() ||
        (!injectIccPath.empty() && outputIccPath.size() != 0) ||
        (injectIccPath.empty() && !outputPngPath.empty())) {
        Usage();
        LOG_ERROR("Invalid command-line arguments.");
        return EXIT_FAILURE;
    }

// --- Injection Mode ---
if (!injectIccPath.empty()) {
    if (outputPngPath.empty()) {
        Usage();
        LOG_ERROR("Missing --output argument for write mode.");
        return EXIT_FAILURE;
    }

    // #2692: --write-icc names an INPUT here, the profile to embed, so the
    // output is refused if it is either input.
    if (icOutputIsInput(outputPngPath.c_str(), inputPath.c_str()) ||
        icOutputIsInput(outputPngPath.c_str(), injectIccPath.c_str()))
        return EXIT_FAILURE;

    // #2414: the operands are echoed here and below, so a path carrying
    // terminal control sequences reached the console verbatim (#2406).
    printf("[INFO] Injecting ICC profile '%s' into PNG: '%s'\n",
           icSanitizeConsoleText(injectIccPath).c_str(),
           icSanitizeConsoleText(outputPngPath).c_str());

    if (!InjectIccProfile(inputPath, injectIccPath, outputPngPath)) {
        safe_exit("Failed to inject ICC profile.");
    }

    printf("[INFO] Injection successful.\n");
    return 0;
}

    // --- Extraction Mode ---
    // #2692: extracting over the image replaced it with its own profile.
    if (!outputIccPath.empty() &&
        icOutputIsInput(outputIccPath.c_str(), inputPath.c_str()))
        return EXIT_FAILURE;
    printf("[INFO] Opening PNG file: %s\n",
           icSanitizeConsoleText(inputPath).c_str());
    FILE *fp = fopen(inputPath.c_str(), "rb");
    if (!fp) {
        LOG_ERROR("File cannot be opened.");
        safe_exit("File error.");
    }

    unsigned char header[8];
    if (fread(header, 1, 8, fp) != 8 || png_sig_cmp(header, 0, 8)) {
        LOG_ERROR("Not a valid PNG file.");
        fclose(fp);
        safe_exit("Invalid PNG format.");
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        BAIL_OUT("Failed to create PNG read structure.");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        BAIL_OUT("Failed to create PNG info structure.");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        BAIL_OUT("LibPNG error encountered.");
    }

    png_set_user_limits(png_ptr, kMaxPngDimension, kMaxPngDimension);
    png_set_chunk_malloc_max(png_ptr, kMaxPngChunkBytes);
    png_set_chunk_cache_max(png_ptr, 1000);
    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    PrintPngInfo(png_ptr, info_ptr);

    unsigned char *pProfMem = NULL;
    unsigned int nLen = 0;
    if (ExtractIccProfile(png_ptr, info_ptr, &pProfMem, &nLen)) {
        if (!PrintIccProfileInfo(pProfMem, nLen,
                                 outputIccPath.empty() ? NULL : outputIccPath.c_str())) {
            free(pProfMem);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            safe_exit("ICC profile validation failed or extracted profile could not be saved.");
        }
        free(pProfMem);
    } else {
        printf("[INFO] No embedded ICC Profile found.\n");
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    printf("[INFO] iccPngDump completed successfully.\n");
    return 0;
}

// =====================================================================
// Function: safe_exit
// Description: Logs an error message and safely exits the program
// =====================================================================
/**
 * Logs an error message and exits the program safely.
 *
 * @param reason A string containing the reason for exiting.
 */
void safe_exit(const char *reason) {
    LOG_ERROR(reason);
    exit(EXIT_FAILURE);
}

// =====================================================================
// Function: Usage
// Description: Prints usage information for the program
// =====================================================================
/**
 * Displays usage information for the iccPngDump tool.
 *
 * This function provides details on the correct command-line syntax.
 */
void Usage() {
    printf("Usage:\n");
    printf("  iccPngDump <input.png> [output.icc]\n");
    printf("    - Extracts ICC profile from PNG.\n");
    printf("    - Saves profile to [output.icc] if specified.\n");
    printf("\n");
    printf("  iccPngDump <input.png> --write-icc <profile.icc> --output <output.png>\n");
    printf("    - Injects specified ICC profile into PNG.\n");
    printf("    - Outputs modified PNG as <output.png>.\n");
}

// =====================================================================
// Function: InjectIccProfile
// Description: Injects an ICC profile into a PNG file
// =====================================================================
static FILE* OpenPngOutputFile(const char* outputPng)
{
    // PNG export paths are intentional caller-selected output files after regular-file validation.

    // codeql[cpp/path-injection]
    return icOpenRegularWriteBinaryFile(outputPng);
}

static bool IsRegularOutputDestination(const char* outputPng)
{
    if (!outputPng || !outputPng[0])
        return false;

#if defined(_WIN32)
    DWORD attributes = GetFileAttributesA(outputPng);
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        const DWORD rejectedAttributes = FILE_ATTRIBUTE_DEVICE |
                                         FILE_ATTRIBUTE_DIRECTORY |
                                         FILE_ATTRIBUTE_REPARSE_POINT;
        return !(attributes & rejectedAttributes);
    }

    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
#else
    struct stat st;
    if (lstat(outputPng, &st) == 0)
        return S_ISREG(st.st_mode);
    return errno == ENOENT;
#endif
}

static FILE* OpenPngTempOutputFile(const char* outputPng, std::string& tempName)
{
    if (!IsRegularOutputDestination(outputPng))
        return NULL;

    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        char suffix[64];
#if defined(_WIN32)
        snprintf(suffix, sizeof(suffix), ".tmp-%ld-%u", (long)_getpid(), attempt);
        tempName = std::string(outputPng) + suffix;
        int fd = _open(tempName.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                       _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            FILE* fp = _fdopen(fd, "wb");
            if (fp)
                return fp;
            _close(fd);
            remove(tempName.c_str());
            return NULL;
        }
#else
        snprintf(suffix, sizeof(suffix), ".tmp-%ld-%u", (long)getpid(), attempt);
        tempName = std::string(outputPng) + suffix;
        int fd = open(tempName.c_str(), O_WRONLY | O_CREAT | O_EXCL,
                      S_IRUSR | S_IWUSR);
        if (fd >= 0) {
            FILE* fp = fdopen(fd, "wb");
            if (fp)
                return fp;
            close(fd);
            remove(tempName.c_str());
            return NULL;
        }
#endif
        if (errno != EEXIST)
            return NULL;
    }

    return NULL;
}

static bool SetPngOutputPermissions(FILE* output, const char* outputPng)
{
#if !defined(_WIN32)
    struct stat st;
    mode_t permissions;
    if (lstat(outputPng, &st) == 0) {
        if (!S_ISREG(st.st_mode))
            return false;
        permissions = st.st_mode & 0777;
    } else {
        if (errno != ENOENT)
            return false;
        // This single-threaded CLI retains the normal new-output umask policy.
        mode_t mask = umask(077);
        umask(mask);
        permissions = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) & ~mask;
    }
    return fchmod(fileno(output), permissions) == 0;
#else
    (void)output;
    (void)outputPng;
    return true;
#endif
}

static bool CommitPngTempOutputFile(const std::string& tempName,
                                    const char* outputPng)
{
    if (!IsRegularOutputDestination(outputPng))
        return false;

#if defined(_WIN32)
    return MoveFileExA(tempName.c_str(), outputPng,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tempName.c_str(), outputPng) == 0;
#endif
}

static bool ReadBinaryStream(std::istream& in, std::vector<unsigned char>& data)
{
    unsigned char buffer[4096];

    data.clear();
    while (in) {
        in.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        std::streamsize bytesRead = in.gcount();
        if (bytesRead > 0) {
            if (data.size() > kMaxIccProfileBytes -
                    static_cast<size_t>(bytesRead)) {
                LOG_ERROR("ICC profile exceeds the 64 MiB input limit.");
                return false;
            }
            data.insert(data.end(), buffer, buffer + bytesRead);
        }
    }

    return in.eof() && !in.bad();
}

static bool CopyPngAncillaryChunks(png_structp read_ptr,
                                   png_infop read_info,
                                   png_structp write_ptr,
                                   png_infop write_info)
{
    double gamma = 0.0;
    if (png_get_gAMA(read_ptr, read_info, &gamma))
        png_set_gAMA(write_ptr, write_info, gamma);

    double white_x = 0.0, white_y = 0.0;
    double red_x = 0.0, red_y = 0.0;
    double green_x = 0.0, green_y = 0.0;
    double blue_x = 0.0, blue_y = 0.0;
    if (png_get_cHRM(read_ptr, read_info, &white_x, &white_y, &red_x, &red_y,
                     &green_x, &green_y, &blue_x, &blue_y))
        png_set_cHRM(write_ptr, write_info, white_x, white_y, red_x, red_y,
                     green_x, green_y, blue_x, blue_y);

#ifdef PNG_cICP_SUPPORTED
    png_byte primaries = 0, transfer = 0, matrix = 0, full_range = 0;
    if (png_get_cICP(read_ptr, read_info, &primaries, &transfer, &matrix,
                     &full_range))
        png_set_cICP(write_ptr, write_info, primaries, transfer, matrix,
                     full_range);
#endif

#ifdef PNG_cLLI_SUPPORTED
    png_uint_32 max_content_light = 0, max_frame_average_light = 0;
    if (png_get_cLLI_fixed(read_ptr, read_info, &max_content_light,
                           &max_frame_average_light))
        png_set_cLLI_fixed(write_ptr, write_info, max_content_light,
                           max_frame_average_light);
#endif

#ifdef PNG_mDCV_SUPPORTED
    png_fixed_point mastering_white_x = 0, mastering_white_y = 0;
    png_fixed_point mastering_red_x = 0, mastering_red_y = 0;
    png_fixed_point mastering_green_x = 0, mastering_green_y = 0;
    png_fixed_point mastering_blue_x = 0, mastering_blue_y = 0;
    png_uint_32 max_luminance = 0, min_luminance = 0;
    if (png_get_mDCV_fixed(read_ptr, read_info, &mastering_white_x,
                           &mastering_white_y, &mastering_red_x,
                           &mastering_red_y, &mastering_green_x,
                           &mastering_green_y, &mastering_blue_x,
                           &mastering_blue_y, &max_luminance, &min_luminance))
        png_set_mDCV_fixed(write_ptr, write_info, mastering_white_x,
                           mastering_white_y, mastering_red_x, mastering_red_y,
                           mastering_green_x, mastering_green_y,
                           mastering_blue_x, mastering_blue_y, max_luminance,
                           min_luminance);
#endif

    png_color_8p significant_bits = NULL;
    if (png_get_sBIT(read_ptr, read_info, &significant_bits))
        png_set_sBIT(write_ptr, write_info, significant_bits);

    png_color_16p background = NULL;
    if (png_get_bKGD(read_ptr, read_info, &background))
        png_set_bKGD(write_ptr, write_info, background);

    if (png_get_color_type(read_ptr, read_info) == PNG_COLOR_TYPE_PALETTE) {
        png_colorp palette = NULL;
        int palette_entries = 0;
        if (!png_get_PLTE(read_ptr, read_info, &palette, &palette_entries) ||
            !palette || palette_entries <= 0) {
            LOG_ERROR("Palette PNG is missing a valid PLTE chunk.");
            return false;
        }
        png_set_PLTE(write_ptr, write_info, palette, palette_entries);
    }

    png_bytep trans_alpha = NULL;
    int trans_entries = 0;
    png_color_16p trans_color = NULL;
    if (png_get_tRNS(read_ptr, read_info, &trans_alpha, &trans_entries,
                     &trans_color))
        png_set_tRNS(write_ptr, write_info, trans_alpha, trans_entries,
                     trans_color);

    png_uint_32 res_x = 0, res_y = 0;
    int res_unit = 0;
    if (png_get_pHYs(read_ptr, read_info, &res_x, &res_y, &res_unit))
        png_set_pHYs(write_ptr, write_info, res_x, res_y, res_unit);

    png_int_32 offset_x = 0, offset_y = 0;
    int offset_unit = 0;
    if (png_get_oFFs(read_ptr, read_info, &offset_x, &offset_y, &offset_unit))
        png_set_oFFs(write_ptr, write_info, offset_x, offset_y, offset_unit);

    png_uint_16p histogram = NULL;
    if (png_get_hIST(read_ptr, read_info, &histogram))
        png_set_hIST(write_ptr, write_info, histogram);

    png_uint_32 exif_length = 0;
    png_bytep exif_data = NULL;
    if (png_get_eXIf_1(read_ptr, read_info, &exif_length, &exif_data))
        png_set_eXIf_1(write_ptr, write_info, exif_length, exif_data);

    png_textp text = NULL;
    int text_count = 0;
    if (png_get_text(read_ptr, read_info, &text, &text_count) > 0)
        png_set_text(write_ptr, write_info, text, text_count);

    png_timep mod_time = NULL;
    if (png_get_tIME(read_ptr, read_info, &mod_time))
        png_set_tIME(write_ptr, write_info, mod_time);

    png_sPLT_tp suggested_palettes = NULL;
    int suggested_palette_count = png_get_sPLT(read_ptr, read_info,
                                                &suggested_palettes);
    if (suggested_palette_count > 0)
        png_set_sPLT(write_ptr, write_info, suggested_palettes,
                     suggested_palette_count);

    png_charp purpose = NULL;
    png_int_32 x0 = 0, x1 = 0;
    int calibration_type = 0, parameter_count = 0;
    png_charp units = NULL;
    png_charpp parameters = NULL;
    if (png_get_pCAL(read_ptr, read_info, &purpose, &x0, &x1,
                     &calibration_type, &parameter_count, &units, &parameters))
        png_set_pCAL(write_ptr, write_info, purpose, x0, x1, calibration_type,
                     parameter_count, units, parameters);

    int scale_unit = 0;
    png_charp scale_width = NULL, scale_height = NULL;
    if (png_get_sCAL_s(read_ptr, read_info, &scale_unit, &scale_width,
                       &scale_height))
        png_set_sCAL_s(write_ptr, write_info, scale_unit, scale_width,
                       scale_height);

    png_unknown_chunkp unknown_chunks = NULL;
    int unknown_count = png_get_unknown_chunks(read_ptr, read_info,
                                                &unknown_chunks);
    if (unknown_count > 0) {
        std::vector<png_unknown_chunk> preserved_chunks;
        preserved_chunks.reserve(static_cast<size_t>(unknown_count));
        for (int i = 0; i < unknown_count; ++i) {
            if (memcmp(unknown_chunks[i].name, "iCCP", 4) != 0)
                preserved_chunks.push_back(unknown_chunks[i]);
        }
        if (!preserved_chunks.empty()) {
            png_set_unknown_chunks(write_ptr, write_info, preserved_chunks.data(),
                                   static_cast<int>(preserved_chunks.size()));
            for (size_t i = 0; i < preserved_chunks.size(); ++i)
                png_set_unknown_chunk_location(write_ptr, write_info,
                                               static_cast<int>(i),
                                               preserved_chunks[i].location);
        }
    }
    return true;
}

static bool InjectIccProfileData(const char* inputPng,
                                 const char* outputPng,
                                 const unsigned char* iccData,
                                 png_uint_32 iccDataSize)
{
    FILE* fpIn = fopen(inputPng, "rb");
    if (!fpIn) {
        LOG_ERROR("Failed to open input PNG file.");
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fpIn);
        LOG_ERROR("Failed to create PNG read struct.");
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fpIn);
        LOG_ERROR("Failed to create PNG info struct.");
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fpIn);
        LOG_ERROR("LibPNG header read error.");
        return false;
    }

    png_set_user_limits(png_ptr, kMaxPngDimension, kMaxPngDimension);
    png_set_chunk_malloc_max(png_ptr, kMaxPngChunkBytes);
    png_set_chunk_cache_max(png_ptr, 1000);
    png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_IF_SAFE, NULL, 0);
    png_init_io(png_ptr, fpIn);
    png_read_info(png_ptr, info_ptr);

    std::string tempOutputPng;
    FILE* volatile fpOut = OpenPngTempOutputFile(outputPng, tempOutputPng);
    if (!fpOut) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fpIn);
        LOG_ERROR("Failed to open temporary output PNG file.");
        return false;
    }

    png_structp write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!write_ptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("Failed to create PNG write struct.");
        return false;
    }

    png_infop write_info_ptr = png_create_info_struct(write_ptr);
    if (!write_info_ptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        png_destroy_write_struct(&write_ptr, NULL);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("Failed to create PNG write info struct.");
        return false;
    }

    png_infop read_end_info_ptr = png_create_info_struct(png_ptr);
    png_infop write_end_info_ptr = png_create_info_struct(write_ptr);
    if (!read_end_info_ptr || !write_end_info_ptr) {
        png_destroy_read_struct(&png_ptr, &info_ptr,
                                read_end_info_ptr ? &read_end_info_ptr : NULL);
        if (write_end_info_ptr)
            png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("Failed to create PNG end info struct.");
        return false;
    }

    png_bytepp volatile row_pointers = NULL;
    volatile png_uint_32 rowsAllocated = 0;
    if (setjmp(png_jmpbuf(write_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &read_end_info_ptr);
        for (png_uint_32 y = 0; y < rowsAllocated; ++y)
            free(row_pointers[y]);
        free(row_pointers);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("LibPNG write error.");
        return false;
    }

    png_init_io(write_ptr, fpOut);
    png_set_compression_level(write_ptr, Z_BEST_COMPRESSION);

    // MUST propagate original info to write_info_ptr before setting ICC
    png_set_rows(write_ptr, write_info_ptr, nullptr);  // optional
    png_set_IHDR(write_ptr, write_info_ptr,
                 png_get_image_width(png_ptr, info_ptr),
                 png_get_image_height(png_ptr, info_ptr),
                 png_get_bit_depth(png_ptr, info_ptr),
                 png_get_color_type(png_ptr, info_ptr),
                 png_get_interlace_type(png_ptr, info_ptr),
                 png_get_compression_type(png_ptr, info_ptr),
                 png_get_filter_type(png_ptr, info_ptr));

    if (!CopyPngAncillaryChunks(png_ptr, info_ptr, write_ptr, write_info_ptr)) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        return false;
    }

    // only now is it safe to attach ICC
    png_set_iCCP(write_ptr, write_info_ptr, "icc", 0, iccData, iccDataSize);

    // finally write header
    png_write_info(write_ptr, write_info_ptr);


    // Re-read the input image data and write to output
    const png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    const png_size_t rowBytes = png_get_rowbytes(png_ptr, info_ptr);
    if (height == 0 || rowBytes == 0 ||
        static_cast<size_t>(rowBytes) > kMaxPngRasterBytes / height) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &read_end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("PNG raster exceeds the 256 MiB processing limit.");
        return false;
    }
    row_pointers = static_cast<png_bytepp>(calloc(height, sizeof(png_bytep)));
    if (!row_pointers) {
        png_destroy_read_struct(&png_ptr, &info_ptr, &read_end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("Failed to allocate PNG row pointers.");
        return false;
    }

    for (png_uint_32 y = 0; y < height; y++) {
        row_pointers[y] = static_cast<png_bytep>(malloc(rowBytes));
        if (!row_pointers[y]) {
            for (png_uint_32 i = 0; i < rowsAllocated; i++) {
                free(row_pointers[i]);
            }
            free(row_pointers);
            png_destroy_read_struct(&png_ptr, &info_ptr, &read_end_info_ptr);
            png_destroy_info_struct(write_ptr, &write_end_info_ptr);
            png_destroy_write_struct(&write_ptr, &write_info_ptr);
            fclose(fpIn); fclose(fpOut);
            remove(tempOutputPng.c_str());
            LOG_ERROR("Failed to allocate PNG row.");
            return false;
        }
        rowsAllocated++;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        for (png_uint_32 y = 0; y < rowsAllocated; y++) {
            free(row_pointers[y]);
        }
        free(row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, &read_end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        fclose(fpIn); fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("LibPNG image read error.");
        return false;
    }

    png_read_image(png_ptr, row_pointers);
    png_write_image(write_ptr, row_pointers);
    png_read_end(png_ptr, read_end_info_ptr);

    png_textp end_text = NULL;
    int end_text_count = 0;
    if (png_get_text(png_ptr, read_end_info_ptr, &end_text, &end_text_count) > 0)
        png_set_text(write_ptr, write_end_info_ptr, end_text, end_text_count);

    png_timep end_mod_time = NULL;
    if (png_get_tIME(png_ptr, read_end_info_ptr, &end_mod_time))
        png_set_tIME(write_ptr, write_end_info_ptr, end_mod_time);

    png_uint_32 end_exif_length = 0;
    png_bytep end_exif_data = NULL;
    if (png_get_eXIf_1(png_ptr, read_end_info_ptr, &end_exif_length,
                       &end_exif_data))
        png_set_eXIf_1(write_ptr, write_end_info_ptr, end_exif_length,
                       end_exif_data);

    png_unknown_chunkp end_unknown_chunks = NULL;
    int end_unknown_count = png_get_unknown_chunks(png_ptr, read_end_info_ptr,
                                                   &end_unknown_chunks);
    if (end_unknown_count > 0) {
        png_set_unknown_chunks(write_ptr, write_end_info_ptr,
                               end_unknown_chunks, end_unknown_count);
        for (int i = 0; i < end_unknown_count; ++i)
            png_set_unknown_chunk_location(write_ptr, write_end_info_ptr, i,
                                           end_unknown_chunks[i].location);
    }

    png_write_end(write_ptr, write_end_info_ptr);

    // Cleanup
    for (png_uint_32 y = 0; y < rowsAllocated; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);

    png_destroy_read_struct(&png_ptr, &info_ptr, &read_end_info_ptr);
    png_destroy_info_struct(write_ptr, &write_end_info_ptr);
    png_destroy_write_struct(&write_ptr, &write_info_ptr);
    fclose(fpIn);
    if (!SetPngOutputPermissions(fpOut, outputPng)) {
        fclose(fpOut);
        remove(tempOutputPng.c_str());
        LOG_ERROR("Failed to set output PNG permissions.");
        return false;
    }
    if (!icFlushAndClose(fpOut)) {
        remove(tempOutputPng.c_str());
        LOG_ERROR("Failed to close output PNG file.");
        return false;
    }
    if (!CommitPngTempOutputFile(tempOutputPng, outputPng)) {
        remove(tempOutputPng.c_str());
        LOG_ERROR("Failed to commit output PNG file.");
        return false;
    }
    return true;
}

/**
 * Injects a new ICC profile into a PNG image and writes the output.
 *
 * @param inputPng Path to the source PNG file.
 * @param iccFile Path to the ICC file to embed.
 * @param outputPng Path to write the modified PNG file.
 * @return true on success, false on failure.
 */
bool InjectIccProfile(const std::string& inputPng,
                      const std::string& iccFile,
                      const std::string& outputPng) {
    std::ifstream iccIn(iccFile, std::ios::binary);
    if (!iccIn.is_open()) {
        LOG_ERROR("Failed to open ICC profile file for reading.");
        return false;
    }

    std::vector<unsigned char> iccData;
    if (!ReadBinaryStream(iccIn, iccData)) {
        LOG_ERROR("Failed to read ICC profile file.");
        return false;
    }
    if (iccData.empty() || iccData.size() > std::numeric_limits<png_uint_32>::max()) {
        LOG_ERROR("Invalid ICC profile size for PNG iCCP chunk.");
        return false;
    }

    return InjectIccProfileData(inputPng.c_str(), outputPng.c_str(), iccData.data(),
                                static_cast<png_uint_32>(iccData.size()));
}

// =====================================================================
// Function: ExtractIccProfile
// Description: Extracts ICC profile data from a PNG image
// =====================================================================

/**
 * Extracts the embedded ICC profile from a PNG image.
 *
 * @param png_ptr    Pointer to the PNG read structure.
 * @param info_ptr   Pointer to the PNG info structure.
 * @param pProfMem   Output pointer to store the ICC profile data.
 * @param nLen       Output variable to store the profile data length.
 * @return           1 if an ICC profile is found and extracted successfully, 0 otherwise.
 */
int ExtractIccProfile(png_structp png_ptr, png_infop info_ptr, unsigned char **pProfMem, unsigned int *nLen) {
    png_charp profile_name = NULL;
    int compression_type = 0;
    png_bytep profile_data_raw = NULL;
    png_uint_32 profile_length = 0;

    if (png_get_iCCP(png_ptr, info_ptr, &profile_name, &compression_type,
                     &profile_data_raw, &profile_length)) {

        if (profile_length == 0) {
            LOG_ERROR("Empty ICC profile found.");
            return 0;
        }

        *nLen = profile_length;
        *pProfMem = static_cast<unsigned char*>(malloc(profile_length));
        if (!(*pProfMem)) {
            LOG_ERROR("Memory allocation for ICC profile failed.");
            return 0;
        }

        memcpy(*pProfMem, profile_data_raw, profile_length);
        printf("[INFO] ICC Profile extracted: %u bytes\n", profile_length);

        return 1;
    }

    return 0;
}

// =====================================================================
// Function: PrintPngInfo
// Description: Displays detailed PNG metadata
// =====================================================================

/**
 * Displays detailed metadata information about a PNG image.
 *
 * @param png_ptr   Pointer to the PNG read structure.
 * @param info_ptr  Pointer to the PNG info structure.
 */
void PrintPngInfo(png_structp png_ptr, png_infop info_ptr) {
    // Retrieve PNG metadata
    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);
    int interlace_type = png_get_interlace_type(png_ptr, info_ptr);

    printf("--------------------> PNG Image Metadata <---------------------------\n");
    printf("Size:              (%d x %d) pixels\n", width, height);
    printf("Bit Depth:         %d\n", bit_depth);

    // Decode color type
    printf("Color Type:        ");
    switch (color_type) {
        case PNG_COLOR_TYPE_GRAY:
            printf("Grayscale\n");
            break;
        case PNG_COLOR_TYPE_RGB:
            printf("RGB (Truecolor)\n");
            break;
        case PNG_COLOR_TYPE_PALETTE:
            printf("Indexed Color (Palette)\n");
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            printf("Grayscale with Alpha\n");
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            printf("RGBA (Truecolor with Alpha)\n");
            break;
        default:
            printf("Unknown (%d)\n", color_type);
            break;
    }

    // Decode interlace type
    printf("Interlace Type:    ");
    switch (interlace_type) {
        case PNG_INTERLACE_NONE:
            printf("None\n");
            break;
        case PNG_INTERLACE_ADAM7:
            printf("Adam7 Interlacing\n");
            break;
        default:
            printf("Unknown (%d)\n", interlace_type);
            break;
    }

    printf("--------------------------------------------------------------------\n");
}

// =====================================================================
// Function: PrintIccProfileInfo
// Description: Parses and displays ICC profile data, and optionally saves it
// =====================================================================

/**
 * Parses and displays ICC profile metadata, and optionally saves it to a file.
 *
 * @param pProfMem   Pointer to the ICC profile data.
 * @param nLen       Length of the ICC profile data in bytes.
 * @param outputFile Path to the output file for saving the ICC profile (optional).
 */
bool PrintIccProfileInfo(const unsigned char *pProfMem, unsigned int nLen, const char *outputFile) {
    printf("--------------------> ICC Profile Information <---------------------------\n");
    printf("Profile Size:      %u bytes\n", nLen);

    if (outputFile) {
        FILE *outFile = OpenPngOutputFile(outputFile);
        if (!outFile) {
            LOG_ERROR("Unable to open output file for writing.");
            return false;
        }
        if (fwrite(pProfMem, 1, nLen, outFile) != nLen) {
            LOG_ERROR("Failed to write ICC profile.");
            fclose(outFile);
            return false;
        }
        if (!icFlushAndClose(outFile)) {
            LOG_ERROR("Failed to close ICC profile output file.");
            return false;
        }
        printf("[INFO] Raw ICC Profile saved to: %s\n",
               icSanitizeConsoleText(outputFile).c_str());
    }

    // Attempt to parse the ICC profile
    CIccProfile *pProfile = OpenIccProfile(pProfMem, nLen);
    if (!pProfile) {
        LOG_ERROR("Failed to parse ICC Profile.");
        return false;
    }

    // Extract ICC header details
    icHeader *pHdr = &pProfile->m_Header;
    CIccInfo Fmt;

    printf(" Color Space:      %s\n", Fmt.GetColorSpaceSigName(pHdr->colorSpace));
    printf(" Colorimetric PCS: %s\n", Fmt.GetColorSpaceSigName(pHdr->pcs));
    printf(" Profile Version:  %u.%u.%u\n",
           (unsigned int) ((pHdr->version >> 24) & 0xFF),   // Major version
           (unsigned int) ((pHdr->version >> 20) & 0x0F),   // Minor version
           (unsigned int) ((pHdr->version >> 16) & 0x0F));  // Sub-minor version

    delete pProfile;
    
    printf("--------------------------------------------------------------------\n");
    return true;
}
