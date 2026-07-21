//
//  MiniPDF.hpp
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

#ifndef MiniPDF_hpp
#define MiniPDF_hpp

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include "vizShared.hpp"

/******************************************************************************/

std::ostream& operator<<( std::ostream &os, const Rect2D &r );
std::ostream& operator<<( std::ostream &os, const point2D &r );

/******************************************************************************/

struct RGB8Color {
  RGB8Color() : r(0), g(0), b(0) {}
  
  RGB8Color( uint8_t rr, uint8_t gg, uint8_t bb ) :
    r(rr), g(gg), b(bb) {}

  RGB8Color( uint32_t hexRGB ) {
        r = (uint8_t)((hexRGB >> 16) & 0xFF);
        g = (uint8_t)((hexRGB >>  8) & 0xFF);
        b = (uint8_t)((hexRGB >>  0) & 0xFF);
  }
  
  bool constexpr operator==( const RGB8Color &y ) const {
    return (r==y.r) && (g==y.g) && (b==y.b);
  }
  
  bool constexpr operator!=( const RGB8Color &y ) const {
    return !(*this == y);
  }

public:
  uint8_t r, g, b;
};

std::ostream& operator<<( std::ostream &os, const RGB8Color &col );

/******************************************************************************/

// parent class
class PDFObject
{
public:
  PDFObject() : m_offset(0) {}
  virtual ~PDFObject() {}

  virtual void WriteContent( std::ostream &out ) = 0;

public:
  size_t m_offset;
};

typedef std::vector<PDFObject *> pdf_object_list;

/******************************************************************************/

// root object
class PDFRoot : public PDFObject
{
public:
  PDFRoot( size_t pageObj, size_t outlineObj ) :
        PDFObject(), m_pageObj(pageObj), m_outlineObj(outlineObj) {}

  virtual void WriteContent( std::ostream &out ) final;

public:
  size_t m_pageObj;
  size_t m_outlineObj;
};

/******************************************************************************/

// pages parent
class PDFPageParent : public PDFObject
{
public:
  PDFPageParent() : PDFObject() {}

  virtual void WriteContent( std::ostream &out ) final;
  
  void AddPage( size_t pageIndex, const std::string &pageName )
    {
    m_pageObjectIndices.push_back( pageIndex );
    m_pageNames.push_back( pageName );
    }

public:
    std::vector<size_t> m_pageObjectIndices;
    std::vector<std::string> m_pageNames;
};

/******************************************************************************/

// a page object, with references to all sub-objects
class PDFPage: public PDFObject
{
public:
  PDFPage( float widthPt, float heightPt, size_t parent, size_t content, size_t procSet = 0, size_t font = 0, size_t xobject = 0, std::string xobjectName = std::string() ) :
    PDFObject(), m_pageWidth(widthPt), m_pageHeight(heightPt),
    m_pageParentIndex(parent), m_pageContentIndex(content),
    m_procset(procSet), m_font(font), m_xobjectIndex(xobject),
    m_xobjectName(xobjectName)
     {}

  virtual void WriteContent( std::ostream &out ) final;

  void AddAnnotation( size_t index ) {
    m_annotations.push_back(index);
  }

  void AddAnnotationList( const std::vector<size_t> &annots ) {
    m_annotations.insert( m_annotations.end(), annots.begin(), annots.end() );
  }

public:
  float m_pageWidth;
  float m_pageHeight;
  size_t m_pageParentIndex;     // could avoid this if Write passed in PDFWriter reference
  size_t m_pageContentIndex;
  size_t m_procset;
  size_t m_font;
  size_t m_xobjectIndex;
  size_t m_pageObjectIndex;
  std::vector<size_t> m_annotations;
  std::string m_xobjectName;
};

/******************************************************************************/

// outline parent
class PDFOutlineParent : public PDFObject
{
public:
  PDFOutlineParent() : PDFObject() {}

  virtual void WriteContent( std::ostream &out ) final;
  
  void AddOutlineObject( size_t index )
    {
    m_outlineObjectIndices.push_back( index );
    }

public:
    std::vector<size_t> m_outlineObjectIndices;    // should match up with pageParent list
};

/******************************************************************************/

// an outline entry that references a single page
class PDFOutlineEntry: public PDFObject
{
public:
  PDFOutlineEntry( size_t parentIndex, size_t pageIndex, const std::string &title,
                    size_t prev, size_t next) :
    m_outlineParentIndex(parentIndex), m_pageIndex(pageIndex),
      m_prevIndex(prev), m_nextIndex(next), m_name(title)
     {}

  virtual void WriteContent( std::ostream &out ) final;

public:
  size_t m_outlineParentIndex;
  size_t m_pageIndex;
  size_t m_prevIndex;
  size_t m_nextIndex;
  std::string m_name;
};

/******************************************************************************/

// TODO - should this be more abstract as a simple string container class?
class PDFProcSet : public PDFObject
{
public:
  PDFProcSet( const std::string &proc ) : PDFObject(), m_buf(proc) {}

  virtual void WriteContent( std::ostream &out ) final;

public:
  std::string m_buf;
};

/******************************************************************************/

class PDFGraphic : public PDFObject
{
public:
  PDFGraphic( const std::string &content ) : PDFObject(), m_buf(content) {}

  virtual void WriteContent( std::ostream &out ) final;

public:
  std::string m_buf;
};

/******************************************************************************/

class PDFAnnotation : public PDFObject
{
public:
  PDFAnnotation( Rect2D &area, size_t index) : PDFObject(),
    m_area(area), m_pageIndex(index)
    {}

  virtual void WriteContent( std::ostream &out ) final;

public:
  Rect2D m_area;
  size_t m_pageIndex;
};

/******************************************************************************/

class PDFFont : public PDFObject
{
public:
  PDFFont( const std::string &font ) : PDFObject(), m_fontname(font) {}

  virtual void WriteContent( std::ostream &out ) final;

public:
  std::string m_fontname;
};

/******************************************************************************/

class PDFGroup : public PDFObject
{
public:
  PDFGroup() : PDFObject() {}

  virtual void WriteContent( std::ostream &out ) final;

public:
    // nothing yet
};

/******************************************************************************/

class PDFXObject : public PDFObject
{
public:
  PDFXObject( const std::string &buf, const Rect2D &bounds,
            size_t group = 0, size_t font = 0, size_t procSet = 0 ) :
    PDFObject(), m_buf(buf), m_bounds(bounds), m_procset(procSet),
    m_font(font), m_group(group)
        {}

  virtual void WriteContent( std::ostream &out ) final;

public:
  std::string m_buf;
  Rect2D m_bounds;
  size_t m_procset;
  size_t m_font;
  size_t m_group;
};

/******************************************************************************/

typedef std::map<std::string,size_t>    object_name_to_index_map;

// units are in points, as is common for PDF
class PDFWriter
{
public:
  PDFWriter() : m_pageWidth(0), m_pageHeight(0), m_pageCount(0),
            m_xrefStart(0), m_pageParentIndex(0), m_outlineParentIndex(0),
            m_fontIndex(0), m_groupIndex(0), m_procsetIndex(0)
    { }
  PDFWriter( const std::string &filename, float widthPt, float heightPt ):
            m_pageWidth(0), m_pageHeight(0), m_pageCount(0), m_xrefStart(0),
            m_pageParentIndex(0), m_outlineParentIndex(0), m_fontIndex(0),
            m_groupIndex(0), m_procsetIndex(0)
    { OpenFile(filename, widthPt, heightPt); }

  ~PDFWriter()
    { CloseFile(); }

  void OpenFile( const std::string &filename, float pageWidth, float pageHeight );
  void CloseFile();

public:

  void AddXObject( const Rect2D &bounds, std::string &content, std::string name,
                size_t group = 0, size_t font = 0, size_t procSet = 0 );

  size_t PageCount() const { return m_pageCount; }
  float PageWidth() const { return m_pageWidth; }
  float PageHeight() const { return m_pageHeight; }
  size_t ObjectCount() const { return m_objects.size(); }
  bool xobjectExists( std::string name );

  void AddObject( PDFObject *obj ) {
    m_objects.push_back( obj );
  }

  void AddPage( std::string name, size_t content, std::string xObjectName );

protected:

  void WriteHeader( std::ostream &out );
  void WriteObjects( std::ostream &out );
  void WriteXRefs( std::ostream &out );
  void WriteFooter( std::ostream &out );

  size_t lookupXObjectByName( std::string name );

  PDFPageParent *GetPageParent() {
    if (!m_pageParentIndex) {
      fprintf(stderr,"FATAL - PDF page parent index not set!\n");
      return NULL;
    }
    PDFObject *parentObj( m_objects[m_pageParentIndex-1] );
    PDFPageParent *pageParent = dynamic_cast<PDFPageParent *>(parentObj);
    return pageParent;
  }

  PDFOutlineParent *GetOutlineParent() {
    if (!m_outlineParentIndex) {
      fprintf(stderr,"FATAL - PDF outline parent index not set!\n");
      return NULL;
    }
    PDFObject *parentObj( m_objects[m_outlineParentIndex-1] );
    PDFOutlineParent *outParent = dynamic_cast<PDFOutlineParent *>(parentObj);
    return outParent;
  }

private:
  
  void CreateTOCFromPages();
  void CreateOutlineFromPages();
  size_t AddPageHidden( size_t contentIndex, const std::vector<size_t> &annots );

private:
  float m_pageWidth;     // used to init pages
  float m_pageHeight;    // used to init pages

  size_t m_pageCount;
  size_t m_xrefStart;
  size_t m_pageParentIndex;
  size_t m_outlineParentIndex;

  object_name_to_index_map m_xobjects;
  size_t m_fontIndex;       // used to init pages   // TODO - make this a map from name to object index
  size_t m_groupIndex;      // used to init pages   // this may not change too quickly
  size_t m_procsetIndex;    // used to init pages   // this may not change too quickly

  std::string m_filename;
  pdf_object_list m_objects;
};

/******************************************************************************/

struct tableEntry {
  tableEntry() : m_backgroundColor(0xffffff), m_textColor(0) {}

public:
  std::string m_text;
  RGB8Color m_backgroundColor;
  RGB8Color m_textColor;
    // FUTURE - alignment

  float m_width;        // used by layout code
};

/******************************************************************************/

// row outer, column inner
typedef std::vector< tableEntry > tableRowData;
typedef std::vector< tableRowData > tableData;

/******************************************************************************/

class gridTable {

public:
  gridTable() : m_lineWeight(0.0f), m_textSize(10.0f), m_cellMargin(2.0f) {}
  
  void AddRow( const tableRowData &one_row ) { m_data.push_back( one_row ); }

public:
    float m_lineWeight;
    float m_textSize;
    float m_cellMargin;
    RGB8Color m_lineColor;
    tableData m_data;
};

/******************************************************************************/

// utility functions to create graphics

enum PDFTextAlignment {
    kPDFTextAlignLeft = 0,
    kPDFTextAlignCenter = 1,
    kPDFTextAlignRight = 2,
    kPDFTextAlignCenterLeft = 3,  // for graph top label, center then left with indent
};

std::string PDFSingleLineTextLabel( const point2D &basepoint, bool isVertical,
                    const point2D &offset, float labelSize,
                    const std::string &text,
                    PDFTextAlignment align = kPDFTextAlignCenter );

// this will break the text string into lines
std::string PDFMultiLineText( const point2D &basepoint,
                    float labelSize, float leading, float second_line_indent,
                    const std::string &text,
                    PDFTextAlignment align,
                    bool allowBlankLines = true );

// this takes premade lines
std::string PDFMultiLineText( const point2D &basepoint,
                    float textSizePts, float leading, float second_line_indent,
                    std::vector<std::string> &lines, PDFTextAlignment align  = kPDFTextAlignLeft,
                    bool allowBlankLines = true );

// take a long string and break it up into lines based on the limit
std::string PDFParagraphText( const point2D &basepoint,
                    float textSizePts, float leading, float second_line_indent,
                    size_t characterLineLimit,
                    const std::string &text, PDFTextAlignment align = kPDFTextAlignLeft,
                    bool allowBlankLines = true );

// draw a table/grid/spreadsheet
std::string PDFDrawGridTable( gridTable &table, const point2D &basepoint,
                float columnWidthMinimum = 10.0, float columnWidthMaximum = 9e99 );

/******************************************************************************/

// Debug PDF utilities and features
int PDFDebugPages( PDFWriter &pdffile );

/******************************************************************************/

#endif /* MiniPDF_hpp */
