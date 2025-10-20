/*
    File:       IccJsonUtil.h

    Contains:   Utility function with working with nlohmann/json objects

    Version:    V1

    Copyright:  (c) see below
*/

/*
 * Copyright (c) International Color Consortium.
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

#ifndef ICCJSONUTIL_H
#define ICCJSONUTIL_H

#include <list>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../../../IccProfLib/IccDefs.h"

using json = nlohmann::json;

// ========================
// JSON Conversion Helpers
// ========================
template <typename T>
std::string arrayToJson(T* a, int nCount);

template <typename T>
std::string valueToJson(const char* name, T v);

// Specialized `valueToJson` Templates
template <>
std::string valueToJson(const char* name, const char* v);

template <>
std::string valueToJson(const char* name, char* v);

std::string valueToJson(const char* name, const char* v, bool& bPreviousLine);

template <>
std::string valueToJson(const char* name, float v);

template <>
std::string valueToJson(const char* name, double v);

template <>
std::string valueToJson(const char* name, const std::string& v);

std::string valueToJsonUuid(const char* name, const char* v, bool bBinary = false);
std::string fixJsonString(const char* v);

// ========================
// JSON Parsing Helpers
// ========================
template <typename T>
bool jsonToValue(const json& j, T& nValue);

template <>
bool jsonToValue(const json& j, bool& nValue);

template <>
bool jsonToValue(const json& j, std::string& nValue);

bool jsonToString(const json& j, std::string& value);

template <typename T>
bool jsonToArray(const json& v, T* vals, int n);

template <typename T>
bool jsonToArray(const json& v, std::vector<T>& vals);

bool jsonToArray(const json& v, std::vector<std::string>& vals);
bool jsonToList(const json& v, std::list<std::string>& vals);
bool jsonToCStr(const json& j, char* str, int nSize);
bool jsonToColorSpace(const json& j, icColorSpaceSignature& sig);

bool jsonExistsField(const json& j, const char* field);

// ========================
// JSON File I/O
// ========================
bool saveJsonAs(const json& j, const char* szFname, int indent = 1);
bool loadJsonFrom(json& j, const char* szFname);

// ========================
// Prevent Duplicate Explicit Instantiation
// ========================
extern template bool jsonToValue<int>(const json&, int&);
extern template bool jsonToValue<bool>(const json&, bool&);
extern template bool jsonToValue<std::string>(const json&, std::string&);

#endif  // ICCJSONUTIL_H
