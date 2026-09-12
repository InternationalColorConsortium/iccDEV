/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the
 * ICC Software License are met.
 */

typedef unsigned long size_t;

extern "C" size_t strlen(const char *value);
extern "C" int strncmp(const char *left, const char *right, size_t count);

bool unguarded(const char *color_name, const char *suffix)
{
  size_t suffix_length;
  size_t color_name_length;
  suffix_length = strlen(suffix);
  color_name_length = strlen(color_name);
  return strncmp(color_name + (color_name_length - suffix_length), suffix,
    suffix_length) == 0;
}

bool guarded(const char *color_name, const char *suffix)
{
  size_t suffix_length;
  size_t color_name_length;
  suffix_length = strlen(suffix);
  color_name_length = strlen(color_name);
  if (color_name_length < suffix_length)
    return false;
  return strncmp(color_name + (color_name_length - suffix_length), suffix,
    suffix_length) == 0;
}
