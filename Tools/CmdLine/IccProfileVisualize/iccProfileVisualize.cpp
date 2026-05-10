/*
  File:     iccProfileVisualize.cpp

  Contains:   Console app to output LUTs as images and PDF plots

  Version:  V1

  Copyright:  (c) see below
*/

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
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <string>
#include <memory>
#include <algorithm>
#include "IccProfile.h"
#include "IccTag.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include "MiniTIFF.hpp"
#include "MiniSVG.hpp"
#include "MiniPDF.hpp"

// #define MEMORY_LEAK_CHECK to enable C RTL memory leak checking (slow!)
#define MEMORY_LEAK_CHECK

#if defined(_WIN32) || defined(WIN32)
#include <crtdbg.h>
#elif defined(__GLIBC__)
#include <mcheck.h>
#endif

/******************************************************************************/

// NOTE - ccox - multipage SVG doesn't work, but we may want to use SVG for UI drawing.
// So I'm keeping the code, but disabling output.
#ifndef USE_SVG
#define USE_SVG     false
#endif

/******************************************************************************/

#if USE_SVG
static
void DrawAxisSVG( SVGOut &svgfile, const point2D &basepoint, const point2D &range,
        const point2D &tickLength, const std::string &label )
{
  // main line
  svgfile.AddLine( basepoint, basepoint+range );

  // big marks for 0.0, 0.5, and 1.0
  point2D start0 = basepoint;
  svgfile.AddLine( start0, start0+tickLength );
  point2D start1 = basepoint + range;
  svgfile.AddLine( start1, start1+tickLength );
  point2D start2 = basepoint + range*0.5;
  svgfile.AddLine( start2, start2+tickLength );

// TODO - values???  0.1 increments?

  // small marks for each tenth that isn't 0.5
  for (int i = 1; i < 10; ++i) {
    if (i == 5) continue;
    point2D startN = basepoint + range*(i/10.0);
    svgfile.AddLine( startN, startN+tickLength*0.5);
  }

  // small marks for each hundredth
  for (int i = 1; i < 100; ++i) {
    if ((i % 10) == 0) continue;
    point2D startN = basepoint + range*(i/100.0);
    svgfile.AddLine( startN, startN+tickLength*0.25);
  }

  // label near halfway
  std::string font = "Arial";
  std::string style = "Regular";
  std::string align = "Center";
  point2D labelPt = basepoint + range*0.5 + tickLength*2.0;
  float rotation = (range.x == 0.0) ? 90 : 0;   // horiz or vertical
  svgfile.AddText( labelPt.x, labelPt.y, label, 14, font, style, align, rotation );
}
#endif  // USE_SVG

/******************************************************************************/

static
std::string DrawAxisPDF( const point2D &basepoint, const point2D &range,
        const point2D &tickLength, float labelSize, const std::string &label )
{
  std::ostringstream commands;

  // main line
  commands << "q\n";
  commands << basepoint << " m " << (basepoint+range) << " l S\n";

  // big marks for 0.0, 0.5, and 1.0
  point2D start0 = basepoint;
  commands << start0 << " m " << (start0+tickLength) << " l S\n";
  point2D start1 = basepoint + range;
  commands << start1 << " m " << (start1+tickLength) << " l S\n";
  point2D start2 = basepoint + range*0.5;
  commands << start2 << " m " << (start2+tickLength) << " l S\n";

// TODO - values???  0.1 increments?
  // small marks for each tenth that isn't 0.5
  for (int i = 1; i < 10; ++i) {
    if (i == 5) continue;
    point2D startN = basepoint + range*(i/10.0);
    commands << startN << " m " << (startN+tickLength*0.5) << " l S\n";
  }

  // small marks for each hundredth
  for (int i = 1; i < 100; ++i) {
    if ((i % 10) == 0) continue;
    point2D startN = basepoint + range*(i/100.0);
    commands << startN << " m " << (startN+tickLength*0.25) << " l S\n";
  }

  // label near halfway
  float textHalf = labelSize * 0.25f * label.size(); // very approximate, not using font metrics
  point2D labelPt = basepoint + range*0.5;
  commands << "BT /F1 " << labelSize << " Tf ";
  if (range.x == 0.0) {
    labelPt += tickLength*1.25f - point2D(0,textHalf);
    commands << "0 " << 1 << " " << -1 << " 0 " << labelPt << " Tm ";
  }
  else {
    labelPt += tickLength - point2D(textHalf,labelSize);
    commands << labelPt << " Td ";
  }
  commands << "(" << label << ") Tj ET\n";
  commands << "Q\n";

  return commands.str();
}

/******************************************************************************/

void CreateAxesXobject( PDFWriter &pdfout )
{
  std::string commands;
  float margin = 0.5*inch2point;
  float tickLength = 12.0f; // pt

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdfout.PageHeight();
  float right = pdfout.PageWidth();
  Rect2D bounds ( left, right, bottom, top );

  // draw axes
  // horizontal
  point2D basepoint( margin, bottom+margin );
  point2D rangeX( right-2*margin, 0.0 );
  point2D tickLengthX( 0, -tickLength );
  commands += DrawAxisPDF( basepoint, rangeX, tickLengthX, 12.0, "Input" );

  // vertical
  point2D rangeY( 0.0, (top-2*margin) );
  point2D tickLengthY( -tickLength, 0 );
  commands += DrawAxisPDF( basepoint, rangeY, tickLengthY, 12.0, "Output" );

  pdfout.AddXObject( bounds, commands );
}

/******************************************************************************/

#if USE_SVG
static
void graph1DLUTSVG( CIccCurve *curve, const std::string &name,
        const std::string &description, SVGOut &svgfile, int steps )
{
  svgfile.NextPage();
  svgfile.StartGroup( name );

  // draw title/description
  std::string font = "Arial";
  std::string style = "Bold";
  std::string align = "Center";

  std::string clean_description( description );
  // remove any line breaks from our text, because SVG doesn't do line breaks
  std::replace( clean_description.begin(), clean_description.end(), '\n', ' ');
  // and wrap our text in CDATA, because SVG doesn't like < > &
  std::string outdescription = "<![CDATA[" + name + " " + clean_description + "]]>";
  svgfile.AddText( 8*0.5*inch2mm, 0.25*inch2mm, outdescription, 14, font, style, align );

  // draw axes
  point2D basepoint( 0.5*inch2mm, 7.5*inch2mm );
  point2D rangeX( 7.0*inch2mm, 0.0 );
  point2D tickLengthX( 0, 5 );
  DrawAxisSVG( svgfile, basepoint, rangeX, tickLengthX, "Input" );

  point2D rangeY( 0.0, -7.0*inch2mm );
  point2D tickLengthY( -5, 0 );
  DrawAxisSVG( svgfile, basepoint, rangeY, tickLengthY, "Output" );

  // draw the curve
  pointList points(steps+1);
  float scale = (7.5-0.5)*inch2mm;
  point2D base( 0.5*inch2mm, 7.5*inch2mm );
  for (int i = 0; i <= steps; ++i ) {
    float input = i / (float)steps;
    float output = curve->Apply( input );
    if (std::isnan(output)) output = 0.0;
    if (std::isinf(output)) output = 1.0;
    if (output > 1.0) output = 1.0;
    if (output < 0.0) output = 0.0;
    points[i] = point2D( input*scale, -output*scale ) + base;
  }
  svgfile.AddPolyLine( points, false, false );

  svgfile.EndGroup();
}
#endif  // USE_SVG

/******************************************************************************/

std::vector<std::string> splitLines(const std::string& str)
{
    const char newline = '\n';
    std::vector<std::string> lines;
    size_t start = 0;
    size_t end = str.find(newline);
    while (end != std::string::npos) {
        lines.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(newline, start);
    }
    auto temp = str.substr(start);
    if (temp.size() > 0)
        lines.push_back(temp);
    return lines;
}

/******************************************************************************/

static
void graph1DLUTPDF( CIccCurve *curve, const std::string &name,
        const std::string &description, PDFWriter &pdffile, int steps )
{
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );


  // add the common axes
  commands << "/Axes Do\n";

  // label
  std::vector<std::string> lines = splitLines( description );
  float labelSize = 12;     // points
  float leading = labelSize * 1.1f;
  float indent = 0.5f * inch2point;
  commands << "BT /F1 " << labelSize << " Tf ";
  size_t line_num = 0;
  for (size_t i = 0; i < lines.size(); ++i, ++line_num) {
    std::string label = lines[i];
    if (label.size() == 0)  // double returns are not pretty
        continue;
    if (line_num == 0) {
      label = name + " " + label;
      float textHalf = labelSize * 0.25f * label.size();
      point2D labelPt( 0.5f*right - textHalf, top - 0.2f*inch2point );
      commands << labelPt << " Td ";
    }
    else {
      commands << " " << indent << " " << -leading << " Td ";
      indent = 0.0f;
    }
    commands << "(" << label << ") Tj\n";
  }
  commands << "ET\n";


  // draw the curve
  // optimization - draw only 3 points for identity curve
  if (curve->IsIdentity())
    steps = 2;
  float scale = (7.5f-0.5f)*inch2point;
  const point2D base( 0.5f*inch2point, 0.5f*inch2point );
  commands << base << " m\n";
  for (int i = 0; i <= steps; ++i ) {
    float input = i / (float)steps;
    float output = curve->Apply( input );
    if (std::isnan(output)) output = 0.0f;
    if (std::isinf(output)) output = 1.0f;
    if (output > 1.0f) output = 1.0f;
    if (output < 0.0f) output = 0.0f;
    point2D currentPt( input*scale, output*scale );
    commands << (base+currentPt) << " l\n";
  }
  commands << "S\n";

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( content );
}


/******************************************************************************/

static
void describe1DLUT( CIccTagCurve *curve, std::string &description )
{
  auto size = curve->GetSize();

  if (size == 0) {
    description += "Y = X";
  } else if (size == 1) {
    icFloatNumber value0 = (*curve)[0];
    icFloatNumber dGamma = (icFloatNumber)(value0 * 256.0);
    description += "Y = X ^ " + std::to_string(dGamma);   // TODO - truncate to 4 decimal places
  } else {
    description += "LookupTable[" + std::to_string(size) + "]";
  }
}

/******************************************************************************/

static
void describe1DLUT( CIccTagParametricCurve *curve, std::string &description )
{
  curve->Describe( description, 100 );
}

static
void describe1DLUT( CIccTagSegmentedCurve *curve, std::string &description )
{
  curve->Describe( description, 100 );
}

/******************************************************************************/

// output graphic representation of 1D LUTs
static
void output1DLUT(CIccProfile * /* pIcc */, CIccTag *tag, const std::string &sigDesc,
        PDFWriter &pdffile )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  if (!tag) {
    fprintf(stderr, "ERROR - missing data for %s\n", sigDesc.c_str());
    return;
  }

  icTagTypeSignature typeSig = tag->GetType();

  switch(typeSig) {
    case icSigCurveType:
      {
      CIccTagCurve *curve = dynamic_cast<CIccTagCurve*> (tag);
      if (curve) {
        std::string description;
        describe1DLUT(curve, description);
        int size = curve->GetSize();
        int steps = std::max( 1000, size );
#if USE_SVG
        graph1DLUTSVG( curve, sigDesc, description, svgfile, steps );
#endif
        graph1DLUTPDF( curve, sigDesc, description, pdffile, steps );
        }
      }
      break;

    // TODO - might merge below
    case icSigParametricCurveType:
      {
      CIccTagParametricCurve *pCurve = dynamic_cast<CIccTagParametricCurve*> (tag);
      if (pCurve) {
        std::string description;
        describe1DLUT(pCurve, description);
#if USE_SVG
        graph1DLUTSVG( pCurve, sigDesc, description, svgfile, 1000 );
#endif
        graph1DLUTPDF( pCurve, sigDesc, description, pdffile, 1000 );
        }
      }
      break;

    // TODO - might merge below
    case icSigSegmentedCurveType:
      {
      CIccTagSegmentedCurve *sCurve = dynamic_cast<CIccTagSegmentedCurve*> (tag);
      if (sCurve) {
        std::string description;
        describe1DLUT(sCurve, description);
#if USE_SVG
        graph1DLUTSVG( sCurve, sigDesc, description, svgfile, 1000 );
#endif
        graph1DLUTPDF( sCurve, sigDesc, description, pdffile, 1000 );
        }
      }
      break;

    default:
      printf("Unknown 1D LUT type %s for tag %s\n",
         icGetSig(buf, bufSize, typeSig), sigDesc.c_str() );
      {
      CIccCurve *uCurve = dynamic_cast<CIccCurve*> (tag);
      if (uCurve) {
        std::string description;
        uCurve->Describe( description, 100 );
#if USE_SVG
        graph1DLUTSVG( uCurve, sigDesc, description, svgfile, 1000 );
#endif
        graph1DLUTPDF( uCurve, sigDesc, description, pdffile, 1000 );
        }
      }
      break;

  }   // end switch by type

}   // end output1DLUT()

/******************************************************************************/

static
int TIFFColorModelFromICCModel( icColorSpaceSignature colorSig )
{
  switch(colorSig) {
    case icSigRgbData:
    case icSigCmyData:
    case icSigXYZData:
    case icSigLuvData:
    case icSigYCbCrData:
    case icSigYxyData:
    case icSigHsvData:
    case icSigHlsData:
      return TIFF_MODE_RGB;
      break;

    case icSigCmykData:
      return TIFF_MODE_CMYK;
      break;

    case icSigLabData:
      return TIFF_MODE_CIELAB;
      break;

    case icSigGrayData:
    case icSigGamutData:
      return TIFF_MODE_GRAY_BLACKZERO;
      break;

    default:
      // and N-ink should be multichannel
      return TIFF_MODE_GRAY_WHITEZERO;
      break;

  }

  // some compilers are picky, and stupid
  return TIFF_MODE_GRAY_BLACKZERO;
}

/******************************************************************************/

static
std::string channelName(int index, bool isInputMatrix, icColorSpaceSignature inputSpace,
                        icColorSpaceSignature outputSpace,
                        int inputChannels, int outputChannels)
{
  const size_t bufSize = 128;
  char buf[bufSize];
  icColorIndexName(buf, bufSize, isInputMatrix ? inputSpace :  outputSpace,
                  index, isInputMatrix ? inputChannels : outputChannels,
                  isInputMatrix ? "In" : "Out");
  return std::string(buf);
}

/******************************************************************************/

// output graphic representation of nD LUTs
static
void output3DLUT(CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
        const std::string &basename, PDFWriter &pdffile )
{
  const size_t bufSize = 128;
  char buf[bufSize];

  if (!tag) {
    fprintf(stderr, "Skipping %s: unable to load tag\n", sigDesc.c_str());
    return;
  }

  icTagTypeSignature typeSig = tag->GetType();
  switch(typeSig) {

// CIccTagLut8, CIccTagLut16, CIccTagLutAtoB, icSigLutBtoAType are subclases of CIccMBB
// this might be usable for all types

  case icSigLut8Type:   // CIccTagLut8
  case icSigLut16Type:  // CIccTagLut16
  case icSigLutAtoBType:  // CIccTagLutAtoB
  case icSigLutBtoAType:  // CIccTagLutBtoA
    {
    CIccMBB *lut = dynamic_cast<CIccMBB*> (tag);
    if (lut) {
      std::string description;
      lut->Describe( description, 100 );

      // output input and output curves
      CIccCurve **curveA = lut->GetCurvesA();
      CIccCurve **curveB = lut->GetCurvesB();
      CIccCurve **curveM = lut->GetCurvesM();
      std::string curveDesc = sigDesc + ": ";

      int inputChannels = lut->InputChannels();
      int outputChannels = lut->OutputChannels();
      icColorSpaceSignature inputSpace = lut->GetCsInput();
      icColorSpaceSignature outputSpace = lut->GetCsOutput();
      bool isInputMatrix = lut->IsInputMatrix();

      if (inputChannels <= 0 || outputChannels <= 0) {
        fprintf(stderr, "Skipping %s: invalid channel count\n", sigDesc.c_str());
        return;
      }

      if (curveA) {
        int curveACount = isInputMatrix ? outputChannels : inputChannels;
        for (int i = 0; i < curveACount; ++i) {
          if (curveA[i]) {
          std::string channel = channelName( i, !isInputMatrix,
                    inputSpace, outputSpace, inputChannels, outputChannels );
          std::string channelDesc = curveDesc + "curveA[ " + channel + " ]";
          output1DLUT( pIcc, curveA[i], channelDesc, pdffile );
          }
        }
      }

      if (curveB) {
        int curveBCount = isInputMatrix ? inputChannels : outputChannels;
        for (int i = 0; i < curveBCount; ++i) {
          if (curveB[i]) {
          std::string channel = channelName( i, isInputMatrix,
                  inputSpace, outputSpace, inputChannels, outputChannels );
          std::string channelDesc = curveDesc + "curveB[ " + channel + " ]";
          output1DLUT( pIcc, curveB[i], channelDesc, pdffile );
          }
        }
      }

      if (curveM) {
        int curveMCount = isInputMatrix ? inputChannels : outputChannels;
        for (int i = 0; i < curveMCount; ++i) {
          if (curveM[i]) {
          std::string channel = channelName( i, isInputMatrix,
                    inputSpace, outputSpace, inputChannels, outputChannels );
          std::string channelDesc = curveDesc + "curveM[ " + channel + " ]";
          output1DLUT( pIcc, curveM[i], channelDesc, pdffile );
          }
        }
      }


    // write nD Data to TIFF
      int bytes = lut->GetPrecision();    // currently only 1 or 2
      CIccCLUT *clut = lut->GetCLUT();
      if (!clut) {
        // clut is optional in mAB and mBA tags - only report if it isn't one of those
        if ( !(typeSig == icSigLutAtoBType || typeSig == icSigLutBtoAType) ) {
            std::string typeDesc = icGetSigStr(buf, bufSize, typeSig);
            fprintf(stderr,"ERROR - clut data could not be read for tag '%s' of type '%s' in file '%s'\n", sigDesc.c_str(), typeDesc.c_str(), basename.c_str() );
        }
        return;
      }

      clut->Begin();  // initialize some grid information

      int gridPoints = clut->GridPoints(); // gridSize[0]
      int tiles = gridPoints;
      if (gridPoints <= 0) {
        fprintf(stderr, "Skipping %s: invalid CLUT grid\n", sigDesc.c_str());
        return;
      }

      int tileWidth = 1;
      int tileHeight = 1;

      if (inputChannels >= 2) {
        tileWidth = clut->GridPoint(1);
        if (tileWidth <= 0) {
          fprintf(stderr, "Skipping %s: invalid CLUT width\n", sigDesc.c_str());
          return;
        }
      }

      if (inputChannels >= 3) {
        tileHeight = clut->GridPoint(2);
        if (tileHeight <= 0) {
          fprintf(stderr, "Skipping %s: invalid CLUT height\n", sigDesc.c_str());
          return;
        }
      }

      if (inputChannels > 3) {
        for (int i = 3; i < inputChannels; ++i) {
          int extraGridPoints = clut->GridPoint(i);
          if (extraGridPoints <= 0) {
            fprintf(stderr, "Skipping %s: invalid CLUT tile count\n", sigDesc.c_str());
            return;
          }
          tiles *= extraGridPoints;
        }
      }

      // special case for single dimensional LUT
      if (inputChannels == 1) {
        tileWidth = tiles;
        tiles = 1;
        tileHeight = 1;
      }

      // special case for 2 dimensional LUT
      if (inputChannels == 2) {
        tileHeight = tiles;
        tiles = 1;
      }

      // find tile arrangement closest to a square
      int tilesWide = (int)std::sqrt(tiles);

      // some odd counts need a tweak to align and look more sane
      if (inputChannels > 3 && (inputChannels & 1)) {
        auto oldValue = tilesWide;
        // round down to a multiple of the grid size to better align rows
        tilesWide -= (tilesWide % (gridPoints*tileWidth));
        if (tilesWide == 0) {
            // this does happen -- should I round up in some cases?
            tilesWide = oldValue;
        }
      }

      int tilesHigh = (tiles + (tilesWide-1)) / tilesWide;

      // multiply out by tile size
      int imageWidth = tilesWide * tileWidth;
      int imageHeight = tilesHigh * tileHeight;
      if (imageWidth <= 0 || imageHeight <= 0 || bytes <= 0) {
        fprintf(stderr, "Skipping %s: invalid image geometry\n", sigDesc.c_str());
        return;
      }

      //size_t clutSize = (size_t)tiles * (size_t)tileWidth * (size_t)tileHeight * (size_t)outputChannels;
      size_t bufferSize = (size_t)imageWidth * (size_t)imageHeight * (size_t)outputChannels * bytes;
      // NOTE that bufferSize will usually be greater than clutSize
      if (!bufferSize) {
        fprintf(stderr, "Skipping %s: empty image buffer\n", sigDesc.c_str());
        return;
      }

      std::unique_ptr<uint8_t[]> imageBuffer( new uint8_t[ bufferSize ] );
      uint8_t *imageBuf = imageBuffer.get();
      uint16_t *imageBuf16 = (uint16_t *)imageBuf;
      float *imageBuf32 = (float *)imageBuf;
      memset( imageBuf, 0, bufferSize );

      // copy data from CLUT to image buffer
      icFloatNumber *clutData = clut->GetData(0);



#if 0
// TEST - same as below, just more expensive calculation
    size_t gridCount = (size_t)tileWidth * (size_t)tileHeight * (size_t)tiles;
    for (size_t k = 0; k < gridCount; ++k ) {
        size_t y = (gridPoints -1) - (k % gridPoints);  // turn LAB to look as expected
        size_t x = (k / gridPoints) % gridPoints;
        size_t tile = k / (gridPoints*gridPoints);
        size_t tileX = tile % tilesWide;
        size_t tileY = tile / tilesWide;
        size_t outputIndex = outputChannels * ((tileY * gridPoints * imageWidth) + (tileX * gridPoints) + (y * imageWidth) + x);
        size_t inputIndex = outputChannels * k;
        if (bytes == 4 || bytes == 8)
          for (int c = 0; c < outputChannels; ++c)
            imageBuf32[outputIndex+c] = clutData[inputIndex+c];
        else if (bytes == 2)
          for (int c = 0; c < outputChannels; ++c)
            imageBuf16[outputIndex+c] = clutData[inputIndex+c] * 65535.0;
        else
          for (int c = 0; c < outputChannels; ++c)
            imageBuf[outputIndex+c] = clutData[inputIndex+c] * 255.0;
    }

#else
      size_t n001 = tileWidth * tileHeight * outputChannels;
      size_t n010 = tileWidth * outputChannels;
      size_t n100 = outputChannels;

      if (inputChannels < 2)
        std::swap(n010,n100);

      size_t outTileStepV = imageWidth * tileHeight * outputChannels;
      size_t outTileStepH = tileWidth * outputChannels;
      size_t outColStep = outputChannels;
      size_t outRowStep = imageWidth * outputChannels;

      for (int z = 0; z < tiles; ++z) {
        int z2 = z % tilesWide; // tile # horiz
        int z3 = z / tilesWide; // tile # vert
        for (int x = 0; x < tileWidth; ++x)
        for (int y = 0; y < tileHeight; ++y) {
          size_t inputIndex = z * n001 + x * n010 + (tileHeight-1-y) * n100;  // turn LAB to look as expected
          size_t outputIndex = z3 * outTileStepV + z2 * outTileStepH + y * outRowStep + x * outColStep;
          if (bytes == 4 || bytes == 8)
            for (int c = 0; c < outputChannels; ++c)
              imageBuf32[outputIndex+c] = clutData[inputIndex+c];
          else if (bytes == 2)
            for (int c = 0; c < outputChannels; ++c)
              imageBuf16[outputIndex+c] = clutData[inputIndex+c] * 65535.0;
          else
            for (int c = 0; c < outputChannels; ++c)
              imageBuf[outputIndex+c] = clutData[inputIndex+c] * 255.0;
        }
      }
#endif

      std::string tiffPath2 = basename + "_" + sigDesc + ".tif";
      int tiffColor = TIFFColorModelFromICCModel( outputSpace );
      if (!WriteTIFF( tiffPath2.c_str(), 100, tiffColor, imageBuf,
                imageWidth, imageHeight, outputChannels, 8*bytes )) {
        fprintf(stderr, "Failed to write TIFF: %s\n", tiffPath2.c_str());
        }
      }
    }
    break;

  case icSigMultiProcessElementType:
    // do nothing for now, because we don't know how to render the Multiprocess elements
    break;

  default:
    printf("Unknown nD LUT type %s for tag %s\n",
         icGetSig(buf, bufSize, typeSig),
         sigDesc.c_str() );
    break;
  }   // end switch by type

}   // end output3DLUT()

/******************************************************************************/

static
std::string remove_extension( const std::string& filename)
{
  size_t lastdot = filename.find_last_of(".");
  if (lastdot == std::string::npos || lastdot == 0) {
    return filename;
  }
  return filename.substr(0, lastdot);
}

/******************************************************************************/

// output graphic representation of 1D and nD LUTs
static
int processLuts(CIccProfile *pIcc, const char *profilePath )
{
  const size_t bufSize = 64;
  char buf1[bufSize];
  int outputItems = 0;

  std::string basename = remove_extension( profilePath );


// write next to input file
// write output to basename + _luts.pdf
// write basename + _ + tag + .tiff for nD LUTs

#if USE_SVG
  std::string svgPath = basename + "_luts.svg";
  SVGOut svgfile( svgPath );
  svgfile.SetPageSize( 8*inch2mm, 8*inch2mm );
#endif

  std::string pdfPath = basename + "_luts.pdf";
  PDFWriter pdffile( pdfPath, 8*inch2point, 8*inch2point );
  CreateAxesXobject( pdffile );



  for ( auto &tag: pIcc->m_Tags ) {
    icTagSignature sig = tag.TagInfo.sig;
    //icTagTypeSignature typeSig = tag.pTag->GetType();

// Switching by data type is easier from a programmming standpoint.
// But name will limit us to known tags and ignore bogus tags.

    switch (sig) {

      // 1D LUTs
      case icSigRedTRCTag:
      case icSigGreenTRCTag:
      case icSigBlueTRCTag:
      case icSigGrayTRCTag:
// response curve struct?
// response curve array?
        {
        const char *sigDesc = icGetSigStr(buf1, bufSize, sig);
        CIccTag *pTag = pIcc->FindTag(tag); // load if needed
        output1DLUT(pIcc, pTag, sigDesc, pdffile );
        outputItems++;
        }
        break;

      // nD LUTs
      case icSigAToB0Tag:
      case icSigAToB1Tag:
      case icSigAToB2Tag:
      case icSigAToB3Tag:
      case icSigBToA0Tag:
      case icSigBToA1Tag:
      case icSigBToA2Tag:
      case icSigBToA3Tag:
      case icSigGamutTag:
      case icSigPreview0Tag:
      case icSigPreview1Tag:
      case icSigPreview2Tag:
        {
        std::string sigDesc = icGetSigStr(buf1, bufSize, sig);
        CIccTag *pTag = pIcc->FindTag(tag); // load if needed
        output3DLUT(pIcc, pTag, sigDesc, basename, pdffile );
        outputItems++;
        }
        break;

      // ignore everything else
      default:
        break;

    }   // end switch over tag signatures
  }   // end loop over tags


#if USE_SVG
  svgfile.CloseFile();
#endif
  pdffile.CloseFile();

  return outputItems;

}   // end processLuts()

/******************************************************************************/

static
void printUsage(void)
{
  printf("Usage: iccProfileVisualize input_profile\n");
  printf("  output will be TIFF and PDF files next to the input profile.\n");
  printf("iccProfileVisualize built with IccProfLib version " ICCPROFLIBVER "\n\n");
}

/******************************************************************************/

int main(int argc, char* argv[])
{
#if defined(MEMORY_LEAK_CHECK) && defined(_DEBUG)
#if defined(WIN32) || defined(_WIN32)
#if 0
  // Suppress windows dialogs for assertions and errors - send to stderr instead during batch CLI processing
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  int tmp = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
  tmp = tmp | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF; // | _CRTDBG_CHECK_ALWAYS_DF;
  _CrtSetDbgFlag(tmp);
  //_CrtSetBreakAlloc(1163);
#elif __GLIBC__
  mcheck(NULL);
#endif // WIN32
#endif // MEMORY_LEAK_CHECK && _DEBUG

  if (argc <= 1) {
    printUsage();
    return 0;
  }

// if we need options in the future, then parse -* and add all unknowns to a list of filenames

#if defined(_DEBUG) || defined(DEBUG)
  int nValid = 0;
#endif

  for (int k = 1; k < argc; ++k) {
    CIccProfile *pIcc = OpenIccProfile( argv[k] );
    if (!pIcc) {
      printf("Unable to parse '%s' as ICC profile!\n", argv[k]);
      continue;
    }
// DEBUGGING printf("Processing profile '%s'\n", argv[k]);
    auto count = processLuts( pIcc, argv[k] );
    if (!count) {
        printf("Profile %s had no content for output\n", argv[k] );
    }
#if defined(_DEBUG) || defined(DEBUG)
    else {
      nValid++;
    }
#endif
    delete pIcc;
  }

#if defined(_DEBUG) || defined(DEBUG)
  printf("EXIT %d\n", nValid);
#endif

  return 0;
}

/******************************************************************************/
/******************************************************************************/
