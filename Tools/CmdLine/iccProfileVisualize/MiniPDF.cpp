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
#include <string>
#include <vector>
#include <cmath>
#include "MiniPDF.hpp"


/*
PDF structure:
    Header
        %PDF-1.7
    objects
        first catalog, lists major sections
            1 obj
            <</Pages 2 0 R/Type/Catalog>>
            endobj
        metadata (XMP - yuck, optional)
        outlines (optional, can be created from pages)
        pages parent
            page definitions (artbox,viewbox,cropbox, content refs, XObject refs, etc.)
        shared objects
        text objects
        graphics objects
        
    directory to objects (20 bytes each)
        xref
        0 count
        0000000000 65535 f
        offset 00000 n
        ....
    trailer
        object count, root ref, metadata ref, GUIDs?
        offset of directory
        %%EOF


 */
/******************************************************************************/

void PDFWriter::OpenFile( const std::string &filename, float widthMM, float heightMM ) {
  if (!m_filename.empty()) {
    fprintf(stderr,"WARNING - PDF file already open!\n");
  }
  
  m_filename = filename;
  m_pageWidth = widthMM * mm2point;
  m_pageHeight = heightMM * mm2point;

  m_objects.clear();
  m_xrefStart = 0;          // set when writing
  m_pageParentIndex = 2;    // PDF index from 1
  m_outlineIndex = 3;

  // Create root object, always object 1
  PDFRoot *rootObj = new PDFRoot(m_pageParentIndex,m_outlineIndex);
  m_objects.emplace_back( rootObj );

  // Create page parent 2, link to add children out of order
  PDFPageParent *pageParentObj = new PDFPageParent();
  m_objects.emplace_back( pageParentObj );
  
  // Create outline data 3 from pages
  PDFOutlineParent *outlineObj = new PDFOutlineParent();
  m_objects.emplace_back( outlineObj );




// TODO - somehow this is confusing Acrobat
// what is missing or wrong?  Works in Safari, Photoshop, Illustrator.


// temporary fake page

    // procset
  PDFProcSet *procObj = new PDFProcSet( "[/PDF /Text]" );
  m_objects.emplace_back( procObj );
  size_t procSet = m_objects.size();
  
    // font definition
  PDFFont *fontObj = new PDFFont( "Helvetica" );
  m_objects.emplace_back( fontObj );
  size_t font = m_objects.size();



  size_t content = m_objects.size() + 2;
  PDFPage *pageObj = new PDFPage( m_pageWidth, m_pageHeight, m_pageParentIndex, content, procSet, font );
  m_objects.emplace_back( pageObj );
  pageParentObj->m_pageObjectIndices.push_back( m_objects.size() );
  m_pageCount++;
  
  PDFGraphic *graphics = new PDFGraphic();
  // add text data
  graphics->m_buf += "150 250 m 150 350 l S";
  graphics->m_buf += " 200 300 50 75 re B";
  graphics->m_buf += " BT\n /F1 24 Tf 100 100 Td (Hello World 1) Tj\nET";
  m_objects.emplace_back( graphics );


// second page
  content = m_objects.size() + 2;
  PDFPage *pageObj2 = new PDFPage( m_pageWidth, m_pageHeight, m_pageParentIndex, content, procSet, font );
  m_objects.emplace_back( pageObj2 );
  pageParentObj->m_pageObjectIndices.push_back( m_objects.size() );
  m_pageCount++;
  
  PDFGraphic *graphics2 = new PDFGraphic();
  // add text data
  graphics2->m_buf += "150 250 m 150 350 l S";
  graphics2->m_buf += " 200 300 50 75 re B";
  graphics2->m_buf += " BT\n /F1 24 Tf 200 100 Td (Hello World 2) Tj\nET";
  m_objects.emplace_back( graphics2 );

// need to add common content (axes and basic labels)

}

/******************************************************************************/

void PDFWriter::CloseFile() {

  if (!m_filename.empty()) {
    if (PageCount() > 0) {
        std::ofstream out(m_filename);
        WriteHeader(out);
        WriteObjects(out);
        WriteXRefs(out);
        WriteFooter(out);
        out.close();
        m_objects.clear();
    }
    m_filename.clear();
  }
}

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
        fprintf(stderr,"WARNING - PDF object referenced but not written yet\n");
    snprintf( buf, bufSize, "%10.10zu", obj->m_offset );
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
    fprintf(stderr,"WARNING - PDF trailer written before xref directory\n");
  out << m_xrefStart;
  out << "\n%%EOF\n";
}

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
  if (m_procset || m_font) {
    out << " /Resources <<";
    if (m_procset)
        out << "/ProcSet " << m_procset << " 0 R";
    if (m_font)
        out << " /Font <</F1 " << m_font << " 0 R>>";
    out << " >> ";
  }
  out << ">>\n";
}

/******************************************************************************/

void PDFProcSet::WriteContent( std::ostream &out )
{
  out << m_buf << "\n";
}

/******************************************************************************/

void PDFFont::WriteContent( std::ostream &out )
{
  out << "<< /Type /Font /SubType /Type1 /Encoding /MacRomanEncoding ";
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

// type size is in points, rotation in degrees
void PDFWriter::AddText( const float xCoord, const float yCoord, const std::string &text,
          const float size, const std::string &font,
          const std::string &style, const std::string &align,
          const float rotation )
{
}

/******************************************************************************/
/******************************************************************************/
