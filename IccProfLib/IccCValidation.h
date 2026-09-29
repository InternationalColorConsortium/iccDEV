/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2026 The International Color Consortium. All rights
 * reserved.
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
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
 * individuals on behalf of The International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
 */

// C-only validation API for IccProfLib. This header deliberately depends only
// on the C standard library so a consumer can resolve the function with dlopen.

#ifndef ICC_C_VALIDATION_H
#define ICC_C_VALIDATION_H

#include <stddef.h>

#if defined(_WIN32)
  #if defined(ICC_C_API_EXPORTS)
    #define ICC_C_API __declspec(dllexport)
  #elif defined(ICC_C_API_IMPORTS)
    #define ICC_C_API __declspec(dllimport)
  #else
    #define ICC_C_API
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #define ICC_C_API __attribute__((visibility("default")))
#else
  #define ICC_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum icc_validation_status {
  ICC_VALIDATION_OK = 0,
  ICC_VALIDATION_WARNING = 1,
  ICC_VALIDATION_NON_COMPLIANT = 2,
  ICC_VALIDATION_CRITICAL_ERROR = 3,
  ICC_VALIDATION_INVALID_ARGUMENT = 4,
  ICC_VALIDATION_INTERNAL_ERROR = 5
} icc_validation_status;

// Validates an in-memory ICC profile. report is optional; when supplied with a
// non-zero size it is always NUL-terminated, and the report may be truncated.
ICC_C_API icc_validation_status icc_validate_profile(
  const unsigned char *icc_data,
  size_t icc_size,
  char *report,
  size_t report_size);

#ifdef __cplusplus
}
#endif

#endif  // ICC_C_VALIDATION_H
