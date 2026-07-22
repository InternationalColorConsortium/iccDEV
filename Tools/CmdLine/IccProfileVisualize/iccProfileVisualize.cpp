/*
  File:     iccProfileVisualize.cpp

  Contains:   Console app to output parts of a profile as images and PDF plots

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
#include <cstdarg>
#include <iostream>
#include <fstream>
#include <sstream>
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
#include "../IccCmdLineUtil.h"
#include "vizShared.hpp"
#include "MiniTIFF.hpp"
#include "spectralLocus.hpp"
#include "errorLog.hpp"
#include "dataModel.hpp"
#include "dataToPDF.hpp"
#include "dataToJSON.hpp"

/******************************************************************************/

// command line option to disable warning and error reports
bool gRunSilent = false;

// internal option - should only be used by client code that wants to display errors separately
bool gLogErrorsToString = false;

// global storage of accumulated error reports
// abstracted below because this can easily become more complicated in the future
std::string gErrorLogs;

// output data to JSON instead of PDF
bool gOutputJSON = false;

/******************************************************************************/

void ClearErrorLogs()
{
  gErrorLogs.clear();
}

/******************************************************************************/

std::string &GetErrorLogs()
{
  return gErrorLogs;
}

/******************************************************************************/

// currently just used by LogAnError, but could be exported if needed
static
void AddErrorStringToLog(const std::string &input)
{
  gErrorLogs += input;
}

/******************************************************************************/

void LogAnError(FILE *stream, const char* format, ...)
{
  try {
    if (gLogErrorsToString) {
      std::va_list args;
      va_start(args, format);
      const size_t bufSize = 4096;      // could also print twice to get size, but this is simpler
      char buf [ bufSize ];
      auto len = std::vsnprintf(buf, bufSize, format, args);
      if (len > 0)
        AddErrorStringToLog( buf );
      else
        AddErrorStringToLog( "Internal buffer error while formatting: \"" + std::string(format) + "\"\n" );
      va_end(args);
      return;
    }

    // are we running silent (but not deep)?
    if (gRunSilent)
      return;

    // else normal output
    std::va_list args;
    va_start(args, format);
    (void)std::vfprintf(stream, format, args);
    va_end(args);
  }
  catch(...) {
    // don't let any exceptions escape, don't rethrow
  }

}

/******************************************************************************/

// Cross product 2 vectors from O to A and B
// Returns a positive value, if OAB makes a counter-clockwise turn,
// negative for clockwise turn, and zero if the points are collinear.
float cross(const point2D &O, const point2D &A, const point2D &B)
{
  return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

/******************************************************************************/

float cross(const XYColor &O, const XYColor &A, const XYColor &B)
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

  // Sort points in x then y
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
bool getXYZTag( CIccTag *tag, XYColor *result = NULL )
{
  std::ostringstream commands;

  auto theXYZTag = dynamic_cast<CIccTagXYZ*>(tag);
  if (theXYZTag) {
    auto theXYZ = theXYZTag->GetXYZ(0);
    if (theXYZ) {
      auto theXY = xyFromICCXYZ( theXYZ );
      if (result)
        *result = theXY;
      return true;
    }
  }

  return false;
}

/******************************************************************************/

static
float cctFromXY( const XYColor &theXY )
{
  // McCamy's formula
  // McCamy, Calvin S. (April 1992).
  // "Correlated color temperature as an explicit function of chromaticity coordinates"
  float n = (theXY.x - 0.3320f) / (0.1858f - theXY.y);
  float n2 = n*n;
  float n3 = n*n*n;
  //float cct = -437.0f *n3 + 3601.0f *n2 - 6861.0f *n + 5514.31f;    // first eq. from article
  //float cct = -449.0f *n3 + 3525.0f *n2 - 6823.3f *n + 5520.33f;  // second eq. from article
  float cct = 437.0f *n3 + 3601.0f *n2 + 6861.0f *n + 5514.31f;    // first eq.  CLOSEST!
  //float cct = 449.0f *n3 + 3525.0f *n2 + 6823.3f *n + 5520.33f;  // second eq. CLOSE!

  if (cct < 0.0 || cct > 2e9 || !std::isfinite(cct))
    return 0.0;
  else
    return cct;
}

/******************************************************************************/

struct XYName {
  std::string name;
  float x, y;
};

static
bool matches( const XYColor &u, const XYName &k )
{
  float tolerance = 0.002;
  bool matchx = fabs(u.x - k.x) < tolerance;
  bool matchy = fabs(u.y - k.y) < tolerance;
  return matchx && matchy;
}

// Are any FL or LED illuminants needed?
static const
std::vector<XYName> namedIlluminants = {
  { "D50", 0.34567, 0.35850 },
  { "D65", 0.31272, 0.32903 },
  { "D75", 0.29902, 0.31485 },
  { "D93", 0.28315, 0.29711 },
  { "D55", 0.33242, 0.34743 },
  { "Illuminant A", 0.44758, 0.40745 },	   // incandescent / tungsten
  { "Illuminant B", 0.34842, 0.35161 },    // direct sunlight at noon
  { "Illuminant C", 0.31006, 0.31616 },    // North sky daylight
  { "Illuminant E", 0.33333, 0.33333 },    // equal energy
};

/******************************************************************************/

static
std::string cctStringFromXYZ( const icXYZNumber *theXYZ )
{
  if (!theXYZ)
    return std::string();

  XYColor theXY = xyFromICCXYZ( theXYZ );

  // test known, named illuminants first
  for ( const auto &illum : namedIlluminants ) {
    if ( matches( theXY, illum ) )
        return std::string("(") + illum.name + std::string(")");
  }

  // if we can't find a match of common whitepoints, estimate the CCT
  float cct = cctFromXY( theXY );
  int32_t cctI = (int32_t)cct; // we want the integer part, don't need precision
  return std::string("(~") + std::to_string(cctI) + std::string("K)");;
}

/******************************************************************************/

static
void processChromaticity( CIccProfile *pIcc, profileVisualizationData &data )
{
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
    return;

  namedXYList points_alone;
  namedXYList points_connected;

  if (hasWhite) {
    std::string CCTText;
    auto theXYZTag = dynamic_cast<CIccTagXYZ*>(whiteTag);
    if (theXYZTag) {
      icXYZNumber *theXYZ = theXYZTag->GetXYZ(0);
      if (theXYZ)
        CCTText = cctStringFromXYZ( theXYZ );
    }
    XYColor xyVal;

    if (getXYZTag(whiteTag,&xyVal)) {
      std::string label = std::string("White") + CCTText;
      points_alone.push_back( namedXY(label,xyVal.x,xyVal.y) );
    }
  }

  if (hasRGB) {
    XYColor redPt, greenPt, bluePt;
    if (getXYZTag(redTag,&redPt)) {
      points_connected.push_back( namedXY("R",redPt.x,redPt.y) );
    }
    if (getXYZTag(greenTag,&greenPt)) {
      points_connected.push_back( namedXY("G",greenPt.x,greenPt.y) );
    }
    if (getXYZTag(blueTag,&bluePt)) {
      points_connected.push_back( namedXY("B",bluePt.x,bluePt.y) );
    }
  }

  xyPlotData *plot = new xyPlotData( "Profile Chromaticities", "Chromaticity xy", true,
                                    points_alone, points_connected );
  data.addPage(plot);
}

/******************************************************************************/

static
lutPlotData * graph1DLUT( CIccCurve *curve, const std::string &name,
                            const std::string &description, int steps )
{
  pointList points;

  // iterate the curve
  // optimization - draw only 3 points for identity curve
  if (curve->IsIdentity())
    steps = 2;
  if (steps > 0) {
      points.resize(steps+1);
      for (int i = 0; i <= steps; ++i ) {
        float input = i / (float)steps;
        float output = curve->Apply( input );
        if (std::isnan(output)) output = 0.0f;
        if (std::isinf(output)) output = 1.0f;
        if (output > 1.0f) output = 1.0f;
        if (output < 0.0f) output = 0.0f;
        points[i] = point2D( input, output );  // x is input, y is output
      }
  }

  lutPlotData *out = new lutPlotData( name, description, curve->IsIdentity(), points );

  return out;
}

/******************************************************************************/

static
bool describe1DLUT( CIccTagCurve *curve, std::string &description,
                    const std::string &sigDesc, const std::string &filename )
{
  std::string path(":");
  path += sigDesc;
  std::string report;
  if (curve->Validate(path, report, NULL) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - curve failed validation:\n%s\n", filename.c_str(), report.c_str() );
    description = "simpleCurve";
    return true;
  }

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

  return false;
}

/******************************************************************************/

static
bool describe1DLUT( CIccTagParametricCurve *curve, std::string &description,
                    const std::string &sigDesc, const std::string &filename )
{
  std::string path(":");
  path += sigDesc;
  std::string report;
  if (curve->Validate(path, report, NULL) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - curve failed validation:\n%s\n", filename.c_str(), report.c_str() );
    description = "parametric";
    return true;
  }
  curve->Describe( description, 100 );
  return false;
}

/******************************************************************************/

static
bool describe1DLUT( CIccTagSegmentedCurve *curve, std::string &description,
                    const std::string &sigDesc, const std::string &filename )
{
  std::string path(":");
  path += sigDesc;
  std::string report;
  if (curve->Validate(path, report, NULL) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - curve failed validation:\n%s\n", filename.c_str(), report.c_str() );
    description = "segmented";
    return true;
  }
  curve->Describe( description, 100 );
  return false;
}

/******************************************************************************/

static
bool describe1DLUT( CIccCurve *curve, std::string &description,
                    const std::string &sigDesc, const std::string &filename )
{
  std::string path(":");
  path += sigDesc;
  std::string report;
  if (curve->Validate(path, report, NULL) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - curve failed validation:\n%s\n", filename.c_str(), report.c_str() );
    description = "unknown";
    return true;
  }
  curve->Describe( description, 100 );
  return false;
}

/******************************************************************************/

static
bool describe3DLUT( CIccMBB *curve, CIccProfile *pIcc, std::string &description,
                    const std::string &sigDesc, const std::string &filename )
{
  std::string path(":");
  path += sigDesc;
  std::string report;
  if (curve->Validate(path, report, pIcc ) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - 3D table failed validation:\n%s\n", filename.c_str(), report.c_str() );
    description = "MBBLut";
    return true;
  }
  curve->Describe( description, 100 );
  return false;
}

/******************************************************************************/

// output graphic representation of 1D LUTs
static
void process1DLUT(CIccProfile * /* pIcc */, CIccTag *tag, const std::string &sigDesc,
                const std::string &filename, profileVisualizationData &data )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  if (!tag) {
    LogAnError(stderr, "%s: ERROR - missing data for %s\n", filename.c_str(), sigDesc.c_str() );
    return;
  }

  icTagTypeSignature typeSig = tag->GetType();


  switch(typeSig) {
    case icSigCurveType:
      {
      CIccTagCurve *curve = dynamic_cast<CIccTagCurve*> (tag);
      if (curve) {
        std::string description;
        if (describe1DLUT(curve, description, sigDesc, filename)) {
          return;
        }
        int size = curve->GetSize();
        int steps = std::max( 1000, size );
        lutPlotData * lutData = graph1DLUT( curve, sigDesc, description, steps );
        data.addPage( lutData );
        }
      }
      break;

    case icSigParametricCurveType:
      {
      CIccTagParametricCurve *pCurve = dynamic_cast<CIccTagParametricCurve*> (tag);
      if (pCurve) {
        std::string description;
        if (describe1DLUT(pCurve, description, sigDesc, filename)) {
          return;
        }
        lutPlotData * lutData = graph1DLUT( pCurve, sigDesc, description, 1000 );
        data.addPage( lutData );
        }
      }
      break;

    case icSigSegmentedCurveType:
      {
      CIccTagSegmentedCurve *sCurve = dynamic_cast<CIccTagSegmentedCurve*> (tag);
      if (sCurve) {
        std::string description;
        if (describe1DLUT(sCurve, description, sigDesc, filename)) {
          return;
        }
        lutPlotData * lutData = graph1DLUT( sCurve, sigDesc, description, 1000 );
        data.addPage( lutData );
        }
      }
      break;

    default:
      LogAnError(stderr,"%s: Unknown 1D LUT type %s for tag %s\n",
         filename.c_str(),
         icGetSig(buf, bufSize, typeSig), sigDesc.c_str() );
      {
      CIccCurve *uCurve = dynamic_cast<CIccCurve*> (tag);
      if (uCurve) {
        std::string description;
        if (describe1DLUT( uCurve, description, sigDesc, filename)) {
          return;
        }
        lutPlotData * lutData = graph1DLUT( uCurve, sigDesc, description, 1000 );
        data.addPage( lutData );
        }
      }
      break;

  }   // end switch by type

}   // end process1DLUT()

/******************************************************************************/

#if 0
// output graphic representation of response curve 1D LUTs
//     or would, if I could find any example of profiles using response curves...
static
int outputResponseCurves(CIccProfile * /* pIcc */, CIccTag *tag, const std::string &sigDesc,
                        PDFWriter &pdffile, const std::string &filename )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  if (!tag) {
    LogAnError(stderr, "%s: ERROR - missing data for %s\n", filename.c_str(), sigDesc.c_str());
    return 0;
  }

  icTagTypeSignature typeSig = tag->GetType();
  if (typeSig != icSigResponseCurveSet16Type)  {
    LogAnError(stderr,"%s: Unknown ResponseCurve type %s for tag %s\n",
         filename.c_str(), icGetSig(buf, bufSize, typeSig), sigDesc.c_str() );
    return 0;
  }

  CIccTagResponseCurveSet16 *curves = dynamic_cast<CIccTagResponseCurveSet16*> (tag);
  if (!curves) {
      LogAnError(stderr, "%s: Skipping %s: unable to convert response curves\n", filename.c_str(), sigDesc.c_str());
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

/********************************************************************************/

inline
float Edge3( float a, float b, float c )
{
  return std::abs( b - (a+c)*0.5f );
}

/********************************************************************************/

inline
double Edge3( double a, double b, double c )
{
  return std::abs( b - (a+c)*0.5f );
}

/********************************************************************************/

inline
uint8_t Edge3( uint8_t a, uint8_t b, uint8_t c )
{
  return (uint8_t) std::abs( (int)b - ((int)(a+c)>>1) );
}

/********************************************************************************/

inline
uint16_t Edge3( uint16_t a, uint16_t b, uint16_t c )
{
  return (uint16_t) std::abs( (int)b - ((int)(a+c)>>1) );
}

/********************************************************************************/

template<typename T>
void FindEdgesInner( const T *input, T *output,
            const std::vector<size_t> &dimensions_in,
            const std::vector<size_t> &steps_in,
            size_t channels, size_t totalSize, size_t depth )
{
  memset(output,0,totalSize*(depth>>3));

  size_t dimensionCount = dimensions_in.size();
  if (dimensionCount < 1)
    return;

  std::vector<size_t> wsteps( steps_in );
  std::vector<size_t> wdimensions( dimensions_in );
  std::vector<size_t> loopIndices( dimensionCount );

  if (dimensionCount == 1) {
    // fake a second dimension so the loops work
    wdimensions.push_back(1);
    wsteps.push_back(0);
    loopIndices.push_back(0);
    dimensionCount = 2;
  }

  for (size_t m = 0; m < dimensionCount; ++m) {
    size_t lastDimension = wdimensions[dimensionCount-1];

    if (lastDimension <= 2)    // nothing to operate on in this dimension
      continue;

    size_t innerLimit = lastDimension-1;
    size_t colStep = wsteps[dimensionCount-1];

    std::fill( loopIndices.begin(), loopIndices.end(), 0 );

    while( loopIndices[0] < wdimensions[0] ) {

      size_t index = 0;
      for (size_t n = 0; n < (dimensionCount-1); ++n)
        index += loopIndices[n] * wsteps[n];

      // ignore first pixel, edge duplication doesn't work well with edge detection
      index += colStep;

      for (size_t k = 1; k < innerLimit; ++k) {
        for (size_t c = 0; c < channels; ++c) {
          T prev = input[ index - colStep + c ];
          T current = input[ index + c ];
          T next = input[ index + colStep + c ];
          T result = Edge3( prev, current, next );
          T old = output[ index + c ];
          output[ index + c ] = std::max( result, old );
        }
        index += colStep;
      }

      // ignore last pixel, edge duplication doesn't work well with edge detection

      // increment loop counters, ignoring last dimension we iterated above
      //    if incremented is >= limit[j], reset and roll upward in list
      //    if we don't overflow, save the incremented value and break out of the loop
      for (int j = ((int)dimensionCount-2); j >= 0; --j) {
        auto increment_temp = loopIndices[(size_t)j] + 1;
        if (increment_temp >= wdimensions[(size_t)j] && j != 0)   // we want counter 0 to overflow, to end the big loop
          loopIndices[(size_t)j] = 0;
        else {
          loopIndices[(size_t)j] = increment_temp;
          break;
        }
      }   // end loop counter update

    }   // end voxel loop

  // prepare for next dimension
  (void)std::rotate( wsteps.begin(), wsteps.begin()+1, wsteps.end() );
  (void)std::rotate( wdimensions.begin(), wdimensions.begin()+1, wdimensions.end() );

  }   // end outer dimension loop

}

/********************************************************************************/

template<typename T>
void FindEdgesN( const T *input, T *output, const std::vector<size_t> &dimensions_in,
            const std::vector<size_t> &steps_in,
            size_t channels, size_t totalSize, size_t depth )
{
  if (depth == 8) {
    FindEdgesInner( (uint8_t *)input, (uint8_t *)output,
                    dimensions_in, steps_in,
                    channels, totalSize, 8 );
  } else if (depth == 16) {
    FindEdgesInner( (uint16_t *)input, (uint16_t *)output,
                    dimensions_in, steps_in,
                    channels, totalSize, 16 );
  } else if (depth == 32) {
    FindEdgesInner( (float *)input, (float *)output,
                    dimensions_in, steps_in,
                    channels, totalSize, 32 );
  } else if (depth == 64) {
    FindEdgesInner( (double *)input, (double *)output,
                    dimensions_in, steps_in,
                    channels, totalSize, 64 );
  } else {
    LogAnError(stderr,"ERROR - unknown clut data depth %z\n", depth );
  }
}

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

// where black = no problem, white = big problem
static
int TIFFEdgeColorModelFromICCModel( icColorSpaceSignature colorSig )
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
    case icSigLabData:
      return TIFF_MODE_RGB;
      break;

    default:
    case icSigCmykData:
    case icSigGrayData:
    case icSigGamutData:
      return TIFF_MODE_GRAY_BLACKZERO;
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

static
icFloatNumber ClipFloat( const icFloatNumber &input )
{
  if (std::isnan(input))
    return 0;
  if (std::isinf(input))
    return 1000.0;
  return input;
}

/******************************************************************************/

void CopyLUTtoTIFF( icFloatNumber *clutData, uint8_t *imageBuf,
            int tileWidth, int tileHeight, int tiles,
            int outputChannels, int inputChannels,
            int /* gridPoints */, int tilesWide, int imageWidth,
            int bytes )
{
  uint16_t *imageBuf16 = (uint16_t *)imageBuf;
  float *imageBuf32 = (float *)imageBuf;
  double *imageBuf64 = (double *)imageBuf;

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
    if (bytes == 8)
      for (int c = 0; c < outputChannels; ++c)
        imageBuf64[outputIndex+c] = ClipFloat(clutData[inputIndex+c]);
    else if (bytes == 4)
      for (int c = 0; c < outputChannels; ++c)
        imageBuf32[outputIndex+c] = ClipFloat(clutData[inputIndex+c]);
    else if (bytes == 2)
      for (int c = 0; c < outputChannels; ++c)
        imageBuf16[outputIndex+c] = ClipU16( clutData[inputIndex+c] * 65535.0f );
    else
      for (int c = 0; c < outputChannels; ++c)
        imageBuf[outputIndex+c] = ClipU8( clutData[inputIndex+c] * 255.0f );
  }

#else
  size_t n001 = (size_t)tileWidth * (size_t)tileHeight * (size_t)outputChannels;
  size_t n010 = (size_t)tileHeight * (size_t)outputChannels;
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
      if (bytes == 8)
        for (int c = 0; c < outputChannels; ++c)
          imageBuf64[outputIndex+c] = ClipFloat(clutData[inputIndex+c]);
      else if (bytes == 4)
        for (int c = 0; c < outputChannels; ++c)
          imageBuf32[outputIndex+c] = ClipFloat(clutData[inputIndex+c]);
      else if (bytes == 2)
        for (int c = 0; c < outputChannels; ++c)
          imageBuf16[outputIndex+c] = ClipU16( clutData[inputIndex+c] * 65535.0f );
      else
        for (int c = 0; c < outputChannels; ++c)
          imageBuf[outputIndex+c] = ClipU8( clutData[inputIndex+c] * 255.0f );
    }
  }
#endif

}

/******************************************************************************/

static
void processMBBType(CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
                const std::string &basename,  profileVisualizationData &data,
                bool doEdges = true )
{
  const size_t bufSize = 128;
  char buf[bufSize];

  icTagTypeSignature typeSig = tag->GetType();

  CIccMBB *lut = dynamic_cast<CIccMBB*> (tag);
  if (!lut) {
    LogAnError(stderr, "%s: Skipping %s: unable to convert LUT\n", basename.c_str(), sigDesc.c_str());
    return;
  }

  std::string description;
  if (describe3DLUT( lut, pIcc, description, sigDesc, basename)) {
    return;
  }

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
    LogAnError(stderr, "%s: Skipping %s: invalid channel count\n", basename.c_str(), sigDesc.c_str());
    return;
  }

  if (curveA) {
    int curveACount = isInputMatrix ? outputChannels : inputChannels;
    for (int i = 0; i < curveACount; ++i) {
      if (curveA[i]) {
        std::string channel = channelName( i, !isInputMatrix,
                  inputSpace, outputSpace, inputChannels, outputChannels );
        std::string channelDesc = curveDesc + "curveA[ " + channel + " ]";
        process1DLUT( pIcc, curveA[i], channelDesc, basename, data );
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
        process1DLUT( pIcc, curveB[i], channelDesc, basename, data );
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
        process1DLUT( pIcc, curveM[i], channelDesc, basename, data );
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
      LogAnError(stderr,"%s: ERROR - clut data could not be read for tag '%s' of type '%s'\n",
              basename.c_str(), sigDesc.c_str(), typeDesc.c_str() );
    }
    return;
  }

  // validate is called back before the Describe call
  clut->Begin();  // initialize some grid information

  int gridPoints = clut->GridPoints(); // gridSize[0]
  int tiles = gridPoints;
  if (gridPoints <= 0) {
    LogAnError(stderr, "%s: Skipping %s: invalid CLUT grid\n", basename.c_str(), sigDesc.c_str());
    return;
  }

  int tileWidth = 1;
  int tileHeight = 1;

  if (inputChannels >= 2) {
    tileWidth = clut->GridPoint(1);
    if (tileWidth <= 0) {
      LogAnError(stderr, "%s: Skipping %s: invalid CLUT width\n", basename.c_str(), sigDesc.c_str());
      return;
    }
  }

  if (inputChannels >= 3) {
    tileHeight = clut->GridPoint(2);
    if (tileHeight <= 0) {
      LogAnError(stderr, "%s: Skipping %s: invalid CLUT height\n", basename.c_str(), sigDesc.c_str());
      return;
    }
  }

  if (inputChannels > 3) {
    for (int i = 3; i < inputChannels; ++i) {
      int extraGridPoints = clut->GridPoint(i);
      if (extraGridPoints <= 0) {
        LogAnError(stderr, "%s: Skipping %s: invalid CLUT tile count\n", basename.c_str(), sigDesc.c_str());
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
  if (tiles <= 0) {
    LogAnError(stderr,"%s: WARNING - tile count overflow.\n", basename.c_str() );
    tiles = 1;
  }

  auto tempResult = std::sqrt(tiles);
  if (tempResult > std::numeric_limits<int>::max()) {
    LogAnError(stderr,"%s: ERROR - sqrt bad result!\n", basename.c_str() );
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
    LogAnError(stderr, "%s: Skipping %s: invalid image geometry\n", basename.c_str(), sigDesc.c_str());
    return;
  }

  //size_t clutSize = (size_t)tiles * (size_t)tileWidth * (size_t)tileHeight * (size_t)outputChannels;
  size_t bufferSize = (size_t)imageWidth * (size_t)imageHeight * (size_t)outputChannels * bytes;
  // NOTE that bufferSize will usually be greater than clutSize
  if (!bufferSize) {
    LogAnError(stderr, "%s: Skipping %s: empty image buffer\n", basename.c_str(), sigDesc.c_str());
    return;
  }

  std::unique_ptr<uint8_t[]> imageBuffer( new uint8_t[ bufferSize ] );
  uint8_t *imageBuf = imageBuffer.get();
  memset( imageBuf, 0, bufferSize );

  // copy data from CLUT to image buffer
  icFloatNumber *clutData = clut->GetData(0);
  CopyLUTtoTIFF( clutData, imageBuf, tileWidth, tileHeight, tiles, outputChannels, inputChannels,
              gridPoints, tilesWide, imageWidth, bytes );

  // write the LUT as TIFF
  std::string tiffPath2 = basename + "_" + sigDesc;
  int tiffColor = TIFFColorModelFromICCModel( outputSpace );
  imageData *image = new imageData( tiffPath2, imageBuffer.release(),
                          imageWidth, imageHeight, outputChannels, 8*bytes, tiffColor, bytes>=4 );
  data.addPage( image );

  if (doEdges) {
    imageBuffer.reset( new uint8_t[ bufferSize ] );
    imageBuf = imageBuffer.get();
    memset( imageBuf, 0, bufferSize );

    // build edge data from CLUT
    bufferSize = (size_t)imageWidth * (size_t)imageHeight * (size_t)outputChannels;
    std::unique_ptr<icFloatNumber[]> edgeBuffer( new icFloatNumber[ bufferSize ] );
    icFloatNumber *edgeData = edgeBuffer.get();

    // build data for N-dimensional edge finding
    size_t step = outputChannels;     // innermost column step == output channels
    std::vector<size_t> dimensions(inputChannels);
    std::vector<size_t> loopSteps(inputChannels);

    for (int i = inputChannels-1; i >= 0; --i) {
      loopSteps[i] = step;
      dimensions[i] = clut->GridPoint(i);
      step *= dimensions[i];
    }

    // process edges from CLUT
    FindEdgesInner( clutData, edgeData, dimensions, loopSteps, outputChannels,
                      bufferSize, 8*sizeof(icFloatNumber) );

    // copy edge data to image
    memset( imageBuf, 0, bufferSize );
    CopyLUTtoTIFF( edgeData, imageBuf, tileWidth, tileHeight, tiles, outputChannels, inputChannels,
                  gridPoints, tilesWide, imageWidth, bytes );

    // write edge data as TIFF
    std::string tiffPath3 = basename + "_" + sigDesc + "_edges";
    int edgeColor = TIFFEdgeColorModelFromICCModel( outputSpace );
    imageData *edgeImage = new imageData( tiffPath3, imageBuffer.release(),
                          imageWidth, imageHeight, outputChannels, 8*bytes, edgeColor, bytes>=4 );
    data.addPage( edgeImage );

  }   // end if doEdges
}

/******************************************************************************/

// output graphic representation of nD LUTs
// return count of output objects created, 0 if none
static
void process3DLUT( CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
        const std::string &basename, profileVisualizationData &data,
        bool doEdges = true )
{
  const size_t bufSize = 128;
  char buf[bufSize];

  if (!tag) {
    LogAnError(stderr, "%s: Skipping %s: unable to load tag\n", basename.c_str(), sigDesc.c_str());
    return;
  }

  icTagTypeSignature typeSig = tag->GetType();
  switch(typeSig) {

    // these are all subclases of CIccMBB, and can share most of the code
    case icSigLut8Type:   // CIccTagLut8
    case icSigLut16Type:  // CIccTagLut16
    case icSigLutAtoBType:  // CIccTagLutAtoB
    case icSigLutBtoAType:  // CIccTagLutBtoA
      processMBBType( pIcc, tag, sigDesc, basename, data, doEdges );
      break;

    case icSigMultiProcessElementType:
      // do nothing for now, because we don't know how to render the Multiprocess elements
      break;

    default:
      LogAnError(stderr,"%s: Unknown nD LUT type %s for tag %s\n",
         basename.c_str(),
         icGetSig(buf, bufSize, typeSig),
         sigDesc.c_str() );
      break;

  }   // end switch by type

}   // end process3DLUT()

/******************************************************************************/

static
void processNamedColorListXY( namedLabList &colorsOut, const std::string &description,
                        icFloatNumber *XYZIlluminant, profileVisualizationData &data )
{
  namedXYList xyList;
  xyList.reserve( colorsOut.size() );

  for (auto &sample : colorsOut) {
    icFloatNumber icLAB[3];
    icFloatNumber xyzOut[3];
    icLAB[0] = sample.L;
    icLAB[1] = sample.a;
    icLAB[2] = sample.b;
    icLabtoXYZ( xyzOut, icLAB, XYZIlluminant );
    XYColor theXY = xyFromICCXYZFloat( xyzOut );
    namedXY temp( sample.name, theXY.x, theXY.y );
    xyList.push_back( temp );
  }

  xyPlotData *plot = new xyPlotData( description + " xy Plot", description,
                                    true, xyList, namedXYList() );
  data.addPage( plot );
}

/******************************************************************************/

static
void processNamedColorListAB( namedLabList &colorsOut, const std::string &description,
                            profileVisualizationData &data )
{
  namedLabList abList;
  abList.reserve( colorsOut.size() );

  for (auto &sample : colorsOut) {
    namedLAB temp( sample.name, sample.L, sample.a, sample.b );
    abList.push_back( temp );
  }

  abPlotData *plot = new abPlotData( description + " ab Plot", description,
                                    true, abList, namedLabList() );
  data.addPage( plot );
}
/******************************************************************************/

static
void processNamedColorList( namedLabList &colorsOut, const std::string &description,
                           icFloatNumber *XYZIlluminant, profileVisualizationData &data )
{
  processNamedColorListAB( colorsOut, description, data );
  processNamedColorListXY( colorsOut, description, XYZIlluminant, data );

// DEFERRED - CIECAM16 plot as well?
    //#include "IccCAM.h" -- is CIECAM02
    // would have to add CAM16 code
}

/******************************************************************************/

static
void processColorantTable(CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
                        profileVisualizationData &data, const std::string &filename )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  namedLabList colorsOut;

  CIccTagColorantTable *table = dynamic_cast<CIccTagColorantTable*> (tag);
  if (!table) {
    LogAnError(stderr, "%s: Skipping %s: unable to convert colorantTable\n", filename.c_str(), sigDesc.c_str());
    return;
  }

  std::string path(":");
  path += sigDesc;
  std::string report;
  if (table->Validate(path, report, NULL) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - colorantTable failed validation:\n%s\n", filename.c_str(), report.c_str() );
    return;
  }


  icFloatNumber XYZIlluminant[3];
  pIcc->getNormIlluminantXYZ( XYZIlluminant );

  icColorSpaceSignature pcs = pIcc->m_Header.pcs;
  if (pcs != icSigXYZData && pcs != icSigLabData) {
    if (pcs != icSigNoColorData)                                // TODO - remove this once we can handle spectral data
      LogAnError(stderr,"%s: WARNING - unknown pcs for colors: %s\n",
                        filename.c_str(), icGetSig(buf, bufSize, pcs) );
    return;
  }


/*
CIccTagColorantTable::m_PCS is never set, so testing the value always fails.
This value is not written, or read as part of the table -- so we must assume that data PCS == profile PCS
*/

  icUInt32Number colorCount = table->GetSize();

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
  processNamedColorList( colorsOut, description + sigDesc, XYZIlluminant, data );
}

/******************************************************************************/

static
void processNamedColor2(CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
                        profileVisualizationData &data, const std::string &filename )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  namedLabList colorsOut;

  CIccTagNamedColor2 *table = dynamic_cast<CIccTagNamedColor2*> (tag);
  if (!table) {
    LogAnError(stderr, "%s: Skipping %s: unable to convert namedColorTable\n", filename.c_str(), sigDesc.c_str());
    return;
  }

  std::string path(":");
  path += sigDesc;
  std::string report;
  if (table->Validate(path, report, NULL) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - namedColorTable failed validation:\n%s\n", filename.c_str(), report.c_str() );
    return;
  }

  icFloatNumber XYZIlluminant[3];
  pIcc->getNormIlluminantXYZ( XYZIlluminant );

  icColorSpaceSignature pcs = pIcc->m_Header.pcs;
  if (pcs != icSigXYZData && pcs != icSigLabData) {
    if (pcs != icSigNoColorData)                                // TODO - remove this once we can handle spectral data
      LogAnError(stderr,"%s: WARNING - unknown pcs for colors: %s\n",
                        filename.c_str(), icGetSig(buf, bufSize, pcs) );
    return;
  }

  icColorSpaceSignature table_pcs = table->GetPCS();
  if (pcs != table_pcs) {
    LogAnError(stderr,"%s: WARNING - bad pcs for namedColorTable: %s\n",
                        filename.c_str(), icGetSig(buf, bufSize, pcs) );
    return;
  }

  icUInt32Number colorCount = table->GetSize();

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
  processNamedColorList( colorsOut, description + sigDesc, XYZIlluminant, data );
}

/******************************************************************************/

static
void processNamedColorArray(CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
                            profileVisualizationData &data, const std::string &filename )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  namedLabList colorsOut;

  CIccTagArray *array = dynamic_cast<CIccTagArray*> (tag);
  if (!array) {
    LogAnError(stderr, "%s: Skipping %s: unable to convert named color array\n", filename.c_str(), sigDesc.c_str());
    return;
  }

  icArraySignature arrayType = array->GetTagArrayType();
  if (arrayType != icSigColorantInfoArray
    && arrayType != icSigNamedColorArray) {
    LogAnError(stderr,"%s: WARNING - unknown color array type: %s for tag %s\n",
                    filename.c_str(),
                    icGetSig(buf, bufSize, arrayType),
                    sigDesc.c_str() );
    return;
  }

  std::string path(":");
  path += sigDesc;
  std::string report;
  if (array->Validate(path, report, NULL) > icValidateWarning) {
    LogAnError(stderr,"%s: WARNING - named color array failed validation:\n%s\n", filename.c_str(), report.c_str() );
    return;
  }

  icFloatNumber XYZIlluminant[3];
  pIcc->getNormIlluminantXYZ( XYZIlluminant );

  icColorSpaceSignature pcs = pIcc->m_Header.pcs;
  if (pcs != icSigXYZData && pcs != icSigLabData) {
    if (pcs != icSigNoColorData)                                // TODO - remove this once we can handle spectral data
      LogAnError(stderr,"%s: WARNING - unknown pcs for colors: %s\n",
                        filename.c_str(), icGetSig(buf, bufSize, pcs) );
    return;
  }

  namedLabList tempColorValues;

  icUInt32Number items = array->GetSize();

  for (icUInt32Number i = 0; i < items; ++i) {
    CIccTag *thisItem = array->GetIndex(i);
    if (!thisItem)
        continue;

    tempColorValues.clear();

    auto structType = thisItem->GetTagStructType();

    if (structType != icSigColorantInfoStruct
        && structType != icSigTintZeroStruct
        && structType != icSigNamedColorStruct) {

        LogAnError(stderr,"%s: Unknown named color struct %s for tag %s\n",
                filename.c_str(),
                icGetSig(buf, bufSize, structType),
                sigDesc.c_str() );
        continue;
    }

    CIccTagStruct *structPtr = dynamic_cast<CIccTagStruct*> (thisItem);
    if (!structPtr)
      continue;

// TODO - can we easily convert spectra to PCS? Probably not without specifying viewing conditions.
/*
CIccPcsXform::pushRef2Xyz
CIccPcsXform::pushRad2Xyz
CIccPcsXform::pushBiRef2Xyz
*/
    CIccTag *pcsElem = structPtr->FindElem(icSigCinfPcsDataMbr);
    if (!pcsElem)
      continue;

    tempColorValues.clear();

    icTagTypeSignature pcsDataType = pcsElem->GetType();

    if (pcsDataType != icSigFloat64ArrayType
        && pcsDataType != icSigFloat32ArrayType
        && pcsDataType != icSigFloat16ArrayType) {

        LogAnError(stderr,"%s: Unknown named color struct data type %s for tag %s\n",
                filename.c_str(),
                icGetSig(buf, bufSize, pcsDataType),
                sigDesc.c_str() );
        continue;
    }

    CIccTagNumArray *flt16 = dynamic_cast<CIccTagNumArray*> (pcsElem);
    if (!flt16)
      continue;

    icUInt32Number dataCount = flt16->GetNumValues();
    icUInt32Number colorCount = dataCount / 3; // ignoring any partials

    // loop over count, convert to LAB as needed
    for (icUInt32Number k = 0; k < colorCount; ++k) {
        icFloatNumber valueTemp[3];
        icFloatNumber labTemp[3];

        flt16->GetValues(valueTemp, k*3, 3);

        if (pcs == icSigXYZData) {
            // XYZ float
            icXYZtoLab( labTemp, valueTemp, XYZIlluminant );
        } else {
            //  LAB float
            labTemp[0] = valueTemp[0];
            labTemp[1] = valueTemp[1];
            labTemp[2] = valueTemp[2];
            // assume LAB directly coded as float (as seen in examples)
        }

        namedLAB tempNamed;     // leaving name empty for now
        tempNamed.L = labTemp[0];
        tempNamed.a = labTemp[1];
        tempNamed.b = labTemp[2];
        tempColorValues.push_back( tempNamed );
    }


    // now we try to find names to match the colors
    CIccTag *nameElem = structPtr->FindElem(icSigCinfNameMbr);
    if (!nameElem)      // fallback to other tag type
      nameElem = structPtr->FindElem(icSigCinfLocalizedNameMbr);        // unused so far?

    // if we don't have names, just skip it and still plot the color valuess
    if (nameElem) {
      icTagTypeSignature nameDataType = nameElem->GetType();
      std::string nameString;
      switch( nameDataType) {
        case icSigUtf8TextType:
          {
          CIccTagUtf8Text *nameUTF8 = dynamic_cast<CIccTagUtf8Text*> (nameElem);
          if (nameUTF8)
            nameString = std::string( (char *)nameUTF8->GetText() );
          }
          break;

        case icSigUtf16TextType:
          {
          CIccTagUtf16Text *nameUTF16 = dynamic_cast<CIccTagUtf16Text*> (nameElem);
          if (nameUTF16) {
            std::string buffer;
            nameString = std::string( (char *)nameUTF16->GetText(buffer) );   // GetText converts to UTF8
            }
          }
          break;

        case icSigTextType:
          {
          CIccTagText *nameText = dynamic_cast<CIccTagText*> (nameElem);
          if (nameText)
            nameString = std::string( (char *)nameText->GetText() );
          }
          break;

        case icSigDictType: // supposed to be multiLocalizedUnicodeType, but so far unused?
        case icSigMultiLocalizedUnicodeType:
          {
          CIccTagMultiLocalizedUnicode *nameDict = dynamic_cast<CIccTagMultiLocalizedUnicode*> (nameElem);
          if (nameDict) {
                // has language and first entry fallbacks
            CIccLocalizedUnicode *uniText = nameDict->Find( icLanguageCodeEnglish, icCountryCodeUSA );
            if (uniText)
              uniText->GetText(nameString);
            }
          }
          break;

        default:
          LogAnError(stderr,"%s: Unknown named color struct name type %s for tag %s\n",
                filename.c_str(),
                icGetSig(buf, bufSize, nameDataType),
                sigDesc.c_str() );
          break;
      }

      if (nameString.size() > 0) {
        for (auto &color: tempColorValues)
          color.name = nameString;
      }

    } // end name element handling


    // now try to match tint values and append to name, if present
    CIccTag *tintElem = structPtr->FindElem(icSigNmclTintMbr);
    if (tintElem) {
      icTagTypeSignature tintDataType = tintElem->GetType();

      if (tintDataType == icSigFloat64ArrayType
          || tintDataType == icSigFloat32ArrayType
          || tintDataType == icSigFloat16ArrayType) {
        CIccTagNumArray *fltTint = dynamic_cast<CIccTagNumArray*> (tintElem);
        if (fltTint) {
            icUInt32Number tintCount = fltTint->GetNumValues();
            if (tintCount <= tempColorValues.size()) {
              for (icUInt32Number k = 0; k < tintCount; ++k) {
                icFloatNumber valueTemp;
                fltTint->GetValues(&valueTemp, k, 1);
                if (!std::isfinite(valueTemp))
                  valueTemp = 0.0f;
                if (valueTemp > 1.0f)
                  valueTemp = 1.0f;
                if (valueTemp < 0.0f)
                  valueTemp = 0.0f;
                int percent = (int)round(valueTemp * 100.0f);
                tempColorValues[k].name += std::string("(") + std::to_string(percent) + std::string("%)");
              }
            }
          }
        }
        else {
            LogAnError(stderr,"%s: Unknown named color tint data type %s for tag %s\n",
                    filename.c_str(),
                    icGetSig(buf, bufSize, tintDataType),
                    sigDesc.c_str() );
            // skipping this still allows colors and names, even if we don't have tint percentages
            // so don't call continue
        }

    }   // end tint value handling

    // add temp values to our list
    if (tempColorValues.size() > 0)
      colorsOut.insert( colorsOut.end(), tempColorValues.begin(), tempColorValues.end() );

  } // end loop over items in array

  // make sure we found some usable colors and names
  if (colorsOut.size() == 0)
    return;

  std::string description("Color Array: ");
  processNamedColorList( colorsOut, description + sigDesc, XYZIlluminant, data );
}

/******************************************************************************/

// convert all types of color lists to a known vector of names and LAB or XYZ colors
// then plot that
static
void processNamedColors(CIccProfile *pIcc, CIccTag *tag, const std::string &sigDesc,
                        profileVisualizationData &data, const std::string &filename )
{
  const size_t bufSize = 64;
  char buf[bufSize];

  namedLabList colorsOut;

  if (!tag) {
    LogAnError(stderr, "%s: Skipping %s: unable to load tag\n", filename.c_str(), sigDesc.c_str());
    return;
  }

  icTagTypeSignature typeSig = tag->GetType();

  switch(typeSig) {

    case icSigColorantTableType:    // colorant tables -- name and PCS only
      processColorantTable( pIcc, tag, sigDesc, data, filename );
      break;

    case icSigNamedColor2Type:      // named color - PCS and colorspace (PCS optional?)
      processNamedColor2( pIcc, tag, sigDesc, data, filename );
      break;

    case icSigTagArrayType:         // v5 only
      processNamedColorArray( pIcc, tag, sigDesc, data, filename );
      break;

    default:
      LogAnError(stderr,"%s: Unknown named color type %s for tag %s\n",
         filename.c_str(),
         icGetSig(buf, bufSize, typeSig),
         sigDesc.c_str() );
      break;
  }

}

/******************************************************************************/

static
std::string remove_extension( const std::string& filename )
{
  size_t lastdot = filename.find_last_of(".");
  if (lastdot == std::string::npos || lastdot == 0) {
    return filename;
  }
  return filename.substr(0, lastdot);
}

/******************************************************************************/

static
std::string remove_path( const std::string& filename )
{
  size_t lastPath = filename.find_last_of("/");
  if (lastPath == std::string::npos || lastPath == 0) {
    lastPath = filename.find_last_of("\\");
  }
  if (lastPath == std::string::npos || lastPath == 0) {
    return filename;
  }
  return filename.substr(lastPath+1, filename.size() );
}

/******************************************************************************/

// create graphic representation of LUTs, named colors, etc.
static
void processProfile( CIccProfile *pIcc, const std::string &basename,
                    profileVisualizationData &data )
{
  const size_t bufSize = 64;
  char buf1[bufSize];


  // plot RGB chromaticities, white point
  processChromaticity( pIcc, data );


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
      case icSigAppleAltRedTRC:
      case icSigAppleAltGreenTRC:
      case icSigAppleAltBlueTRC:
        {
        const char *sigDesc = icGetSigStr(buf1, bufSize, sig);
        CIccTag *pTag = pIcc->FindTag(tag); // load if needed
        process1DLUT(pIcc, pTag, sigDesc, basename, data );
        }
        break;

#if 0
// I can't find any profiles that use the response tag!
// which makes it difficult to test
      case icSigOutputResponseTag:
        {
//printf("**** Found response curves in %s\n", profilePath );     // DEBUG
        const char *sigDesc = icGetSigStr(buf1, bufSize, sig);
        CIccTag *pTag = pIcc->FindTag(tag); // load if needed
        outputItems += outputResponseCurves(pIcc, pTag, sigDesc, pdffile, basename );
        }
        break;
#endif

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
        process3DLUT(pIcc, pTag, sigDesc, basename, data, sig != icSigGamutTag  );
// TODO - plot gamut from A2B and B2A tags into xy and LAB plots
        }
        break;

      // named color lists
      case icSigNamedColorTag:
      case icSigNamedColor2Tag:
      case icSigColorantTableTag:
      case icSigColorantTableOutTag:
      case icSigColorantInfoTag:
      case icSigColorantInfoOutTag:
       {
// TODO - plot named spectra as graphs?
        const char *sigDesc = icGetSigStr(buf1, bufSize, sig);
        CIccTag *pTag = pIcc->FindTag(tag); // load if needed
        processNamedColors(pIcc, pTag, sigDesc, data, basename );
       }
        break;

// TODO - embedded height image
// TODO - embedded normal image
// TODO - BRDF images?
// TODO - LUT content from MPE tags?
// TODO - spectral viewing conditions?
// TODO - all XYZ type tags?
// TODO - curveSetElement
// TODO - singleSampledCurve
/* need Apple tag structures: 'vcgt', 'vcgp', 'ndin' */

      // ignore everything else
      default:
        break;

    }   // end switch over tag signatures
  }   // end loop over tags

}   // end processProfile()

/******************************************************************************/

static
void printUsage(void)
{
  printf("Usage: iccProfileVisualize <args> input_profiles\n");
  printf("\t-silent         don't output any warnings or errors.\n");
  printf("\t-json           write data to JSON instead of PDF/TIFF.\n");
  printf("\t-V              print usage and version.\n");
  printf("\t-help           print usage and version.\n");
  printf("  output will be TIFF and PDF files next to each input profile.\n");
  printf("iccProfileVisualize built with IccProfLib version " ICCPROFLIBVER "\n\n");
}

/******************************************************************************/

typedef std::vector<std::string> filename_list;

static
filename_list parse_arguments( int argc, char *argv[] )
{
  filename_list filenames;

  for ( int c = 1; c < argc; ++c ) {

    if ( (strcasecmp( argv[c], "-silent" ) == 0 ) ) {
      gRunSilent = true;
    }
    else if ( (strcasecmp( argv[c], "-json" ) == 0 ) ) {
      gOutputJSON = true;
    }
    else if ( strcasecmp( argv[c], "-V" ) == 0
            || strcasecmp( argv[c], "--V" ) == 0
            || strcasecmp( argv[c], "-help" ) == 0
            || strcasecmp( argv[c], "--help" ) == 0
            || strcasecmp( argv[c], "-version" ) == 0
            ) {
      printUsage();
      exit (0);
    }
    else if (argv[c][0] == '-') {
      // unrecognized switch
      printUsage();
      exit (1);
    }
    else {
      // not a switch, treat it as an input file
      filenames.push_back( argv[c] );
    }

  } // end loop over arguments


  if (filenames.size() == 0) {
      printUsage();
      exit (0);
  }

  return filenames;
}

/******************************************************************************/

int main(int argc, char* argv[])
{
  if (argc <= 1) {
    printUsage();
    return 0;
  }

  filename_list fileList = parse_arguments(argc,argv);

  for (auto &file : fileList) {
    std::string sanitizedFile = icSanitizeFileName( file );

    try {
      ClearErrorLogs(); // NOTE - this is so we can get logs per input file

//printf("Processing profile '%s'\n", file.c_str() );     // DEBUGGING
      CIccProfile *pIcc = OpenIccProfile( file.c_str() );
      if (!pIcc) {
        LogAnError(stderr,"Unable to parse '%s' as ICC profile!\n", sanitizedFile.c_str() );
        continue;
      }

      std::string basename = remove_extension( sanitizedFile );

      profileVisualizationData data;
      data.name = remove_path( sanitizedFile );
      processProfile( pIcc, basename, data );

      size_t count = 0;
      if (gOutputJSON)
        count = outputDataToJSON( data, sanitizedFile );
      else
        count = outputDataToPDF( data, basename );
      
      if (!count) {
        LogAnError(stderr,"Profile %s had no content for output\n", sanitizedFile.c_str() );
      }

      delete pIcc;
    }   // end try
    catch (const std::exception& e) {
      LogAnError(stderr, "%s: ERROR exception: '%s'\n", sanitizedFile.c_str() , e.what() );
    }
    catch (...) {
      LogAnError(stderr, "%s: ERROR: unknown exception\n", sanitizedFile.c_str() );
    }

    // NOTE - consume error logs here if needed, so exceptions are included

  } // end for file list

  return 0;
}

/******************************************************************************/
/******************************************************************************/
