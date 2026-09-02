//
//  MiniPDF.cpp
//
//  Writes a subset of PDF files
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
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include "vizShared.hpp"
#include "MiniPDF.hpp"
#include "IccCmdLineUtil.h"
#include "errorLog.hpp"


/******************************************************************************/

std::ostream& operator<<( std::ostream &os, const Rect2D &r )
{
  // llx lly urx ury
  return os << r.left << " " << r.bottom << " " << r.right << " " << r.top;
}

/******************************************************************************/

std::ostream& operator<<( std::ostream &os, const point2D &p )
{
  return os << p.x << " " << p.y;
}

/******************************************************************************/

std::ostream& operator<<( std::ostream &os, const RGB8Color &col )
{
  // r g b fractions
  return os << (col.r/255.0f) << " " << (col.g/255.0f) << " " << (col.b/255.0f);
}

/******************************************************************************/

// very approximate, not using real font metrics
float PDFEstimateStringWidth( const std::string &str, float textSizePts )
{
#if 0
  return 0.49f * textSizePts * str.size();
#else
  float first = str.size();
  float adjustment = 0.0f;
  
  // adjust glyphs that are far from the base size,
  // using very approximate metrics for Helvetica and similar sans-serif type
// TODO - extract real metrics, use a table
//    and why doesn't PDF do this automatically?
  for (const auto c : str ) {
    switch(c) {
      case '\'':
        adjustment -= 0.57;
        break;

      case 'i':
      case 'j':
      case 'l':
      case '|':
        adjustment -= 0.52;
        break;

      case '.':
      case ',':
      case 'I':
      case ':':
      case ';':
      case '!':
      case 't':
      case 'f':
      case '/':
      case '[':
      case ']':
      case ' ':
        adjustment -= 0.42;
        break;

      case 'r':
      case '`':
      case '-':
        adjustment -= 0.32;
        break;

      case '(':
      case ')':
      case '{':
      case '}':
        adjustment -= 0.30;
        break;

      case '"':
        adjustment -= 0.28;
        break;

      case '*':
        adjustment -= 0.2;
        break;

      case '^':
        adjustment -= 0.05;
        break;

      case '&':
        adjustment += 0.32;
        break;

      case '@':
        adjustment += 0.97;
        break;

      case '%':
        adjustment += 0.75;
        break;

      case '+':
        adjustment += 0.17;
        break;

      case '<':
      case '>':
      case '=':
      case '~':
        adjustment += 0.17;
        break;

      case 'a':
      case 'b':
      case 'd':
      case 'e':
      case 'g':
      case 'h':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'u':
        adjustment += 0.09;
        break;

      case 'm':
        adjustment += 0.64;
        break;

      case 'w':
        adjustment += 0.42;
        break;

      case 'J':
        adjustment += 0.0;
        break;

      case 'L':
        adjustment += 0.1;
        break;

      case 'F':
      case 'Z':
      case 'T':
        adjustment += 0.2;
        break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '#':
      case '$':
      case '_':
        adjustment += 0.1;
        break;

      case 'A':
      case 'B':
      case 'E':
      case 'K':
      case 'P':
      case 'S':
      case 'V':
      case 'X':
      case 'Y':
        adjustment += 0.30;
        break;

      case 'C':
      case 'D':
      case 'H':
      case 'N':
      case 'R':
      case 'U':
        adjustment += 0.4;
        break;

      case 'G':
      case 'O':
      case 'Q':
        adjustment += 0.52;
        break;

      case 'M':
        adjustment += 0.62;
        break;

      case 'W':
        adjustment += 0.82;
        break;

      default:
        break;
    }   // end switch on glyph/character
  
  } // end loop over characters in string
  
  float combined = 0.526f * textSizePts * (first + adjustment);
  return combined;
#endif
}

/******************************************************************************/
/******************************************************************************/

bool PDFWriter::OpenFile( const std::string &filename, float widthPt, float heightPt )
{
  if (!m_filename.empty()) {
    LogAnError(stderr,"WARNING - PDF file already open!\n");
  }

  m_filename = filename;
  m_pageWidth = widthPt;
  m_pageHeight = heightPt;

  m_objects.clear();
  m_xobjects.clear();
  m_pageCount = 0;
  m_xrefStart = 0;          // set when writing
  m_ok = true;

  // root = 1
  m_outlineParentIndex = 2;
  m_pageParentIndex = 3;

  // Create root object, always object 1
  if (!AddNewObject<PDFRoot>(m_pageParentIndex, m_outlineParentIndex))
    return false;

  // Create outline data from pages
  if (!AddNewObject<PDFOutlineParent>())
    return false;

  // Create page parent, link to add children out of order
  if (!AddNewObject<PDFPageParent>())
    return false;


  // common procset
  if (!AddNewObject<PDFProcSet>("[/PDF /Text]"))
    return false;
  size_t procSet = m_objects.size();
  m_procsetIndex = procSet;

  // common font definition
  if (!AddNewObject<PDFFont>("Helvetica"))
    return false;
  size_t font = m_objects.size();
  m_fontIndex = font;

  // common group object
  if (!AddNewObject<PDFGroup>())
    return false;
  size_t group = m_objects.size();
  m_groupIndex = group;

    // preliminaries are done

#if 0
// enable to debug text layout and other PDF utilities
  (void)PDFDebugPages( *this );
#endif

  return true;
}

/******************************************************************************/

bool PDFWriter::CloseFile()
{
  bool success = m_ok;

  if (!m_filename.empty()) {
    if (m_ok && PageCount() > 0) {
        try  {
          std::ostringstream out;
          out.exceptions(std::ios::badbit | std::ios::failbit);

          CreateTOCFromPages();     // this inserts pages, be careful!
          CreateOutlineFromPages();
          if (!m_ok) {
            LogAnError(stderr, "PDF writing error in '%s': unable to allocate PDF objects\n", m_filename.c_str());
            m_filename.clear();
            m_objects.clear();
            m_xobjects.clear();
            return false;
          }
          WriteHeader(out);
          WriteObjects(out);
          WriteXRefs(out);
          WriteFooter(out);

          // PDF export paths are intentional caller-selected output files after regular-file validation.

          // codeql[cpp/path-injection]
          FILE* outFile = icOpenRegularWriteTextFile(m_filename.c_str());
          // icWriteAndClose() writes through the handle icOpenRegularWriteTextFile()
          // just validated, then closes it (#2154). It replaced a byte-identical
          // private WritePdfTextFile() copy that lived here.
          if (!icWriteAndClose(outFile, out.str())) {
            LogAnError(stderr, "PDF writing error in '%s': unable to open regular output file\n", m_filename.c_str());
            success = false;
          }

        }
        catch (const std::exception& e) {
          LogAnError(stderr, "PDF writing error in '%s': '%s'\n", m_filename.c_str(), e.what() );
          success = false;
        }
    }
    else if (!m_ok) {
      LogAnError(stderr, "PDF writing error in '%s': unable to allocate PDF objects\n", m_filename.c_str());
      success = false;
    }
    m_filename.clear();
  }

  m_objects.clear();
  m_xobjects.clear();

  return success;
}

/******************************************************************************/

void PDFWriter::CreateOutlineFromPages()
{
  PDFPageParent *pageParent = GetPageParent();
  PDFOutlineParent *outParent = GetOutlineParent();
  if (!pageParent || !outParent)
    return;

  const size_t pageCount = pageParent->m_pageObjectIndices.size();
  for ( size_t k = 0; k < pageCount; ++k ) {
    size_t pageIndex = pageParent->m_pageObjectIndices[k];
    std::string &name = pageParent->m_pageNames[k];
    size_t currentIndex = ObjectCount() + 1;
    size_t prevIndex = 0;
    size_t nextIndex = 0;
    if (k > 0)
      prevIndex = currentIndex - 1;
    if (k < (pageCount-1))
      nextIndex = currentIndex + 1;
    std::string bookmarkName = std::to_string(k+1) + " " + name;    // may want to match for TOC
    if (!AddNewObject<PDFOutlineEntry>(m_outlineParentIndex, pageIndex, bookmarkName, prevIndex, nextIndex))
      return;
    outParent->AddOutlineObject( ObjectCount() );
  }

}

/******************************************************************************/

// currently used by TOC - no xobjects
size_t PDFWriter::AddPageHidden( size_t contentIndex, const std::vector<size_t> &annots )
{
    PDFPage *pageObj = AddNewObject<PDFPage>(m_pageWidth, m_pageHeight,
                    m_pageParentIndex, contentIndex, m_procsetIndex,
                    m_fontIndex, 0, "" );
    if (!pageObj)
      return 0;
    pageObj->AddAnnotationList( annots );
    return ObjectCount();
}

/******************************************************************************/

void PDFWriter::CreateTOCFromPages()
{
  const float margin = 20.0;
  const float bottom = 0.0f + margin;
  const float left = 0.0f + margin;
  const float top = PageHeight() - margin;
  const float right = PageWidth() - margin;
  const Rect2D bounds ( left, right, bottom, top );
  const float tocTextSize = 10.0f;
  const float tocTextLeading = 1.5f * tocTextSize;
  
  PDFPageParent *pageParent = GetPageParent();
  if (!pageParent)
    return;
  size_t pageCount = pageParent->m_pageObjectIndices.size();
  
  if (pageCount == 0)
    return;

  // how many pages those links will take up?
  float height = fabsf(bottom - top);
  float linesPerPage = floorf( height / tocTextLeading );
  size_t tocPageEstimate = (size_t) ceilf( pageCount / linesPerPage );
  
  std::vector<size_t> tocPages;
  std::vector<std::string> tocPageNames;
  std::vector<size_t> linkList;
  std::ostringstream commands;
  
  size_t lineCount = 0;
  size_t contentPageNumber = 1;
  for (size_t k = 0; k < pageCount; ++k ) {
    ++lineCount;
    std::string &name = pageParent->m_pageNames[k];
    size_t pageIndex = pageParent->m_pageObjectIndices[k];
    
    float lineStartY = top - lineCount*tocTextLeading;
    point2D pointLeft( left, lineStartY );
    size_t pageNumber = k + 1 + tocPageEstimate;
    std::string pageLabel = name + " .... " + std::to_string( pageNumber );
    commands << PDFSingleLineTextLabel( pointLeft, false, point2D(0,0), tocTextSize,
                            pageLabel, kPDFTextAlignLeft );

    float textWidth = PDFEstimateStringWidth( pageLabel, tocTextSize );
    Rect2D area( left, left+textWidth, lineStartY - tocTextLeading, lineStartY );
    if (!AddNewObject<PDFAnnotation>(area, pageIndex))
      return;
    
    linkList.push_back( ObjectCount() );

    if (lineCount >= linesPerPage || k == (pageCount-1) ) {
      std::string contentsName = "Table of Contents";
      if (contentPageNumber > 1)
        contentsName+= " " + std::to_string(contentPageNumber);
      tocPageNames.push_back(contentsName);
    
      point2D pointLabel( 0.5f*(left+right), top + margin );
      commands << PDFSingleLineTextLabel( pointLabel, false, point2D(0,0), tocTextSize,
                            contentsName, kPDFTextAlignLeft );
    
      if (!AddNewObject<PDFGraphic>(commands.str()))
        return;
      size_t tocPageContentIndex = ObjectCount();
      size_t tocPageIndex = AddPageHidden( tocPageContentIndex, linkList );
      tocPages.push_back( tocPageIndex );
      
      linkList.clear();
      commands.str("");
      commands.clear();
      lineCount = 0;
      ++contentPageNumber;
    }
  } // end loop over pages

  // add TOC pages to the FRONT of the page list
  pageParent->m_pageObjectIndices.insert( pageParent->m_pageObjectIndices.begin(),
                        tocPages.begin(), tocPages.end() );
  pageParent->m_pageNames.insert( pageParent->m_pageNames.begin(),
                        tocPageNames.begin(), tocPageNames.end() );
  m_pageCount += tocPages.size();

}

/******************************************************************************/
/******************************************************************************/

void PDFWriter::WriteHeader( std::ostream &out )
{
  out << "%PDF-1.7\n\n";
}

/******************************************************************************/

void PDFWriter::WriteObjects( std::ostream &out )
{
  size_t currentObj = 1;

  for ( auto &obj : m_objects ) {
    obj->m_offset = out.tellp();
    out << currentObj << " 0 obj\n";
    obj->WriteContent( out );
    out << "endobj\n\n";
    currentObj++;
  }
}

/******************************************************************************/

void PDFWriter::WriteXRefs( std::ostream &out ) {
  const size_t bufSize = 32;
  char buf[ bufSize ];

  m_xrefStart = out.tellp();

  out << "xref\n";
  out << "0 " << (m_objects.size()+1) << "\n";
  out << "0000000000 65535 f \n";  // first required fake entry

  for( auto &obj : m_objects ) {
    if (obj->m_offset == 0)
        LogAnError(stderr,"WARNING - PDF object referenced but not written yet\n");
    snprintf( buf, bufSize, "%10.10zu", obj->m_offset );    // must be precisely formatted
    out << buf << " 00000 n \n";  // 20 bytes each, exactly
  }
  out << "\n\n";
}

/******************************************************************************/

void PDFWriter::WriteFooter( std::ostream &out ) {
  out << "trailer\n";
  out << "<< /Size " << (m_objects.size()+1) << " /Root 1 0 R>>\n";
  out << "startxref\n";
  if (m_xrefStart == 0)
    LogAnError(stderr,"WARNING - PDF trailer written before xref directory\n");
  out << m_xrefStart;
  out << "\n%%EOF\n";
}

/******************************************************************************/
/******************************************************************************/

void PDFRoot::WriteContent( std::ostream &out )
{
  out << "<< /Type /Catalog /Pages "<< m_pageObj << " 0 R";
  if (m_outlineObj > 0)
    out << " /Outlines " << m_outlineObj << " 0 R";
  out << " >>\n";
}

/******************************************************************************/

void PDFPageParent::WriteContent( std::ostream &out )
{
  out << "<< /Type /Pages /Kids [";

  for ( auto &page : m_pageObjectIndices ) {
    out << " " << page << " 0 R";
  }

  out << "] /Count " << m_pageObjectIndices.size() << " >>\n";
}

/******************************************************************************/

void PDFOutlineParent::WriteContent( std::ostream &out )
{
  out << "<< /Type /Outlines /Count " << m_outlineObjectIndices.size();
  if (m_outlineObjectIndices.size() > 0) {
    out << " /First " << m_outlineObjectIndices[0] << " 0 R";
    out << " /Last " << m_outlineObjectIndices[ m_outlineObjectIndices.size()-1 ] << " 0 R";
  }
  out << " >>\n";
}

/******************************************************************************/

void PDFOutlineEntry::WriteContent( std::ostream &out )
{
  out << "<< /Title (" << m_name << ")";
  out << " /Parent " << m_outlineParentIndex << " 0 R";
  if (m_prevIndex != 0)
    out << " /Prev " << m_prevIndex << " 0 R";
  if (m_nextIndex != 0)
    out << " /Next " << m_nextIndex << " 0 R";
  out << " /Dest [" << m_pageIndex << " 0 R /Fit]";
  out << " >>\n";
}

/******************************************************************************/

void PDFPage::WriteContent( std::ostream &out )
{
  out << "<< /Type /Page /Parent " << m_pageParentIndex << " 0 R";
  out << " /MediaBox [0 0 " << m_pageWidth << " " << m_pageHeight << "]";
  out << " /Contents " << m_pageContentIndex << " 0 R";

// TODO - can't lookup here because we don't have the PDFWriter pointer
// should probably pass that in to all writers

  if (m_procset || m_font || m_xobjectIndex) {
    out << " /Resources <<";
    if (m_xobjectIndex) {  // could be abstracted to a list if needed
      out << " /XObject<<";
      out << "/" << m_xobjectName << " " << m_xobjectIndex << " 0 R";
      out << ">>";
    }
    if (m_procset)  // could also be a list if needed
        out << "/ProcSet " << m_procset << " 0 R";
    if (m_font)     // could also be a list if needed
        out << " /Font << /F1 " << m_font << " 0 R>>";
    out << " >> ";
  }
  if (m_annotations.size() > 0) {
    out << " /Annots [ ";
    for (auto &aindex: m_annotations) {
      out << std::to_string(aindex) << " 0 R\n";
    }
    out << "]\n";
  }
  out << ">>\n";
}

/******************************************************************************/

void PDFXObject::WriteContent( std::ostream &out )
{
  out << "<< /Subtype/Form /BBox[ " << m_bounds << "]";

  if (m_group)
    out << " /Group " << m_group << " 0 R";
  out << " /Length " << m_buf.size();
  if (m_procset || m_font) {
    out << " /Resources <<";
    if (m_procset)
        out << "/ProcSet " << m_procset << " 0 R";
    if (m_font)
        out << " /Font << /F1 " << m_font << " 0 R>>";
    out << " >> ";
  }
  out << ">>\n";

  out << "stream\n" << m_buf << "\nendstream\n";
}

/******************************************************************************/

void PDFProcSet::WriteContent( std::ostream &out )
{
  out << m_buf << "\n";
}

/******************************************************************************/

void PDFFont::WriteContent( std::ostream &out )
{
  out << "<< /Type /Font /Subtype /Type1 /Encoding /MacRomanEncoding ";
  out << "/Name /F1 /BaseFont /" << m_fontname << " >>\n";
}

/******************************************************************************/

void PDFGraphic::WriteContent( std::ostream &out )
{
  out << "<</Length " << m_buf.size() << ">>\nstream\n";
  out << m_buf;
  out << "\nendstream\n";
}

/******************************************************************************/

void PDFGroup::WriteContent( std::ostream &out )
{
    // I == isolated       K == knockout
  out << "<< /I true /K false /S /Transparency /Type/Group >>\n";
}

/******************************************************************************/

void PDFAnnotation::WriteContent(  std::ostream &out )
{
  out << "<< /Type/Annot /Subtype/Link\n";
  out << " /Rect[ " << m_area << "]";
  out << " /Border[ 0 0 0 ]\n";     // PDF 8.4.1 Annotation Dictionaries
  out << " /Dest[" << std::to_string(m_pageIndex) << " 0 R /Fit]\n";
  out << ">>\n";
}

/******************************************************************************/

void PDFWriter::AddXObject( const Rect2D &bounds, std::string &content, std::string name,
                size_t group, size_t font, size_t procSet )
{
  if (group == 0)
    group = m_groupIndex;
  if (font == 0)
    font = m_fontIndex;
  if (procSet == 0)
    procSet = m_procsetIndex;

  if (!AddNewObject<PDFXObject>(content, bounds, group, font, procSet))
    return;

  try {
    m_xobjects[ name ] = m_objects.size();
  }
  catch (const std::exception&) {
    m_ok = false;
  }
}

/******************************************************************************/

size_t PDFWriter::lookupXObjectByName( std::string name )
{
  if (name == std::string())
    return 0;   // don't waste time on lookup for empty strings
  auto lookup = m_xobjects.find(name);
  if (lookup == m_xobjects.end())
    return 0; // not found, indicates to use none
  else
    return lookup->second;
}

/******************************************************************************/

bool PDFWriter::xobjectExists( std::string name )
{
  if (name == std::string())
    return false;   // don't waste time on lookup for empty strings
  auto lookup = m_xobjects.find(name);
  return (lookup != m_xobjects.end());  // C++20 can use map.contains
}

/******************************************************************************/

void PDFWriter::AddPage( std::string name, size_t content, std::string xObjectName )
{
  size_t xindex = lookupXObjectByName( xObjectName );
  PDFPage *pageObj = AddNewObject<PDFPage>(m_pageWidth, m_pageHeight,
                    m_pageParentIndex, content, m_procsetIndex,
                    m_fontIndex, xindex, xObjectName );
  if (!pageObj)
    return;

  PDFPageParent *pageParent = GetPageParent();
  if (!pageParent)
    return;
  pageParent->AddPage( ObjectCount(), name );
  m_pageCount++;
}

/******************************************************************************/

std::string PDFSingleLineTextLabel( const point2D &basepoint, bool isVertical,
        const point2D &tickLength, float textSizePts,
        const std::string &text,
        PDFTextAlignment align )
{
  std::ostringstream commands;

  float textWidth = PDFEstimateStringWidth( text, textSizePts );

  point2D position(0,0);
  switch(align) {
    default:
    case kPDFTextAlignLeft:
        // do nothing
        break;

    case kPDFTextAlignCenter:
    case kPDFTextAlignCenterLeft:
        position = point2D(-0.5f * textWidth,0);
        break;

    case kPDFTextAlignRight:
        position = point2D(-textWidth,0);
        break;
  }

  point2D pt00 = basepoint;
  commands << "BT /F1 " << textSizePts << " Tf ";
  if (isVertical) {
    std::swap(position.x,position.y);
    pt00 += tickLength*1.25f + position;
    commands << "0 " << 1 << " " << -1 << " 0 " << pt00 << " Tm ";
  }
  else {
    pt00 += tickLength + position - point2D(0,textSizePts);
    commands << pt00 << " Td ";
  }
  commands << "(" << text << ") Tj ET\n";

  return commands.str();
}

/******************************************************************************/

// Center and right aligned are not perfect, because we are not using the font metrics and only estimating width
std::string PDFMultiLineText( const point2D &basepoint,
                    float textSizePts, float leading, float second_line_indent,
                    std::vector<std::string> &lines, PDFTextAlignment align,
                    bool allowBlankLines )
{
  std::ostringstream commands;
  
  commands << "BT /F1 " << textSizePts << " Tf ";
  
  float last_textWidth = 0.0f;
  float last_textHalf = 0.0f;
  for (size_t line_num = 0; line_num < lines.size(); ++line_num) {
    std::string label = lines[ line_num ];
    if (!allowBlankLines && label.size() == 0)  // double returns are not pretty
      continue;
    
    float textWidth = PDFEstimateStringWidth( label, textSizePts );
    float textHalf = 0.5f * textWidth;
    
    point2D offset(0,0);
    switch(align) {
      default:
      case kPDFTextAlignLeft:
        // do nothing
        break;

      case kPDFTextAlignCenter:
        offset = point2D(-textHalf+last_textHalf,0);
        break;

      case kPDFTextAlignRight:
        offset = point2D(-textWidth+last_textWidth,0);
        break;
    
      case kPDFTextAlignCenterLeft:
        if (line_num == 0)
          offset = point2D(-textHalf,0);
        break;
    }

    if (line_num == 0) {
      commands << (basepoint+offset) << " Td ";
    }
    else {
      point2D relative( second_line_indent, -leading );
      commands << " " << (relative+offset) << " Td ";
      second_line_indent = 0.0f;
    }
    commands << "(" << label << ") Tj\n";
    
    last_textHalf = textHalf;
    last_textWidth = textWidth;
  }
  
  commands << "ET\n";

  return commands.str();
}

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

// Center and right aligned are not perfect, because we are not using the font metrics and only estimating width
std::string PDFMultiLineText( const point2D &basepoint,
                    float textSizePts, float leading, float second_line_indent,
                    const std::string &text, PDFTextAlignment align,
                    bool allowBlankLines )
{
  std::vector<std::string> lines = splitTextLines( text );
  return PDFMultiLineText( basepoint,  textSizePts, leading,
                    second_line_indent, lines, align, allowBlankLines );
}

/******************************************************************************/

// break long lines of text into simple lines with a character limit
static
std::vector<std::string> splitTextParagraph(const std::string& str, size_t rowLimit )
{
  const char newline = '\n';
  std::vector<std::string> lines;
  size_t start = 0;
  size_t lineLength = 0;
  size_t last_space = 0;
  for ( size_t k = 0; k < str.size(); ++k ) {
    ++lineLength;
    char c = str[k];
    if (c == newline || lineLength > rowLimit) {
        if (c == newline)
            last_space = k;
        lines.push_back(str.substr(start, last_space - start));
        start = last_space + 1;
        lineLength = 0;
    }
    else if (c == ' ' || c == '\t')  // '\t' '\v' '\f' '\r' ' ' for isspace() (which is bloody slow!)
        last_space = k;
  }

  auto temp = str.substr(start);
  if (temp.size() > 0)
    lines.push_back(temp);
  return lines;
}

/******************************************************************************/

// Center and right aligned are not perfect, because we are not using the font metrics and only estimating width
// This breaks up lines of text into approximately the character size limit given
std::string PDFParagraphText( const point2D &basepoint,
                    float textSizePts, float leading, float second_line_indent,
                    size_t characterLineLimit,
                    const std::string &text, PDFTextAlignment align,
                    bool allowBlankLines )
{
  std::vector<std::string> lines = splitTextParagraph( text, characterLineLimit );
  
  return PDFMultiLineText( basepoint,  textSizePts, leading,
                    second_line_indent, lines, align, allowBlankLines );
}

/******************************************************************************/

std::string PDFDrawGridTable( gridTable &table, const point2D &basepoint,
                        float columnWidthMinimum, float columnWidthMaximum )
{
  std::ostringstream commands;
  float rowHeight = 1.2 * table.m_textSize + 2*table.m_cellMargin; // single line for now, paragraphs later
  float textBaseOffset = 0.2 * table.m_textSize;    // push up so descenders are still readable
  size_t totalRows = table.m_data.size();
  const RGB8Color white(0xfffffff);
  const RGB8Color black(0);

  if (totalRows == 0)
    return commands.str();

  // determine total number of columns
  size_t totalColumns = 0;
  for (const auto &row: table.m_data) {
    size_t columns = row.size();
    totalColumns = std::max( totalColumns, columns );
  }

  if (totalColumns == 0)
    return commands.str();

  // measure cells for text width
  // assuming single line text for now
  std::vector<float> colWidth(totalColumns,0.0f);
  for (size_t y = 0; y < totalRows; ++y) {
    auto &row = table.m_data[y];
    for (size_t x = 0; x < totalColumns && x < row.size(); ++x) {
        auto &cell = row[x];
        float cellWidth = PDFEstimateStringWidth( cell.m_text, table.m_textSize ) + 2*table.m_cellMargin;
        colWidth[x] = std::max( colWidth[x], cellWidth );
        cell.m_width = cellWidth;
    }
  }

  // enforce limits on colWidth
  if (columnWidthMinimum > columnWidthMaximum)
    std::swap( columnWidthMinimum, columnWidthMaximum );

  for (size_t x = 0; x < totalColumns ; ++x) {
    auto width = colWidth[x];
    width = std::max( width, columnWidthMinimum );
    width = std::min( width, columnWidthMaximum );
    colWidth[x] = width;
  }
  

  // sum columns to get offsets
  std::vector<float> offsetByColumn(totalColumns+1);
  offsetByColumn[0] = 0.0f;
  for (size_t k = 1; k <= totalColumns; ++k) {
    offsetByColumn[k] = colWidth[k-1] + offsetByColumn[k-1];
  }

  // draw cell backgrounds, if not white
  commands << "q\n";
  for (size_t y = 0; y < totalRows; ++y) {
    const auto &row = table.m_data[y];
    for (size_t x = 0; x < totalColumns && x < row.size(); ++x) {
      if ( row[x].m_backgroundColor == white)
        continue;
      point2D offset( offsetByColumn[x], -rowHeight*y );
      offset += basepoint;
      point2D sizeX( colWidth[x], 0.0f );
      point2D sizeY( 0.0f, -rowHeight );
      commands << row[x].m_backgroundColor << " rg ";
      commands << offset << " m " << offset+sizeX << " l ";
      commands << offset+sizeX+sizeY << " l " << offset+sizeY << " l h f\n";
    }
  }
  commands << "Q\n";
  
  
  // draw text
  commands << "q\n";
  for (size_t y = 0; y < totalRows; ++y) {
    const auto &row = table.m_data[y];
    for (size_t x = 0; x < totalColumns && x < row.size(); ++x) {
      if (row[x].m_text.size() == 0)
        continue;
      point2D offset( offsetByColumn[x] + table.m_cellMargin, -rowHeight*(y+1) + table.m_cellMargin + textBaseOffset );
      offset += basepoint;
      commands << row[x].m_textColor << " rg ";
      commands << "BT /F1 " << table.m_textSize << " Tf ";
      commands << offset << " Td ";
      commands << "(" << row[x].m_text << ") Tj ET\n";
    }
  }
  // grestore
  commands << "Q\n";
  
  
  // draw border lines above everything
  if (table.m_lineWeight > 0.0f) {
    commands << "q\n";
    commands << table.m_lineWeight << " w ";
    commands << table.m_lineColor << " RG\n";
    
    // horizontals
    for (size_t y = 0; y <= totalRows; ++y) {
      point2D start( basepoint.x - table.m_lineWeight/2, basepoint.y - rowHeight*y );
      point2D length( offsetByColumn[totalColumns] + table.m_lineWeight, 0 );
      commands << start << " m " << start+length << " l S\n";
    }

    // verticals
    for (size_t x = 0; x <= totalColumns; ++x) {
      point2D start( basepoint.x + offsetByColumn[x], basepoint.y );
      point2D length( 0, -rowHeight*totalRows - table.m_lineWeight/2 );
      commands << start << " m " << start+length << " l S\n";
    }
    commands << "Q\n";
  }

  
  return commands.str();
}

/******************************************************************************/
/******************************************************************************/

// randomly generated lorem ipsum text
static
std::string random_ipsum_lines(
"Ut at diam leo. Mauris libero nulla,\n"
"auctor sit amet venenatis Stiles,\n"
"lorem eget odio. Integer vitae dui\n"
"Fairchild magna dignissim ICC in\n"
"id diam.\n"
"\n"
"Sed quis imperdiet lorem, vel ipsum\n"
"diam. Nam varius Berns leo quis\n"
"pretium. Hunt fringilla a mi a\n"
"facilisis. Pellentesque Wyszecki\n"
"porttitor consequat." );

// same text, without linebreaks, and one forced linebreak
static
std::string random_ipsum_paragraph(
"Ut at diam leo. Mauris libero nulla auctor sit amet venenatis Stiles, lorem eget odio. Integer vitae dui Fairchild magna dignissim ICC in id diam.\n\nSed quis imperdiet lorem, vel ipsum diam. Nam varius Berns leo quis pretium. Hunt fringilla a mi a facilisis. Pellentesque Wyszecki porttitor consequat." );

// Debug PDF utilities and features
int PDFDebugPages( PDFWriter &pdffile )
{
  std::string glyphs1( "abcdefghijklmnopqrstuvwxyz1234567890|/;:'\",.<>/?");
  std::string glyphs2( "ABCDEFGHIJKLMNOPQRSTUVWXYZ-_`~!@#$%^&*\\\\()[]{}");

  std::string measure1( "xxxxxxxxxx\naaaaaaaaaa\nbbbbbbbbbb\ncccccccccc\n"
                        "dddddddddd\neeeeeeeeee\nffffffffff\ngggggggggg\n"
                        "hhhhhhhhhh\niiiiiiiiii\njjjjjjjjjj\nkkkkkkkkkk\n"
                        "llllllllll\nmmmmmmmmmm\nnnnnnnnnnn\noooooooooo\n"
                        "pppppppppp\nqqqqqqqqqq\nrrrrrrrrrr\nssssssssss\n"
                        "tttttttttt\nuuuuuuuuuu\nvvvvvvvvvv\nwwwwwwwwww\n"
                        "xxxxxxxxxx\nyyyyyyyyyy\nzzzzzzzzzz\n"
                        "1111111111\n2222222222\n3333333333\n4444444444\n"
                        "9999999999\n0000000000\nxxxxxxxxxx\n");

  std::string measure2( "xxxxxxxxxx\n::::::::::\n;;;;;;;;;;\n..........\n"
                        ",,,,,,,,,,\n``````````\n''''''''''\n\"\"\"\"\"\"\"\"\"\"\n"
                        "//////////\n<<<<<<<<<<\n>>>>>>>>>>\n()()()()()\n"
                        "[][][][][]\n----------\n__________\n++++++++++\n"
                        "==========\n%%%%%%%%%%\n~~~~~~~~~~\n{}{}{}{}{}\n"
                        "@@@@@@@@@@\n&&&&&&&&&&\n$$$$$$$$$$\n!!!!!!!!!!\n"
                        "**********\n^^^^^^^^^^\n##########\n"
                        "5555555555\n6666666666\n7777777777\n8888888888\n"
                        "x        x\nxxxxxxxxxx\n");

  std::string measure3( "xxxxxxxxxx\nAAAAAAAAAA\nBBBBBBBBBB\nCCCCCCCCCC\n"
                        "DDDDDDDDDD\nEEEEEEEEEE\nFFFFFFFFFF\nGGGGGGGGGG\n"
                        "HHHHHHHHHH\nIIIIIIIIII\nJJJJJJJJJJ\nKKKKKKKKKK\n"
                        "LLLLLLLLLL\nMMMMMMMMMM\nNNNNNNNNNN\nOOOOOOOOOO\n"
                        "PPPPPPPPPP\nQQQQQQQQQQ\nRRRRRRRRRR\nSSSSSSSSSS\n"
                        "TTTTTTTTTT\nUUUUUUUUUU\nVVVVVVVVVV\nWWWWWWWWWW\n"
                        "XXXXXXXXXX\nYYYYYYYYYY\nZZZZZZZZZZ\nxxxxxxxxxx\n");

// Single line/label text
#if 1
  {
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );

// Single Line text
  float textSizePts = 12.0f;
  
  point2D pointLeft( left, top - 1*inch2point );
  std::string labelLeft( "LeftAligned\n" );
  commands << PDFSingleLineTextLabel( pointLeft, false, point2D(0,0), textSizePts,
                            labelLeft, kPDFTextAlignLeft );

  point2D pointLeft2( left + textSizePts, top - 1*inch2point );
  commands << PDFSingleLineTextLabel( pointLeft2, true, point2D(0,0), textSizePts,
                            labelLeft, kPDFTextAlignLeft );


  point2D pointCenter( 0.5f*right, top - 3*inch2point );
  std::string labelCenter( "CenterAligned\n" );
  commands << PDFSingleLineTextLabel( pointCenter, false, point2D(0,0), textSizePts,
                            labelCenter, kPDFTextAlignCenter );

  commands << PDFSingleLineTextLabel( pointCenter, true, point2D(0,0), textSizePts,
                            labelCenter, kPDFTextAlignCenter );
  
  
  point2D pointRight( right, top - 5*inch2point );
  std::string labelRight( "RightAligned\n" );
  commands << PDFSingleLineTextLabel( pointRight, false, point2D(0,0), textSizePts,
                            labelRight, kPDFTextAlignRight );

  point2D pointRight2( right - textSizePts, top - 5*inch2point - 1.1*textSizePts );
  commands << PDFSingleLineTextLabel( pointRight2, true, point2D(0,0), textSizePts,
                            labelRight, kPDFTextAlignRight );

  float size2 = textSizePts * 1.5f;
  point2D pointRight3( right, bottom + 4*size2 );
  commands << PDFSingleLineTextLabel( pointRight3, false, point2D(0,0), size2,
                            glyphs1, kPDFTextAlignRight );
  point2D pointRight4( right, bottom + 2*size2 );
  commands << PDFSingleLineTextLabel( pointRight4, false, point2D(0,0), size2,
                            glyphs2, kPDFTextAlignRight );


  // and finally create the graphics object and page
  if (!pdffile.AddNewObject<PDFGraphic>(commands.str()))
    return 0;
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( "DEBUG Single Line Text", content, std::string() );
  }
#endif

// Multiline measurement of text
#if 1
  {
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );

  float textSizePts = 14.0f;
  float leading = textSizePts * 1.1f;
  
  point2D pointRight( right-2.0, top - leading );
  float rightIndent = 0.0;
  commands << PDFMultiLineText( pointRight, textSizePts, leading,
                rightIndent, measure1, kPDFTextAlignRight );
  
  point2D pointRight2( right-3*inch2point, top - leading );
  commands << PDFMultiLineText( pointRight2, textSizePts, leading,
                rightIndent, measure2, kPDFTextAlignRight );
  
  point2D pointRight3( right-6*inch2point, top - leading );
  commands << PDFMultiLineText( pointRight3, textSizePts, leading,
                rightIndent, measure3, kPDFTextAlignRight );
  

  // and finally create the graphics object and page
  if (!pdffile.AddNewObject<PDFGraphic>(commands.str()))
    return 0;
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( "DEBUG Character Measurement", content, std::string() );
  }
#endif

// Multiline text
#if 1
  {
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );

  float textSizePts = 10.0f;
  float leading = textSizePts * 1.1f;
  
  
  point2D pointLeft( left + 20.0f, top - 0.2f*inch2point );
  std::string labelLeft( "LeftAligned\n" );
  float leftIndent = -20.0;
  commands << PDFMultiLineText( pointLeft, textSizePts, leading,
                leftIndent, labelLeft+random_ipsum_lines, kPDFTextAlignLeft );

  point2D pointCenter( 0.5f*right, top - 2*inch2point );
  std::string labelCenter( "CenterAligned\n" );
  float centerIndent = 0.0;
  commands << PDFMultiLineText( pointCenter, textSizePts, leading,
                centerIndent, labelCenter+random_ipsum_lines, kPDFTextAlignCenter );
  
  point2D pointRight( right, top - 6*inch2point );
  std::string labelRight( "RightAligned\n" );
  float rightIndent = 0.0;
  commands << PDFMultiLineText( pointRight, textSizePts, leading,
                rightIndent, labelRight+random_ipsum_lines, kPDFTextAlignRight );
  
  point2D pointCenterLeft( 0.5f*right, top - 4*inch2point );
  std::string labelCenterLeft( "CenterLeftAligned\n" );
  float centerLeftIndent = 0.5f * inch2point;
  commands << PDFMultiLineText( pointCenterLeft, textSizePts, leading,
                centerLeftIndent, labelCenterLeft+random_ipsum_lines, kPDFTextAlignCenterLeft );

  // and finally create the graphics object and page
  if (!pdffile.AddNewObject<PDFGraphic>(commands.str()))
    return 0;
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( "DEBUG Multi Line Text", content, std::string() );
  }
#endif

// Paragraph text
#if 1
  {
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );

  float textSizePts = 10.0f;
  float leading = textSizePts * 1.1f;
  size_t lineLimit = 30;
  
  point2D pointLeft( left, top - 0.2f*inch2point );
  std::string labelLeft( "LeftAlignedParagraph\n" );
  float leftIndent = 0.0f;
  commands << PDFParagraphText( pointLeft, textSizePts, leading,
                leftIndent, lineLimit, labelLeft+random_ipsum_paragraph, kPDFTextAlignLeft );

  point2D pointCenter( 0.5f*right, top - 2*inch2point );
  std::string labelCenter( "CenterAlignedParagraph\n" );
  float centerIndent = 0.0;
  commands << PDFParagraphText( pointCenter, textSizePts, leading,
                centerIndent, lineLimit, labelCenter+random_ipsum_paragraph, kPDFTextAlignCenter );
  
  point2D pointRight( right, top - 6*inch2point );
  std::string labelRight( "RightAlignedParagraph\n" );
  float rightIndent = 0.0;
  commands << PDFParagraphText( pointRight, textSizePts, leading,
                rightIndent, lineLimit, labelRight+random_ipsum_paragraph, kPDFTextAlignRight );
  
  point2D pointCenterLeft( 0.5f*right, top - 4*inch2point );
  std::string labelCenterLeft( "CenterLeftAlignedParagraph\n" );
  float centerLeftIndent = 0.5f * inch2point;
  commands << PDFParagraphText( pointCenterLeft, textSizePts, leading,
                centerLeftIndent, lineLimit, labelCenterLeft+random_ipsum_paragraph, kPDFTextAlignCenterLeft );

  // and finally create the graphics object and page
  if (!pdffile.AddNewObject<PDFGraphic>(commands.str()))
    return 0;
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( "DEBUG Paragraph Text", content, std::string() );
  }
#endif

// Tables
#if 1
  std::ostringstream commands;

  float bottom = 0.0f;
  float left = 0.0f;
  float top = pdffile.PageHeight();
  float right = pdffile.PageWidth();
  Rect2D bounds ( left, right, bottom, top );
  const float margin = 10.0;

  float textSizePts = 10.0f;
  
  gridTable table1;
  table1.m_textSize = textSizePts;
  table1.m_lineWeight = 1.0f;
  table1.m_data.resize(11);
  for (auto &row: table1.m_data)
    row.resize(2);

  table1.m_data[0][0].m_text = "Akai";
  table1.m_data[0][1].m_textColor = 0xffff00;
  table1.m_data[0][1].m_text = "Red";
  table1.m_data[0][1].m_backgroundColor = 0xff0000;
  table1.m_data[1][0].m_text = "Midori";
  table1.m_data[1][1].m_text = "Green";
  table1.m_data[1][1].m_backgroundColor = 0x00FF00;
  table1.m_data[2][0].m_text = "Aoi";
  table1.m_data[2][1].m_text = "Blue";
  table1.m_data[2][1].m_backgroundColor = 0x0000ff;
  table1.m_data[3][0].m_text = "Shiroi";
  table1.m_data[3][1].m_text = "White";
  table1.m_data[3][1].m_backgroundColor = 0xffffff;
  table1.m_data[4][0].m_text = "Kuroi";
  table1.m_data[4][1].m_text = "Black";
  table1.m_data[4][1].m_textColor = 0xffffff;
  table1.m_data[4][1].m_backgroundColor = 0x000000;
  table1.m_data[5][0].m_text = "Shian";
  table1.m_data[5][1].m_text = "Cyan";
  table1.m_data[5][1].m_backgroundColor = 0x00ffff;
  table1.m_data[6][0].m_text = "Mazenta";
  table1.m_data[6][1].m_text = "Magenta";
  table1.m_data[6][1].m_backgroundColor = 0xff00ff;
  table1.m_data[7][0].m_text = "Kiiroi";
  table1.m_data[7][1].m_text = "Yellow";
  table1.m_data[7][1].m_backgroundColor = 0xffff00;
  table1.m_data[8][0].m_text = "";
  table1.m_data[8][1].m_text = "Blank";
  table1.m_data[9][0].m_text = "Blank";
  table1.m_data[9][1].m_text = "";
  table1.m_data[10][0].m_text = "";
  table1.m_data[10][1].m_text = "";
  
  point2D point1( left+margin, top - margin);
  commands << PDFDrawGridTable( table1, point1, 60.0 );
  
  point2D point2( left+margin + 200, top - margin);
  table1.m_lineWeight = 0.0f;
  table1.m_cellMargin = 8.0f;
  commands << PDFDrawGridTable( table1, point2 );

  point2D point3( left+margin + 400, top - margin);
  table1.m_lineWeight = 4.0f;
  table1.m_cellMargin = 9.0f;
  table1.m_lineColor = 0xff6020;
  commands << PDFDrawGridTable( table1, point3 );

  
  // diagonal
  gridTable table2;
  table2.m_textSize = textSizePts;
  table2.m_lineWeight = 0.2f;
  table2.m_lineColor = 0x004422;
  table2.m_cellMargin = 2.0f;
  table2.m_data.resize(6);
  table2.m_data[0].resize(1);
  table2.m_data[1].resize(2);
  table2.m_data[2].resize(3);
  table2.m_data[3].resize(4);
  table2.m_data[4].resize(5);
  table2.m_data[5].resize(6);

  table2.m_data[0][0].m_text = "1";
  table2.m_data[1][1].m_text = "2";
  table2.m_data[2][2].m_text = "3";
  table2.m_data[3][3].m_text = "4";
  table2.m_data[4][4].m_text = "5";
  table2.m_data[5][5].m_text = "6";
  
  point2D point4( left+margin, 300 );
  commands << PDFDrawGridTable( table2, point4 );


  // sparse with only single entry
  gridTable table3;
  table3.m_textSize = textSizePts;
  table3.m_lineWeight = 1.0f;
  table3.m_lineColor = 0x002244;
  table3.m_cellMargin = 2.0f;
  table3.m_data.resize(6);
  table3.m_data[5].resize(6);
  table3.m_data[5][5].m_text = "X";
  
  point2D point5( left+margin + 90, 300 );
  commands << PDFDrawGridTable( table3, point5 );
  
  
    
  // and finally create the graphics object and page
  if (!pdffile.AddNewObject<PDFGraphic>(commands.str()))
    return 0;
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( "DEBUG Tables", content, std::string() );
#endif

return 1;

}

/******************************************************************************/
/******************************************************************************/
