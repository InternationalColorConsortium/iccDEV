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
#include <limits>
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
#include "spectralLocus.hpp"

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

#ifndef M_SQRT2
#define M_SQRT2  1.41421356237309504880168872420969808
#endif
#ifndef M_PI
#define M_PI  3.14159265358979323846264338327950288
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
  point2D start2 = basepoint + range*0.5f;
  svgfile.AddLine( start2, start2+tickLength );

  // small marks for each tenth that isn't 0.5
  for (int i = 1; i < 10; ++i) {
    if (i == 5) continue;
    point2D startN = basepoint + range*(i/10.0f);
    svgfile.AddLine( startN, startN+tickLength*0.5f);
  }

  // small marks for each hundredth
  for (int i = 1; i < 100; ++i) {
    if ((i % 10) == 0) continue;
    point2D startN = basepoint + range*(i/100.0f);
    svgfile.AddLine( startN, startN+tickLength*0.25f);
  }

  // label near halfway
  std::string font = "Arial";
  std::string style = "Regular";
  std::string align = "Center";
  point2D labelPt = basepoint + range*0.5f + tickLength*2.0f;
  float rotation = (range.x == 0.0) ? 90 : 0;   // horiz or vertical
  svgfile.AddText( labelPt.x, labelPt.y, label, 14, font, style, align, rotation );
}
#endif  // USE_SVG

/******************************************************************************/

enum TextAlignment {
    kTextAlignLeft = 0,
    kTextAlignCenter = 1,
    kTextAlignRight = 2
};

static
std::string AddGraphLabels( const point2D &basepoint, bool isVertical,
        const point2D &tickLength, float labelSize, const std::string &text,
        TextAlignment align = kTextAlignCenter )
{
  std::ostringstream commands;

  float textWidth = labelSize * 0.6f * text.size(); // very approximate, not using font metrics
  float textHalf = 0.5f * textWidth;

  point2D position(0,0);
  switch(align) {
    default:
    case kTextAlignLeft:
        // do nothing
        break;

    case kTextAlignCenter:
        position = point2D(-textHalf,0);
        break;

    case kTextAlignRight:
        position = point2D(-textWidth,0);
        break;
  }

  point2D pt00 = basepoint;
  commands << "BT /F1 " << labelSize << " Tf ";
  if (isVertical) {
    std::swap(position.x,position.y);
    pt00 += tickLength*1.25f + position;
    commands << "0 " << 1 << " " << -1 << " 0 " << pt00 << " Tm ";
  }
  else {
    pt00 += tickLength + position - point2D(0,labelSize);
    commands << pt00 << " Td ";
  }
  commands << "(" << text << ") Tj ET\n";

  return commands.str();
}

/******************************************************************************/

static
std::string DrawAxisPDF( const point2D &basepoint, const point2D &range,
        const point2D &tickLength, const point2D &fullLength, float labelSize, const std::string &label )
{
  std::ostringstream commands;

  // save gstate
  commands << "q\n";

  // grid behind major axes
  commands << "0.05 0 0 0 K\n";
  for (int i = 1; i <= 100; ++i) {
    if ((i % 10) == 0) continue;
    point2D startN = basepoint + range*(i/100.0f);
    commands << startN << " m " << (startN+fullLength) << " l S\n";
  }
  commands << "0.1 0 0 0 K\n";
  for (int i = 1; i <= 10; ++i) {
    point2D startN = basepoint + range*(i/10.0f);
    commands << startN << " m " << (startN+fullLength) << " l S\n";
  }
  // identity line
  commands << basepoint << " m " << (basepoint+fullLength+range) << " l S\n";
  // end colored grid, grestore, gsave
  commands << "Q q\n";

  // main line
  commands << basepoint << " m " << (basepoint+range) << " l S\n";

  // big marks for 0.0, 0.5, and 1.0
  point2D start0 = basepoint;
  commands << start0 << " m " << (start0+tickLength) << " l S\n";
  point2D start1 = basepoint + range;
  commands << start1 << " m " << (start1+tickLength) << " l S\n";
  point2D start2 = basepoint + range*0.5f;
  commands << start2 << " m " << (start2+tickLength) << " l S\n";

  // small marks for each tenth that isn't 0.5
  for (int i = 1; i < 10; ++i) {
    if (i == 5) continue;
    point2D startN = basepoint + range*(i/10.0f);
    commands << startN << " m " << (startN+tickLength*0.5) << " l S\n";
  }

  // small marks for each hundredth
  for (int i = 1; i < 100; ++i) {
    if ((i % 10) == 0) continue;
    point2D startN = basepoint + range*(i/100.0f);
    commands << startN << " m " << (startN+tickLength*0.25) << " l S\n";
  }

  // labels for 0, 50, 100%
  std::string zero("0");
  std::string half("50%");
  std::string full("100%");
  bool isVertical = (range.x == 0);
  commands << AddGraphLabels( basepoint, isVertical, tickLength, labelSize, zero );
  commands << AddGraphLabels( basepoint+0.5f*range, isVertical, tickLength, labelSize, half );
  commands << AddGraphLabels( basepoint+range, isVertical, tickLength, labelSize, full, kTextAlignRight );

  // IO label near 2/3
  commands << AddGraphLabels( basepoint + range*0.66, isVertical, tickLength, labelSize, label );

  // grestore at end
  commands << "Q\n";

  return commands.str();
}

/******************************************************************************/

static
void CreateAxesXobject( PDFWriter &pdfout )
{
  std::string commands;
  const float margin = 0.5f*inch2point;
  const float tickLength = 12.0f; // pt

  const float bottom = 0.0f;
  const float left = 0.0f;
  const float top = pdfout.PageHeight();
  const float right = pdfout.PageWidth();
  const Rect2D bounds ( left, right, bottom, top );

  // draw axes
  // horizontal
  const point2D basepoint( margin, bottom+margin );
  const point2D rangeX( right-2*margin, 0.0f );
  const point2D tickLengthX( 0, -tickLength );
  const point2D fullLengthX( 0, (top-margin) - (bottom+margin) );
  commands += DrawAxisPDF( basepoint, rangeX, tickLengthX, fullLengthX, 12.0f, "Input" );

  // vertical
  const point2D rangeY( 0.0, (top-2*margin) );
  const point2D tickLengthY( -tickLength, 0 );
  const point2D fullLengthY( (right-margin) - (left+margin), 0 );
  commands += DrawAxisPDF( basepoint, rangeY, tickLengthY, fullLengthY, 12.0f, "Output" );

  pdfout.AddXObject( bounds, commands, "Axes" );
}

/******************************************************************************/

struct XYColor
{
    XYColor (float xx, float yy) : x(xx), y(yy) {}

    float x;
    float y;
};

// https://en.wikipedia.org/wiki/Planckian_locus
// Bongsoon Kang; Ohak Moon; Changhee Hong; Honam Lee; Bonghwan Cho; Youngsun Kim (December 2002).
// "Design of Advanced Color Temperature Control System for HDTV Applications"
// Journal of the Korean Physical Society. 41 (6): 865–871. S2CID 4489377
static
XYColor approx_planck( double t )
{
    const double c3a = -0.2661239;
    const double c2a = -0.2343589;
    const double c1a =  0.8776956;
    const double c0a =  0.179910;

    const double c3b = -3.0258469;
    const double c2b =  2.1070379;
    const double c1b =  0.2226347;
    const double c0b =  0.240390;

    const double k3a = -1.1063814;
    const double k2a = -1.34811020;
    const double k1a =  2.18555832;
    const double k0a = -0.20219683;

    const double k3b = -0.9549476;
    const double k2b = -1.37418593;
    const double k1b =  2.09137015;
    const double k0b = -0.16748867;

    const double k3c =  3.0817580;
    const double k2c = -5.87338670;
    const double k1c =  3.75112997;
    const double k0c = -0.37001483;

    double t2 = t*t;
    double t3 = t*t*t;

    double x = 0.0;

    if (t < 4000.0) {
        x = c3a*(1e9/t3) + c2a*(1e6/t2) + c1a*(1e3/t) + c0a;
    } else {
        x = c3b*(1e9/t3) + c2b*(1e6/t2) + c1b*(1e3/t) + c0b;
    }

    double x2 = x*x;
    double x3 = x*x*x;

    double y = 0.0;

    if (t < 2222.0) {
        y = k3a*x3 + k2a*x2 + k1a*x + k0a;
    } else if (t < 4000.0) {
        y = k3b*x3 + k2b*x2 + k1b*x + k0b;
    } else {
        y = k3c*x3 + k2c*x2 + k1c*x + k0c;
    }

    return XYColor(x,y);
}

/******************************************************************************/

/*
label points for spectrum in nm
sorta, kinda evenly spaced, plus endpoints
 */
std::vector<int> locusLabelWavelengths =
{
    360,
    460, 450,
    470, 475, 480, 485, 490, 495, 500, 505, 510, 515,
    520, 530, 540, 550, 560, 570, 580, 590, 600, 610, 620,
    640,
    700
};

/******************************************************************************/

static
point2D spectrumLabelOffset( int nm, float textSize, TextAlignment &align )
{
// NOTE - Yes, I could create normal vectors from the locus points, etc.
// but this looks better with less math, and is much easier to debug.

    if (nm < 515) {
        // go left
        align = kTextAlignRight;
        return point2D( -2.0f, 0.0f );
    } else if (nm <= 520) {
        // go up
        align = kTextAlignCenter;
        return point2D( -3.0f, textSize*1.55f );
    } else {
        // go right
        align = kTextAlignLeft;
        return point2D( textSize*0.5, textSize );
    }

    // unreachable
}

/******************************************************************************/

// x range [ 0.00364, 0.73469 ]   for 2degree 1931 observer
// y range [ 0.00529, 0.83409 ]
const float chromaticityChartScale = 0.85f;

static
void CreateXYPlotXobject( PDFWriter &pdfout )
{
  std::ostringstream commands;
  float margin = 0.25*inch2point;

  const float fineIncrement = 0.01f;
  const float coarseIncrement = 0.1f;

  const float bottom = 0.0f;
  const float left = 0.0f;
  const float top = pdfout.PageHeight();
  const float right = pdfout.PageWidth();
  const Rect2D bounds ( left, right, bottom, top );
  const point2D basepoint( left+margin, bottom+margin );
  const point2D rangeX( right-left-2*margin, 0 );
  const point2D rangeY( 0, top-bottom-2*margin );


  // draw grid
  commands << "q\n";

  // vertical fine grid
  commands << "0.05 0 0 0 K\n";
  for (float i = 0.0f; i <= chromaticityChartScale; i += fineIncrement) {
    point2D startN = basepoint + i/chromaticityChartScale * rangeX;
    commands << startN << " m " << (startN+rangeY) << " l S\n";
  }
  // horizontal fine grid
  for (float i = 0.0f; i <= chromaticityChartScale; i += fineIncrement) {
    point2D startN = basepoint + i/chromaticityChartScale * rangeY;
    commands << startN << " m " << (startN+rangeX) << " l S\n";
  }

  // vertical coarse grid
  commands << "0.1 0 0 0 K\n";
  for (float i = 0.0f; i <= chromaticityChartScale; i += coarseIncrement) {
    point2D startN = basepoint + i/chromaticityChartScale * rangeX;
    commands << startN << " m " << (startN+rangeY) << " l S\n";
  }
  // horizontal coarse grid
  for (float i = 0.0f; i <= chromaticityChartScale; i += coarseIncrement) {
    point2D startN = basepoint + i/chromaticityChartScale * rangeY;
    commands << startN << " m " << (startN+rangeX) << " l S\n";
  }

  // end colored grid, grestore, gsave
  commands << "Q q\n";

  // spectral locus
  commands << "0.5 0.5 0 0 K\n";
  const point2D scaling = (rangeX + rangeY) / chromaticityChartScale;
  point2D firstPoint = basepoint + scaling * point2D( spectralLocus2degree[0].x , spectralLocus2degree[0].y );
  commands << firstPoint << " m\n";
  for (size_t k = 1; k < spectralLocus2degree.size(); ++k ) {
    point2D thispoint = basepoint + scaling * point2D( spectralLocus2degree[k].x , spectralLocus2degree[k].y );
    commands << thispoint << " l\n";
  }
  // close and stroke the shape
  commands << "s\n";

  // labels for spectral locus
  commands << "0.5 0.5 0 0 k\n";
  const float labelSize = 9.0f;
  const int wavelengthOffset = spectralLocus2degree[0].wavelength;
  for (auto &nm : locusLabelWavelengths ) {
    TextAlignment align;
    point2D offset = spectrumLabelOffset( nm,labelSize, align );
    size_t index = nm - wavelengthOffset;
    point2D thispoint = basepoint + scaling * point2D( spectralLocus2degree[index].x,
                                                       spectralLocus2degree[index].y );
    std::string number = std::to_string(nm);
    commands << AddGraphLabels( thispoint + offset, false, point2D(0,0), labelSize, number, align );
  }

  // plankian white curve
  commands << "0 0.25 0.25 0 K\n";
  const float start_temp = 1500.0f;   // degrees Kelvin
  const float end_temp = 20000.0f;
  const float temp_step = 200.0f;

  // scan over the planck curve and plot the lines
  XYColor firstXY = approx_planck( start_temp );
  firstPoint = basepoint + scaling * point2D( firstXY.x, firstXY.y );
  commands << firstPoint << " m\n";
  for (float temp = start_temp+temp_step; temp <= end_temp; temp += temp_step ) {
        XYColor thisXY = approx_planck( temp );
        point2D thispoint = basepoint + scaling * point2D( thisXY.x, thisXY.y );
        commands << thispoint << " l\n";
  }
  // stroke the curve
  commands << "S\n";

  // grestore
  commands << "Q\n";
  std::string commandString = commands.str();
  pdfout.AddXObject( bounds, commandString, "xyPlot" );
}

/******************************************************************************/

static
std::string plotCirclePDF( const point2D &center, float radius )
{
  std::ostringstream commands;

  const float handle_factor = float(4.0 * (M_SQRT2 - 1.0) / 3.0);
  const float K = radius * handle_factor;
  const point2D rx(radius,0);
  const point2D kx(K,0);
  const point2D ry(0,radius);
  const point2D ky(0,K);

  commands << center+rx << " m\n";
  commands << center+rx-ky << " " << center+kx-ry << " " << center-ry << " c\n";
  commands << center-kx-ry << " " << center-rx-ky << " " << center-rx << " c\n";
  commands << center-rx+ky << " " << center-kx+ry << " " << center+ry << " c\n";
  commands << center+kx+ry << " " << center+rx+ky << " " << center+rx << " c s\n";

  return commands.str();
}

/******************************************************************************/

// DEFERRED - full 128+ range is probably excessive for real world use
// what is an appropriate limit?        So far 130 looks fine.
const float abChartScale = 2 * 130.0f;

static
void CreateABPlotXobject( PDFWriter &pdfout )
{
  std::ostringstream commands;
  float margin = 0.25f*inch2point;

  const float coarseIncrement = 10.0f;

  const float bottom = 0.0f;
  const float left = 0.0f;
  const float top = pdfout.PageHeight();
  const float right = pdfout.PageWidth();
  const Rect2D bounds ( left, right, bottom, top );
  const point2D basepoint( left+margin, bottom+margin );
  const point2D rangeX( right-left-2*margin, 0 );
  const point2D rangeY( 0, top-bottom-2*margin );
  const point2D center = 0.5f * (basepoint + point2D(right-margin,top-margin));
  const float maxRadius = std::max( right-left-2*margin, top-bottom-2*margin );


  // draw grid
  commands << "q\n";

  // clip to chart area
  commands << basepoint << " m " << (basepoint+rangeY) << " l\n";
  commands << (basepoint+rangeY+rangeX) << " l\n";
  commands << (basepoint+rangeX) << " l h W n\n";

  // vertical grid
  commands << "0.1 0 0 0 K\n";
  const point2D centerX(center.x,bottom+margin);
  for (float i = coarseIncrement; i <= abChartScale; i += coarseIncrement) {
    point2D startN = centerX + i/abChartScale * rangeX;
    commands << startN << " m " << (startN+rangeY) << " l S\n";
    point2D start2 = centerX - i/abChartScale * rangeX;
    commands << start2 << " m " << (start2+rangeY) << " l S\n";
  }
  // horizontal grid
  const point2D centerY(left+margin,center.y);
  for (float i = coarseIncrement; i <= abChartScale; i += coarseIncrement) {
    point2D startN = centerY + i/abChartScale * rangeY;
    commands << startN << " m " << (startN+rangeX) << " l S\n";
    point2D start2 = centerY - i/abChartScale * rangeY;
    commands << start2 << " m " << (start2+rangeX) << " l S\n";
  }

  // constant chroma circles are helpful
  const float chromaIncrement = 30.0f;
  const float chromaMax = 150.0f;
  for (float i = chromaIncrement; i <= chromaMax; i += chromaIncrement) {
    commands << plotCirclePDF( center, i*maxRadius/abChartScale );
  }

  // axes
  commands << "0.4 0 0 0 K\n";
  commands << centerX << " m " << (centerX+rangeY) << " l S\n";
  commands << centerY << " m " << (centerY+rangeX) << " l S\n";

// axes labels
  commands << "0.4 0 0 0 k\n";
  float labelSize = 10.0f;
  point2D labelPtYellow(center.x,top-margin);
  commands << AddGraphLabels( labelPtYellow, false, point2D(0,0), labelSize, "+b Yellow", kTextAlignCenter );
  point2D labelPtBlue( center.x,bottom+margin+labelSize*1.2f);
  commands << AddGraphLabels( labelPtBlue, false, point2D(0,0), labelSize, "-b Blue", kTextAlignCenter );
  point2D labelPtMagenta(right-margin,center.y);
  commands << AddGraphLabels( labelPtMagenta, false, point2D(0,0), labelSize, "+a Magenta", kTextAlignRight );
  point2D labelPtTeal(left+margin,center.y);
  commands << AddGraphLabels( labelPtTeal, false, point2D(0,0), labelSize, "-a Green", kTextAlignLeft );

  // end colored grid, grestore, gsave
  commands << "Q q\n";

  // grestore
  commands << "Q\n";
  std::string commandString = commands.str();
  pdfout.AddXObject( bounds, commandString, "abPlot" );
}

/******************************************************************************/

static
XYColor xyFromICCXYZ( const icXYZNumber *xyz )
{
    // integers, so don't have to test for NaN or Inf
    float X = xyz->X / 65535.0f;
    float Y = xyz->Y / 65535.0f;
    float Z = xyz->Z / 65535.0f;

    float sum = X + Y + Z;
    if (sum <= 1e-8)
        return XYColor(0,0);

    float x = X / sum;
    float y = Y / sum;
    return XYColor(x,y);
}

/******************************************************************************/

static
XYColor xyFromICCXYZFloat( const icFloatNumber *xyz )
{
    float X = xyz[0];
    float Y = xyz[1];
    float Z = xyz[2];

    float sum = X + Y + Z;
    if (sum <= 1e-8)
        return XYColor(0,0);

    float x = X / sum;
    float y = Y / sum;
    return XYColor(x,y);
}

/******************************************************************************/

static
std::string plotSquarePDF( const point2D &center, float size )
{
  std::ostringstream commands;

  float half = 0.5f * size;

  point2D pt0(center.x-half,center.y-half);
  point2D pt1(center.x-half,center.y+half);
  point2D pt2(center.x+half,center.y+half);
  point2D pt3(center.x+half,center.y-half);

  commands << pt0 << " m " << pt1 << " l\n";
  commands << pt2 << " l\n";
  commands << pt3 << " l s\n";

  return commands.str();
}

/******************************************************************************/

static
std::string plotXYZTag( CIccTag *tag, std::string label, const point2D &basepoint,
                        const point2D &scaling, float symbolSize, float textSize,
                        point2D *result = NULL )
{
  std::ostringstream commands;

  auto theXYZTag = dynamic_cast<CIccTagXYZ*>(tag);
  if (theXYZTag) {
    auto theXYZ = theXYZTag->GetXYZ(0);
    if (theXYZ) {
      auto theXY = xyFromICCXYZ( theXYZ );
      point2D thePt = basepoint + scaling * point2D( theXY.x, theXY.y );
      commands << plotSquarePDF( thePt, symbolSize );
      point2D textOffset( 0, symbolSize+2+textSize );
      commands << AddGraphLabels( thePt + textOffset, false, point2D(0,0),
                                    textSize, label, kTextAlignLeft );
      if (result)
        *result = thePt;
    }
  }

  return commands.str();
}

/******************************************************************************/

static
int graphChromaticityPDF( CIccProfile *pIcc, PDFWriter &pdffile )
{
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );
  float margin = 0.25f*inch2point;

    // icSigMediaBlackPointTag ????
  auto whiteTag = pIcc->FindTag( icSigMediaWhitePointTag );
  bool hasWhite = (whiteTag != NULL);

  auto redTag = pIcc->FindTag( icSigRedColorantTag );
  auto greenTag = pIcc->FindTag( icSigGreenColorantTag );
  auto blueTag = pIcc->FindTag( icSigBlueColorantTag );
  bool hasRGB = (redTag && greenTag && blueTag);

  // bail if there is nothing to plot
  // NOTE - plotting white alone just seems weird, and produces a lot of noise
  if (!hasRGB)
    return 0;


  // if the xyPlot xobject doesn't exist, create it now
  if (!pdffile.xobjectExists("xyPlot"))
    CreateXYPlotXobject( pdffile );

  // add the common axes
  commands << "/xyPlot Do\n";

  // add label
  point2D range( right - left, 0 );
  point2D labelBase( left, top );
  point2D tickLength(0,0);
  commands << AddGraphLabels( labelBase + range*0.5, false, tickLength, 12.0f, "Chromaticity xy" );


  // gsave, color black
  commands << "q 0 0 0 1 K\n";

  point2D basepoint( left+margin, bottom+margin );
  point2D rangeX( right-bottom-2*margin, 0 );
  point2D rangeY( 0, top-bottom-2*margin );
  point2D scaling = (rangeX + rangeY) / chromaticityChartScale;
  float markSize = 4.0f;
  float textSize = 10.0f;

  if (hasWhite) {
    std::string CCTText;
#if 0
// not working correctly with 9300 whitepoint displays, gives 3175K
    auto theXYZTag = dynamic_cast<CIccTagXYZ*>(whiteTag);
    if (theXYZTag) {
      auto theXYZ = theXYZTag->GetXYZ(0);
      if (theXYZ) {
        auto theXY = xyFromICCXYZ( theXYZ );
        // McCamy's forumula
        // McCamy, Calvin S. (April 1992).
        // "Correlated color temperature as an explicit function of chromaticity coordinates"
        float n = (theXY.x - 0.3320f) / (0.1858f - theXY.y);
        float n2 = n*n;
        float n3 = n*n*n;
        //float cct = -437.0f *n3 + 3601.0f *n2 - 6861.0f *n + 5514.31f;    // first eq.
        float cct = -449.0f *n3 + 3525.0f *n2 - 6823.3f *n + 5520.33f;  // second eq.
        int32_t cctI = (int32_t)cct; // we want the integer part, don't need precision
        CCTText = std::string("( ~") + std::to_string(cctI) + std::string("K)");
      }
    }
#endif

    std::string label = std::string("White") + CCTText;
    commands << plotXYZTag( whiteTag, label, basepoint,
                        scaling, markSize, textSize );
  }

  if (hasRGB) {
    point2D redPt, greenPt, bluePt;
    commands << plotXYZTag( redTag, "R", basepoint,
                        scaling, markSize, textSize, &redPt );
    commands << plotXYZTag( greenTag, "G", basepoint,
                        scaling, markSize, textSize, &greenPt );
    commands << plotXYZTag( blueTag, "B", basepoint,
                        scaling, markSize, textSize, &bluePt );

    // draw lines between points for gamut
    commands << "0 0 0 0.5 K\n";
    commands << redPt << " m " << greenPt << " l\n";
    commands << bluePt << " l s\n";
  }

  // grestore
  commands << "Q\n";

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( content, "xyPlot" );

  return 1;
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
  svgfile.AddText( 8*0.5*inch2mm, 0.25f*inch2mm, outdescription, 14.0f, font, style, align );

  // draw axes
  point2D basepoint( 0.5f*inch2mm, 7.5f*inch2mm );
  point2D rangeX( 7.0f*inch2mm, 0.0f );
  point2D tickLengthX( 0, 5 );
  DrawAxisSVG( svgfile, basepoint, rangeX, tickLengthX, "Input" );

  point2D rangeY( 0.0, -7.0*inch2mm );
  point2D tickLengthY( -5, 0 );
  DrawAxisSVG( svgfile, basepoint, rangeY, tickLengthY, "Output" );

  // draw the curve
  pointList points(steps+1);
  float scale = (7.5f-0.5f)*inch2mm;
  point2D base( 0.5f*inch2mm, 7.5f*inch2mm );
  for (int i = 0; i <= steps; ++i ) {
    float input = i / (float)steps;
    float output = curve->Apply( input );
    if (std::isnan(output)) output = 0.0;
    if (std::isinf(output)) output = 1.0;
    if (output > 1.0f) output = 1.0f;
    if (output < 0.0f) output = 0.0f;
    points[i] = point2D( input*scale, -output*scale ) + base;
  }
  svgfile.AddPolyLine( points, false, false );

  svgfile.EndGroup();
}
#endif  // USE_SVG

/******************************************************************************/

static
std::vector<std::string> splitTextLines(const std::string& str)
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

  // if the axes xobject doesn't exist, create it now
  if (!pdffile.xobjectExists("Axes"))
    CreateAxesXobject( pdffile );

  // add the common axes
  commands << "/Axes Do\n";

  // label (may be a couple of lines)
  std::vector<std::string> lines = splitTextLines( description );
  float labelSize = 12.0f;     // points
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
      float textHalf = labelSize * 0.3f * label.size();
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
  if (steps > 0) {
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
  }

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( content, "Axes" );
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
    icFloatNumber dGamma = (icFloatNumber)(value0 * 256.0f);
    description += "Y = X ^ " + std::to_string(dGamma);
  } else {
    description += "LookupTable[" + std::to_string(size) + "]";
  }
}

/******************************************************************************/

static
void describe1DLUT( CIccTagParametricCurve *curve, std::string &description )
{
  std::string path("parametricCurve");
  std::string report;
  if (curve->Validate(path, report, NULL) > icValidateWarning) {
    fprintf(stderr,"WARNING - curve failed validation: %s\n", report.c_str() );
    description = "parametric";
    return;
  }
  curve->Describe( description, 100 );
}

/******************************************************************************/

static
void describe1DLUT( CIccTagSegmentedCurve *curve, std::string &description )
{
  std::string path("segmentedCurve");
  std::string report;
  if (curve->Validate(path, report, NULL) > icValidateWarning) {
    fprintf(stderr,"WARNING - curve failed validation: %s\n", report.c_str() );
    description = "segmented";
    return;
  }
  curve->Describe( description, 100 );
}

/******************************************************************************/

static
void describe1DLUT( CIccCurve *curve, std::string &description )
{
  std::string path("unknownCurve");
  std::string report;
  if (curve->Validate(path, report, NULL) > icValidateWarning) {
    fprintf(stderr,"WARNING - curve failed validation: %s\n", report.c_str() );
    description = "unknown";
    return;
  }
  curve->Describe( description, 100 );
}

/******************************************************************************/

static
void describe3DLUT( CIccMBB *curve, CIccProfile *pIcc, std::string &description )
{
  std::string path("MBBLut");
  std::string report;
  if (curve->Validate(path, report, pIcc ) > icValidateWarning) {
    fprintf(stderr,"WARNING - table failed validation: %s\n", report.c_str() );
    description = "MBBLut";
    return;
  }
  curve->Describe( description, 100 );
}

/******************************************************************************/

// output graphic representation of 1D LUTs
// return 1 if output created, 0 if none
static
int output1DLUT(CIccProfile * /* pIcc */, CIccTag *tag, const std::string &sigDesc,
        PDFWriter &pdffile )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  if (!tag) {
    fprintf(stderr, "ERROR - missing data for %s\n", sigDesc.c_str());
    return 0;
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
        return 1;
        }
      }
      break;

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
        return 1;
        }
      }
      break;

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
        return 1;
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
        describe1DLUT( uCurve, description );
#if USE_SVG
        graph1DLUTSVG( uCurve, sigDesc, description, svgfile, 1000 );
#endif
        graph1DLUTPDF( uCurve, sigDesc, description, pdffile, 1000 );
        return 1;
        }
      }
      break;

  }   // end switch by type

  return 0; // no output created

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
      // where white = no ink, black = full ink
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

static
uint8_t ClipU8( const icFloatNumber &input )
{
  if (std::isnan(input))
    return 0;
  if (std::isinf(input))
    return 255;
  if (input < 0)
    return 0;
  if (input > 255)
    return 255;
  return (uint8_t)input;
}

/******************************************************************************/

static
uint16_t ClipU16( const icFloatNumber &input )
{
  if (std::isnan(input))
    return 0;
  if (std::isinf(input))
    return 65535;
  if (input < 0)
    return 0;
  if (input > 65535)
    return 65535;
  return (uint16_t)input;
}

/******************************************************************************/

// output graphic representation of nD LUTs
// return count of output objects created, 0 if none
static
int output3DLUT( CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
        const std::string &basename, PDFWriter &pdffile )
{
  const size_t bufSize = 128;
  char buf[bufSize];
  int outputCount = 0;

  if (!tag) {
    fprintf(stderr, "Skipping %s: unable to load tag\n", sigDesc.c_str());
    return 0;
  }

  icTagTypeSignature typeSig = tag->GetType();
  switch(typeSig) {

  // these are all subclases of CIccMBB, and can share most of the code
  case icSigLut8Type:   // CIccTagLut8
  case icSigLut16Type:  // CIccTagLut16
  case icSigLutAtoBType:  // CIccTagLutAtoB
  case icSigLutBtoAType:  // CIccTagLutBtoA
    {
    CIccMBB *lut = dynamic_cast<CIccMBB*> (tag);
    if (!lut) {
      fprintf(stderr, "Skipping %s: unable to convert LUT\n", sigDesc.c_str());
      return outputCount;
    }

    std::string description;
    describe3DLUT( lut, pIcc, description );

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
      return outputCount;
    }

    if (curveA) {
      int curveACount = isInputMatrix ? outputChannels : inputChannels;
      for (int i = 0; i < curveACount; ++i) {
        if (curveA[i]) {
          std::string channel = channelName( i, !isInputMatrix,
                    inputSpace, outputSpace, inputChannels, outputChannels );
          std::string channelDesc = curveDesc + "curveA[ " + channel + " ]";
          outputCount += output1DLUT( pIcc, curveA[i], channelDesc, pdffile );
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
          outputCount += output1DLUT( pIcc, curveB[i], channelDesc, pdffile );
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
          outputCount += output1DLUT( pIcc, curveM[i], channelDesc, pdffile );
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
        fprintf(stderr,"ERROR - clut data could not be read for tag '%s' of type '%s' in file '%s'\n",
                sigDesc.c_str(), typeDesc.c_str(), basename.c_str() );
      }
      return outputCount;
    }

    // validate is called back before the Describe call
    clut->Begin();  // initialize some grid information

    int gridPoints = clut->GridPoints(); // gridSize[0]
    int tiles = gridPoints;
    if (gridPoints <= 0) {
      fprintf(stderr, "Skipping %s: invalid CLUT grid\n", sigDesc.c_str());
      return outputCount;
    }

    int tileWidth = 1;
    int tileHeight = 1;

    if (inputChannels >= 2) {
      tileWidth = clut->GridPoint(1);
      if (tileWidth <= 0) {
        fprintf(stderr, "Skipping %s: invalid CLUT width\n", sigDesc.c_str());
        return outputCount;
      }
    }

    if (inputChannels >= 3) {
      tileHeight = clut->GridPoint(2);
      if (tileHeight <= 0) {
        fprintf(stderr, "Skipping %s: invalid CLUT height\n", sigDesc.c_str());
        return outputCount;
      }
    }

    if (inputChannels > 3) {
      for (int i = 3; i < inputChannels; ++i) {
        int extraGridPoints = clut->GridPoint(i);
        if (extraGridPoints <= 0) {
          fprintf(stderr, "Skipping %s: invalid CLUT tile count\n", sigDesc.c_str());
          return outputCount;
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
    if (tiles <= 0) {
      fprintf(stderr,"WARNING - tile count overflow.\n");
      tiles = 1;
    }

    auto tempResult = std::sqrt(tiles);
    if (tempResult > std::numeric_limits<int>::max()) {
      fprintf(stderr,"ERROR - sqrt bad result!\n");
      tempResult = tiles/2;
    }
    int tilesWide = (int)tempResult;

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
      return outputCount;
    }

    //size_t clutSize = (size_t)tiles * (size_t)tileWidth * (size_t)tileHeight * (size_t)outputChannels;
    size_t bufferSize = (size_t)imageWidth * (size_t)imageHeight * (size_t)outputChannels * bytes;
    // NOTE that bufferSize will usually be greater than clutSize
    if (!bufferSize) {
      fprintf(stderr, "Skipping %s: empty image buffer\n", sigDesc.c_str());
      return outputCount;
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
            imageBuf16[outputIndex+c] = ClipU16( clutData[inputIndex+c] * 65535.0f );
        else
          for (int c = 0; c < outputChannels; ++c)
            imageBuf[outputIndex+c] = ClipU8( clutData[inputIndex+c] * 255.0f );
    }

#else
      size_t n001 = (size_t)tileWidth * (size_t)tileHeight * (size_t)outputChannels;
      size_t n010 = (size_t)tileWidth * (size_t)outputChannels;
      size_t n100 = (size_t)outputChannels;

      if (inputChannels < 2)
        std::swap(n010,n100);

      size_t outTileStepV = (size_t)imageWidth * (size_t)tileHeight * (size_t)outputChannels;
      size_t outTileStepH = (size_t)tileWidth * (size_t)outputChannels;
      size_t outColStep = (size_t)outputChannels;
      size_t outRowStep = (size_t)imageWidth * (size_t)outputChannels;

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
              imageBuf16[outputIndex+c] = ClipU16( clutData[inputIndex+c] * 65535.0f );
          else
            for (int c = 0; c < outputChannels; ++c)
              imageBuf[outputIndex+c] = ClipU8( clutData[inputIndex+c] * 255.0f );
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
    return ++outputCount;
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

  return 0; // no output was created

}   // end output3DLUT()

/******************************************************************************/

struct namedLAB {
    namedLAB() : L(0), a(0), b(0) {}

    namedLAB( std::string nn, float LL, float aa, float bb ) :
        name(nn), L(LL), a(aa), b(bb) {}

    std::string name;
    float L, a, b;
};

typedef std::vector<namedLAB> namedLabList;

/******************************************************************************/

static
int graphNamedColorsXYPDF( namedLabList &colorsOut, const std::string &description,
                        icFloatNumber *XYZIlluminant, PDFWriter &pdffile )
{
  std::ostringstream commands;

  const float bottom = 0.0f;
  const float left = 0.0f;
  const float top = pdffile.PageHeight();
  const float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );
  const float margin = 0.25f*inch2point;
  const point2D basepoint( left+margin, bottom+margin );
  const point2D rangeX( right-left-2*margin, 0 );
  const point2D rangeY( 0, top-bottom-2*margin );
  point2D scaling = (rangeX + rangeY) / chromaticityChartScale;
  float markSize = 4.0f;
  float textSize = 10.0f;

  // plot on AB grid
  // if the xyPlot xobject doesn't exist, create it now
  if (!pdffile.xobjectExists("xyPlot"))
    CreateXYPlotXobject( pdffile );

  // add the common axes
  commands << "/xyPlot Do\n";

  // add label
  point2D range( right - left, 0 );
  point2D labelBase( left, top );
  point2D tickLength(0,0);
  commands << AddGraphLabels( labelBase + range*0.5, false, tickLength, 12, description );

  point2D labelOffset( 0, markSize + 2 + textSize );
  for (auto &sample : colorsOut) {
    icFloatNumber icLAB[3];
    icFloatNumber xyzOut[3];
    icLAB[0] = sample.L;
    icLAB[1] = sample.a;
    icLAB[2] = sample.b;
    icLabtoXYZ( xyzOut, icLAB, XYZIlluminant );
    XYColor theXY = xyFromICCXYZFloat( xyzOut );

    point2D plotCenter = basepoint + scaling * point2D( theXY.x, theXY.y );
    commands << plotSquarePDF( plotCenter, markSize);
    commands << AddGraphLabels( plotCenter+labelOffset, false, point2D(0,0),
                                textSize, sample.name, kTextAlignLeft );
  }

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( content, "xyPlot" );

  return 1;
}

/******************************************************************************/

static
int graphNamedColorsABPDF( namedLabList &colorsOut, const std::string &description,
                        icFloatNumber * /*XYZIlluminant*/, PDFWriter &pdffile )
{
  std::ostringstream commands;

  const float bottom = 0.0f;
  const float left = 0.0f;
  const float top = pdffile.PageHeight();
  const float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );
  const float margin = 0.25*inch2point;
  const point2D basepoint( left+margin, bottom+margin );
  const point2D rangeX( right-left-2*margin, 0 );
  const point2D rangeY( 0, top-bottom-2*margin );
  const point2D center = 0.5f * (basepoint + point2D(right-margin,top-margin));
  float maxRadius = std::max( right-left-2*margin, top-bottom-2*margin );

  // plot on AB grid
  // if the abPlot xobject doesn't exist, create it now
  if (!pdffile.xobjectExists("abPlot"))
    CreateABPlotXobject( pdffile );

  // add the common axes
  commands << "/abPlot Do\n";

  // add label
  point2D range( right - left, 0 );
  point2D labelBase( left, top );
  point2D tickLength(0,0);
  commands << AddGraphLabels( labelBase + range*0.5, false, tickLength, 12, description );


  float symbolSize = 4.0f;
  float labelSize = 10.0f;
  point2D labelOffset( 0, symbolSize + 2 + labelSize );
  for (auto &sample : colorsOut) {
    point2D colorPt( sample.a*maxRadius/abChartScale, sample.b*maxRadius/abChartScale );
    point2D plotCenter = center + colorPt;
    commands << plotSquarePDF( plotCenter, symbolSize);
    commands << AddGraphLabels( plotCenter+labelOffset, false, point2D(0,0),
                                labelSize, sample.name, kTextAlignLeft );
  }


  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( content, "abPlot" );

  return 1;
}

/******************************************************************************/

static
int graphNamedColorsPDF( namedLabList &colorsOut, const std::string &description,
                        icFloatNumber *XYZIlluminant, PDFWriter &pdffile )
{
  std::ostringstream commands;
  int outputObjects = 0;

  outputObjects += graphNamedColorsABPDF( colorsOut, description, XYZIlluminant, pdffile );

  outputObjects += graphNamedColorsXYPDF( colorsOut, description, XYZIlluminant, pdffile );

// TODO - CIECAM16 plot as well?
    //#include "IccCAM.h" -- is CIECAM02
    // would have to add CAM16 code

  return outputObjects;
}

/******************************************************************************/

// convert all types of color lists to a known vector of names and LAB or XYZ colors
// then plot that
static
int outputNamedColors(CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
        PDFWriter &pdffile )
{
  const size_t bufSize = 64;
  char buf[bufSize];
  namedLabList colorsOut;

  int outputCount = 0;

  if (!tag) {
    fprintf(stderr, "Skipping %s: unable to load tag\n", sigDesc.c_str());
    return 0;
  }

  icTagTypeSignature typeSig = tag->GetType();

  switch(typeSig) {

    case icSigColorantTableType:    // colorant tables -- name and PCS only
      {
      CIccTagColorantTable *table = dynamic_cast<CIccTagColorantTable*> (tag);
      if (!table) {
        fprintf(stderr, "Skipping %s: unable to convert colorantTable\n", sigDesc.c_str());
        return 0;
      }

      std::string path("colorantTable");
      std::string report;
      if (table->Validate(path, report, NULL) > icValidateWarning) {
        fprintf(stderr,"WARNING - colorantTable failed validation: %s\n", report.c_str() );
        return 0;
      }

      icColorSpaceSignature pcs = pIcc->m_Header.pcs; // table->GetPCS();   // never initialized in table!

      if (pcs != icSigXYZData && pcs != icSigLabData) {
        fprintf(stderr,"WARNING - unknown pcs for colorantTable: %s\n",
                            icGetSig(buf, bufSize, pcs) );
        return 0;
      }

      icUInt32Number colorCount = table->GetSize();

      icFloatNumber XYZIlluminant[3];
      pIcc->getNormIlluminantXYZ( XYZIlluminant );

      colorsOut.reserve(colorCount);

      for (icUInt32Number i = 0; i < colorCount; ++i) {
        icFloatNumber labTemp[3];
        icColorantTableEntry *entry = table->GetEntry( i );
        namedLAB tempNamed;
        tempNamed.name = std::to_string(i+1) + std::string(" ") + std::string(entry->name);
        if (pcs == icSigXYZData) {
            // XYZ 16 bit integer
            icFloatNumber xyzTemp[3];
            xyzTemp[0] = icU16toF( entry->data[0] );
            xyzTemp[1] = icU16toF( entry->data[1] );
            xyzTemp[2] = icU16toF( entry->data[2] );
            icXYZtoLab( labTemp, xyzTemp, XYZIlluminant );
        } else {
            //  LAB 16bit integer
            labTemp[0] = icU16toF( entry->data[0] );
            labTemp[1] = icU16toF( entry->data[1] );
            labTemp[2] = icU16toF( entry->data[2] );
            icLabFromPcs( labTemp );
        }

        tempNamed.L = labTemp[0];
        tempNamed.a = labTemp[1];
        tempNamed.b = labTemp[2];

        colorsOut.push_back(tempNamed);
      }

      std::string description("Colorant Table: ");
      outputCount += graphNamedColorsPDF( colorsOut, description + sigDesc,
                        XYZIlluminant, pdffile );
      }
      break;

    case icSigNamedColor2Type:      // named color - PCS and colorspace (PCS optional?)
      {
      CIccTagNamedColor2 *table = dynamic_cast<CIccTagNamedColor2*> (tag);
      if (!table) {
        fprintf(stderr, "Skipping %s: unable to convert namedColorTable\n", sigDesc.c_str());
        return 0;
      }

      std::string path("namedColorTable");
      std::string report;
      if (table->Validate(path, report, NULL) > icValidateWarning) {
        fprintf(stderr,"WARNING - namedColorTable failed validation: %s\n", report.c_str() );
        return 0;
      }

      icColorSpaceSignature pcs = table->GetPCS();// pIcc->m_Header.pcs;

      if (pcs != icSigXYZData && pcs != icSigLabData) {
        fprintf(stderr,"WARNING - unknown pcs for namedColorTable: %s\n",
                            icGetSig(buf, bufSize, pcs) );
        return 0;
      }

      icUInt32Number colorCount = table->GetSize();

      icFloatNumber XYZIlluminant[3];
      pIcc->getNormIlluminantXYZ( XYZIlluminant );

      colorsOut.reserve(colorCount);

      std::string prefix = table->GetPrefix();
      std::string suffix = table->GetSufix();
      for (icUInt32Number i = 0; i < colorCount; ++i) {
        icFloatNumber labTemp[3];
        SIccNamedColorEntry *entry = table->GetEntry( i );
        namedLAB tempNamed;
        tempNamed.name = prefix + std::string(entry->rootName) + suffix;
        if (pcs == icSigXYZData) {
            // XYZ float
            icXYZtoLab( labTemp, entry->pcsCoords, XYZIlluminant );
        } else {
            //  LAB float
            icFloatNumber labTemp2[3];
            labTemp2[0] = entry->pcsCoords[0];
            labTemp2[1] = entry->pcsCoords[1];
            labTemp2[2] = entry->pcsCoords[2];
            table->Lab2ToLab4(labTemp,labTemp2);
            icLabFromPcs( labTemp );
        }

        tempNamed.L = labTemp[0];
        tempNamed.a = labTemp[1];
        tempNamed.b = labTemp[2];

        colorsOut.push_back(tempNamed);
      }

      std::string description("Named Color Table: ");
      outputCount += graphNamedColorsPDF( colorsOut, description + sigDesc,
                            XYZIlluminant, pdffile );
      }
      break;

    case icSigTagArrayType:         // v5 only
      {
      CIccTagArray *array = dynamic_cast<CIccTagArray*> (tag);
      if (!array) {
        fprintf(stderr, "Skipping %s: unable to convert named color array\n", sigDesc.c_str());
        return 0;
      }

// TODO - dissect structure, figure out LAB values for colors

      }
      break;

    default:
      printf("Unknown named color type %s for tag %s\n",
         icGetSig(buf, bufSize, typeSig),
         sigDesc.c_str() );
      break;
  }


  return outputCount;
}


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


  // plot RGB chromaticities, white point
  outputItems += graphChromaticityPDF( pIcc, pdffile );


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
        outputItems += output1DLUT(pIcc, pTag, sigDesc, pdffile );
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
        outputItems += output3DLUT(pIcc, pTag, sigDesc, basename, pdffile );
// TODO - plot gamut from A2B and B2A tags into xy and LAB plots
        }
        break;

      case icSigNamedColorTag:
      case icSigNamedColor2Tag:
      case icSigColorantTableTag:
      case icSigColorantTableOutTag:
       {
        const char *sigDesc = icGetSigStr(buf1, bufSize, sig);
        CIccTag *pTag = pIcc->FindTag(tag); // load if needed
        outputItems += outputNamedColors(pIcc, pTag, sigDesc, pdffile );
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
  printf("Usage: iccProfileVisualize input_profiles\n");
  printf("  output will be TIFF and PDF files next to each input profile.\n");
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

  for (int k = 1; k < argc; ++k) {
    try {
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

      delete pIcc;
    }   // end try
    catch (const std::exception& e) {
      fprintf(stderr, "ERROR processing '%s': '%s'\n", argv[k], e.what() );
    }
    catch (...) {
      fprintf(stderr, "ERROR processing '%s': unknown exception\n", argv[k] );
    }

  } // end for argc

  return 0;
}

/******************************************************************************/
/******************************************************************************/
