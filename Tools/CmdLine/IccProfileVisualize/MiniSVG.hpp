//
//  MiniSVG.hpp
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
#ifndef MiniSVG_hpp
#define MiniSVG_hpp

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "MiniPDF.hpp"

/******************************************************************************/

// all input units are in mm, but SVG works mostly in points
class SVGOut
{
public:
  SVGOut() : m_GroupLevel(0), m_pageCount(0), m_pageInProgress(false),
               m_pageWidth(0), m_pageHeight(0)
    { }
  SVGOut( const std::string &filename ): m_GroupLevel(0),
            m_pageCount(0), m_pageInProgress(false),
            m_pageWidth(0), m_pageHeight(0)
    { OpenFile(filename); }

  ~SVGOut()
    { CloseFile(); }

  void OpenFile( const std::string &filename );
  void CloseFile();

public:

  void AddCircle( float radius, float xCenter, float yCenter, bool isFilled ) {
    m_buf << "<circle cx=\"" << xCenter << "mm\" cy=\"" << yCenter
          << "mm\" r=\"" << radius << "mm\"";
    m_buf << " " << DefaultFillStroke(isFilled) << " />\n";
    }

  void AddLine( float xStart, float yStart, float xEnd, float yEnd ) {
    m_buf << "<line x1=\"" << xStart << "mm\" y1=\"" << yStart << "mm\" x2=\""
          << xEnd << "mm\" y2=\"" << yEnd << "mm\"";
    m_buf << " " << DefaultFillStroke(false) << " />\n";
    }

  void AddLine( const point2D &start, const point2D &end ) {
    AddLine( start.x, start.y, end.x, end.y );
    }

  void AddRect( float left, float top, float right, float bottom, bool isFilled ) {
    m_buf << "<rect x=\"" << left << "mm\" y=\"" << top << "mm\" width=\""
          << (right-left) << "mm\" height=\"" << (bottom-top) << "mm\"";
    m_buf << " " << DefaultFillStroke(isFilled) << " />\n";
    }

  void AddRect( const point2D &topLeft, const point2D &bottomRight, bool isFilled ) {
    AddRect( topLeft.x, topLeft.y, bottomRight.x, bottomRight.y, isFilled );
    }

  void AddPolyLine( const pointList &points, bool isClosed, bool isFilled ) {
    m_buf << "<polyline points=\"";
    for ( auto &item : points)
      m_buf << mm2point*item.x << "," << mm2point*item.y << " ";
    if (isClosed)
      m_buf << mm2point*points[0].x << "," << mm2point*points[0].y << " ";
    m_buf << "\"";
    m_buf << " " << DefaultFillStroke(isFilled) << " />\n";
    }

  void AddText( const float xCoord, const float yCoord, const std::string &text,
        const float size, const std::string &font, const std::string &style,
        const std::string &align, const float rotation = 0.0 );

  void StartGroup( const std::string &name ) {
    m_buf << "<g id=\"" << name << "\">\n";
    m_GroupLevel++;
    }

  void EndGroup() {
    m_buf << "</g>\n";
    m_GroupLevel--;
    }

    // Set the page size in mm. Call once before adding content.
    void SetPageSize( float widthMM, float heightMM ) {
      m_pageWidth = widthMM * mm2point;
      m_pageHeight = heightMM * mm2point;
    }

    // Start a new page. Each graph gets its own page, offset vertically.
    void NextPage() {
      if (m_pageInProgress) {
        m_buf << "</g>\n";
      }
      float yOffset = m_pageCount * m_pageHeight;
      m_buf << "<g transform=\"translate(0," << yOffset << ")\">\n";
      m_pageCount++;
      m_pageInProgress = true;
    }

    int PageCount() const { return m_pageCount; }

protected:

  void WriteHeader( std::ostream &out );

  void WriteFooter( std::ostream &out ) {
    out << "</svg>\n";
    }

  // this could be made variable, but just static color and width for now
  std::string DefaultFillStroke( bool isFilled ) {
    if (isFilled)
      return std::string("fill=\"black\" stroke=\"none\"");
    else
      return std::string("fill=\"none\" stroke=\"black\" stroke-width=\"0.5\"");  // in points
    }

private:
  int64_t m_GroupLevel;
  int m_pageCount;
  bool m_pageInProgress;
  float m_pageWidth;
  float m_pageHeight;
  std::string m_filename;
  std::ostringstream m_buf;
};

/******************************************************************************/

#endif /* MiniSVG_hpp */
