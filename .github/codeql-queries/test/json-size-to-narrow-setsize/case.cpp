/*
 * Copyright (c) 2026 International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the
 * ICC Software License are met.
 */

typedef unsigned short icUInt16Number;
typedef unsigned int icUInt32Number;

class JsonArray {
public:
  unsigned long size() const;
};

icUInt32Number icJsonSafeU32(unsigned long value);

class Colorants {
public:
  bool SetSize(icUInt16Number count);
};

bool unguarded(Colorants &colorants, const JsonArray &values)
{
  icUInt32Number count = icJsonSafeU32(values.size());
  return colorants.SetSize(count);
}

bool guarded(Colorants &colorants, const JsonArray &values)
{
  icUInt32Number count = icJsonSafeU32(values.size());
  if (count > 65535)
    return false;
  return colorants.SetSize(count);
}
