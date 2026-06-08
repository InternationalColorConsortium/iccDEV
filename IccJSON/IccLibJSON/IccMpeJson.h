/** @file
File:       IccMpeJson.h

Contains:   Header for implementation of CIccMpeJson class and
            creation factories

Version:    V1

Copyright:  (c) see Software License
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

#ifndef _ICCMPEJSON_H
#define _ICCMPEJSON_H

#include "IccMpeBasic.h"
#include "IccMpeACS.h"
#include "IccMpeCalc.h"
#include "IccMpeSpectral.h"
#include "../../IccProfLib/IccJsonTypes.h"
#include <map>
#include <list>
#include <string>

using IccJson = iccJson::json;

// ---------------------------------------------------------------------------
// Base extension interface
// ---------------------------------------------------------------------------
class CIccMpeJson : public IIccExtensionMpe
{
public:
  virtual ~CIccMpeJson() {}

  virtual bool ToJson(IccJson &j) = 0;
  virtual bool ParseJson(const IccJson &j, std::string &parseStr) = 0;

  virtual const char *GetExtClassName() { return "CIccMpeJson"; }
};

// ---------------------------------------------------------------------------
// Unknown element
// ---------------------------------------------------------------------------
class CIccMpeJsonUnknown : public CIccMpeUnknown, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonUnknown() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonUnknown"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Segmented curve helper (no MPE base, used by CIccMpeJsonCurveSet)
// ---------------------------------------------------------------------------
class CIccSegmentedCurveJson : public CIccSegmentedCurve
{
public:
  CIccSegmentedCurveJson() : CIccSegmentedCurve() {}
  CIccSegmentedCurveJson(const CIccSegmentedCurve &parent) : CIccSegmentedCurve(parent) {}
  CIccSegmentedCurveJson(const CIccSegmentedCurve *parent) : CIccSegmentedCurve(*parent) {}

  bool ToJson(IccJson &j);
  bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Standard MPE elements
// ---------------------------------------------------------------------------
class CIccMpeJsonCurveSet : public CIccMpeCurveSet, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonCurveSet() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonCurveSet"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonTintArray : public CIccMpeTintArray, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonTintArray() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonTintArray"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Tone-map element
// ---------------------------------------------------------------------------
class CIccJsonToneMapFunc : public CIccToneMapFunc
{
public:
  CIccJsonToneMapFunc() = default;
  CIccJsonToneMapFunc(const CIccJsonToneMapFunc &other) : CIccToneMapFunc(other) {}
  virtual ~CIccJsonToneMapFunc() {}
  CIccJsonToneMapFunc& operator=(const CIccJsonToneMapFunc &other) {
    CIccToneMapFunc::operator=(other);
    return *this;
  }
  virtual CIccToneMapFunc *NewCopy() const;
  virtual const char *GetClassName() const { return "CIccJsonToneMapFunc"; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonToneMap : public CIccMpeToneMap, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonToneMap() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonToneMap"; }
  virtual CIccToneMapFunc *NewToneMapFunc() { return new CIccJsonToneMapFunc(); }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonMatrix : public CIccMpeMatrix, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonMatrix() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonMatrix"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonCLUT : public CIccMpeCLUT, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonCLUT() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonCLUT"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonExtCLUT : public CIccMpeExtCLUT, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonExtCLUT() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonExtCLUT"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonBAcs : public CIccMpeBAcs, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonBAcs() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonBAcs"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonEAcs : public CIccMpeEAcs, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonEAcs() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonEAcs"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonJabToXYZ : public CIccMpeJabToXYZ, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonJabToXYZ() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonJabToXYZ"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonXYZToJab : public CIccMpeXYZToJab, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonXYZToJab() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonXYZToJab"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

// ---------------------------------------------------------------------------
// Calculator element -- helper types (parallel to IccMpeXml.h)
// ---------------------------------------------------------------------------
class CIccMpePtr
{
public:
  CIccMpePtr(CIccMultiProcessElement *pMpe = NULL, int nIndex = -1)
    : m_ptr(pMpe), m_nIndex(nIndex) {}
  CIccMultiProcessElement *m_ptr;
  int m_nIndex;
};

typedef std::map<std::string, CIccMpePtr>  MpePtrMap;
typedef std::list<CIccMpePtr>              MpePtrList;

class CIccTempVar
{
public:
  CIccTempVar(std::string name = "", int pos = -1, icUInt16Number size = 1)
    : m_name(name), m_pos(pos), m_size(size) {}
  std::string    m_name;
  int            m_pos;
  icUInt16Number m_size;
};

typedef std::map<std::string, CIccTempVar> TempVarMap;
typedef std::list<CIccTempVar>             TempVarList;
typedef std::pair<int,int>                 IndexSizePair;
typedef std::map<std::string, IndexSizePair> ChanVarMap;

class CIccTempDeclVar
{
public:
  CIccTempDeclVar(std::string name = "", int pos = -1, icUInt16Number size = 1)
    : m_name(name), m_pos(pos), m_size(size) {}
  std::string    m_name;
  int            m_pos;
  icUInt16Number m_size;
  TempVarList    m_members;
};

typedef std::map<std::string, CIccTempDeclVar> TempDeclVarMap;
typedef std::map<std::string, std::string>      MacroMap;

class CIccMpeJsonCalculator : public CIccMpeCalculator, public CIccMpeJson
{
public:
  CIccMpeJsonCalculator() : CIccMpeCalculator(), m_sImport("*"), m_nNextVar(0), m_nNextMpe(0) {}
  virtual ~CIccMpeJsonCalculator() { clean(); }

  virtual const char *GetClassName() const { return "CIccMpeJsonCalculator"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);

  bool ParseImport(const IccJson &j, std::string importPath, std::string &parseStr);

protected:
  void clean();
  static bool validNameChar(char c, bool bFirst);
  static bool validName(const char *szName);
  bool ValidMacroCalls(const char *szMacroText, std::string macroStack, std::string &parseStr) const;
  bool ValidateMacroCalls(std::string &parseStr) const;
  // CWE-674: nDepth caps macro-call recursion as a defense-in-depth net.
  // ValidateMacroCalls catches direct cycles via macroStack; nDepth bounds
  // attacker-crafted long non-cyclic chains and any bypass of validation.
  // See codeql recursive-parse-no-depth-limit.ql.
  static const icUInt32Number kMaxFlattenDepth = 256;
  bool Flatten(std::string &flatStr, std::string macroName, const char *szFunc,
               std::string &parseStr, icUInt32Number nLocalsOffset = 0,
               icUInt32Number nDepth = 0);
  bool UpdateLocals(std::string &func, std::string szFunc, std::string &parseStr, int nLocalsOffset);
  bool ParseChanMap(ChanVarMap &chanMap, const char *szNames, int nChannels);

  ChanVarMap     m_inputMap;
  ChanVarMap     m_outputMap;
  std::string    m_sImport;
  TempDeclVarMap m_declVarMap;
  int            m_nNextVar;
  TempVarMap     m_varMap;
  MpePtrMap      m_mpeMap;
  int            m_nNextMpe;
  MpePtrList     m_mpeList;
  MacroMap       m_macroMap;
  TempDeclVarMap m_macroLocalMap;
};

// ---------------------------------------------------------------------------
// Spectral elements
// ---------------------------------------------------------------------------
class CIccMpeJsonEmissionMatrix : public CIccMpeEmissionMatrix, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonEmissionMatrix() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonEmissionMatrix"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonInvEmissionMatrix : public CIccMpeInvEmissionMatrix, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonInvEmissionMatrix() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonInvEmissionMatrix"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonEmissionCLUT : public CIccMpeEmissionCLUT, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonEmissionCLUT() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonEmissionCLUT"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonReflectanceCLUT : public CIccMpeReflectanceCLUT, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonReflectanceCLUT() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonReflectanceCLUT"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonEmissionObserver : public CIccMpeEmissionObserver, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonEmissionObserver() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonEmissionObserver"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

class CIccMpeJsonReflectanceObserver : public CIccMpeReflectanceObserver, public CIccMpeJson
{
public:
  virtual ~CIccMpeJsonReflectanceObserver() {}
  virtual const char *GetClassName() const { return "CIccMpeJsonReflectanceObserver"; }
  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToJson(IccJson &j);
  virtual bool ParseJson(const IccJson &j, std::string &parseStr);
};

#endif // _ICCMPEJSON_H
