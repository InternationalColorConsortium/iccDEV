/** @file
File:       IccMpeXml.h

Contains:   Header for implementation of CIccMpeXml class and
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


#ifndef _ICCMPEXML_H
#define _ICCMPEXML_H
#include "IccMpeBasic.h"
#include "IccMpeACS.h"
#include "IccMpeCalc.h"
#include "IccMpeSpectral.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>

class CIccMpeXml : public IIccExtensionMpe
{
public:
  virtual ~CIccMpeXml(void) {}

  virtual bool ToXml(std::string &xml, std::string blanks="") = 0;
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr) = 0;

  virtual const char *GetExtClassName() { return "CIccMpeXml"; }
};

class CIccMpeXmlUnknown : public CIccMpeUnknown, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlUnknown() {}

  virtual const char *GetClassName() const { return "CIccTagXmlUnknown"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};


class CIccSegmentedCurveXml : public CIccSegmentedCurve
{
public:
  CIccSegmentedCurveXml() : CIccSegmentedCurve() {}
  CIccSegmentedCurveXml( const CIccSegmentedCurve &parent) : CIccSegmentedCurve(parent) {}
  CIccSegmentedCurveXml( const CIccSegmentedCurve *parent) : CIccSegmentedCurve(*parent) {}

public:
  bool ToXml(std::string &xml, std::string blanks/* = ""*/);
  bool ParseXml(xmlNode *pNode, std::string &parseStr);
};


class CIccMpeXmlCurveSet : public CIccMpeCurveSet, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlCurveSet() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlCurveSet"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);

protected:
  //virtual CIccCurveSetCurve* CreateCurve(icCurveElemSignature sig);
};

class CIccMpeXmlTintArray : public CIccMpeTintArray, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlTintArray() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlTintArray"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccXmlToneMapFunc : public CIccToneMapFunc
{
public:
  virtual ~CIccXmlToneMapFunc() {}

  virtual CIccToneMapFunc* NewCopy() const;

  virtual const char* GetClassName() const { return "CIccXmlToneMapFunc"; }

  virtual bool ToXml(std::string& xml, std::string blanks = "");
  virtual bool ParseXml(xmlNode* pNode, std::string& parseStr);
};

class CIccMpeXmlToneMap : public CIccMpeToneMap, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlToneMap() {}

  virtual const char* GetClassName() const { return "CIccMpeXmlToneMap"; }

  virtual CIccToneMapFunc* NewToneMapFunc() { return new CIccXmlToneMapFunc(); }

  virtual IIccExtensionMpe* GetExtension() { return this; }

  virtual bool ToXml(std::string& xml, std::string blanks = "");
  virtual bool ParseXml(xmlNode* pNode, std::string& parseStr);
};

class CIccMpeXmlMatrix : public CIccMpeMatrix, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlMatrix() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlMatrix"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlCLUT : public CIccMpeCLUT, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlCLUT() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlCLUT"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlExtCLUT : public CIccMpeExtCLUT, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlExtCLUT() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlExtCLUT"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlBAcs : public CIccMpeBAcs, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlBAcs() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlBAcs"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlEAcs : public CIccMpeEAcs, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlEAcs() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlEAcs"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlJabToXYZ : public CIccMpeJabToXYZ, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlJabToXYZ() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlJabToXYZ"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlXYZToJab : public CIccMpeXYZToJab, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlXYZToJab() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlXYZToJab"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpePtr
{
public:
  CIccMpePtr(CIccMultiProcessElement *pMpe = NULL, int nIndex = -1) { m_ptr = pMpe; m_nIndex = nIndex; }
  
  CIccMultiProcessElement *m_ptr;
  int m_nIndex;
};

typedef std::map<std::string, CIccMpePtr> MpePtrMap;
typedef std::list<CIccMpePtr> MpePtrList;

class CIccTempVar
{
public:
  CIccTempVar(std::string name = "", int pos = -1, icUInt16Number size = 1) { m_name = name; m_pos = pos; m_size = size;}
  std::string m_name;
  int m_pos;
  icUInt16Number m_size;
};

typedef std::map<std::string, CIccTempVar> TempVarMap;
typedef std::list<CIccTempVar> TempVarList;
typedef std::pair<int, int> IndexSizePair;
typedef std::map<std::string, IndexSizePair> ChanVarMap;

class CIccTempDeclVar
{
public:
  CIccTempDeclVar(std::string name = "", int pos = -1, icUInt16Number size = 1) { m_name = name; m_pos = pos; m_size = size; }
  
  std::string m_name;
  int m_pos;
  icUInt16Number m_size;

  TempVarList m_members;
};

typedef std::map<std::string, CIccTempDeclVar> TempDeclVarMap;

typedef std::map<std::string, std::string> MacroMap;

class CIccMpeXmlCalculator : public CIccMpeCalculator, public CIccMpeXml
{
public:
  CIccMpeXmlCalculator() : CIccMpeCalculator() { m_sImport = "*"; }

public:
  virtual ~CIccMpeXmlCalculator() {  clean(); }

  virtual const char *GetClassName() const { return "CIccMpeXmlCalculator"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks = "");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);

  bool ParseImport(xmlNode *pNode, std::string importPath, std::string &parseStr);

protected:
  void clean();
  static bool validNameChar(char c, bool bFirst);
  static bool validName(const char *saName);
  bool ValidMacroCalls(const char *szMacroText, std::string macroStack, std::string &parseStr) const;
  bool ValidateMacroCalls(std::string &parseStr) const;
  bool Flatten(std::string &flatStr, std::string macroName, const char *szFunc, std::string &parseStr, icUInt32Number nLocalsOffset=0);
  bool UpdateLocals(std::string &func, std::string szFunc, std::string &parseStr, int nLocalsOffset);
  bool ParseChanMap(ChanVarMap& chanMap, const char *szNames, int nChannels);

  ChanVarMap m_inputMap;
  ChanVarMap m_outputMap;

  std::string m_sImport;

  TempDeclVarMap m_declVarMap;
  int m_nNextVar;
  TempVarMap m_varMap;

  MpePtrMap m_mpeMap;
  int m_nNextMpe;
  MpePtrList m_mpeList;

  MacroMap m_macroMap;
  TempDeclVarMap m_macroLocalMap;
};

class CIccMpeXmlEmissionMatrix : public CIccMpeEmissionMatrix, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlEmissionMatrix() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlEmissionMatrix"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlInvEmissionMatrix : public CIccMpeInvEmissionMatrix, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlInvEmissionMatrix() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlInvEmissionMatrix"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlEmissionCLUT : public CIccMpeEmissionCLUT, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlEmissionCLUT() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlEmissionCLUT"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlReflectanceCLUT : public CIccMpeReflectanceCLUT, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlReflectanceCLUT() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlReflectanceCLUT"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlEmissionObserver : public CIccMpeEmissionObserver, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlEmissionObserver() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlEmissionObserver"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

class CIccMpeXmlReflectanceObserver : public CIccMpeReflectanceObserver, public CIccMpeXml
{
public:
  virtual ~CIccMpeXmlReflectanceObserver() {}

  virtual const char *GetClassName() const { return "CIccMpeXmlReflectanceObserver"; }

  virtual IIccExtensionMpe *GetExtension() { return this; }

  virtual bool ToXml(std::string &xml, std::string blanks="");
  virtual bool ParseXml(xmlNode *pNode, std::string &parseStr);
};

#endif //_ICCMPEXML_H
