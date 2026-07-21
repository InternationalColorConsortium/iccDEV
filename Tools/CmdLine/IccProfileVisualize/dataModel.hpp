//
//  dataModel.hpp
//      for use with iccProfileVisualize
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

#ifndef dataModel_hpp
#define dataModel_hpp

#include <cstdint>
#include <string>
#include <memory>
#include "MiniTIFF.hpp"     // need color modes

/******************************************************************************/

struct namedXY {
    namedXY() : x(0), y(0) {}

    namedXY( std::string nn, float xx, float yy ) :
        name(nn), x(xx), y(yy) {}

    std::string name;
    float x, y;
};

typedef std::vector<namedXY> namedXYList;

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

// parent classs for abstraction
struct dataAbstractionBase {
  virtual ~dataAbstractionBase() {};
  virtual std::string getDataType() const = 0;
};

/******************************************************************************/

// document class - takes ownership of page pointers
struct profileVisualizationData {

  // because this class owns the pointers
  ~profileVisualizationData() {
    for (auto ptr : pages)
        delete ptr;
  }

  // this class takes ownership of the pointers
  void addPage( dataAbstractionBase *obj ) {
    pages.push_back(obj);
  }

  std::string name;             // sanitized filename
  std::vector<dataAbstractionBase *> pages;
};

/******************************************************************************/

// xy chromaticity
struct xyPlotData : dataAbstractionBase {

  xyPlotData() : printLabels(true) {}

  xyPlotData(const std::string &name, const std::string &label, bool printLabelFlag,
              const namedXYList &points_alone, const namedXYList &points_outlined ) {
    object_name = name;
    object_label = label;
    printLabels = printLabelFlag;
    points_unconnected = points_alone;
    points_connected = points_outlined;
  }

  virtual std::string getDataType() const { return "xyPlotData"; };

  std::string object_name;        // page name in PDF
  std::string object_label;       // graph label, may be multiline
  bool printLabels;
  namedXYList points_unconnected;
  namedXYList points_connected;     // always connected and closed
};

/******************************************************************************/

// ab chromaticity
struct abPlotData : dataAbstractionBase {

  abPlotData() : printLabels(true) {}

  abPlotData(const std::string &name, const std::string &label, bool printLabelFlag,
              const namedLabList &points_alone, const namedLabList &points_outlined ) {
    object_name = name;
    object_label = label;
    printLabels = printLabelFlag;
    points_unconnected = points_alone;
    points_connected = points_outlined;
  }

  virtual std::string getDataType() const { return "abPlotData"; };

  std::string object_name;           // page name in PDF
  std::string object_label;          // graph label, may be multiline
  bool printLabels;
  namedLabList points_unconnected;
  namedLabList points_connected;     // always connected and closed, TODO - sort and convex hull
};

/******************************************************************************/

// LookUpTable Graph
struct lutPlotData : dataAbstractionBase {

  lutPlotData() : isIdentity(false) {}

  lutPlotData(const std::string &name, const std::string &label, bool identityFlag,
              const pointList &points_input ) {
    object_name = name;
    object_label = label;
    isIdentity = identityFlag;
    points = points_input;
  }

  virtual std::string getDataType() const { return "lutPlotData"; };

  std::string object_name;        // page name in PDF
  std::string object_label;       // graph label, may be multiline
  bool isIdentity;
  pointList points;               // always connected, but not closed
};

/******************************************************************************/

enum {
    IMAGE_MODE_GRAY_BLACKZERO = TIFF_MODE_GRAY_BLACKZERO,
    IMAGE_MODE_GRAY_WHITEZERO = TIFF_MODE_GRAY_WHITEZERO,
    IMAGE_MODE_RGB = TIFF_MODE_RGB,
    IMAGE_MODE_CIELAB = TIFF_MODE_CIELAB,
    IMAGE_MODE_CMYK = TIFF_MODE_CMYK,
};

// this takes ownership of the buffer pointer passed in
// pointer must be allocated with new[]
// TODO - can I use a unique_ptr to manage this?
struct imageData : dataAbstractionBase {

  imageData() : width(0), height(0), channels(0), depth(0),
                mode(IMAGE_MODE_GRAY_BLACKZERO),
                isFloatingPoint(false), data(NULL) {}

  imageData(const std::string &name, uint8_t *imageBuffer, int imageWidth, int imageHeight,
            int imageChannels, int imageDepth, int imageMode,
            bool dataIsFloatingPoint = false ) {
    object_name = name;
    width = imageWidth;
    height = imageHeight;
    channels = imageChannels,
    depth = imageDepth;
    mode = imageMode;
    isFloatingPoint = dataIsFloatingPoint;
    data = imageBuffer;
  }

  ~imageData() {
    delete[] data;
  }

  virtual std::string getDataType() const { return "imageData"; };

  std::string object_name;  // page name in PDF, filename on disk
  int width;
  int height;
  int channels;
  int depth;                // bits per sample [8,16,32,64]
  int mode;                 // interpretation of channels, matches TIFF modes
  bool isFloatingPoint;     // because 32 bit integer can be useful, as well as 32 bit float
  uint8_t *data;
};

/******************************************************************************/

struct XYColor
{
  XYColor () : x(0), y(0) {}
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

/******************************************************************************/

#endif /* dataModel_hpp */
