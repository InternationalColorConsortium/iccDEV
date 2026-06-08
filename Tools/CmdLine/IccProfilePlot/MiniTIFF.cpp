//
//  MiniTIFF.cpp
//
//  Writes a subset of TIFF files, uncompressed.

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

#include <cstdio>
#include <cstdint>
#include <climits>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>
#include <memory>
#include <limits>
#include "../IccCmdLineUtil.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "MiniTIFF.hpp"

/******************************************************************************/

static
FILE* icOpenWriteBinaryFile(const char* szFname)
{
  return icOpenRegularWriteBinaryFile(szFname);
}

/******************************************************************************/

// NOTE - we don't have to worry about byte order, because the TIFF will always be written in host byte order
// returns true if the write failed, otherwise false
static
bool putByte( uint8_t val, FILE *out )
{
  return fputc( val, out ) != val;
}

static
bool putShort( uint16_t val, FILE *out )
{
  return fwrite( &val, 2, 1, out ) != 1;
}

static
bool putShort( int16_t val, FILE *out )
{
  return fwrite( &val, 2, 1, out ) != 1;
}

static
bool putLong( uint32_t val, FILE *out )
{
  return fwrite( &val, 4, 1, out ) != 1;
}

static
bool putLong( int32_t val, FILE *out )
{
  return fwrite( &val, 4, 1, out ) != 1;
}

#if 0
// not supporting BIGTIFF yet, and don't need any other 8 byte quantities
static
bool putLongLong( uint64_t val, FILE *out )
{
  return fwrite( &val, 8, 1, out ) != 1;
}

static
bool putLongLong( int64_t val, FILE *out )
{
  return fwrite( &val, 8, 1, out ) != 1;
}
#endif

/******************************************************************************/

// programmers prefer big endian because it is much eaiser to read and debug
static
bool isMachineBigEndian()
{
  const uint32_t testLong = 0x01020304;
  const uint8_t *testChar = (uint8_t *)&testLong;
  return (*testChar == 0x01);
}

// We can hardcode these with <bit> in C++20
static bool bigEndian = isMachineBigEndian();

/******************************************************************************/

// unsigned 0..128..255 -> signed -127..0..127
static
void shiftTIFFLAB( uint8_t *in, size_t count )
{
  for ( size_t i = 0; i < count; ++i ) {
    size_t index = 3*i;
    uint8_t l = in[index+0];
    int a = in[index+1];
    int b = in[index+2];
    in[index+0] = l; // just copy
    in[index+1] = uint8_t(a - 128);
    in[index+2] = uint8_t(b - 128);
  }
}

/******************************************************************************/

// unsigned 0..32768..65535 -> signed -32767..0..32767
static
void shiftTIFFLAB( uint16_t *in, size_t count )
{
  for ( size_t i = 0; i < count; ++i ) {
    size_t index = 3*i;
    uint16_t l = in[index+0];
    int a = in[index+1];
    int b = in[index+2];

    in[index+0] = l; // just copy
    in[index+1] = uint16_t(a - 0x8000);
    in[index+2] = uint16_t(b - 0x8000);
  }
}

/******************************************************************************/

// returns true if write failed
static
bool putIFDLong( uint16_t tag, uint16_t type, uint32_t count, uint32_t value, FILE *out )
{
  uint16_t tagval = tag;
  uint16_t typeval = type;
  uint32_t countval = count;

  if (fwrite( &tagval, 2, 1, out ) != 1)
    return true;
  if (fwrite( &typeval, 2, 1, out ) != 1)
    return true;
  if (fwrite( &countval, 4, 1, out ) != 1)
    return true;
  if (fwrite( &value, 4, 1, out ) != 1)
    return true;
  
  return false;
}

/******************************************************************************/

/// Write the image buffer to a TIFF (.tif) file
bool WriteTIFF( const std::string &name, float dpi, int color_model, uint8_t *buffer,
        size_t width, size_t height, int channels, int depth )
{
  FILE *outfile = NULL;
  bool writeFailed = false;

  // see if we can create or update this filename
  outfile = icOpenWriteBinaryFile(name.c_str());
  if(outfile==NULL) {
    fprintf(stderr,"Could not create output file %s\n", name.c_str());
    return false;
  }

  // TIFF header, and byte order indicator
  // if constexpr (std::endian::native == std::endian::big) {  // only works in C++20 and up
  if (bigEndian) {
    writeFailed |= putByte('M',outfile);
    writeFailed |= putByte('M',outfile);
  } else {
    writeFailed |= putByte('I',outfile);
    writeFailed |= putByte('I',outfile);
  }

  writeFailed |= putShort( (int16_t)42, outfile );
  writeFailed |= putLong( (int32_t)8, outfile );  // offset to first IFD, from start of file

  // check for early failure
  if (writeFailed || ferror(outfile)) {
    fprintf(stderr, "ERROR: I/O error writing TIFF header to %s\n", name.c_str() );
    fclose(outfile);
    return false;
  }

  // IFD
  // number of entries
// TODO: make a data structure to store IFD entries, sort, then write values and offsets
  uint16_t tagCount = 15;
  writeFailed |= putShort( tagCount, outfile );
  

  uint32_t width32 = uint32_t(width);
  writeFailed |= putIFDLong( TIFF_WIDTH, TIFF_LONG, 1, width32, outfile );
  uint32_t height32 = uint32_t(height);
  writeFailed |= putIFDLong( TIFF_HEIGHT, TIFF_LONG, 1, height32, outfile );

  uint16_t bits = (uint16_t) depth;

  uint32_t ifd_end = 8 + 2 + 4 + tagCount*12;
  uint32_t align_bytes = (4 - (ifd_end & 0x03)) & 0x03;
  uint32_t start_data = ifd_end + align_bytes;   // align to 4 byte boundary

  uint32_t bits_offset = start_data;
  uint32_t xres_offset = bits_offset + (uint32_t)channels*2;
  uint32_t yres_offset = xres_offset + 8;

  // some readers break if the bitsPerSample is not a short value
  if (channels == 1)
    writeFailed |= putIFDLong( TIFF_BITSPERSAMPLE, TIFF_LONG, 1, bits, outfile );
  else if (channels == 2)
    writeFailed |= putIFDLong( TIFF_BITSPERSAMPLE, TIFF_SHORT, (uint32_t)channels, ((uint32_t)bits << 16) | bits, outfile );
  else
    writeFailed |= putIFDLong( TIFF_BITSPERSAMPLE, TIFF_SHORT, (uint32_t)channels, bits_offset, outfile );

  writeFailed |= putIFDLong( TIFF_COMPRESSION, TIFF_LONG, 1, TIFF_COMPRESS_NONE, outfile );

  size_t nrowBytes = ( (size_t)channels * (size_t)width * (size_t)depth + 7) / 8;
  assert( nrowBytes > 0 );
  size_t rowsPerBuffer = height;
  size_t stripCount = 1;        // may be changed later

  size_t rowsPerStrip = rowsPerBuffer;
  //size_t stripBytes = rowsPerStrip * nrowBytes;

  uint32_t stripOffset_offset = yres_offset + 8;

  size_t stripCountSize = stripCount * 4;
  assert( (stripOffset_offset + stripCountSize) <= UINT_MAX );
  uint32_t stripByteCount_offset = uint32_t( stripOffset_offset + stripCountSize );

  assert( (stripByteCount_offset + stripCountSize) <= UINT_MAX );
  uint32_t pixelData_offset = uint32_t ( stripByteCount_offset + stripCountSize );

  writeFailed |= putIFDLong( TIFF_INTERPRETATION, TIFF_LONG, 1, (uint32_t)color_model, outfile );

  // using an offset for offsets and bytecounts trips up tiffinfo
  if (stripCount > 1)
    writeFailed |= putIFDLong( TIFF_STRIPOFFSETS, TIFF_LONG, uint32_t(stripCount), stripOffset_offset, outfile );
  else
    writeFailed |= putIFDLong( TIFF_STRIPOFFSETS, TIFF_LONG, 1, pixelData_offset, outfile );

  writeFailed |= putIFDLong( TIFF_SAMPLESPERPIXEL, TIFF_LONG, 1, (uint32_t)channels, outfile );
  writeFailed |= putIFDLong( TIFF_ROWSPERSTRIP, TIFF_LONG, 1, uint32_t(rowsPerStrip), outfile );

  long byteCountOffset = ftell( outfile );
  if (byteCountOffset < 0) {
    fprintf(stderr, "ERROR: TIFF ftell failed\n");
    (void)fclose(outfile);
    return false;
  }
  if (stripCount > 1)
    writeFailed |= putIFDLong( TIFF_STRIPBYTECOUNTS, TIFF_LONG, uint32_t(stripCount), stripByteCount_offset, outfile );
  else
    writeFailed |= putIFDLong( TIFF_STRIPBYTECOUNTS, TIFF_LONG, 1, 0, outfile );

  uint32_t resDenom32 = 1000;
  if ((dpi * resDenom32) > (float) std::numeric_limits<uint32_t>::max()) {
        dpi = 96.0f;
  }
  uint32_t resRatio32 = (uint32_t)(dpi * resDenom32);
  
  writeFailed |= putIFDLong( TIFF_XRESOLUTION, TIFF_RATIO, 1, xres_offset, outfile );
  writeFailed |= putIFDLong( TIFF_YRESOLUTION, TIFF_RATIO, 1, yres_offset, outfile );
  writeFailed |= putIFDLong( TIFF_PLANARCONFIG, TIFF_LONG, 1, 1, outfile );
  writeFailed |= putIFDLong( TIFF_RESOLUTIONUNIT, TIFF_LONG, 1, 2, outfile );  // inches

  writeFailed |= putIFDLong( TIFF_PREDICTOR, TIFF_LONG, 1, 1, outfile );  // no predictor

  if (bits == 32 || bits == 64)
    writeFailed |= putIFDLong( TIFF_SAMPLE_FORMAT, TIFF_LONG, 1, TIFF_SAMPLE_FLOAT, outfile );
  else
    writeFailed |= putIFDLong( TIFF_SAMPLE_FORMAT, TIFF_LONG, 1, TIFF_SAMPLE_UINT, outfile );

  writeFailed |= putLong( (uint32_t)0, outfile );  // offset to next IFD

  // align to 4 byte boundary
  for (uint32_t i = 0; i < align_bytes; ++i)
    writeFailed |= putByte( 0, outfile );

// bits_offset:
  // bits per sample, because some readers break if it's just a byte instead of short
  for (int i = 0; i < channels; ++i)
    writeFailed |= putShort( bits, outfile );

  // resolution ratios
// xres_offset:
  writeFailed |= putLong( resRatio32, outfile );  // X dpi
  writeFailed |= putLong( resDenom32, outfile );  // denominator

// yres_offset:
  writeFailed |= putLong( resRatio32, outfile );  // Y dpi
  writeFailed |= putLong( resDenom32, outfile );  // denominator

// stripOffset_offset
  // file with zeros, then backfill once we compress the strips
  for (size_t i = 0; i < stripCount; ++i)
    writeFailed |= putLong( 0, outfile );

// stripByteCount_offset
  // file with zeros, then backfill once we compress the strips
  for (size_t i = 0; i < stripCount; ++i)
    writeFailed |= putLong( 0, outfile );

// pixelData_offset:
  // Pixel Data
  
  // check again after writing the tag directory
  if (writeFailed || ferror(outfile)) {
    fprintf(stderr, "ERROR: I/O error writing TIFF directory to %s\n", name.c_str() );
    fclose(outfile);
    return false;
  }

  std::vector<uint32_t> stripOffsetList( stripCount );
  std::vector<uint32_t> stripSizeList( stripCount );

  for (size_t strip = 0; strip < stripCount; ++strip) {

    size_t startRow = strip * rowsPerStrip;
    size_t rowCount = rowsPerStrip;
    if (startRow + rowsPerStrip > height)
      rowCount = height - startRow;

    //size_t stripBytes2 = rowCount * nrowBytes;
    size_t offset = startRow * nrowBytes;

    if (color_model == TIFF_MODE_CIELAB) {
      if (depth == 8)
        shiftTIFFLAB( buffer + offset, width * rowCount );
      else if (depth == 16)
        shiftTIFFLAB( (uint16_t *)(buffer + offset), width * rowCount );
    }

    long stripStart = ftell( outfile );
    if (stripStart < 0 || (unsigned long)stripStart > UINT32_MAX) {
      fprintf(stderr, "ERROR: TIFF strip offset exceeds 32-bit range\n");
      fclose(outfile);
      return false;
    }

    size_t pixelBytes = (size_t)width * (size_t)channels * (size_t)(depth/8);
    if (fwrite( buffer + offset, pixelBytes, rowCount, outfile ) != rowCount) {
      fprintf(stderr, "ERROR: TIFF failed to write pixel data\n");
      fclose(outfile);
      return false;
    }

    long stripEnd = ftell( outfile );
    if (stripEnd < 0 || (unsigned long)stripEnd > UINT32_MAX) {
      fprintf(stderr, "ERROR: TIFF strip end offset exceeds 32-bit range\n");
      fclose(outfile);
      return false;
    }
    stripOffsetList[strip] = uint32_t(stripStart);
    size_t len = (size_t)(stripEnd - stripStart);
    stripSizeList[strip] = uint32_t(len);

  }   // for strip

  // and update our strip offsets and sizes
  if (stripCount == 1) {
    writeFailed |= (fseek(outfile, byteCountOffset, SEEK_SET) != 0);
    uint32_t compressed = stripSizeList[0];
    writeFailed |= putIFDLong( TIFF_STRIPBYTECOUNTS, TIFF_LONG, 1, compressed, outfile );
  } else {
    writeFailed |= (fseek(outfile, stripOffset_offset, SEEK_SET) != 0);
    for (size_t i = 0; i < stripCount; ++i)
      writeFailed |= putLong( stripOffsetList[i], outfile );

    writeFailed |= (fseek(outfile, stripByteCount_offset, SEEK_SET) != 0);
    for (size_t i = 0; i < stripCount; ++i)
      writeFailed |= putLong( stripSizeList[i], outfile );
  }

  if (writeFailed || ferror(outfile)) {
    fprintf(stderr, "ERROR: I/O error writing TIFF data to %s\n", name.c_str() );
    fclose(outfile);
    return false;
  }

  if (!icFlushAndClose(outfile)) {
    fprintf(stderr, "ERROR: fclose failed for %s\n", name.c_str() );
    return false;
  }

  return true;
}

/******************************************************************************/
/******************************************************************************/
