//
//  MiniSVG.cpp
//
//  Writes a subset of SVG files
//

/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include "MiniSVG.hpp"
#include "IccCmdLineUtil.h"
#include "errorLog.hpp"

/******************************************************************************/

void SVGOut::OpenFile( const std::string &filename )
{
  if (!m_filename.empty()) {
    LogAnError(stderr,"WARNING - SVG file already open!\n");
  }
  m_GroupLevel = 0;
  m_pageCount = 0;
  m_pageInProgress = false;
  m_filename = filename;
  m_buf.str("");
  m_buf.clear();
}

/******************************************************************************/

void SVGOut::CloseFile()
{
  if (m_pageInProgress) {
    m_buf << "</g>\n";
    m_pageInProgress = false;
  }
  
  if (m_GroupLevel > 0)
    LogAnError(stderr,"WARNING - SVG group levels not closed.\n");
  if (m_GroupLevel < 0)
    LogAnError(stderr,"WARNING - Too many SVG group levels closed.\n");

  if (!m_filename.empty()) {
    if (m_pageCount > 0) {
      try  {
        FILE* checkFile = icOpenRegularWriteTextFile(m_filename.c_str());
        if (!checkFile || !icFlushAndClose(checkFile)) {
          LogAnError(stderr, "SVG writing error in '%s': unable to open regular output file\n", m_filename.c_str());
          m_filename.clear();
          return;
        }

        std::ofstream out;
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out.open(m_filename);
        WriteHeader(out);
        out << m_buf.str();
        WriteFooter(out);
        out.close();
      }
      catch (const std::exception& e) {
        LogAnError(stderr, "SVG writing error in '%s': '%s'\n", m_filename.c_str(), e.what() );
      }
      catch (...) {
        LogAnError(stderr, "SVG writing error in '%s': unknown exception\n", m_filename.c_str());
      }
    }   // if pages > 0
    
    m_filename.clear();
  } // if filename !empty

}

/******************************************************************************/

// NOTE - the viewBox and background are always interpreted as points for some reason, can't use mm for scale modification
// NOTE - pageset / page in SVG 1.2 sound nice, but don't appear to be supported anywhere anymore.
// To get this working everywhere, we may have to spit out a file per page!
//  OR use PDF.
void SVGOut::WriteHeader( std::ostream &out )
{
  float w = m_pageWidth;
  float h = m_pageHeight * m_pageCount;
  out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
  out << "<svg version=\"1.1\" id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n";
  out << "  width=\"" << w << "\" height=\"" << h
      << "\" viewBox=\"0 0 " << w << " " << h << "\">\n";
}

/******************************************************************************/

// type size is in points, rotation in degrees
void SVGOut::AddText( const float xCoord, const float yCoord, const std::string &text,
          const float size, const std::string &font,
          const std::string &style, const std::string &align,
          const float rotation )
{
  m_buf << "<text";

  if (align != std::string()) {
    if (align == "left" || align == "Left")
      m_buf << " text-anchor=\"start\"";
    else if (align == "middle" || align == "Middle"
        || align == "center" || align == "Center")
      m_buf << " text-anchor=\"middle\"";
    else if (align == "right" || align == "Right")
      m_buf << " text-anchor=\"end\"";
    else
      LogAnError(stderr,"WARNING - unknown text alignment %s while exporting SVG\n", align.c_str());
    }

  float xx = xCoord*mm2point;
  float yy = yCoord*mm2point;
  m_buf << " x=\"" << xx << "\" y=\"" << yy << "\"";

  if (rotation != 0.0)
    m_buf << " transform=\"rotate(" << rotation <<", " << xx << ", " << yy << ")\"";

  if (font != std::string())
    m_buf << " font-family=\"" << font << "\"";

  if (style != std::string()) {
    if (style == "Regular" || style == "Normal")
      m_buf << " font-weight=\"normal\"";
    else if (style == "Bold")
      m_buf << " font-weight=\"bold\"";
    else if (style == "Italic")
      m_buf << " font-style=\"italic\"";
    else if (style == "Oblique")
      m_buf << " font-style=\"oblique\"";
    else if (style == "Light")
      m_buf << " font-weight=\"lighter\"";
    else if (style == "Light Oblique")
      m_buf << " font-weight=\"lighter\"" << " font-style=\"oblique\"";
    else if (style == "Bold Oblique")
      m_buf << " font-weight=\"bold\"" << " font-style=\"oblique\"";
    else if (style == "Light Italic")
      m_buf << " font-weight=\"lighter\"" << " font-style=\"italic\"";
    else if (style == "Bold Italic")
      m_buf << " font-weight=\"bold\"" << " font-style=\"italic\"";
    else
      LogAnError(stderr,"WARNING - unknown text style %s while exporting SVG\n", style.c_str() );
    }

    m_buf << " font-size=\"" << size << "pt\"";

    m_buf << ">" << text << "</text>\n";
}

/******************************************************************************/
/******************************************************************************/
