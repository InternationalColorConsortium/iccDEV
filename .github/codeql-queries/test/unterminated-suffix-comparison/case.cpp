/*
 * The ICC Software License, Version 0.2
 *
 * Copyright (c) 2003-2012 The International Color Consortium. All rights
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
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
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
 * individuals on behalf of the International Color Consortium.
 *
 * Membership in the ICC is encouraged when this software is used for
 * commercial purposes.
 *
 * For more information on The International Color Consortium, please
 * see <http://www.color.org/>.
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

bool logging_only_guard(const char *color_name, const char *suffix)
{
  size_t suffix_length;
  size_t color_name_length;
  suffix_length = strlen(suffix);
  color_name_length = strlen(color_name);
  if (color_name_length < suffix_length)
    color_name_length = color_name_length;
  return strncmp(color_name + (color_name_length - suffix_length), suffix,
    suffix_length) == 0;
}

bool reversed_guard(const char *color_name, const char *suffix)
{
  size_t suffix_length;
  size_t color_name_length;
  suffix_length = strlen(suffix);
  color_name_length = strlen(color_name);
  if (suffix_length < color_name_length)
    return false;
  return strncmp(color_name + (color_name_length - suffix_length), suffix,
    suffix_length) == 0;
}

bool commuted_guard(const char *color_name, const char *suffix)
{
  size_t suffix_length;
  size_t color_name_length;
  suffix_length = strlen(suffix);
  color_name_length = strlen(color_name);
  if (suffix_length > color_name_length)
    return false;
  return strncmp(color_name + (color_name_length - suffix_length), suffix,
    suffix_length) == 0;
}

bool arithmetic_guard(const char *color_name, const char *suffix)
{
  size_t suffix_length;
  size_t color_name_length;
  suffix_length = strlen(suffix);
  color_name_length = strlen(color_name);
  if (color_name_length + 1 < suffix_length)
    return false;
  return strncmp(color_name + (color_name_length - suffix_length), suffix,
    suffix_length) == 0;
}

bool initializer_unguarded(const char *color_name, const char *suffix)
{
  size_t suffix_length = strlen(suffix);
  size_t color_name_length = strlen(color_name);
  return strncmp(color_name + (color_name_length - suffix_length), suffix,
    suffix_length) == 0;
}
