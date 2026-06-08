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
#include <vector>
#include <memory>
#include <algorithm>
#include "IccProfile.h"
#include "IccTag.h"
#include "IccUtil.h"
#include "IccProfLibVer.h"
#include "../IccCmdLineUtil.h"
#include "MiniTIFF.hpp"
#include "MiniSVG.hpp"
#include "MiniPDF.hpp"
#include "spectralLocus.hpp"
#include "IccVizModel.hpp"   // data-first visualization engine (graphs + raster)

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

// Cross product 2 vectors from O to A and B
// Returns a positive value, if OAB makes a counter-clockwise turn,
// negative for clockwise turn, and zero if the points are collinear.
float cross(const point2D &O, const point2D &A, const point2D &B)
{
  return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

/******************************************************************************/

// Returns a list of points on the convex hull from the set of input points.
// Duplicate points and colinear points are removed.
// Monotone chain algorithm  O(NlogN+2N)
// NOTE - Could be abstracted to any random access container
template <typename P>
std::vector<P> convex_hull2D(std::vector<P> points_in)
{
  size_t n = points_in.size();

  if (n <= 3)
    return points_in;

  std::vector<P> result(2*n); // worst case storage

  // Sort points
  std::sort( points_in.begin(), points_in.end() );

  // Build lower hull
  size_t k = 0;
  for (size_t i = 0; i < n; ++i) {
    while (k >= 2 && cross(result[k-2], result[k-1], points_in[i]) <= 0)
      k--;
    result[k++] = points_in[i];
  }

  // Build upper hull
  for (size_t i = n-1, t = k+1; i > 0; --i) {
    while (k >= t && cross(result[k-2], result[k-1], points_in[i-1]) <= 0)
      k--;
    result[k++] = points_in[i-1];
  }

  result.resize(k-1);
  return result;
}

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

  bool operator<(const XYColor& o) const {
    if (x == o.x)
      return y < o.y;
    else
      return x < o.x;
  }

public:
  float x;
  float y;
};

float cross(const XYColor &O, const XYColor &A, const XYColor &B)
{
  return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

/******************************************************************************/

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

#if 0
// output graphic representation of response curve 1D LUTs
//     or would, if I could find any example of profiles using response curves...
// return number of output items created
static
int outputResponseCurves(CIccProfile * /* pIcc */, CIccTag *tag, const std::string &sigDesc,
                        PDFWriter &pdffile, const std::string &filename )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  if (!tag) {
    fprintf(stderr, "%s: ERROR - missing data for %s\n", filename.c_str(), sigDesc.c_str());
    return 0;
  }

  icTagTypeSignature typeSig = tag->GetType();
  if (typeSig != icSigResponseCurveSet16Type)  {
    fprintf(stderr,"%s: Unknown ResponseCurve type %s for tag %s\n",
         filename.c_str(), icGetSig(buf, bufSize, typeSig), sigDesc.c_str() );
    return 0;
  }

  CIccTagResponseCurveSet16 *curves = dynamic_cast<CIccTagResponseCurveSet16*> (tag);
  if (!curves) {
      fprintf(stderr, "%s: Skipping %s: unable to convert response curves\n", filename.c_str(), sigDesc.c_str());
      return 0;
  }
  
  //icUInt16Number channels = curves->GetNumChannels();
  CIccResponseCurveStruct *curveIter = curves->GetFirstCurves();
  while (curveIter != NULL) {

// TODO - read and output

    curveIter = curves->GetNextCurves();
  }


  return 0; // no output created

}   // end outputResponseCurves()
#endif

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

// ── IccViz-backed rendering ──────────────────────────────────────────────────
//
// The functions below replace the inline profile-data extraction that the
// original output1DLUT / output3DLUT / outputNamedColors / graphChromaticityPDF
// performed by walking tags themselves. Instead they consume the DATA that
// IccVizModel (iccviz::RenderGraph / RenderRaster) returns — 2-D point series
// and ICC-normalized raster samples — and feed it through the SAME MiniPDF /
// MiniTIFF drawing primitives the original used. The artifacts look the same;
// only the data source moved behind the iccviz API.

// find a series by id within an iccviz graph (NULL if absent)
static
const iccviz::Series *findSeries( const iccviz::Graph &graph, const std::string &id )
{
  for (const auto &s : graph.series)
    if (s.id == id)
      return &s;
  return NULL;
}

/******************************************************************************/

// plot RGB chromaticities + white point from an iccviz ChromaticityXY graph.
// (replaces graphChromaticityPDF, which read the colorant/whitepoint tags itself)
static
int renderChromaticityGraph( const iccviz::Graph &graph, PDFWriter &pdffile )
{
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );
  float margin = 0.25f*inch2point;

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

  // the "primaries" series carries White (if present) then R/G/B as (x,y)
  const iccviz::Series *prim = findSeries( graph, "primaries" );
  if (prim) {
    point2D labelOffset( 0, markSize+2+textSize );
    for (const auto &v : prim->verts) {
      point2D thePt = basepoint + scaling * point2D( v.x, v.y );
      commands << plotSquarePDF( thePt, markSize );
      commands << AddGraphLabels( thePt + labelOffset, false, point2D(0,0),
                                  textSize, v.label, kTextAlignLeft );
    }
  }

  // the "gamut" series is the R-G-B triangle outline
  const iccviz::Series *gamut = findSeries( graph, "gamut" );
  if (gamut && gamut->verts.size() >= 3) {
    point2D redPt   = basepoint + scaling * point2D( gamut->verts[0].x, gamut->verts[0].y );
    point2D greenPt = basepoint + scaling * point2D( gamut->verts[1].x, gamut->verts[1].y );
    point2D bluePt  = basepoint + scaling * point2D( gamut->verts[2].x, gamut->verts[2].y );
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

// draw a 1-D tone curve from an iccviz Curve1D graph.
// (replaces graph1DLUTPDF, which sampled curve->Apply itself; the iccviz "curve"
// series is already sampled, clipped to [0,1] and identity-optimized)
static
int renderCurveGraph( const iccviz::Graph &graph, PDFWriter &pdffile )
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

  // label (may be a couple of lines) - the curve name then its description
  const std::string &name = graph.title;
  std::vector<std::string> lines = splitTextLines( graph.description );
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

  // draw the curve from the iccviz "curve" series
  const iccviz::Series *curve = findSeries( graph, "curve" );
  float scale = (7.5f-0.5f)*inch2point;
  const point2D base( 0.5f*inch2point, 0.5f*inch2point );
  if (curve && !curve->verts.empty()) {
      commands << base << " m\n";
      for (const auto &v : curve->verts) {
        point2D currentPt( v.x*scale, v.y*scale );
        commands << (base+currentPt) << " l\n";
      }
      commands << "S\n";
  }

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( content, "Axes" );

  return 1;
}

/******************************************************************************/

// output graphic representation of 1D and nD LUTs
static
int processLuts(CIccProfile *pIcc, const char *profilePath )
{
  int outputItems = 0;

  std::string tmpName = remove_extension( profilePath );
  std::string basename = icSanitizeFileName( tmpName );

// write next to input file
// write output to basename + _luts.pdf
// write basename + _ + tag + .tiff for nD LUTs

  std::string pdfPath = basename + "_luts.pdf";
  PDFWriter pdffile( pdfPath, 8*inch2point, 8*inch2point );

  // illuminant for the named-colour xy projection (Lab -> XYZ -> xy)
  icFloatNumber illum[3];
  pIcc->getNormIlluminantXYZ( illum );

  // Drive everything from the data-first IccVizModel API: enumerate the
  // visualizations the profile supports, render each one's DATA, then draw it
  // with the Mini* writers. The original walked pIcc->m_Tags and extracted the
  // data inline; that work now lives behind iccviz::Enumerate / Render*.
  // Order::TagTable reproduces iccProfileVisualize's tag-table page sequence.
  std::vector<iccviz::Descriptor> descriptors =
      iccviz::Enumerate( pIcc, iccviz::Order::TagTable );

  for (const auto &d : descriptors) {

    // nD CLUT lattice -> TIFF (replaces output3DLUT's raster path)
    if (d.output == iccviz::Output::Raster) {
      iccviz::RasterResult res = iccviz::RenderRaster( pIcc, d.id );
      if (!res.ok)
        continue;
      std::string sigDesc = d.id.substr( std::string("clut:").size() );
      std::string tiffPath = basename + "_" + sigDesc + ".tif";
      // Raster.photometric already carries the TIFF_MODE_* value the original
      // computed via TIFFColorModelFromICCModel(outputSpace).
      if (!WriteTIFF( tiffPath.c_str(), 100, res.raster.photometric,
                      res.raster.samples.data(),
                      res.raster.width, res.raster.height,
                      res.raster.channels, res.raster.bitsPerChannel )) {
        fprintf(stderr, "%s: Failed to write TIFF: %s\n", basename.c_str(), tiffPath.c_str());
      }
      ++outputItems;
      continue;
    }

    // graph kinds
    switch (d.kind) {

      // RGB chromaticities + white point (was graphChromaticityPDF)
      case iccviz::Kind::ChromaticityXY:
        {
        iccviz::GraphResult res = iccviz::RenderGraph( pIcc, d.id );
        if (res.ok)
          outputItems += renderChromaticityGraph( res.graph, pdffile );
        }
        break;

      // 1D TRC / LUT A,B,M curves (was output1DLUT)
      case iccviz::Kind::Curve1D:
        {
        iccviz::GraphResult res = iccviz::RenderGraph( pIcc, d.id );
        if (res.ok)
          outputItems += renderCurveGraph( res.graph, pdffile );
        }
        break;

      // named / colorant colours (was outputNamedColors). The a*b* render
      // carries the Lab colours; build the legacy list once and emit BOTH the
      // a*b* and xy pages, exactly as graphNamedColorsPDF did. The matching
      // NamedColorsXY descriptor is therefore handled here and skipped below.
      case iccviz::Kind::NamedColorsAB:
        {
        iccviz::GraphResult res = iccviz::RenderGraph( pIcc, d.id );
        if (res.ok) {
          const iccviz::Series *colors = findSeries( res.graph, "colors" );
          if (colors) {
            namedLabList colorsOut;
            for (const auto &v : colors->verts)   // x=a*, y=b*, aux=L*, label=name
              colorsOut.push_back( namedLAB( v.label, v.aux, v.x, v.y ) );
            // recover the original "<table>: <sig>" page label from the base
            // title (graph title is "<table> — a*b*"): Colorant Table / Named
            // Color Table / Color Array (v5 tagArray).
            std::string table = "Named Color Table";
            if (res.graph.title.rfind("Colorant Table", 0) == 0)   table = "Colorant Table";
            else if (res.graph.title.rfind("Color Array", 0) == 0) table = "Color Array";
            std::string sigDesc = d.id.substr( std::string("named:ab:").size() );
            outputItems += graphNamedColorsPDF( colorsOut, table + ": " + sigDesc,
                                                illum, pdffile );
          }
        }
        }
        break;

      // emitted together with its NamedColorsAB partner above
      case iccviz::Kind::NamedColorsXY:
        break;

// TODO - embedded height image
// TODO - embedded normal image
// TODO - BRDF images?
// TODO - LUT content from MPE tags?
// TODO - spectral viewing conditions?
// TODO - all XYZ type tags?

      // ignore everything else
      default:
        break;

    }   // end switch over descriptor kind
  }   // end loop over descriptors

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
        fprintf(stderr,"Unable to parse '%s' as ICC profile!\n", argv[k]);
        continue;
      }

      // DEBUGGING printf("Processing profile '%s'\n", argv[k]);
      auto count = processLuts( pIcc, argv[k] );
      if (!count) {
        fprintf(stderr,"Profile %s had no content for output\n", argv[k] );
      }

      delete pIcc;
    }   // end try
    catch (const std::exception& e) {
      fprintf(stderr, "%s: ERROR exception: '%s'\n", argv[k], e.what() );
    }
    catch (...) {
      fprintf(stderr, "%s: ERROR: unknown exception\n", argv[k] );
    }

  } // end for argc

  return 0;
}

/******************************************************************************/
