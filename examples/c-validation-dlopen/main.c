// Copyright (c) 2026 The International Color Consortium. All rights reserved.
// Licensed under the BSD 3-Clause "New" or "Revised" License; see the ICC
// Software License in the repository root and CONTRIBUTING.md.

#include "IccCValidation.h"

#include <stdio.h>
#include <stdlib.h>

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

static const char *status_name(icc_validation_status status)
{
  switch (status) {
    case ICC_VALIDATION_OK:
      return "OK";
    case ICC_VALIDATION_WARNING:
      return "WARNING";
    case ICC_VALIDATION_NON_COMPLIANT:
      return "NON_COMPLIANT";
    case ICC_VALIDATION_CRITICAL_ERROR:
      return "CRITICAL_ERROR";
    case ICC_VALIDATION_INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case ICC_VALIDATION_INTERNAL_ERROR:
      return "INTERNAL_ERROR";
  }
  return "UNKNOWN";
}

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
  char report[4096];
  icc_validation_status status;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <IccProfLib shared library> <profile>\n", argv[0]);
    return 2;
  }

  library = open_library(argv[1]);
  if (!library) {
    fprintf(stderr, "unable to load %s\n", argv[1]);
    return 1;
  }
  validate = (validate_function)load_symbol(library, "icc_validate_profile");
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

  status = validate(profile, profile_size, report, sizeof(report));
  free(profile);
  printf("validation: %s (%d)\n", status_name(status), (int)status);
  if (report[0])
    printf("report:\n%s", report);
  else
    puts("report: none");
  close_library(library);
  return status == ICC_VALIDATION_OK || status == ICC_VALIDATION_WARNING ? 0 : 1;
}
