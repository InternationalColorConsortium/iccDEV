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
#include "MiniSVG.hpp"


/******************************************************************************/

struct Rect2D {
  Rect2D(float leftin, float rightin, float bottomin, float topin) :
    left(leftin), right(rightin), bottom(bottomin), top(topin)
    {}

  Rect2D() : left(0.0f), right(0.0f), bottom(0.0f), top(0.0f)
    {}

  Rect2D(const point2D &ll, const point2D &tr) :
    left(ll.x), right(tr.x), bottom(ll.y), top(tr.y)
    {}

  point2D Size() const  { return point2D( right-left, bottom-top ); }

  float left, right, bottom, top;
};

std::ostream& operator<<( std::ostream &os, const Rect2D &r );
std::ostream& operator<<( std::ostream &os, const point2D &r );

/******************************************************************************/

// parent class
class PDFObject
{
public:
  PDFObject() : m_offset(0) {}
  virtual ~PDFObject() {}

  virtual void WriteContent(  std::ostream &out ) = 0;

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

  virtual void WriteContent(  std::ostream &out ) final;

public:
  size_t m_pageObj;
  size_t m_outlineObj;  // not used yet
};

/******************************************************************************/

// pages parent
class PDFPageParent : public PDFObject
{
public:
  PDFPageParent() : PDFObject() {}

  virtual void WriteContent(  std::ostream &out ) final;

public:
    std::vector<size_t> m_pageObjectIndices;
};

/******************************************************************************/

// outline parent (currently not implemented)
class PDFOutlineParent : public PDFObject
{
public:
  PDFOutlineParent() : PDFObject() {}

  virtual void WriteContent(  std::ostream &out ) final;

public:
    // nothing yet
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

  virtual void WriteContent(  std::ostream &out ) final;

public:
  float m_pageWidth;
  float m_pageHeight;
  size_t m_pageParentIndex;     // could avoid this if Write passed in PDFWriter reference
  size_t m_pageContentIndex;
  size_t m_procset;
  size_t m_font;
  size_t m_xobjectIndex;
  std::string m_xobjectName;
};

/******************************************************************************/

// TODO - should this be more abstract as a simple string container class?
class PDFProcSet : public PDFObject
{
public:
  PDFProcSet( const std::string &proc ) : PDFObject(), m_buf(proc) {}

  virtual void WriteContent(  std::ostream &out ) final;

public:
  std::string m_buf;
};

/******************************************************************************/

class PDFGraphic : public PDFObject
{
public:
  PDFGraphic( const std::string &content ) : PDFObject(), m_buf(content) {}

  virtual void WriteContent(  std::ostream &out ) final;

public:
  std::string m_buf;
};

/******************************************************************************/

class PDFFont : public PDFObject
{
public:
  PDFFont( const std::string &font ) : PDFObject(), m_fontname(font) {}

  virtual void WriteContent(  std::ostream &out ) final;

public:
  std::string m_fontname;
};

/******************************************************************************/

class PDFGroup : public PDFObject
{
public:
  PDFGroup() : PDFObject() {}

  virtual void WriteContent(  std::ostream &out ) final;

public:
    // nothing yet
};

/******************************************************************************/

class PDFXObject : public PDFObject
{
public:
  PDFXObject( const std::string &buf, Rect2D &bounds,
            size_t group = 0, size_t font = 0, size_t procSet = 0 ) :
    PDFObject(), m_buf(buf), m_bounds(bounds), m_procset(procSet),
    m_font(font), m_group(group)
        {}

  virtual void WriteContent(  std::ostream &out ) final;

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
            m_xrefStart(0), m_pageParentIndex(0), m_outlineIndex(0),
            m_fontIndex(0), m_groupIndex(0), m_procsetIndex(0)
    { }
  PDFWriter( const std::string &filename, float widthPt, float heightPt ):
            m_pageWidth(0), m_pageHeight(0), m_pageCount(0), m_xrefStart(0),
            m_pageParentIndex(0), m_outlineIndex(0), m_fontIndex(0),
            m_groupIndex(0), m_procsetIndex(0)
    { OpenFile(filename, widthPt, heightPt); }

  ~PDFWriter()
    { CloseFile(); }

  void OpenFile( const std::string &filename, float pageWidth, float pageHeight );
  void CloseFile();

public:

  void AddXObject( Rect2D &bounds, std::string &content, std::string name,
                size_t group = 0, size_t font = 0, size_t procSet = 0 );

  size_t PageCount() const { return m_pageCount; }
  float PageWidth() const { return m_pageWidth; }
  float PageHeight() const { return m_pageHeight; }
  size_t ObjectCount() const { return m_objects.size(); }
  bool xobjectExists( std::string name );

  void AddObject( PDFObject *obj ) {
    m_objects.push_back( obj );
  }

  void AddPage( size_t content, std::string xObjectName );

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

private:
  float m_pageWidth;     // used to init pages
  float m_pageHeight;    // used to init pages

  size_t m_pageCount;
  size_t m_xrefStart;
  size_t m_pageParentIndex;
  size_t m_outlineIndex;

  object_name_to_index_map m_xobjects;
  size_t m_fontIndex;       // used to init pages   // TODO - make this a map from name to object index
  size_t m_groupIndex;      // used to init pages   // this may not change too quickly
  size_t m_procsetIndex;    // used to init pages   // this may not change too quickly

  std::string m_filename;
  pdf_object_list m_objects;
};

/******************************************************************************/

#endif /* MiniPDF_hpp */
