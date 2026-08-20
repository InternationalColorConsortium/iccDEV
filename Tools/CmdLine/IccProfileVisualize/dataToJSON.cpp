//
//  dataToJSON.cpp
//      convert generic data to JSON
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
#include <iomanip>
#include "IccCmdLineUtil.h"
#include "vizShared.hpp"
#include "errorLog.hpp"
#include "dataModel.hpp"
#include "dataToJSON.hpp"
#include "spectralLocus.hpp"

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

std::ostream& operator<<( std::ostream &out, const namedXYList &data )
{
  out << "[\n";

  for (const auto &entry: data ) {
    out << "\t{ \"name\": \"" << entry.name << "\"";
    out << " \"x\": " << entry.x;
    out << " \"y\": " << entry.y;
    out << " },\n";
  }

  // json doesn't like trailing commas
  if (data.size() > 0) {
    out.seekp( -2, std::ios_base::end );
    out << "\n";
  }

  out << "]";
  return out;
}

/******************************************************************************/

std::ostream& operator<<( std::ostream &out, const namedLabList &data )
{
  out << "[\n";

  for (const auto &entry: data ) {
    out << "\t{ \"name\": \"" << entry.name << "\"";
    out << " \"L\": " << entry.L;
    out << " \"a\": " << entry.a;
    out << " \"b\": " << entry.b;
    out << " },\n";
  }

  // json doesn't like trailing commas
  if (data.size() > 0) {
    out.seekp( -2, std::ios_base::end );
    out << "\n";
  }

  out << "]";
  return out;
}

/******************************************************************************/

std::ostream& operator<<( std::ostream &out, const pointList &data )
{
  out << "[\n";

  for (const auto &entry: data ) {
    out << "\t{ \"x\": " << entry.x;
    out << " \"y\": " << entry.y;
    out << " },\n";
  }

  // json doesn't like trailing commas
  if (data.size() > 0) {
    out.seekp( -2, std::ios_base::end );
    out << "\n";
  }

  out << "]";
  return out;
}

/******************************************************************************/

std::string jsonBool( bool &val ) {
    if (val)
      return "true";
    else
      return "false";
}

/******************************************************************************/

static
void outputNamedColorsXYJSON( xyPlotData *plot, std::ostringstream &out )
{
  std::ostringstream commands;
  
  commands << "\"" << plot->object_name << "\": {\n";
  commands << "\"type\": \"" << plot->getDataType() << "\",\n";
  commands << "\"label\": \"" << plot->object_label << "\",\n";
  commands << "\"printLabels\": " << jsonBool(plot->printLabels) << ",\n";
  
  commands << "\"unconnected_points\": " << plot->points_unconnected << ",\n";
  commands << "\"connected_points\": " << plot->points_connected << "\n";
  
  commands << "}";

  out << commands.str();
}

/******************************************************************************/

static
void outputNamedColorsABJSON( abPlotData *plot, std::ostringstream &out )
{
  std::ostringstream commands;
  
  commands << "\"" << plot->object_name << "\": {\n";
  commands << "\"type\": \"" << plot->getDataType() << "\",\n";
  commands << "\"label\": \"" << plot->object_label << "\",\n";
  commands << "\"printLabels\": " << jsonBool(plot->printLabels) << ",\n";
  
  commands << "\"unconnected_points\": " << plot->points_unconnected << ",\n";
  commands << "\"connected_points\": " << plot->points_connected << "\n";
  
  commands << "}";

  out << commands.str();
}

/******************************************************************************/

static
void output1DLUTJSON( lutPlotData *lut, std::ostringstream &out )
{
  std::ostringstream commands;
  
  commands << "\"" << lut->object_name << "\": {\n";
  commands << "\"type\": \"" << lut->getDataType() << "\"\n";
  commands << "\"label\": \"" << lut->object_label << "\",\n";
  commands << "\"isIdentity\": " << jsonBool(lut->isIdentity) << ",\n";
  
  commands << "\"points\": " << lut->points << "\n";
  
  commands << "}";

  out << commands.str();
}

/******************************************************************************/

// start with something simple
// ccox - I can make it faster later, if necessary
static
std::string hexify( uint8_t *data, size_t byteCount )
{
  std::ostringstream commands;
  commands << std::hex << std::setfill('0');

  for (size_t i = 0; i < byteCount; ++i) {
    int byte = data[i]; // promote to int to prevent stream from printing a character
    commands << std::setw(2) << byte;   // why does width have to reset *every* time?
  }

  return commands.str();
}

/******************************************************************************/

static
void outputImageJSON( imageData *image, std::ostringstream &out )
{
  std::ostringstream commands;
  
  commands << "\"" << remove_path(image->object_name) << "\": {\n";
  commands << "\"type\": \"" << image->getDataType() << "\"\n";
  commands << "\"width\": " << image->width << ",\n";
  commands << "\"height\": " << image->height << ",\n";
  commands << "\"channels\": " << image->channels << ",\n";
  commands << "\"depth\": " << image->depth << ",\n";
  commands << "\"mode\": " << image->mode << ",\n";
  commands << "\"isFloatingPoint\": " << jsonBool(image->isFloatingPoint) << ",\n";
  
  size_t bytes = (size_t)image->width * (size_t)image->height * (size_t)image->channels  * (size_t)(image->depth/8);
  commands << "\"hexData\": \"" << hexify(image->data,bytes) << "\"\n";
  
  commands << "}";

  out << commands.str();
}

/******************************************************************************/

static
void writeHeaderJSON( std::ostringstream &out, profileVisualizationData &data, const std::string &basename )
{
  std::ostringstream commands;
  
  // start of file
  commands << "{\n";
  
  // global data
  commands << "\"format\": \"iccProfileVisualizationData\",\n";
  commands << "\"version\": 1,\n";
  commands << "\"input\": \"" << basename << "\",\n";
  commands << "\"name\": \"" << data.name << "\",\n";
  commands << "\"pageCount\": \"" << data.pages.size() << "\",\n";
  
  // start page array
  commands << "\"pageArray\": [\n";

  out << commands.str();
}

/******************************************************************************/

static
void writeFooterJSON( std::ostringstream &out )
{
  std::ostringstream commands;

  // end of page array, end of file
  commands << "\t]\n}\n";
  
  out << commands.str();
}

/******************************************************************************/

static bool WriteJSONTextFile(FILE* outFile, const std::string& text)
{
  bool failed = false;

  if (!outFile)
    return false;

  if (!text.empty() && fwrite(text.data(), 1, text.size(), outFile) != text.size())
    failed = true;

  if (!icFlushAndClose(outFile))
    failed = true;

  return !failed;
}

/******************************************************************************/

size_t outputDataToJSON( profileVisualizationData &data, const std::string &basename )
{
  if (data.pages.size() == 0)
    return 0;

  std::string JSONPath = remove_extension(basename) + "_data.json";
  std::ostringstream out;
  out.exceptions(std::ios::badbit | std::ios::failbit);

  size_t objectCount = 0;

  writeHeaderJSON( out, data, basename );

  // iterate over pages in data
  for (auto &page : data.pages ) {
    std::string pageType = page->getDataType();

    if (pageType == std::string("lutPlotData")) {
      lutPlotData *lutData = dynamic_cast<lutPlotData*>(page);
      output1DLUTJSON( lutData, out );
      objectCount++;
    }
    else if (pageType == std::string("abPlotData")) {
      abPlotData *abData = dynamic_cast<abPlotData*>(page);
      outputNamedColorsABJSON( abData, out );
      objectCount++;
    }
    else if (pageType == std::string("xyPlotData")) {
      xyPlotData *xyData = dynamic_cast<xyPlotData*>(page);
      outputNamedColorsXYJSON( xyData, out );
      objectCount++;
    }
    else if (pageType == std::string("imageData")) {
      imageData *image = dynamic_cast<imageData*>(page);
      outputImageJSON( image, out );
      objectCount++;
    }
    else {
      LogAnError(stderr, "%s: unknown data type %s\n", basename.c_str(), pageType.c_str());
    }

    if (objectCount > 0)
        out << ",\n";

  } // end iteration over pages
  
  // erase last item comma and newline
  if (objectCount > 0) {
    out.seekp( -2, std::ios_base::end );
    out << "\n";
  }

  writeFooterJSON( out );

  FILE* outFile = icOpenRegularWriteTextFile(JSONPath.c_str());
  if (!WriteJSONTextFile(outFile, out.str())) {
    LogAnError(stderr, "JSON writing error in '%s': unable to open regular output file\n", JSONPath.c_str());
    return 0;
  }

  return objectCount;

}   // end outputDataToJSON()

/******************************************************************************/
/******************************************************************************/
