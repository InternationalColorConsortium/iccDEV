/*
    File:       iccPngDump.cpp

    Contains:   Console app display info about PNG file and its ICC profile

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
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <png.h>
#include "IccCmm.h"
#include "IccUtil.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include <zlib.h>

// Handle platform-specific png_get_iCCP() profile pointer types
#if defined(__APPLE__) && defined(__clang__)
    #define PNG_ICC_PROFILE_CAST (png_charpp)
    typedef png_charp png_icc_profilep;
#else
    #define PNG_ICC_PROFILE_CAST (png_bytepp)
    typedef png_bytep png_icc_profilep;
#endif

// Platform-specific trap macro for debugging fatal errors
#ifdef __x86_64__
    #define TRAP() asm volatile ("ud2")
#elif defined(__aarch64__)
    #define TRAP() asm volatile ("brk #0")
#else
    #define TRAP() abort()
#endif

// Logging macros for error handling
#define LOG_ERROR(msg) do { fprintf(stderr, "[ERROR] %s\n", msg); } while (0)
#define BAIL_OUT(msg) do { LOG_ERROR(msg); TRAP(); } while (0)


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
void PrintIccProfileInfo(const unsigned char *pProfMem, unsigned int nLen, const char *outputFile);

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
        safe_exit("Missing input file argument.");
    }

    const char *inputFile = NULL;
    const char *outputIccFile = NULL;
    const char *injectIccFile = NULL;
    const char *outputPngFile = NULL;

    // Simple CLI arg parsing
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--write-icc") == 0 && i + 1 < argc) {
            injectIccFile = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputPngFile = argv[++i];
        } else if (!inputFile) {
            inputFile = argv[i];
        } else if (!outputIccFile) {
            outputIccFile = argv[i];
        }
    }

    if (!inputFile) {
        Usage();
        safe_exit("Input PNG file not specified.");
    }

// --- Injection Mode ---
if (injectIccFile) {
    if (!outputPngFile) {
        safe_exit("Missing --output argument for write mode.");
    }

    printf("[INFO] Injecting ICC profile '%s' into PNG: '%s'\n", injectIccFile, outputPngFile);

    if (!InjectIccProfile(inputFile, injectIccFile, outputPngFile)) {
        safe_exit("Failed to inject ICC profile.");
    }

    printf("[INFO] Injection successful.\n");
    return 0;
}

    // --- Extraction Mode ---
    printf("[INFO] Opening PNG file: %s\n", inputFile);
    FILE *fp = fopen(inputFile, "rb");
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

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    PrintPngInfo(png_ptr, info_ptr);

    unsigned char *pProfMem = NULL;
    unsigned int nLen = 0;
    if (ExtractIccProfile(png_ptr, info_ptr, &pProfMem, &nLen)) {
        PrintIccProfileInfo(pProfMem, nLen, outputIccFile);
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
        safe_exit("Failed to open ICC profile file for reading.");
    }

    std::vector<unsigned char> iccData((std::istreambuf_iterator<char>(iccIn)),
                                       std::istreambuf_iterator<char>());

    FILE* fpIn = fopen(inputPng.c_str(), "rb");
    if (!fpIn) {
        safe_exit("Failed to open input PNG file.");
    }

    FILE* fpOut = fopen(outputPng.c_str(), "wb");
    if (!fpOut) {
        fclose(fpIn);
        safe_exit("Failed to open output PNG file.");
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fpIn); fclose(fpOut);
        safe_exit("Failed to create PNG read struct.");
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fpIn); fclose(fpOut);
        safe_exit("Failed to create PNG info struct.");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fpIn); fclose(fpOut);
        safe_exit("LibPNG read error.");
    }

    png_init_io(png_ptr, fpIn);
    png_read_info(png_ptr, info_ptr);

    png_structp write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop write_info_ptr = png_create_info_struct(write_ptr);
    if (!write_ptr || !write_info_ptr) {
        safe_exit("Failed to create PNG write structures.");
    }

    if (setjmp(png_jmpbuf(write_ptr))) {
        safe_exit("LibPNG write error.");
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

// only now is it safe to attach ICC
// TODO - this should have a size check to make sure the profile is less than 2 Gig (32 bit api limit)
png_set_iCCP(write_ptr, write_info_ptr, "icc", 0, iccData.data(), (png_uint_32)iccData.size());

// finally write header
png_write_info(write_ptr, write_info_ptr);


    // Re-read the input image data and write to output
    png_bytepp row_pointers = (png_bytepp)png_malloc(png_ptr,
        sizeof(png_bytep) * png_get_image_height(png_ptr, info_ptr));

    for (png_uint_32 y = 0; y < png_get_image_height(png_ptr, info_ptr); y++) {
        row_pointers[y] = (png_bytep)png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));
    }

    png_read_image(png_ptr, row_pointers);
    png_write_image(write_ptr, row_pointers);
    png_write_end(write_ptr, NULL);

    // Cleanup
    for (png_uint_32 y = 0; y < png_get_image_height(png_ptr, info_ptr); y++) {
        png_free(png_ptr, row_pointers[y]);
    }
    png_free(png_ptr, row_pointers);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    png_destroy_write_struct(&write_ptr, &write_info_ptr);
    fclose(fpIn);
    fclose(fpOut);
    return true;
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
                     (png_bytepp)&profile_data_raw, &profile_length)) {

        if (profile_length == 0) {
            LOG_ERROR("Empty ICC profile found.");
            return 0;
        }

        *nLen = profile_length;
        *pProfMem = (unsigned char *)malloc(profile_length);
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
void PrintIccProfileInfo(const unsigned char *pProfMem, unsigned int nLen, const char *outputFile) {
    printf("--------------------> ICC Profile Information <---------------------------\n");
    printf("Profile Size:      %u bytes\n", nLen);

    // Attempt to parse the ICC profile
    CIccProfile *pProfile = OpenIccProfile(pProfMem, nLen);
    if (!pProfile) {
        LOG_ERROR("Failed to parse ICC Profile.");
        return;
    }

    // Extract ICC header details
    icHeader *pHdr = &pProfile->m_Header;
    CIccInfo Fmt;

    printf(" Color Space:      %s\n", Fmt.GetColorSpaceSigName(pHdr->colorSpace));
    printf(" Colorimetric PCS: %s\n", Fmt.GetColorSpaceSigName(pHdr->pcs));
    printf(" Profile Version:  %d.%d.%d\n", 
           (pHdr->version >> 24) & 0xFF,   // Major version
           (pHdr->version >> 20) & 0x0F,   // Minor version
           (pHdr->version >> 16) & 0x0F);  // Sub-minor version

    delete pProfile;

    // If an output file is specified, save the ICC profile
    if (outputFile) {
        FILE *outFile = fopen(outputFile, "wb");
        if (!outFile) {
            LOG_ERROR("Unable to open output file for writing.");
            return;
        }
        fwrite(pProfMem, 1, nLen, outFile);
        fclose(outFile);
        printf("[INFO] ICC Profile saved to: %s\n", outputFile);
    }
    
    printf("--------------------------------------------------------------------\n");
}
