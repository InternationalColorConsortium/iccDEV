/** @file
    File:       IccXmlConfig.cpp

    Contains:   Implementation IccXMl Configuration

    Version:    V1

    Copyright:  (c) see Software License
*/

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
#ifndef _ICCXMLCONFIG_H
#define _ICCXMLCONFIG_H

#include "IccProfLibConf.h"

typedef enum {
  icConvert8Bit=0,
  icConvert16Bit,
  icConvertFloat,
  icConvertVariable
} icConvertType;

#define icXmlHalfFmt "%.8f"
#define icXmlFloatFmt "%.9g"
#define icXmlDoubleFmt "%.24lf"

// Library-wide opt-in for <tag File="..."/> / <tag Filename="..."/>
// loaders in IccLibXML. Off by default because the XML is usually
// attacker-controlled in library/service/WASM contexts. The iccFromXml
// CLI tool flips this on after argument parsing. Exposed as accessor
// functions (not a bare extern bool) so the DLL interface works on
// Windows MSVC where WINDOWS_EXPORT_ALL_SYMBOLS does not export data.
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

ICCPROFLIB_API void IccXmlSetAllowFileIncludes(bool allow);
ICCPROFLIB_API bool IccXmlGetAllowFileIncludes();

#ifdef USEICCDEVNAMESPACE
} // namespace iccDEV
#endif

#endif
