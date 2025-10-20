/** @file
File:       IccMpeCalc.h

Contains:   Header for implementation of Channel Calculator element
            and supporting classes

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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Jan 30, 2005 
//  Initial CIccMpeent prototype development
// -June 16, 2006
//  CIccCalculatorElement prototype development
//
//////////////////////////////////////////////////////////////////////

#ifndef _ICCELEMCALC_H
#define _ICCELEMCALC_H

#include "IccTagMPE.h"
#include "IccSolve.h"
#include <vector>

//CIccFloatTag support
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

//Declare forward references
class CIccMpeCurveSet;
class CIccMpeMatrix;
class CIccMpeCLUT;
class CIccMpeCalculator;
class CIccApplyMpeCalculator;
class IIccOpDef;

#define icMaxDataStackSize 65535



/************************************************************************
 * Channel Function signature
 ************************************************************************/
typedef enum {
  icSigChannelFunction              = 0x66756e63,  /* 'func' */
} icChannelFuncSignature;


/************************************************************************ 
 * Calculator operator signatures
 ************************************************************************/
typedef enum {
  //Floating point constant operation
  icSigDataOp                       = 0x64617461,  /* 'data' */
  icSigPiOp                         = 0x70692020,  /* 'pi  ' */
  icSigPosInfinityOp                = 0x2b494e46,  /* '+INF' */
  icSigNegInfinityOp                = 0x2d494e46,  /* '-INF' */
  icSigNotaNumberOp                 = 0x4e614e20,  /* 'NaN ' */

  //Variable operations
  icSigInputChanOp                  = 0x696e2020,  /* 'in  ' */
  icSigOutputChanOp                 = 0x6f757420,  /* 'out ' */
  icSigTempGetChanOp                = 0x74676574,  /* 'tget' */
  icSigTempPutChanOp                = 0x74707574,  /* 'tput' */
  icSigTempSaveChanOp               = 0x74736176,  /* 'tsav' */

  //Environment variable operation
  icSigEnvVarOp                     = 0x656e7620,  /* 'env ' */

  //Sub-element operations
  icSigApplyCurvesOp                = 0x63757276,  /* 'curv' */
  icSigApplyMatrixOp                = 0x6d747820,  /* 'mtx ' */
  icSigApplyCLutOp                  = 0x636c7574,  /* 'clut' */
  icSigApplyTintOp                  = 0x74696e74,  /* 'tint' */
  icSigApplyToJabOp                 = 0x744a6162,  /* 'tJab' */
  icSigApplyFromJabOp               = 0x664a6162,  /* 'fJab' */
  icSigApplyCalcOp                  = 0x63616c63,  /* 'calc' */
  icSigApplyElemOp                  = 0x656c656d,  /* 'elem' */

  //Stack operations
  icSigCopyOp                       = 0x636f7079,  /* 'copy' */
  icSigRotateLeftOp                 = 0x726f746c,  /* 'rotl' */
  icSigRotateRightOp                = 0x726f7472,  /* 'rotr' */
  icSigPositionDupOp                = 0x706f7364,  /* 'posd' */
  icSigFlipOp                       = 0x666c6970,  /* 'flip' */
  icSigPopOp                        = 0x706f7020,  /* 'pop ' */

  //Variable length operations
  icSigSumOp                        = 0x73756d20,  /* 'sum ' */
  icSigProductOp                    = 0x70726f64,  /* 'prod' */
  icSigMinimumOp                    = 0x6d696e20,  /* 'min ' */
  icSigMaximumOp                    = 0x6d617820,  /* 'max ' */
  icSigAndOp                        = 0x616e6420,  /* 'and ' */
  icSigOrOp                         = 0x6f722020,  /* 'or  ' */

  //Vector Function operations
  icSigNegOp                        = 0x6e656720,  /* 'neg ' */
  icSigAddOp                        = 0x61646420,  /* 'add ' */
  icSigSubtractOp                   = 0x73756220,  /* 'sub ' */
  icSigMultiplyOp                   = 0x6d756c20,  /* 'mul ' */
  icSigDivideOp                     = 0x64697620,  /* 'div ' */
  icSigModulusOp                    = 0x6d6f6420,  /* 'mod ' */
  icSigPowOp                        = 0x706f7720,  /* 'pow ' */
  icSigGammaOp                      = 0x67616d61,  /* 'gama' */
  icSigScalarAddOp                  = 0x73616464,  /* 'sadd' */
  icSigScalarSubtractOp             = 0x73737562,  /* 'ssub' */
  icSigScalarMultiplyOp             = 0x736d756c,  /* 'smul' */
  icSigScalarDivideOp               = 0x73646976,  /* 'sdiv' */
  icSigSquareOp                     = 0x73712020,  /* 'sq  ' */
  icSigSquareRootOp                 = 0x73717274,  /* 'sqrt' */
  icSigCubeOp                       = 0x63622020,  /* 'cb  ' */
  icSigCubeRootOp                   = 0x63627274,  /* 'cbrt' */
  icSigSignOp                       = 0x7369676e,  /* 'sign' */
  icSigAbsoluteValOp                = 0x61627320,  /* 'abs ' */
  icSigFloorOp                      = 0x666c6f72,  /* 'flor' */
  icSigCeilingOp                    = 0x6365696c,  /* 'ceil' */
  icSigTruncateOp                   = 0x74726e63,  /* 'trnc' */
  icSigRoundOp                      = 0x726f6e64,  /* 'rond' */
  icSigExpOp                        = 0x65787020,  /* 'exp ' */
  icSigLogrithmOp                   = 0x6c6f6720,  /* 'log ' */
  icSigNaturalLogOp                 = 0x6c6e2020,  /* 'ln  ' */
  icSigSineOp                       = 0x73696e20,  /* 'sin ' */
  icSigCosineOp                     = 0x636f7320,  /* 'cos ' */
  icSigTangentOp                    = 0x74616e20,  /* 'tan ' */
  icSigArcSineOp                    = 0x6173696e,  /* 'asin' */
  icSigArcCosineOp                  = 0x61636f73,  /* 'acos' */
  icSigArcTangentOp                 = 0x6174616e,  /* 'atan' */
  icSigArcTan2Op                    = 0x61746e32,  /* 'atn2' */
  icSigCartesianToPolarOp           = 0x63746f70,  /* 'ctop' */
  icSigPolarToCartesianOp           = 0x70746f63,  /* 'ptoc' */
  icSigRealNumberOp                 = 0x726e756d,  /* 'rnum' */
  icSigLessThanOp                   = 0x6c742020,  /* 'lt  ' */
  icSigLessThanEqualOp              = 0x6c652020,  /* 'le  ' */
  icSigEqualOp                      = 0x65712020,  /* 'eq  ' */
  icSigNotEqualOp                   = 0x6e657120,  /* 'neq ' */
  icSigNearOp                       = 0x6e656172,  /* 'near' */
  icSigGreaterThanEqualOp           = 0x67652020,  /* 'ge  ' */
  icSigGreaterThanOp                = 0x67742020,  /* 'gt  ' */
  icSigNotOp                        = 0x6e6f7420,  /* 'not ' */
  icSigToLabOp                      = 0x744c6162,  /* 'tLab' */
  icSigToXYZOp                      = 0x7458595a,  /* 'tXYZ' */
  icSigVectorMinimumOp              = 0x766d696e,  /* 'vmin' */
  icSigVectorMaximumOp              = 0x766d6178,  /* 'vmax' */
  icSigVectorAndOp                  = 0x76616e64,  /* 'vand' */
  icSigVectorOrOp                   = 0x766f7220,  /* 'vor ' */

  //Matrix Operations
  icSigSolveOp                      = 0x736f6c76,  /* 'solv' */
  icSigTransposeOp                  = 0x7472616e,  /* 'tran' */

  //Conditional operation
  icSigIfOp                         = 0x69662020,  /* 'if  ' */
  icSigElseOp                       = 0x656c7365,  /* 'else' */

  //Selection operation
  icSigSelectOp                     = 0x73656c20,  /* 'sel ' */
  icSigCaseOp                       = 0x63617365,  /* 'case' */
  icSigDefaultOp                    = 0x64666c74,  /* 'dflt' */

  //Boolean operation
}icSigCalcOp;



/**
****************************************************************************
* Structure: SIccCalcOp
* 
* Purpose: A structure to keep track of channel operations
*****************************************************************************
*/
struct ICCPROFLIB_API SIccCalcOp
{
public:
  icSigCalcOp sig;
  union {
    icFloat32Number num;
    icUInt32Number size;
    struct {
#ifndef ICC_BYTE_ORDER_LITTLE_ENDIAN
      icUInt16Number v1;
      icUInt16Number v2;
#else
      icUInt16Number v2;
      icUInt16Number v1;
#endif
    } select;
  }data;
  unsigned long extra;
  IIccOpDef *def;

  void Describe(std::string &desc, int nVerboseness=100);
  
  static bool IsValidOp(icSigCalcOp sig);

  bool IsValidOp(CIccMpeCalculator *pCalc);
  icUInt16Number ArgsUsed(CIccMpeCalculator *pCalc);
  icUInt16Number ArgsPushed(CIccMpeCalculator *pCalc);
};

typedef std::list<SIccCalcOp> CIccCalcOpList;
typedef CIccCalcOpList* CIccCalcOpListPtr;
typedef std::list<CIccCalcOpListPtr> CIccCalcOpListPtrList;

typedef std::vector<icFloatNumber> CIccFloatVector;

/**
****************************************************************************
* Structure: SIccOpState
* 
* Purpose: Keep everything in one place for IIccOpDef Exec
*****************************************************************************
*/
struct SIccOpState
{
  CIccApplyMpeCalculator *pApply;
  CIccFloatVector *pStack;
  CIccFloatVector *pScratch;
  icFloatNumber *temp;
  const icFloatNumber *pixel;
  icFloatNumber *output;
  icUInt32Number idx;
  icUInt32Number nOps;
};

/**
****************************************************************************
* Interface: IIccOpDef
* 
* Purpose: A structure to keep track of channel operations
*****************************************************************************
*/
class ICCPROFLIB_API IIccOpDef
{
public:
  IIccOpDef() {}
  virtual ~IIccOpDef() {}

  virtual int ArgsPushed(CIccMpeCalculator *pCalc, SIccCalcOp &op) { return op.ArgsPushed(pCalc);}
  virtual int ArgsUsed(CIccMpeCalculator *pCalc, SIccCalcOp &op) { return op.ArgsUsed(pCalc);}

  virtual void Describe(SIccCalcOp &op, std::string &desc, int nVerboseness) {op.Describe(desc, nVerboseness);}

  virtual bool IsValid(CIccMpeCalculator *pCalc, SIccCalcOp &op) { return op.IsValidOp(pCalc); }

  virtual bool Exec(SIccCalcOp *op, SIccOpState &s) {return false;}
};


typedef enum {
  icFuncParseNoError=0,
  icFuncParseSyntaxError,
  icFuncParseInvalidOperation,
  icFuncParseStackUnderflow,
  icFuncParseStackOverflow,
  icFuncParseInvalidChannel,
  icFuncParseEmptyFunction,
} icFuncParseStatus;

#define icSigBeginBlockOp ((icSigCalcOp)0x7b202020)
#define icSigEndBlockOp  ((icSigCalcOp)0x7d202020)
#define icSigBadOp ((icSigCalcOp)0)

class ICCPROFLIB_API CIccFuncTokenizer
{
public:
  CIccFuncTokenizer(const char *szText, bool bAllowReferences = false);
  virtual ~CIccFuncTokenizer();
  
  bool GetNext(bool bForceNoRefs = false);
  const char *GetPos() { return m_text; }
  void SetPos(const char *szText) { m_text = szText; }
  
  icSigCalcOp GetSig();
  bool GetIndex(icUInt16Number &v1, icUInt16Number &v2, icUInt16Number initV1=0, icUInt16Number initV2=0);
  icFloat32Number GetValue();
  bool GetEnvSig(icSigCmmEnvVar &envSig);

  //The following are used by XML variable and macro parsing
  std::string GetName() const;
  std::string GetReference() const;

  std::string &GetLast() { return *m_token; }

protected:
  bool IsWhiteSpace();
  bool IsComment();
  void SkipComment();

  const char *m_text;
  std::string *m_token;

  bool m_bUseRefs;
};

class CIccApplyMpeCalculator;

/**
****************************************************************************
* Class: CIccCalculatorFunc
* 
* Purpose: The Calculator function sequence
*****************************************************************************
*/
class ICCPROFLIB_API CIccCalculatorFunc
{
public:
  CIccCalculatorFunc(CIccMpeCalculator *pCalc);
  CIccCalculatorFunc(const CIccCalculatorFunc &ICF);
  CIccCalculatorFunc &operator=(const CIccCalculatorFunc &ICF);
  virtual CIccCalculatorFunc *NewCopy() const { return new CIccCalculatorFunc(*this);}
  virtual ~CIccCalculatorFunc();

  virtual icChannelFuncSignature GetType() const { return icSigChannelFunction; }
  virtual const icChar *GetClassName() const { return "CIccChannelFunction"; }

  virtual void Describe(std::string &sDescription, int nVerboseness=0, int nBlanks=0);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual bool Begin(const CIccMpeCalculator *pChannelMux, CIccTagMultiProcessElement *pMPE);
  virtual bool Apply(CIccApplyMpeCalculator *pApply) const;
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport,
                                    const CIccMpeCalculator* pChannelCalc=NULL, const CIccProfile* pProfile = NULL) const;

  icFuncParseStatus SetFunction(const char *szFuncDef, std::string &sReport);
  icFuncParseStatus SetFunction(CIccCalcOpList &opList, std::string &sReport);

  icUInt32Number GetMaxTemp() const;
  int CheckUnderflowOverflow(SIccCalcOp *op, icUInt32Number nOps, int nArgs, bool bCheckUnderflow, std::string &sReport) const;
  icFuncParseStatus DoesStackUnderflowOverflow(std::string &sReport) const;
  bool HasValidOperations(std::string &sReport) const;
  bool HasUnsupportedOperations(std::string &sReport, const CIccProfile *pProfile) const;
  bool DoesOverflowInput(icUInt16Number nInputChannels) const;
  bool DoesOverflowOutput(icUInt16Number nOutputChannels) const;
  bool NeedTempReset(icUInt8Number *tempUsage, icUInt32Number nMaxTemp);
  bool SetOpDefs();

protected:

  bool InitSelectOps();
  bool InitSelectOp(SIccCalcOp *ops, icUInt32Number nOps);

  bool SequenceNeedTempReset(SIccCalcOp *op, icUInt32Number nOps, icUInt8Number *tempUsage, icUInt32Number nMaxTemp);

  void InsertBlanks(std::string &sDescription, int nBlanks);
  void DescribeSequence(std::string &sDescription,
                        icUInt32Number nOps, SIccCalcOp *op, int nBlanks);
  bool ApplySequence(CIccApplyMpeCalculator *pApply, icUInt32Number nOps, SIccCalcOp *op) const;

  const char *ParseFuncDef(const char *szFuncDef, CIccCalcOpList &m_list, std::string &sReport);

  CIccMpeCalculator *m_pCalc;

  icUInt32Number m_nReserved;

  icUInt32Number m_nOps;
  SIccCalcOp *m_Op;

};

typedef CIccCalculatorFunc* icCalculatorFuncPtr;

class ICCPROFLIB_API CIccSubCalcApply
{
public:
  CIccSubCalcApply(CIccApplyMpe *pApplyMpe) { m_pApply = pApplyMpe; }
  ~CIccSubCalcApply() { if (m_pApply) delete m_pApply; }

  icUInt16Number NumInputChannels() { return m_pApply->GetElem()->NumInputChannels(); }
  icUInt16Number NumOutputChannels() { return m_pApply->GetElem()->NumOutputChannels(); }

  void Apply(icFloatNumber *pDestPixel, const icFloatNumber *pSrcPixel) { if (m_pApply) m_pApply->Apply(pDestPixel, pSrcPixel); }

protected:
  CIccApplyMpe *m_pApply;
};


/**
****************************************************************************
* Class: CIccMpeCalculator
* 
* Purpose: The Calculator process element
*****************************************************************************
*/
class ICCPROFLIB_API CIccMpeCalculator : public CIccMultiProcessElement
{
public:
  CIccMpeCalculator(icUInt16Number nInputChannels=0, icUInt16Number nOutputChannels=0);
  CIccMpeCalculator(const CIccMpeCalculator &curveSet);
  CIccMpeCalculator &operator=(const CIccMpeCalculator &curveSet);
  virtual CIccMpeCalculator *NewCopy() const { return new CIccMpeCalculator(*this);}
  virtual ~CIccMpeCalculator();

  void Reset() { SetSize(0,0); }
  void SetSize(icUInt16Number nInputChannels, icUInt16Number nOutputChannels);

  icFuncParseStatus SetCalcFunc(icCalculatorFuncPtr newFunc);
  icFuncParseStatus SetCalcFunc(const char *szFuncDef, std::string &sReport);

  bool SetSubElem(icUInt32Number idx, CIccMultiProcessElement *pElem) { return SetElem(idx, pElem, m_nSubElem, &m_SubElem); }

  virtual icElemTypeSignature GetType() const { return icSigCalculatorElemType; }
  virtual const icChar *GetClassName() const { return "CIccMpeCalculator"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual bool Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE);
  virtual CIccApplyMpe *GetNewApply(CIccApplyTagMpe *pApplyTag);
  virtual void Apply(CIccApplyMpe *pApply, icFloatNumber *pDestPixel, const icFloatNumber *pSrcPixel) const;
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE=NULL, const CIccProfile* pProfile = NULL) const;

  CIccMultiProcessElement *GetElem(icSigCalcOp op, icUInt16Number index);

  virtual bool IsLateBinding() const;
  virtual bool IsLateBindingReflectance() const;

protected:

  bool SetElem(icUInt32Number idx, CIccMultiProcessElement *pElem, icUInt32Number &count, CIccMultiProcessElement ***pArray);

  icUInt32Number m_nTempChannels;
  bool m_bNeedTempReset;

  icUInt32Number m_nSubElem;
  CIccMultiProcessElement **m_SubElem;

  icCalculatorFuncPtr m_calcFunc;

  IIccCmmEnvVarLookup *m_pCmmEnvVarLookup;
};


/**
****************************************************************************
* Class: CIccApplyMpeCalculator
* 
* Purpose: The Calculator process element apply data
*****************************************************************************
*/
class CIccApplyMpeCalculator : public CIccApplyMpe
{
  friend class CIccMpeCalculator;
public:
  virtual ~CIccApplyMpeCalculator();

  virtual icElemTypeSignature GetType() const { return icSigCalculatorElemType; }
  virtual const icChar *GetClassName() const { return "CIccApplyMpeCalculator"; }

  const icFloatNumber *GetInput() { return m_input; }
  icFloatNumber *GetOutput() { return m_output; }
  icFloatNumber *GetTemp() { return m_temp; }

  CIccFloatVector *GetStack() { return m_stack; }

  CIccFloatVector *GetScratch() { return m_scratch; }

  CIccSubCalcApply* GetApply(icUInt16Number index);

  bool GetEnvVar(icSigCmmEnvVar sigEnv, icFloatNumber &val);

protected:
  CIccApplyMpeCalculator(CIccMultiProcessElement *pElem);

  CIccFloatVector *m_stack;
  CIccFloatVector *m_scratch;

  //Use member storage for calls between Apply and ApplySequence
  const icFloatNumber *m_input;
  icFloatNumber *m_output;
  icFloatNumber *m_temp;

  //Apply storage for sub-elements
  icUInt32Number m_nSubElem;
  CIccSubCalcApply **m_SubElem;

  IIccCmmEnvVarLookup *m_pCmmEnvVarLookup;
};

class IIccCalcDebugger
{
public:
  static void SetDebugger(IIccCalcDebugger *pDebugger);

  virtual ~IIccCalcDebugger() {}

  virtual void BeginApply() = 0;
  virtual void EndApply() = 0;

  virtual bool BeforeOp(SIccCalcOp *op, SIccOpState &os, SIccCalcOp *ops)=0;
  virtual bool AfterOp(SIccCalcOp *op, SIccOpState &os, SIccCalcOp *ops)=0;

  virtual void Error(const char *msg) = 0;
};

#define icCalcDebuggerNone    ((IIccCalcDebugger*)0) 
#define icCalcDebuggerConsole ((IIccCalcDebugger*)-1) 

//CIccMPElements support  
#ifdef USEICCDEVNAMESPACE
}
#endif

#endif //_ICCELEMCALC_H
