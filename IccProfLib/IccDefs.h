/** @file
    File:       IccDefs.h  

    Contains:   Access ICC profile definitions and structures including
                Version 4 extensions

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
#include <stdint.h>
#include <stddef.h>

/* Header file guard bands */
#ifndef _ICCDEFS_H
#define _ICCDEFS_H

#pragma pack(4)

//Get any platform specific prototypes
#include "IccProfLibConf.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

//Include the standard icProfileHeader definition file
#include "icProfileHeader.h"

//Definitions used for conversion of fixed floating point numbers
typedef icUInt16Number icU1Fixed15Number;
typedef icUInt16Number icU8Fixed8Number;


/**
* All floating point operations/variables in IccProfLib use the icFloatNumber data type.
* It was found that using float instead of double increased performance.  Changing
* the definition to double will add greater precision at the cost of performance.
*/
typedef float              icFloatNumber;

/** String formating macros need to match precision of icFloatNumber
*If precision is double change the "f" below to "lf"
*/
#define ICFLOATSFX "f"
#define ICFLOATFMT "%f"

/* For string operations */
typedef char                icChar;
typedef unsigned char       icUChar; /*For Utf8 encoding*/
typedef unsigned short      icUChar16; /*For Utf16 encoding*/

#ifdef WIN32
typedef wchar_t             icWChar;
#endif

/* Validation Status values */
typedef enum {
  icValidateOK,              /*Profile is valid and conforms to specification*/
  icValidateWarning,         /*Profile conforms to specification with concerns*/ 
  icValidateNonCompliant,    /*Profile does not conform to specification, but may still be useable*/
  icValidateCriticalError,   /*Profile does not conform to specification and is not useable*/
} icValidateStatus;


#pragma pack()

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif /* _ICCDEFS_H */
