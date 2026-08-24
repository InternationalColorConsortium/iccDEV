// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

#include "IccCValidation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Any non-NUL byte; makes an unwritten report byte visible to the checks. */
#define REPORT_SENTINEL 0x7F

#ifdef _WIN32
#include <windows.h>
typedef HMODULE library_handle;

static library_handle open_library(const char *path)
{
  return LoadLibraryA(path);
}

static void *load_symbol(library_handle handle, const char *name)
{
  return (void *)GetProcAddress(handle, name);
}

static void close_library(library_handle handle)
{
  FreeLibrary(handle);
}
#else
#include <dlfcn.h>
typedef void *library_handle;

static library_handle open_library(const char *path)
{
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static void *load_symbol(library_handle handle, const char *name)
{
  return dlsym(handle, name);
}

static void close_library(library_handle handle)
{
  dlclose(handle);
}
#endif

typedef icc_validation_status (*validate_function)(
  const unsigned char *, size_t, char *, size_t);

static unsigned char *read_file(const char *path, size_t *size)
{
  FILE *file = fopen(path, "rb");
  long length;
  unsigned char *data;

  if (!file)
    return NULL;
  if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0 ||
      fseek(file, 0, SEEK_SET)) {
    fclose(file);
    return NULL;
  }

  data = (unsigned char *)malloc((size_t)length);
  if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *size = (size_t)length;
  return data;
}

int main(int argc, char **argv)
{
  library_handle library;
  validate_function validate;
  unsigned char *profile;
  size_t profile_size;
  char report[8];
  char one_byte_report[1] = {'x'};
  icc_validation_status status;
  static const unsigned char malformed[] = {0, 1, 2, 3};

  if (argc != 3) {
    fprintf(stderr, "usage: %s <IccProfLib shared library> <profile>\n", argv[0]);
    return 2;
  }

  library = open_library(argv[1]);
  if (!library) {
    fprintf(stderr, "unable to load %s\n", argv[1]);
    return 1;
  }
  /* ISO C has no conversion from an object pointer to a function pointer, and
     gcc -Wpedantic rejects the direct cast; the repo already builds C with
     -Wall -Wextra -Wpedantic -Werror elsewhere. Copy the representation. */
  {
    void *symbol = load_symbol(library, "icc_validate_profile");
    memcpy(&validate, &symbol, sizeof(validate));
  }
  if (!validate) {
    fprintf(stderr, "icc_validate_profile is not exported\n");
    close_library(library);
    return 1;
  }

  profile = read_file(argv[2], &profile_size);
  if (!profile) {
    fprintf(stderr, "unable to read profile %s\n", argv[2]);
    close_library(library);
    return 1;
  }

  /* Prefill so every termination check below reads bytes this program wrote.
     A conforming profile reports nothing, so icc_validate_profile writes only
     report[0]; probing report[sizeof(report) - 1] would read whatever the
     caller's stack already held and fails under -ftrivial-auto-var-init. */
  memset(report, REPORT_SENTINEL, sizeof(report));
  status = validate(profile, profile_size, report, sizeof(report));
  free(profile);
  if (status != ICC_VALIDATION_OK && status != ICC_VALIDATION_WARNING) {
    /* Deliberately not printing report here: termination is what the next
       check establishes, and %s on an unterminated buffer would over-read
       exactly the defect this test exists to catch. */
    fprintf(stderr, "valid profile returned status %d\n", (int)status);
    close_library(library);
    return 1;
  }
  /* The documented invariant is termination *within* the buffer, which holds
     whether the report is empty or truncated. */
  if (!memchr(report, '\0', sizeof(report))) {
    fprintf(stderr, "validation report was not NUL-terminated\n");
    close_library(library);
    return 1;
  }

  status = validate(NULL, 0, one_byte_report, sizeof(one_byte_report));
  if (status != ICC_VALIDATION_INVALID_ARGUMENT || one_byte_report[0] != '\0') {
    fprintf(stderr, "invalid argument handling failed\n");
    close_library(library);
    return 1;
  }

  /* This report is far longer than the buffer, so it is the case that actually
     exercises truncation: the API must fill sizeof(report) - 1 bytes and
     terminate the last one. */
  memset(report, REPORT_SENTINEL, sizeof(report));
  status = validate(malformed, sizeof(malformed), report, sizeof(report));
  if (status != ICC_VALIDATION_CRITICAL_ERROR) {
    fprintf(stderr, "malformed profile returned status %d\n", (int)status);
    close_library(library);
    return 1;
  }
  if (strlen(report) != sizeof(report) - 1) {
    fprintf(stderr, "truncated report was not terminated at the buffer end\n");
    close_library(library);
    return 1;
  }

  close_library(library);
  puts("C validation dlopen consumer passed");
  return 0;
}
