//
//  dataToPDF.cpp
//      convert generic data to PDF and TIFF files
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

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include "vizShared.hpp"
#include "MiniPDF.hpp"
#include "MiniTIFF.hpp"
#include "errorLog.hpp"
#include "dataModel.hpp"
#include "dataToPDF.hpp"
#include "spectralLocus.hpp"

/******************************************************************************/

/*
label points for spectrum in nm
sorta, kinda evenly spaced, plus endpoints
 */
static
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
point2D spectrumLabelOffset( int nm, float textSize, PDFTextAlignment &align )
{
// NOTE - Yes, I could create normal vectors from the locus points, etc.
// but this looks better with less math, and is much easier to debug.

  if (nm < 515) {
    // go left
    align = kPDFTextAlignRight;
    return point2D( -0.5*textSize, 0.0f );
  } else if (nm == 515) {
    // go left a little bit more
    align = kPDFTextAlignRight;
    return point2D( -0.9*textSize, 0.0f );
  } else if (nm <= 525) {
    // go up
    align = kPDFTextAlignCenter;
    return point2D( 0.0f, textSize*1.5f );
  } else {
    // go right
    align = kPDFTextAlignLeft;
    return point2D( textSize*0.5, textSize );
  }

  // unreachable
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


  if (pdfout.xobjectExists("xyPlot"))
    return;

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
    PDFTextAlignment align;
    point2D offset = spectrumLabelOffset( nm,labelSize, align );
    size_t index = nm - wavelengthOffset;
    point2D thispoint = basepoint + scaling * point2D( spectralLocus2degree[index].x,
                                                       spectralLocus2degree[index].y );
    std::string number = std::to_string(nm);
    commands << PDFSingleLineTextLabel( thispoint + offset, false, point2D(0,0), labelSize, number, align );
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

  if (pdfout.xobjectExists("abPlot"))
    return;

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
  commands << PDFSingleLineTextLabel( labelPtYellow, false, point2D(0,0), labelSize, "+b Yellow", kPDFTextAlignCenter );
  point2D labelPtBlue( center.x,bottom+margin+labelSize*1.2f);
  commands << PDFSingleLineTextLabel( labelPtBlue, false, point2D(0,0), labelSize, "-b Blue", kPDFTextAlignCenter );
  point2D labelPtMagenta(right-margin-8,center.y);
  commands << PDFSingleLineTextLabel( labelPtMagenta, false, point2D(0,0), labelSize, "+a Magenta", kPDFTextAlignRight );
  point2D labelPtTeal(left+margin,center.y);
  commands << PDFSingleLineTextLabel( labelPtTeal, false, point2D(0,0), labelSize, "-a Green", kPDFTextAlignLeft );

  // end colored grid, grestore
  commands << "Q\n";
  std::string commandString = commands.str();
  pdfout.AddXObject( bounds, commandString, "abPlot" );
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
  commands << PDFSingleLineTextLabel( basepoint, isVertical, tickLength, labelSize, zero );
  commands << PDFSingleLineTextLabel( basepoint+0.5f*range, isVertical, tickLength, labelSize, half );
  commands << PDFSingleLineTextLabel( basepoint+range, isVertical, tickLength, labelSize, full, kPDFTextAlignRight );

  // IO label near 2/3
  commands << PDFSingleLineTextLabel( basepoint + range*0.66, isVertical, tickLength, labelSize, label );

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

  if (pdfout.xobjectExists("Axes"))
    return;

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

static
std::string plotSquarePDF( const point2D &center, float size )
{
  std::ostringstream commands;

  float half = 0.5f * size;

  point2D pt0(center.x-half,center.y-half);
  point2D pt1(center.x-half,center.y+half);
  point2D pt2(center.x+half,center.y+half);
  point2D pt3(center.x+half,center.y-half);

  commands << pt0 << " m " << pt1 << " l ";
  commands << pt2 << " l ";
  commands << pt3 << " l s\n";

  return commands.str();
}

/******************************************************************************/

static
void outputNamedColorsXYPDF( xyPlotData *plot, PDFWriter &pdffile )
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
  // create the xyPlot xobject if doesn't exist
  CreateXYPlotXobject( pdffile );

  // add the common axes
  commands << "/xyPlot Do\n";

  // gsave, color black
  commands << "q 0 0 0 1 K\n";

  // add label
  point2D range( right - left, 0 );
  point2D labelBase( left, top );
  point2D tickLength(0,0);
  commands << PDFSingleLineTextLabel( labelBase + range*0.5, false, tickLength, 12, plot->object_label );

  point2D labelOffset( 0, markSize + 2 + textSize );
  for ( auto &sample : plot->points_unconnected ) {
    point2D plotCenter = basepoint + scaling * point2D( sample.x, sample.y );
    commands << plotSquarePDF( plotCenter, markSize);
    commands << PDFSingleLineTextLabel( plotCenter+labelOffset, false, point2D(0,0),
                                textSize, sample.name, kPDFTextAlignLeft );
  }

  // connected line
  size_t connected_count = plot->points_connected.size();
  if (connected_count > 1) {
    // gsave and 50% gray
    commands << "q 0 0 0 0.5 K\n";
    auto &sampleC = plot->points_connected[0];
    point2D plotCenter = basepoint + scaling * point2D( sampleC.x, sampleC.y );
    commands << plotCenter << " m ";
    for (size_t i = 1; i < connected_count; ++i) {
      auto &sample = plot->points_connected[i];
      plotCenter = basepoint + scaling * point2D( sample.x, sample.y );
      commands << plotCenter << " l ";
    }
    // stroke and grestore
    commands << "s Q\n";
  }

  // connected marks and labels
  for (size_t i = 0; i < connected_count; ++i) {
    auto &sample = plot->points_connected[i];
    point2D plotCenter = basepoint + scaling * point2D( sample.x, sample.y );
    commands << plotSquarePDF( plotCenter, markSize );
    commands << PDFSingleLineTextLabel( plotCenter+labelOffset, false, point2D(0,0),
                                textSize, sample.name, kPDFTextAlignLeft );
  }

  // grestore
  commands << "Q\n";

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( plot->object_name, content, "xyPlot" );
}

/******************************************************************************/

static
void outputNamedColorsABPDF( abPlotData *plot, PDFWriter &pdffile )
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
  CreateABPlotXobject( pdffile );

  // add the common axes
  commands << "/abPlot Do\n";

  // gsave, color black
  commands << "q 0 0 0 1 K\n";

  // add label
  point2D range( right - left, 0 );
  point2D labelBase( left, top );
  point2D tickLength(0,0);
  commands << PDFSingleLineTextLabel( labelBase + range*0.5, false, tickLength, 12, plot->object_label );

  float symbolSize = 4.0f;
  float labelSize = 10.0f;
  point2D labelOffset( 0, symbolSize + 2 + labelSize );
  for ( auto &sample : plot->points_unconnected ) {
    point2D colorPt( sample.a*maxRadius/abChartScale, sample.b*maxRadius/abChartScale );
    point2D plotCenter = center + colorPt;
    commands << plotSquarePDF( plotCenter, symbolSize);
    commands << PDFSingleLineTextLabel( plotCenter+labelOffset, false, point2D(0,0),
                                labelSize, sample.name, kPDFTextAlignLeft );
  }

  // connected line
  size_t connected_count = plot->points_connected.size();
  if (connected_count > 1) {
    // gsave and 50% gray
    commands << "q 0 0 0 0.5 K\n";
    auto &sampleC = plot->points_connected[0];
    point2D colorPt( sampleC.a*maxRadius/abChartScale, sampleC.b*maxRadius/abChartScale );
    point2D plotCenter = center + colorPt;
    commands << plotCenter << " m ";
    for (size_t i = 1; i < connected_count; ++i) {
      auto &sample = plot->points_connected[i];
      colorPt = point2D( sample.a*maxRadius/abChartScale, sample.b*maxRadius/abChartScale );
      plotCenter = center + colorPt;
      commands << plotCenter << " l ";
    }
    // stroke and grestore
    commands << "s Q\n";
  }

  // connected marks and labels
  for (size_t i = 0; i < connected_count; ++i) {
    auto &sample = plot->points_connected[i];
    point2D colorPt( sample.a*maxRadius/abChartScale, sample.b*maxRadius/abChartScale );
    point2D plotCenter = center + colorPt;
    commands << plotSquarePDF( plotCenter, symbolSize );
    commands << PDFSingleLineTextLabel( plotCenter+labelOffset, false, point2D(0,0),
                                labelSize, sample.name, kPDFTextAlignLeft );
  }

  // grestore
  commands << "Q\n";


  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();

  pdffile.AddPage( plot->object_name, content, "abPlot" );
}


/******************************************************************************/

static
std::string extract_LUTNameForBookmark(const std::string& str)
{
  size_t start = 0;
  size_t end = str.find(']');
  if (end != std::string::npos)
    return str.substr(start, end - start + 1);
  end = str.find('\n');
  if (end != std::string::npos)
    return str.substr(start, end - start);
  return str;
}

/******************************************************************************/

static
void output1DLUTPDF( lutPlotData *lut, PDFWriter &pdffile )
{
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );

  // if the axes xobject doesn't exist, create it now
  CreateAxesXobject( pdffile );

  // add the common axes
  commands << "/Axes Do\n";

  point2D labelPt( 0.5f*right, top - 0.2f*inch2point );
  float labelSize = 12.0f;     // points
  float leading = labelSize * 1.1f;
  float indent = 0.5f * inch2point;
  std::string fullName = lut->object_name + " " + lut->object_label;;
  commands << PDFMultiLineText( labelPt, labelSize, leading, indent,
                                    fullName, kPDFTextAlignCenterLeft );

  // draw the curve
  // optimization - draw only 3 points for identity curve
  size_t steps = lut->points.size();
  if (steps > 0) {
    float scale = (7.5f-0.5f)*inch2point;
    const point2D base( 0.5f*inch2point, 0.5f*inch2point );
    commands << base << " m\n";
    for (size_t i = 0; i < steps; ++i ) {
      point2D currentPt = lut->points[i] * scale;
      commands << (base+currentPt) << " l\n";
    }
    commands << "S\n";
  }

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();
  std::string pageName = extract_LUTNameForBookmark( fullName );
  pdffile.AddPage( pageName, content, "Axes" );
}

/******************************************************************************/

size_t outputDataToPDF( profileVisualizationData &data, const std::string &basename )
{
  if (data.pages.size() == 0)
    return 0;

// write next to input file
// write output to basename + _luts.pdf
// write basename + _ + tag + .tif for nD LUTs

  std::string pdfPath = basename + "_luts.pdf";
  PDFWriter pdffile( pdfPath, 8*inch2point, 8*inch2point );

  // iterate over pages in data
  for (auto &page : data.pages ) {
    std::string pageType = page->getDataType();

    if (pageType == std::string("lutPlotData")) {
      lutPlotData *lutData = dynamic_cast<lutPlotData*>(page);
      output1DLUTPDF( lutData, pdffile );
    }
    else if (pageType == std::string("abPlotData")) {
      abPlotData *abData = dynamic_cast<abPlotData*>(page);
      outputNamedColorsABPDF( abData, pdffile );
    }
    else if (pageType == std::string("xyPlotData")) {
      xyPlotData *xyData = dynamic_cast<xyPlotData*>(page);
      outputNamedColorsXYPDF( xyData, pdffile );
    }
    else if (pageType == std::string("imageData")) {
      imageData *image = dynamic_cast<imageData*>(page);
      std::string tiffPath2 = image->object_name + ".tif";      // includes tag name
      if (!WriteTIFF( tiffPath2.c_str(), 100, image->mode, image->data,
                    image->width, image->height, image->channels, image->depth )) {
        LogAnError(stderr, "%s: Failed to write TIFF: %s\n", basename.c_str(), tiffPath2.c_str());
      }
    }
    else {
      LogAnError(stderr, "%s: unknown data type %s\n", basename.c_str(), pageType.c_str());
    }
  } // end iteration over pages

  size_t objectCount = pdffile.PageCount(); // grab page count before destroying the pages

  pdffile.CloseFile();

  return objectCount;

}   // end outputDataToPDF()

/******************************************************************************/
/******************************************************************************/
