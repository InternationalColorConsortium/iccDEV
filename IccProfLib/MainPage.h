/** @file
    File:       MainPage.h

  Note: This file was added to provide documentation in doxygen.  Nothing in IccProfLib actually uses it.
*/

/** \mainpage 
 *
 * The IccProfLib is an open source cross platform C++ library for reading, writing, manipulating,
 * and applying ICC profiles.  It is an attempt at a strict interpretation of the ICC profile
 * specification.
 * The structure of the library very closely follows the structure of the specification.
 * A working knowledge of the ICC specification and color management workflows will aid in 
 * understanding the library and it's proper usage. For the latest ICC profile 
 * specification please visit http://www.color.org. Several useful white papers and resources 
 * are also available on the website.  
 *
 * 
 * Here are some of the things that the IccProfLib supports:
 *  - ICC profile file I/O in CIccProfile class
 *    - Version 5.0 profiles (read & write)
 *    - Version 4.3 profiles (read & write)
 *    - Version 2.x profiles (read)
 *  - C++ classes for all specified tag types (based on CIccTag). Default behavior for
 *    unrecognized private tag types is implemented using a generic Tag class.
 *  - Two basic Color Management Module (CMM) implementations
 *    - Basic pixel level transforms in CIccCmm class
 *    - Additional named color profile support in CIccNamedColorCmm class
 *  - File I/O can be re-directed
 *  - All operations performed using floating point.  Pixel precision not limited to integers.
 *  - Transforms are done one pixel at a time.
 *  - Flexible number of profile transforms in a series (as long as the colorspaces match)
 *  - Multidimensional lookup table interpolation
 *    - Three dimensional interpolation uses either linear or tetrahedral interpolation
 *      (selectable at time profile is attached to the CMM).
 *    - Greater than three dimensional interpolation uses linear interpolation
 *  - Matrix/TRC support
 *  - Calculation of Profile ID using the MD5 fingerprinting method (see IccMD5.h)
 *  - Dynamic creation and seemless use of private CIccTag derived objects that are implemented
 *    outside of IccProfLib (IE inside a private library or application that links with
 *    IccProfLib).
 *
 * <b>USAGE COMMENTS</b>
 *  -# The IccProfLib implements very basic CMMs.  These may not
 *   provide the optimum speed in all situations. Profile transforms are done one pixel
 *   at a time for each profile in a profile transformation chain.  Various techniques
 *   can possibly be used to improve performance. An initial thought would be to create a
 *   CMM that uses the basic CIccCmm to generate a single link transform (concatenating
 *   the profiles).  Such a transform could employ integer math if needed.
 *  -# The IccProfLib can be used to open, generate, manipulate (by adding, removing, or
 *   modifying tags), and/or save profiles without needing to use the pixel transformations
 *   provided by the CMM classes.
 *  -# Several applications have been written (in RefIccMAX) that make use of the IccProfLib.
 *   It is advisable to examine these applications for additional guidance in making
 *   the best use of the IccProfLib.
 *  -# Before compiling on non-Windows and non Mac OSX platforms it will be necessary to edit
 *   the configuration parameters in IccProfLibConf.h. 
 *
 *  <b>VERSION HISTORY</b>
 * - See changlog for updates
 *
 * <b>TODO List</b>
 * TBD
 *
 * Copyright � The International Color Consortium.
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
 * <b>CONTACT</b>
 *
 * Please send your questions, comments, and or suggestions to forums
 * on the iccDEV project site. (https://github.com/InternationalColorConsortium/iccDEV).\n
 *
 */

