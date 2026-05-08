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
#include "MiniSVG.hpp"

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

class PDFRoot : public PDFObject
{
public:
  PDFRoot( size_t pageObj, size_t outlineObj ) :
        PDFObject(), m_pageObj(pageObj), m_outlineObj(outlineObj) {}

  virtual void WriteContent(  std::ostream &out );

public:
  size_t m_pageObj;
  size_t m_outlineObj;  // not used yet
};

/******************************************************************************/

class PDFPageParent : public PDFObject
{
public:
  PDFPageParent() : PDFObject() {}
  
  virtual void WriteContent(  std::ostream &out );

public:
    std::vector<size_t> m_pageObjectIndices;
};

/******************************************************************************/

class PDFOutlineParent : public PDFObject
{
public:
  PDFOutlineParent() : PDFObject() {}
  
  virtual void WriteContent(  std::ostream &out );

public:
    // nothing yet
};

/******************************************************************************/

class PDFPage: public PDFObject
{
public:
  PDFPage( float widthPt, float heightPt, size_t parent, size_t content, size_t procSet = 0, size_t font = 0 ) :
    PDFObject(), m_pageWidth(widthPt), m_pageHeight(heightPt),
    m_pageParentIndex(parent), m_pageContentIndex(content),
    m_procset(procSet), m_font(font)
     {}
  
  virtual void WriteContent(  std::ostream &out );

public:
  float m_pageWidth;
  float m_pageHeight;
  size_t m_pageParentIndex;     // could avoid this if Write passed in PDFWriter reference
  size_t m_pageContentIndex;
  size_t m_procset;
  size_t m_font;
};

/******************************************************************************/

// TODO - should this be more abstract as a simple string container class?
class PDFProcSet : public PDFObject
{
public:
  PDFProcSet( const std::string &proc ) : PDFObject(), m_buf(proc) {}
  
  virtual void WriteContent(  std::ostream &out );

public:
  std::string m_buf;
};

/******************************************************************************/

class PDFGraphic : public PDFObject
{
public:
  PDFGraphic() : PDFObject() {}
  
  virtual void WriteContent(  std::ostream &out );

public:
  std::string m_buf;
};

/******************************************************************************/

class PDFFont : public PDFObject
{
public:
  PDFFont( const std::string &font ) : PDFObject(), m_fontname(font) {}
  
  virtual void WriteContent(  std::ostream &out );

public:
  std::string m_fontname;
};

/******************************************************************************/

// all input units are in mm, but SVG works mostly in points
class PDFWriter
{
public:
  PDFWriter() : m_pageWidth(0), m_pageHeight(0), m_pageCount(0),
            m_xrefStart(0), m_pageParentIndex(0), m_outlineIndex(0)
    { }
  PDFWriter( const std::string &filename, float widthMM, float heightMM ):
            m_pageWidth(0), m_pageHeight(0), m_pageCount(0), m_xrefStart(0),
            m_pageParentIndex(0), m_outlineIndex(0)
    { OpenFile(filename, widthMM, heightMM); }
  
  ~PDFWriter()
    { CloseFile(); }

  void OpenFile( const std::string &filename, float pageWidth, float pageHeight );
  void CloseFile();

public:
  
  void AddText( const float xCoord, const float yCoord, const std::string &text,
        const float size, const std::string &font, const std::string &style,
        const std::string &align, const float rotation = 0.0 );

    size_t PageCount() const { return m_pageCount; }

protected:

  void WriteHeader( std::ostream &out );
  void WriteObjects( std::ostream &out );
  void WriteXRefs( std::ostream &out );
  void WriteFooter( std::ostream &out );


private:
  float m_pageWidth;
  float m_pageHeight;
  
  size_t m_pageCount;
  size_t m_xrefStart;
  size_t m_pageParentIndex;
  size_t m_outlineIndex;
// page shared objects?
  
  std::string m_filename;
  pdf_object_list m_objects;
};

/******************************************************************************/

#endif /* MiniPDF_hpp */
