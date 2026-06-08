//
//  MiniTIFF.hpp
//

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in
 *  the documentation and/or other materials provided with the
 *  distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *  International Color Consortium" must not be used to imply that the
 *  ICC organization endorses or promotes products derived from this
 *  software.
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


#ifndef MiniTIFF_hpp
#define MiniTIFF_hpp

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>

/******************************************************************************/

// tag types
enum {
  TIFF_BYTE = 1,
  TIFF_ASCII = 2,
  TIFF_SHORT = 3,
  TIFF_LONG = 4,
  TIFF_RATIO = 5,   // 2 longs
  TIFF_SBYTE = 6,
  TIFF_UNDEFINED = 7,
  TIFF_SSHORT = 8,
  TIFF_SLONG = 9,
  TIFF_SRATIO = 10,
  TIFF_FLOAT = 11,
  TIFF_DOUBLE = 12,
};

// tag values
enum {
  TIFF_SUBFILETYPE = 254,     //   usually 0
  TIFF_WIDTH = 256,         // required
  TIFF_HEIGHT = 257,        // required
  TIFF_BITSPERSAMPLE = 258,     // required
  TIFF_COMPRESSION = 259,     // required
  TIFF_INTERPRETATION = 262,    // required

  TIFF_STRIPOFFSETS = 273,    // required
  TIFF_SAMPLESPERPIXEL = 277,   // required
  TIFF_ROWSPERSTRIP = 278,    // required
  TIFF_STRIPBYTECOUNTS = 279,   // required

  TIFF_XRESOLUTION = 282,     // required
  TIFF_YRESOLUTION = 283,     // required
  TIFF_PLANARCONFIG = 284,    // required for > 1 channel, 1 = interleaved, 2 = not interleaved
  TIFF_RESOLUTIONUNIT = 296,    // required, 1=no unit, 2 = inch, 3 = cm

  TIFF_PREDICTOR = 317,       // 1 = none, 2 = horizontal difference, 3 = floating point
  TIFF_COLORMAP = 320,      // indexed color only
  TIFF_SAMPLE_FORMAT = 339,     // 1 = unsigned, 2 = signed, 3 = float, 4 = undefined
};

enum {
  TIFF_SAMPLE_UINT = 1,
  TIFF_SAMPLE_SINT = 2,
  TIFF_SAMPLE_FLOAT = 3,
  TIFF_SAMPLE_UNDEFINED = 4,
};

enum {
  TIFF_COMPRESS_NONE = 1,
  TIFF_COMPRESS_LZW = 5,
  // 6 was a bad JPEG attempt, and should not be used
  TIFF_COMPRESS_JPEG = 7,
  TIFF_COMPRESS_DEFLATE = 8,
};

enum {
  TIFF_MODE_GRAY_WHITEZERO = 0,
  TIFF_MODE_GRAY_BLACKZERO = 1,
  TIFF_MODE_RGB = 2,
  TIFF_MODE_RGB_PALETTE = 3,
  TIFF_MODE_MASK = 4,
  TIFF_MODE_CMYK = 5,
  TIFF_MODE_YCbCr = 6,
  TIFF_MODE_CIELAB = 8,
};

/******************************************************************************/

bool WriteTIFF( const std::string &name, float dpi, int color_model, uint8_t *buffer,
                size_t width, size_t height, int channels, int depth );

/******************************************************************************/

#endif /* MiniTIFF_hpp */
