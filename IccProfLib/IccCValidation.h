// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.
//
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
