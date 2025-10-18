/*
    File:       IccJsonUtil.cpp

    Contains:   Utility function with working with nlohmann/json objects

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * The ICC Software License, Version 0.2
 *
 *
 * Copyright (c) 2003-2024 The International Color Consortium. All rights
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

 ////////////////////////////////////////////////////////////////////// 
 // HISTORY:
 //
 // -Initial implementation by Max Derhak 1-11-2024
 // -Added Explicit Template Instantiations D Hoyt 18-MAR-2025
 //
 //////////////////////////////////////////////////////////////////////

#include "IccJsonUtil.h"
#include "IccUtil.h"
#include <stdio.h>
#include <string>
#include <sstream>  // For std::to_string fallback

template <typename T>
 std::string arrayToJson(T* a, int nCount)
{
  std::string str, num;
  str += "[ ";

  for (int i = 0; i < nCount; i++) {
    if (i)
      str += ", ";

    num = std::to_string(a[i]);

    str += num;
  }
  str += " ]";

  return str;
}

// Explicit template instantiations
template bool jsonToValue<int>(const json&, int&);
template std::string arrayToJson<icUInt8Number>(icUInt8Number*, int);
template std::string arrayToJson<icUInt16Number>(icUInt16Number*, int);
template std::string arrayToJson<icUInt32Number>(icUInt32Number*, int);
template std::string arrayToJson<icUInt64Number>(icUInt64Number*, int);
template std::string arrayToJson<icInt8Number>(icInt8Number*, int);
template std::string arrayToJson<icInt16Number>(icInt16Number*, int);
template std::string arrayToJson<icInt32Number>(icInt32Number*, int);
template std::string arrayToJson<icInt64Number>(icInt64Number*, int);

// General template implementation
template<typename T>
std::string valueToJson(const char* name, T v)
{
  // Safeguard for nullptr name
  if (!name) {
    throw std::invalid_argument("name cannot be null");
  }

  return std::string("\"") + name + "\": " + std::to_string(v);
}

// Explicit template instantiations for supported types
template std::string valueToJson<icUInt8Number>(const char*, icUInt8Number);
template std::string valueToJson<icUInt16Number>(const char*, icUInt16Number);
template std::string valueToJson<icUInt32Number>(const char*, icUInt32Number);
template std::string valueToJson<icUInt64Number>(const char*, icUInt64Number);
template std::string valueToJson<icInt8Number>(const char*, icInt8Number);
template std::string valueToJson<icInt16Number>(const char*, icInt16Number);
template std::string valueToJson<icInt32Number>(const char*, icInt32Number);
template std::string valueToJson<icInt64Number>(const char*, icInt64Number);
template std::string valueToJson<float>(const char*, float);
template std::string valueToJson<double>(const char*, double);


std::string fixJsonString(const char* v)
{
  std::string rv;
  for (; *v; v++) {
    if (*v == '\\')
      rv += "\\\\";
    else
      rv += *v;
  }

  return rv;
}

// Specialization for std::string
template<>
std::string valueToJson(const char* name, const std::string& v)
{
  if (!name) {
    throw std::invalid_argument("name cannot be null");
  }
  return "\"" + std::string(name) + "\": \"" + fixJsonString(v.c_str()) + "\"";
}

// Specialization for const char*
template<>
std::string valueToJson(const char* name, const char* v)
{
  if (!name) {
    throw std::invalid_argument("name cannot be null");
  }
  if (!v) {
    return "\"" + std::string(name) + "\": null";
  }
  return "\"" + std::string(name) + "\": \"" + fixJsonString(v) + "\"";
}

// Specialization for char*
template<>
std::string valueToJson(const char* name, char* v)
{
  if (!name) {
    throw std::invalid_argument("name cannot be null");
  }
  if (!v) {
    return "\"" + std::string(name) + "\": null";
  }
  return "\"" + std::string(name) + "\": \"" + fixJsonString(v) + "\"";
}

// Overload for valueToJson with a boolean flag
std::string valueToJson(const char* name, const char* v, bool& bPreviousLine)
{
  if (!name) {
    throw std::invalid_argument("name cannot be null");
  }
  if (v && *v) {
    std::string str = valueToJson(name, v);
    if (bPreviousLine) {
      return ", " + str;
    }
    bPreviousLine = true;
    return str;
  }
  return {};
}

template <typename T>
bool jsonToValue(const json& j, T& nValue)
{
  if (j.is_number()) {
    nValue = j.get<T>();
    return true;
  }

  return false;
}

template bool jsonToValue<icInt8Number>(const json&, icInt8Number&);
template bool jsonToValue<icUInt8Number>(const json&, icUInt8Number&);
template bool jsonToValue<icInt16Number>(const json&, icInt16Number&);
template bool jsonToValue<icUInt16Number>(const json&, icUInt16Number&);
template bool jsonToValue<icUInt32Number>(const json&, icUInt32Number&);
template bool jsonToValue<icInt64Number>(const json&, icInt64Number&);
template bool jsonToValue<icUInt64Number>(const json&, icUInt64Number&);
template bool jsonToValue<float>(const json&, float&);
template bool jsonToValue<double>(const json&, double&);

bool jsonToString(const json& j, std::string& value)
{
  if (j.is_string()) {
    value = j.get<std::string>();
    return true;
  }

  return false;
}

template<>
bool jsonToValue(const json& j, bool& value)
{
  if (j.is_boolean()) {
    value = j.get<bool>();
    return true;
  }
  else if (j.is_number_float()) {
    // don't want to check for 0 due to floating point messiness. If the value is greater than 0.5, then
    // it's definitely not 0.
    value = j.get<float>() > 0.5;
    return true;
  }
  else if (j.is_number_integer()) {
    value = j.get<int>() != 0;
    return true;
  }
  else if (j.is_number_unsigned()) {
    value = j.get<unsigned>() != 0;
    return true;
  }

  return false;
}


template<>
bool jsonToValue(const json& j, std::string&value)
{
  return jsonToString(j, value);
}

template <typename T>
bool jsonToArray(const json& v, T* vals, int n)
{
  if (v.is_array()) {
    int nValid = 0;
    auto e = v.begin();
    for (int i = 0; e != v.end() && i < n; e++, i++) {
      if (e->is_number()) {
        vals[i] = e->get<T>();
        nValid++;
      }
    }
    return nValid == v.size();
  }

  return false;
}


template bool jsonToArray<icInt8Number>(const json&, icInt8Number*, int);
template bool jsonToArray<icUInt8Number>(const json&, icUInt8Number*, int);
template bool jsonToArray<icInt16Number>(const json&, icInt16Number*, int);
template bool jsonToArray<icUInt16Number>(const json&, icUInt16Number*, int);
template bool jsonToArray<icInt32Number>(const json&, icInt32Number*, int);
template bool jsonToArray<icUInt32Number>(const json&, icUInt32Number*, int);
template bool jsonToArray<icInt64Number>(const json&, icInt64Number*, int);
template bool jsonToArray<icUInt64Number>(const json&, icUInt64Number*, int);
template bool jsonToArray<float>(const json&, float*, int);
template bool jsonToArray<double>(const json&, double*, int);

bool jsonToArray(const json& v, std::vector<std::string>& vals)
{
  if (v.is_array()) {
    size_t nSize = v.size();
    int nValid = 0;
    vals.resize(nSize);
    auto e = v.begin();
    for (int i = 0; e != v.end(); e++, i++) {
      if (e->is_string()) {
        vals[i] = e->get<std::string>();
        nValid++;
      }
      else {
        vals[i] = "";
      }
    }
    return nValid == nSize;
  }

  return false;
}

bool jsonToList(const json& v, std::list<std::string>& vals)
{
  if (v.is_array()) {
    size_t nSize = v.size();
    int nValid = 0;
    auto e = v.begin();
    for (int i = 0; e != v.end(); e++, i++) {
      if (e->is_string()) {
        vals.push_back(e->get<std::string>());
        nValid++;
      }
    }
    return nValid == nSize;
  }

  return false;
}

template <typename T>
bool jsonToArray(const json& v, std::vector<T>& a)
{
  if (v.is_array()) {
    size_t nSize = v.size();
    int nValid = 0;
    a.resize(nSize);
    auto e = v.begin();
    for (int i = 0; e != v.end(); e++, i++) {
      if (e->is_number()) {
        a[i] = e->get<T>();
        nValid++;
      }
      else {
        a[i] = 0;
      }
    }
    return nValid == v.size();
  }

  return false;
}

template bool jsonToArray<icInt8Number>(const json&, std::vector<icInt8Number>&);
template bool jsonToArray<icUInt8Number>(const json&, std::vector < icUInt8Number>&);
template bool jsonToArray<icInt16Number>(const json&, std::vector < icInt16Number>&);
template bool jsonToArray<icUInt16Number>(const json&, std::vector < icUInt16Number>&);
template bool jsonToArray<icInt32Number>(const json&, std::vector < icInt32Number>&);
template bool jsonToArray<icUInt32Number>(const json&, std::vector < icUInt32Number>&);
template bool jsonToArray<icInt64Number>(const json&, std::vector < icInt64Number>&);
template bool jsonToArray<icUInt64Number>(const json&, std::vector < icUInt64Number>&);
template bool jsonToArray<float>(const json&, std::vector<float>&);
template bool jsonToArray<double>(const json&, std::vector<double>&);


bool jsonToCStr(const json& j, char* str, int nSize)
{
  if (j.is_string() && nSize>0) {
    std::string val = j.get<std::string>();
    strncpy(str, val.c_str(), nSize);
    str[nSize - 1] = 0;
    return true;
  }

  return false;
}

bool jsonToColorSpace(const json& j, icColorSpaceSignature& sig)
{
  std::string str;
  if (jsonToValue(j, str)) {
    if (str.size())
      sig = (icColorSpaceSignature)icGetSigVal(str.c_str());
    else
      sig = icSigUnknownData;

    return true;
  }
  return false;
}

bool jsonExistsField(const json& j, const char* field)
{
  if (!j.is_object())
    return false;

  auto f = j.find(field);
  if (f != j.end())
    return true;
  return false;
}

bool saveJsonAs(const json& j, const char* szFname, int indent)
{
  if (!j.is_object())
    return false;

  bool rv = false;

  FILE* f;

  if (szFname && szFname[0])
    f = fopen(szFname, "wb");
  else
    f = stdout;

  if (f) {
    std::string str = j.dump(indent);

    if (fwrite((void*)(str.c_str()), 1, (unsigned long)str.size(), f) == (long)str.size())
      rv = true;

    if (f!=stdout)
      fclose(f);
  }

  return rv;
}

bool loadJsonFrom(json& j, const char* szFname)
{
  bool rv = false;
  j.clear();

  FILE* f = fopen(szFname, "rb");
  if (f) {
    fseek(f, 0, SEEK_END);
    unsigned long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(flen+1);

    if (buf) {
      buf[flen] = 0;
      if (fread(buf, 1, flen, f) == flen) {
        try {
          j = json::parse(buf);
          rv = true;
        }
        catch (...) {
        }
      }
      free(buf);
    }
    fclose(f);
  }

  return rv;
}
