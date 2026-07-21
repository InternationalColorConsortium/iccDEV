/*
  File:     iccProfileVisualize.hpp

  Contains: Public entry point of the iccProfileVisualizePlot tool - declares
            processLuts() so an out-of-line caller (a fuzz/regression harness,
            an alternate driver) can invoke it without redeclaring it or pulling
            in the whole translation unit and its main().

  Version:  V1

  Copyright:  (c) see below
*/

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

/**
 * iccProfileVisualize - public entry point.
 *
 * processLuts() is the whole-profile driver behind the iccProfileVisualizePlot
 * CLI: given an opened profile it enumerates the visualizations the profile
 * supports (iccviz::Enumerate), renders each one's DATA, and writes the PDF
 * plots + nD-CLUT TIFFs next to the input file. main() is just an argv loop
 * around it.
 *
 * It lives in iccProfileVisualize.cpp with no declaration of its own, so it was
 * reachable only from that translation unit's main(). This header hoists the
 * declaration out so another driver - a libFuzzer/regression harness that feeds
 * profiles straight to processLuts() - can call it after including this file and
 * linking the object, instead of copy-declaring the prototype (which silently
 * rots if the signature changes). Pair it with -DICC_PROFILEVISUALIZE_NO_MAIN
 * on that harness build to drop this file's main() and avoid a duplicate-main
 * clash with the harness's own entry point.
 */

#ifndef ICC_PROFILEVISUALIZE_HPP
#define ICC_PROFILEVISUALIZE_HPP

// Only a CIccProfile* is named in the interface, so forward-declare it rather
// than pull in IccProfile.h - matches IccVizModel.hpp and keeps this header
// cheap for a harness that already has its own ICC includes.
class CIccProfile;

// Render every visualization pIcc supports to PDF/TIFF written alongside
// profilePath (used to derive the output basename). Returns the count of
// output items produced; 0 means the profile yielded nothing to draw, which
// the CLI treats as a per-file failure.
int processLuts(CIccProfile *pIcc, const char *profilePath);

#endif // ICC_PROFILEVISUALIZE_HPP
