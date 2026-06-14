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
#include "MiniPDF.hpp"
#include "../IccCmdLineUtil.h"
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

static bool WritePdfTextFile(FILE* outFile, const std::string& text)
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
/******************************************************************************/

void PDFWriter::OpenFile( const std::string &filename, float widthPt, float heightPt )
{
  if (!m_filename.empty()) {
    LogAnError(stderr,"WARNING - PDF file already open!\n");
  }

  m_filename = filename;
  m_pageWidth = widthPt;
  m_pageHeight = heightPt;

  m_objects.clear();
  m_xrefStart = 0;          // set when writing

  // root = 1
  m_outlineIndex = 2;
  m_pageParentIndex = 3;

  // Create root object, always object 1
  PDFRoot *rootObj = new PDFRoot(m_pageParentIndex,m_outlineIndex);
  m_objects.push_back( rootObj );

  // Create outline data from pages
// DEFERRED - group of tag, subsections for each LUT ???
// or just section for tag start?
// needs a bit of additional structure input
  PDFOutlineParent *outlineObj = new PDFOutlineParent();
  m_objects.push_back( outlineObj );

  // Create page parent, link to add children out of order
  PDFPageParent *pageParentObj = new PDFPageParent();
  m_objects.push_back( pageParentObj );


  // common procset
  PDFProcSet *procObj = new PDFProcSet( "[/PDF /Text]" );
  m_objects.push_back( procObj );
  size_t procSet = m_objects.size();
  m_procsetIndex = procSet;

  // common font definition
  PDFFont *fontObj = new PDFFont( "Helvetica" );
  m_objects.push_back( fontObj );
  size_t font = m_objects.size();
  m_fontIndex = font;

  // common group object
  PDFGroup *groupObj = new PDFGroup();
  m_objects.push_back( groupObj );
  size_t group = m_objects.size();
  m_groupIndex = group;

    // preliminaries are done

#if 0
// enable to debug text layout and other PDF utilities
  (void)PDFDebugPages( *this );
#endif

}

/******************************************************************************/

void PDFWriter::CloseFile()
{
  if (!m_filename.empty()) {
    if (PageCount() > 0) {
        try  {
          std::ostringstream out;
          out.exceptions(std::ios::badbit | std::ios::failbit);
          WriteHeader(out);
          WriteObjects(out);
          WriteXRefs(out);
          WriteFooter(out);

          // PDF export paths are intentional caller-selected output files after regular-file validation.

          // codeql[cpp/path-injection]
          FILE* outFile = icOpenRegularWriteTextFile(m_filename.c_str());
          if (!WritePdfTextFile(outFile, out.str())) {
            LogAnError(stderr, "PDF writing error in '%s': unable to open regular output file\n", m_filename.c_str());
            m_filename.clear();
            return;
          }

        }
        catch (const std::exception& e) {
          LogAnError(stderr, "PDF writing error in '%s': '%s'\n", m_filename.c_str(), e.what() );
        }
        catch (...) {
          LogAnError(stderr, "PDF writing error in '%s': unknown exception\n", m_filename.c_str());
        }
    }
    m_filename.clear();
  }

  // always cleanup all of the allocated objects
  for (auto &obj: m_objects ) {
    delete obj;
    obj = NULL;
  }
  m_objects.clear();

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
// DEFERRED - write this later, if needed
  out << "<< /Type /Outlines /Count 0 >>\n";
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

void PDFWriter::AddXObject( const Rect2D &bounds, std::string &content, std::string name,
                size_t group, size_t font, size_t procSet )
{
  if (group == 0)
    group = m_groupIndex;
  if (font == 0)
    font = m_fontIndex;
  if (procSet == 0)
    procSet = m_procsetIndex;

  PDFXObject *xobjObj = new PDFXObject( content, bounds, group, font, procSet );
  m_objects.emplace_back( xobjObj );

  m_xobjects[ name ] = m_objects.size();
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

void PDFWriter::AddPage( size_t content, std::string xObjectName )
{
  size_t xindex = lookupXObjectByName( xObjectName );
  PDFPage *pageObj = new PDFPage( m_pageWidth, m_pageHeight,
                    m_pageParentIndex, content, m_procsetIndex,
                    m_fontIndex, xindex, xObjectName );
  m_objects.push_back( pageObj );
  GetPageParent()->m_pageObjectIndices.push_back( ObjectCount() );
  m_pageCount++;
}


/******************************************************************************/

std::string PDFSingleLineTextLabel( const point2D &basepoint, bool isVertical,
        const point2D &tickLength, float textSizePts,
        const std::string &text,
        PDFTextAlignment align )
{
  std::ostringstream commands;

  float textWidth = textSizePts * 0.49f * text.size(); // very approximate, not using font metrics
  float textHalf = 0.5f * textWidth;

  point2D position(0,0);
  switch(align) {
    default:
    case kPDFTextAlignLeft:
        // do nothing
        break;

    case kPDFTextAlignCenter:
    case kPDFTextAlignCenterLeft:
        position = point2D(-textHalf,0);
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
    
    float textWidth = textSizePts * 0.49f * label.size();
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
std::string random_ipsum_paragraph(
"Ut at diam leo. Mauris libero nulla auctor sit amet venenatis Stiles, lorem eget odio. Integer vitae dui Fairchild magna dignissim ICC in id diam.\n\nSed quis imperdiet lorem, vel ipsum diam. Nam varius Berns leo quis pretium. Hunt fringilla a mi a facilisis. Pellentesque Wyszecki porttitor consequat." );

// Debug PDF utilities and features
int PDFDebugPages( PDFWriter &pdffile )
{

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

  point2D pointCenter2( 0.5f*right, top - 3*inch2point );
  commands << PDFSingleLineTextLabel( pointCenter2, true, point2D(0,0), textSizePts,
                            labelCenter, kPDFTextAlignCenter );
  
  
  point2D pointRight( right, top - 5*inch2point );
  std::string labelRight( "RightAligned\n" );
  commands << PDFSingleLineTextLabel( pointRight, false, point2D(0,0), textSizePts,
                            labelRight, kPDFTextAlignRight );

  point2D pointRight2( right - textSizePts, top - 5*inch2point );
  commands << PDFSingleLineTextLabel( pointRight2, true, point2D(0,0), textSizePts,
                            labelRight, kPDFTextAlignRight );


  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( content, std::string() );
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
  
  point2D pointRight( right, top - 4*inch2point );
  std::string labelRight( "RightAligned\n" );
  float rightIndent = 0.0;
  commands << PDFMultiLineText( pointRight, textSizePts, leading,
                rightIndent, labelRight+random_ipsum_lines, kPDFTextAlignRight );
  
  point2D pointCenterLeft( 0.5f*right, top - 6*inch2point );
  std::string labelCenterLeft( "CenterLeftAligned\n" );
  float centerLeftIndent = 0.5f * inch2point;
  commands << PDFMultiLineText( pointCenterLeft, textSizePts, leading,
                centerLeftIndent, labelCenterLeft+random_ipsum_lines, kPDFTextAlignCenterLeft );

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( content, std::string() );
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
  
  point2D pointRight( right, top - 4*inch2point );
  std::string labelRight( "RightAlignedParagraph\n" );
  float rightIndent = 0.0;
  commands << PDFParagraphText( pointRight, textSizePts, leading,
                rightIndent, lineLimit, labelRight+random_ipsum_paragraph, kPDFTextAlignRight );
  
  point2D pointCenterLeft( 0.5f*right, top - 6*inch2point );
  std::string labelCenterLeft( "CenterLeftAlignedParagraph\n" );
  float centerLeftIndent = 0.5f * inch2point;
  commands << PDFParagraphText( pointCenterLeft, textSizePts, leading,
                centerLeftIndent, lineLimit, labelCenterLeft+random_ipsum_paragraph, kPDFTextAlignCenterLeft );

  // and finally create the graphics object and page
  PDFGraphic *graphics = new PDFGraphic( commands.str() );
  pdffile.AddObject( graphics );
  size_t content = pdffile.ObjectCount();
  pdffile.AddPage( content, std::string() );
  }
#endif

return 1;

}

/******************************************************************************/
/******************************************************************************/
