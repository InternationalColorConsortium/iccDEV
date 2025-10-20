/** @file
    File:       MainPage.h

  Note: This file was added to provide documentation in doxygen.  Nothing in IccLibXML actually uses it.
*/

/** \mainpage 
 *
 * IccLibXML was written as an extension of SampleICC's IccProfLib.
 * This extension provides Inherited classes for the classes in IccProfLib
 * that provide additional I/O routines to read and write the classes as
 * XML files.  As such, it provides the means of converting ICC profiles
 * into and out of an XML format (hereafter referred to as icc.XML).
 * Documentation for the schema for the icc.XML in the library can be
 * found as part the ICCXML project distribution.  
 * 
 * Along with the IccLibXML library are two command line tools provided:
 * - IccToXML to convert an ICC profile file into and icc.XML file.
 * - IccFromXML to convert an icc.XML file into an ICC profile file.
 * 
 * Since IccLibXML is an extension of IccProfLib, an icc.XML file can be
 * parsed creating a CIccProfile object, and then used as if it had been
 * read from an actual ICC profile.
 * 
 * The purpose of the ICCXML project is to provide a means of convering
 * the data in an ICC profile into a human readable, editable format that
 * can then be converted back into somthing that can be used for color
 * management.  
 * 
 * Having an XML representation of ICC profiles is not meant as a
 * replacement for ICC profiles.  XML is much less efficient in terms of
 * storage, and many support mechanisms are already in place for the use of
 * ICC profiles that will not operate well with XML data.  However, there
 * are times where a human readable/editable form for ICC profile data
 * can be useful.
 * 
 * Note: At this point the icc.XML schema used to represent ICC profiles
 * has not been approved by the ICC.  This schema is currently subject to change,
 * and has not undergone the process of any formal standardization.  Additionally,
 * the XML schema documentation and data representaiton should not be used
 * as a replacement for the ICC profile specificaiton provided by the ICC
 * which does undergo a formal standardization process.
 * 
 * 
 * Copyright © The International Color Consortium.
 *
 * LICENSE
 * -------
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. In the absence of prior written permission, the names "ICC" and "The
 *    International Color Consortium" must not be used to imply that the
 *    ICC organization endorses or promotes products derived from this
 *    software.
 *
 *
 * ====================================================================\n
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED\n
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES\n
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR\n
 * ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF\n
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND\n
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT\n
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n
 * SUCH DAMAGE.\n
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the The International Color Consortium. 
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes. 
 * 
 *
 *  <b>VERSION HISTORY</b>
 * - November 2010
 *  - Fix Bugs related to support for MultiProcessElementType tags
 *  - Fix bug related to support for 'float' encoding in external table files
 *  - Fix buffer overrun bug in CIccTagXmlColorantTable::ToXml() 
 *
  * - July 2010
 *  - Prototype Release
 *
 * <b>CONTACT</b>
 *
 * Please send your questions, comments, and or suggestions to forums
 * on the ICCXML project site. (http://sourceforge.net/projects/iccxml/).\n
 *
 */

