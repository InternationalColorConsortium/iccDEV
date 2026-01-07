/** @file
    File:       IccMpeCalc.cpp

    Contains:   Implementation of Calculator Element

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * The ICC Software License, Version 0.1
 *
 *
 * Copyright (c) 2003-2006 The International Color Consortium. All rights 
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
// -Initial implementation by Max Derhak 1-30-2006
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#pragma warning( disable: 4786) //disable warning in <list.h>
#endif

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "IccMpeBasic.h"
#include "IccMpeCalc.h"
#include "IccIO.h"
#include <map>
#include <limits>
#include "IccUtil.h"

//#define ICC_VERBOSE_CALC_APPLY 1


class CIccConsoleDebugger : public IIccCalcDebugger
{
public:

  virtual ~CIccConsoleDebugger() {}

  virtual void BeginApply()
  {
    printf("\nBegin Calc Apply\n");
  }

  virtual void EndApply()
  {
    printf("End Calculator Apply\n\n");
    fflush(stdout);
  }

  virtual bool BeforeOp(SIccCalcOp *op, SIccOpState &os, SIccCalcOp * /* ops */)
  {
    if (op->sig == icSigIfOp || op->sig == icSigSelectOp) {
      printf("Start:");
        std::string opDesc;
      op->Describe(opDesc, 100); // TODO - propogate nVerboseness
      printf("%9s\t", opDesc.c_str());
      for (int j = 0; j < (int)os.pStack->size(); j++)
        printf(" %.4f", (*os.pStack)[j]);
      printf("\n");
    }
    return false;
  }

  virtual bool AfterOp(SIccCalcOp *op, SIccOpState &os, SIccCalcOp * /* ops */)
  {
    if (op->sig == icSigDataOp) {
      printf("%9s\t", "data");
    }
    else {
      if (op->sig == icSigIfOp || op->sig == icSigSelectOp) {
        printf("End:");
      }
      std::string opDesc;
      op->Describe(opDesc, 100); // TODO - propogate nVerboseness
      printf("%9s\t", opDesc.c_str());
    }

    for (int j = 0; j < (int)os.pStack->size(); j++)
      printf(" %.4f", (*os.pStack)[j]);
    printf("\n");

    if (op->sig==icSigIfOp || op->sig==icSigSelectOp)
      printf("\n");
    fflush(stdout);

    return false;
  }
  virtual void Error(const char *szMsg)
  {
    printf("%s\n", szMsg);
  }
};

static CIccConsoleDebugger g_ConsoleDebugger;

#ifdef ICC_VERBOSE_CALC_APPLY
static IIccCalcDebugger *g_pDebugger = &g_ConsoleDebugger;
#else
static IIccCalcDebugger *g_pDebugger = icCalcDebuggerNone;
#endif

void IIccCalcDebugger::SetDebugger(IIccCalcDebugger *pDebugger)
{
  if (pDebugger == icCalcDebuggerConsole)
    g_pDebugger = &g_ConsoleDebugger;
  else 
    g_pDebugger = pDebugger;
}

// TODO - get rid of these macros and inline the code to reduce errors
// shadowed variable names are way too risky
// templates may be an option
#define OsPopArg(X) { \
  if (!os.pStack->size()) \
    return false; \
  X = *(os.pStack->rbegin()); \
  os.pStack->pop_back(); \
}

#define OsPopArgs(X, N) { \
  icUInt32Number _nv=(N); \
  size_t _ss = os.pStack->size(); \
  if (_nv>_ss) \
    return false; \
  icFloatNumber *_sv = &(*os.pStack)[_ss-_nv]; \
  memcpy((X), _sv, _nv*sizeof(icFloatNumber)); \
  os.pStack->resize(_ss-_nv); \
}

#define OsPushArg(X) { \
  icFloatNumber _V = (X); \
  os.pStack->push_back(_V); \
}

#define OsPushArgs(X, N) { \
  size_t _ss = os.pStack->size(); \
  icUInt32Number _nv=(N); \
  os.pStack->resize(_ss+_nv); \
  icFloatNumber *_sv = &(*os.pStack)[_ss]; \
  memcpy(_sv, (X), _nv*sizeof(icFloatNumber)); \
}

#define OsShrinkArgs(N) { \
  icUInt32Number nv = (N); \
  size_t _ss = os.pStack->size(); \
  if (nv>_ss) \
    return false; \
  os.pStack->resize(_ss-nv); \
}

#define OsExtendArgs(N) { \
  size_t _ss = os.pStack->size(); \
  os.pStack->resize(_ss+(N)); \
}


class CIccOpDefInvalid : public IIccOpDef
{
public:
  virtual bool IsValid(CIccMpeCalculator * /* pCalc */, SIccCalcOp & /* op */) { return false; }
  virtual bool Exec(SIccCalcOp *op, SIccOpState & /* os */)
  {
    if (g_pDebugger)
    {
      std::string opDesc;
      op->Describe(opDesc, 100); // TODO - propogate nVerboseness
      std::string line = "Unknown operator: ";
      line += opDesc;
      g_pDebugger->Error(line.c_str());
    }
    return false;
  }
};

class CIccOpDefData : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    icUInt32Number n, j;
    if (!op->extra) {
      for (n=1; n+os.idx<os.nOps; n++)
        if (op[n].sig!=icSigDataOp)
          break;
      op->extra = n;
    }
    else {
      n=(icUInt32Number)op->extra;
    }
    size_t ss = os.pStack->size();
    os.pStack->resize(ss+n);
    icFloatNumber *s = &(*os.pStack)[ss];
    for (j=0; j<n; j++) {
      s[j] = op[j].data.num;
    }
    os.idx += n-1;
    return true;
  }
};

class CIccOpDefPop : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    size_t ss = os.pStack->size();
    if (((size_t)op->data.select.v1+1)>ss)
      return false;
    os.pStack->resize(ss-(op->data.select.v1+1));
    return true;
  }
};

class CIccOpDefOutputChan : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    OsPopArgs(&os.output[op->data.select.v1], op->data.select.v2+1);
    return true;
  }
};

class CIccOpDefInputChan : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    OsPushArgs(&os.pixel[op->data.select.v1], op->data.select.v2+1);
    return true;
  }
};

class CIccOpDefTempGetChan : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    OsPushArgs(&os.temp[op->data.select.v1], op->data.select.v2+1);
    return true;
  }
};

class CIccOpDefTempPutChan : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    OsPopArgs(&os.temp[op->data.select.v1], op->data.select.v2+1)
    return true;
  }
};

class CIccOpDefTempSaveChan : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    size_t ss = os.pStack->size();
    size_t n=op->data.select.v2+1;

    if (n>ss)
      return false;

    icFloatNumber *s = &(*os.pStack)[ss - n];
    memcpy(&os.temp[op->data.select.v1], s, n*sizeof(icFloatNumber));
    return true;
  }
};

class CIccOpDefEnvVar : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    icSigCmmEnvVar sig = (icSigCmmEnvVar) op->data.size;
    icFloatNumber val=0.0;

    if (sig==icSigTrueVar) {
      OsPushArg((icFloatNumber)1.0);
      OsPushArg((icFloatNumber)1.0);
    }
    else if (sig==icSigNotDefVar) {
      OsPushArg((icFloatNumber)0.0);
      OsPushArg((icFloatNumber)0.0);
    }
    else if (os.pApply->GetEnvVar(sig, val)) {
      OsPushArg((icFloatNumber)val);
      OsPushArg((icFloatNumber)1.0);
    }
    else {
      OsPushArg((icFloatNumber)0.0);
      OsPushArg((icFloatNumber)0.0);
    }
    return true;
  }
};

class CIccOpDefSubElement : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    CIccSubCalcApply *pElemApply = os.pApply->GetApply(op->data.select.v1);
    if (!pElemApply)
      return false;

    icUInt16Number nSrc = pElemApply->NumInputChannels();
    icUInt16Number nDst = pElemApply->NumOutputChannels();

    size_t ss = os.pStack->size();
    if (nSrc>ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss - nSrc];

    if (os.pScratch->size()<(size_t)nDst)
      os.pScratch->resize(nDst);

    icFloatNumber *d = &(*os.pScratch)[0];

    pElemApply->Apply(d, s);

    size_t ns = (size_t)( ss + (int)nDst - (int)nSrc );

    if (ns != ss)
      os.pStack->resize(ns);

    s = &(*os.pStack)[ns - nDst];
    memcpy(s, d, nDst*sizeof(icFloatNumber));
    return true;
  }
};

class CIccOpDefCopy : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    size_t stackSize = os.pStack->size();
    int j;
    int n=op->data.select.v1+1;
    int t=op->data.select.v2+1;
    if (n>(int)stackSize)
      return false;

    if (n && t) {
      OsExtendArgs(n*t);

      icFloatNumber *to = &(*os.pStack)[stackSize];
      icFloatNumber *from = to-n;

      for (j=0; j<t; j++) {
        memcpy(to, from, n*sizeof(icFloatNumber));
        to+=n;
      }
    }
    return true;
  }
};

class CIccOpDefPositionDup : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    size_t stackSize = os.pStack->size();
    int j, n=op->data.select.v1+1;
    int t=op->data.select.v2+1;
    if (n>(int)stackSize)
      return false;

    if (n && t) {
      OsExtendArgs(t);

      icFloatNumber *to = &(*os.pStack)[stackSize];
      icFloatNumber *from = to-n;

      for (j=0; j<t; j++) {
        to[j] = *from;
      }
    }
    return true;
  }
};

class CIccOpDefFlip : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int n = op->data.select.v1+2;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    icFloatNumber t;
    int j, k;
    for (j=0, k=n-1; j<k; j++, k--) {
      t = s[j];
      s[j] = s[k];
      s[k] = t;
    }
    return true;
  }
};

class CIccOpDefRotateLeft : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int nCopy = op->data.select.v1+1;
    int nPos = op->data.select.v2+1;

    if (nCopy) {
      int next = nPos;
      if (os.pScratch->size()<(size_t)nCopy)
        os.pScratch->resize(nCopy);

      icFloatNumber *copyList = &(*os.pScratch)[0];

      OsPopArgs(copyList, nCopy);

      int j;
      for (j=0; j<nCopy; j++) {
        OsPushArg(copyList[(j+next)%nCopy]);
      }
    }
    return true;
  }
};

class CIccOpDefRotateRight : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int nCopy = op->data.select.v1+1;
    int nPos = (op->data.select.v2+1)%nCopy;

    if (nCopy) {
      int next = nCopy - nPos;
      if (os.pScratch->size()<(size_t)nCopy)
        os.pScratch->resize(nCopy);

      icFloatNumber *copyList = &(*os.pScratch)[0];

      OsPopArgs(copyList, nCopy);

      int j;
      for (j=0; j<nCopy; j++) {
        OsPushArg(copyList[(j+next)%nCopy]);
      }
    }
    return true;
  }
};

class CIccOpDefTranspose : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    size_t ss = os.pStack->size();
    int r=op->data.select.v1+1;
    int c=op->data.select.v2+1;
    int nSize = r*c;

    if (nSize>(int)ss)
      return false;

    if (r>1 && c>1) {
      int j, k;

      if (os.pScratch->size()<(size_t)nSize)
        os.pScratch->resize(nSize);

      icFloatNumber *ptrStack = &(*os.pStack)[ss-nSize];
      icFloatNumber *ptrScratch = &(*os.pScratch)[0];
      icFloatNumber *start = ptrStack;
      icFloatNumber *to = ptrScratch;

      for (k=0; k<c; k++) {
        icFloatNumber *from = start;
        for (j=0; j<r; j++) {
          *to++ = *from;
          from += c;
        }
        start++;
      }
      memcpy(ptrStack, ptrScratch, nSize*sizeof(icFloatNumber));
    }
    return true;
  }
};

class CIccOpDefSolve : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    size_t ss = os.pStack->size();
    int r=op->data.select.v1+1;
    int c=op->data.select.v2+1;
    int nMSize = r*c;
    int nSize = nMSize + c;

    if (nSize>(int)ss)
      return false;

    if (r>1 && c>1) {

      if (os.pScratch->size()<(size_t)c)
        os.pScratch->resize(c);
      
      icFloatNumber *ptrStack = &(*os.pStack)[ss-nSize];
      icFloatNumber *x = &(*os.pScratch)[0];
      icFloatNumber *mtx = ptrStack;
      icFloatNumber *y = &ptrStack[nMSize];

      bool bResult = false;

      if (g_pIccMatrixSolver) {
        bResult = g_pIccMatrixSolver->Solve(x, mtx, y, r, c);
      }
      if (bResult) {
        memcpy(ptrStack, x, c*sizeof(icFloatNumber));
        ptrStack[c] = 1.0;
      }
      else {
        memset(ptrStack, 0, c*sizeof(icFloatNumber));
        ptrStack[c] = 0.0;
      }
      os.pStack->resize(ss-nSize+r+1);
    }
    return true;
  }
};

class CIccOpDefPi : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp * /* op */, SIccOpState &os)
  {
    OsPushArg((icFloatNumber)icPiNum);
    return true;
  }
};

class CIccOpDefPosInfinity : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp * /* op */, SIccOpState &os)
  {
    OsPushArg((icFloatNumber)icPosInfinity);
    return true;
  }
};

class CIccOpDefNegInfinity : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp * /* op */, SIccOpState &os)
  {
    OsPushArg((icFloatNumber)icNegInfinity);
    return true;
  }
};

class CIccOpDefNotANumber : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp * /* op */, SIccOpState &os)
  {
    OsPushArg((icFloatNumber)icNotANumber);
    return true;
  }
};

class CIccOpDefSum : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+2;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    if (n==2) {
      s[0] += s[1];
      OsShrinkArgs(1);
    }
    else {
      for (j=1; j<n; j++) {
        s[0] += s[j];
      }
      OsShrinkArgs(n-1);
    }
    return true;
  }
};

class CIccOpDefProduct : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+2;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    if (n==2) {
      s[0] *= s[1];
      OsShrinkArgs(1);
    }
    else {
      for (j=1; j<n; j++) {
        s[0] *= s[j];
      }
      OsShrinkArgs(n-1);
    }
    return true;
  }
};

class CIccOpDefMinimum : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+2;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    if (n==2) {
      s[0] = icMin(s[0], s[1]);
      OsShrinkArgs(1);
    }
    else {
      icFloatNumber mv = s[0];
      for (j=1; j<n; j++) {
        if (s[j]<mv)
          mv = s[j];
      }
      s[0] = mv;
      OsShrinkArgs(n-1);
    }
    return true;
  }
};

class CIccOpDefMaximum : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+2;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    if (n==2) {
      s[0] = icMax(s[0], s[1]);
      OsShrinkArgs(1);
    }
    else {
      icFloatNumber mv = s[0];
      for (j=1; j<n; j++) {
        if (s[j]>mv)
          mv = s[j];
      }
      s[0] = mv;
      OsShrinkArgs(n-1);
    }
    return true;
  }
};

class CIccOpDefAnd : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+2;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];

    for (j=0; j<n; j++) {
      if (s[j]<0.5f)
        break;
    }
    s[0] = j<n ? 0.0f : 1.0f;

    OsShrinkArgs(n-1);
    return true;
  }
};

class CIccOpDefOr : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j,n = op->data.select.v1+2;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];

    for (j=0; j<n; j++) {
      if (s[j]>=0.5f)
        break;
    }
    s[0] = j<n ? 1.0f : 0.0f;

    OsShrinkArgs(n-1);
    return true;
  }
};

class CIccOpDefVectorMinimum : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] = icMin(s[j], s[j+n]);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefVectorMaximum : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] = icMax(s[j], s[j+n]);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefVectorAnd : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] = (s[j]>=0.5f && s[j+n]>=0.5) ? 1.0f : 0.0f;
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefVectorOr : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] = (s[j]>=0.5f || s[j+n]>=0.5) ? 1.0f : 0.0f;
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefAdd : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] += s[j+n];
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefSubtract : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] -= s[j+n];
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefMultiply : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] *= s[j+n];
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefDivide : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] /= s[j+n];
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefModulus : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    const icFloatNumber epsilon = 1e-12;        // value chosen somewhat arbitrarily to allow division to work and not overflow
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      icFloatNumber temp = s[j];
      icFloatNumber tempN = s[j+n];
      if (isnan(temp) || isinf(temp)
        || isnan(tempN) || isinf(tempN) || fabs(tempN) < epsilon)
        s[j] = 0.0;
      else
        s[j] = temp - (icFloatNumber)((int)(temp / tempN))*tempN;
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefGamma : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n+1;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    icFloatNumber p = s[n];
    for (j=0; j<n; j++) {
      s[j] = (icFloatNumber)pow(s[j], p);
    }
    OsShrinkArgs(1);
    return true;
  }
};

class CIccOpDefScalarAdd : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n+1;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    icFloatNumber p = s[n];
    for (j=0; j<n; j++) {
      s[j] = s[j] + p;
    }
    OsShrinkArgs(1);
    return true;
  }
};

class CIccOpDefScalarSubtract : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n+1;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    icFloatNumber p = s[n];
    for (j=0; j<n; j++) {
      s[j] = s[j] - p;
    }
    OsShrinkArgs(1);
    return true;
  }
};

class CIccOpDefScalarMultiply : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n+1;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    icFloatNumber p = s[n];
    for (j=0; j<n; j++) {
      s[j] = s[j] * p;
    }
    OsShrinkArgs(1);
    return true;
  }
};

class CIccOpDefScalarDivide : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n+1;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    icFloatNumber p = s[n];
    for (j=0; j<n; j++) {
      s[j] = s[j] / p;
    }
    OsShrinkArgs(1);
    return true;
  }
};

class CIccOpDefPow : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      s[j] = (icFloatNumber)pow(s[j], s[j+n]);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefSquare : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = a1*a1;
    }
    return true;
  }
};

class CIccOpDefSquareRoot : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)sqrt(a1);
    }
    return true;
  }
};

class CIccOpDefCube : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = a1*a1*a1;
    }
    return true;
  }
};

class CIccOpDefCubeRoot : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)ICC_CBRTF(a1);
    }
    return true;
  }
};

class CIccOpDefSign : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1 + 1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n > (int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss - n];
    for (j = 0; j < n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)(a1 < 0 ? -1 : (a1 > 0 ? 1 : 0));
    }
    return true;
  }
};

class CIccOpDefAbsoluteVal : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (a1 < 0 ? -a1 : a1);
    }
    return true;
  }
};

class CIccOpDefTruncate : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      //Casting to an int results in truncation
      icFloatNumber temp = s[j];
      if (isnan(temp))
        s[j] = 0.0;
      else if (isinf(temp)) {
        if (temp > 0.0)
            s[j] = (icFloatNumber)std::numeric_limits<int>::max();
        else
            s[j] = (icFloatNumber)std::numeric_limits<int>::lowest();
      }
      else
        s[j] = (icFloatNumber)((int)temp);
    }
    return true;
  }
};

class CIccOpDefFloor : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      s[j] = (icFloatNumber)floor(s[j]);
    }
    return true;
  }
};

class CIccOpDefCeiling : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      s[j] = (icFloatNumber)ceil(s[j]);
    }
    return true;
  }
};

class CIccOpDefRound : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      icFloatNumber temp = s[j];
      if (isnan(temp))
        temp = 0.0;
      else if (isinf(temp))
        temp = 10000.0;          // value chosen arbitrarily to not overflow int
      if (temp < 0.0)
        s[j] = icFloatNumber((int)(temp-0.5));
      else
        s[j] = icFloatNumber((int)(temp+0.5));
    }
    return true;
  }
};

class CIccOpDefExp : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)exp(a1);
    }
    return true;
  }
};

class CIccOpDefLogrithm : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)log10(a1);
    }
    return true;
  }
};

class CIccOpDefNaturalLog : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)log(a1);
    }
    return true;
  }
};

class CIccOpDefSine : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)sin(a1);
    }
    return true;
  }
};

class CIccOpDefCosine : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)cos(a1);
    }
    return true;
  }
};

class CIccOpDefTangent : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)tan(a1);
    }
    return true;
  }
};

class CIccOpDefArcSine : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)asin(a1);
    }
    return true;
  }
};

class CIccOpDefArcCosine : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)acos(a1);
    }
    return true;
  }
};

class CIccOpDefArcTangent : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (icFloatNumber)atan(a1);
    }
    return true;
  }
};

class CIccOpDefArcTan2 : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      s[j] = (icFloatNumber)atan2(a2, a1);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefCartesianToPolar : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
        int j, n = op->data.select.v1+1;
        icFloatNumber a1, a2;
        int tn = n*2;
        size_t ss = os.pStack->size();
        if (tn>(int)ss)
          return false;
        icFloatNumber *s = &(*os.pStack)[ss-tn];
        for (j=0; j<n; j++) {
          a1 = s[j];
          a2 = s[j+n];
          s[j] = (icFloatNumber)sqrt(a2*a2 + a1*a1);
          icFloatNumber h = (icFloatNumber)atan2(a2, a1) * 180.0f / (icFloatNumber)icPiNum;
          if (h<0.0f)
            h += 360.0f;
          s[j+n] = h;
        }
        return true;
  }
};

class CIccOpDefPolarToCartesian : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n] * (icFloatNumber)icPiNum / 180.0f;

      s[j] = a1 * (icFloatNumber)cos(a2);
      s[j+n] = a1 * (icFloatNumber)sin(a2);
    }
    return true;
  }
};

class CIccOpDefLessThan : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n =op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      s[j] = (a1 < a2 ? (icFloatNumber)1.0 : (icFloatNumber)0.0);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefRealNumber : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    icFloatNumber nan = icNotANumber;
    for (j=0; j<n; j++) {
      a1 = s[j];
      if (a1==icPosInfinity || a1==icNegInfinity || !memcmp(&a1,&nan, sizeof(nan)))
        s[j] = 0.0;
      else
        s[j] = 1.0;
    }
    return true;
  }
};

class CIccOpDefLessThanEqual : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      s[j] = (a1 <= a2 ? (icFloatNumber)1.0 : (icFloatNumber)0.0);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefEqual : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber nan = icNotANumber;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      s[j] = (a1 == a2 ? 
               (icFloatNumber)1.0 : 
               ((!memcmp(&a1, &nan, sizeof(icFloatNumber)) && !memcmp(&a1, &a2, sizeof(a1))) ? 
                 (icFloatNumber)1.0 : 
                 (icFloatNumber)0.0));
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefNotEqual : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1 + 1;
    icFloatNumber a1, a2;
    int tn = n * 2;
    size_t ss = os.pStack->size();
    if (tn > (int)ss)
      return false;
    icFloatNumber nan = icNotANumber;
    icFloatNumber *s = &(*os.pStack)[ss - tn];
    for (j = 0; j < n; j++) {
      a1 = s[j];
      a2 = s[j + n];
      s[j] = (a1 == a2 ?
        (icFloatNumber)0.0 :
        ((!memcmp(&a1, &nan, sizeof(icFloatNumber)) && !memcmp(&a1, &a2, sizeof(a1))) ?
        (icFloatNumber)0.0 :
          (icFloatNumber)1.0));
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefNear : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      s[j] = (fabs(a1-a2)<1.0e-5 ? (icFloatNumber)1.0 : (icFloatNumber)0.0);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefGreaterThanEqual : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      s[j] = (a1 >= a2 ? (icFloatNumber)1.0 : (icFloatNumber)0.0);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefGreaterThan : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1, a2;
    int tn = n*2;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      s[j] = (a1 > a2 ? (icFloatNumber)1.0 : (icFloatNumber)0.0);
    }
    OsShrinkArgs(n);
    return true;
  }
};

class CIccOpDefNot : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = (a1 >= (icFloatNumber)0.5 ? (icFloatNumber)0.0 : (icFloatNumber)1.0);
    }
    return true;
  }
};

class CIccOpDefNeg : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    icFloatNumber a1;
    size_t ss = os.pStack->size();
    if (n>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-n];
    for (j=0; j<n; j++) {
      a1 = s[j];
      s[j] = -a1;
    }
    return true;
  }
};

class CIccOpDefToLab : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int n2 = n+n;
    int tn = n*3;
    icFloatNumber a1, a2, a3;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = icCubeth(s[j]);
      a2 = icCubeth(s[j+n]);
      a3 = icCubeth(s[j+n2]);

      s[j] = (icFloatNumber)(116.0 * a2 - 16.0);
      s[j+n] = (icFloatNumber)(500.0 * (a1 - a2));
      s[j+n2] = (icFloatNumber)(200.0 * (a2 - a3));
    }
    return true;
  }
};

class CIccOpDefFromLab : public IIccOpDef
{
public:
  virtual bool Exec(SIccCalcOp *op, SIccOpState &os)
  {
    int j, n = op->data.select.v1+1;
    int n2 = n+n;
    int tn = n*3;
    icFloatNumber a1, a2, a3;
    size_t ss = os.pStack->size();
    if (tn>(int)ss)
      return false;
    icFloatNumber fy, *s = &(*os.pStack)[ss-tn];
    for (j=0; j<n; j++) {
      a1 = s[j];
      a2 = s[j+n];
      a3 = s[j+n2];

      fy = (icFloatNumber)((a1 + 16.0f) / 116.0f);

      s[j] = icICubeth((icFloatNumber)(a2/500.0 + fy));
      s[j+n] = icICubeth(fy);
      s[j+n2] = icICubeth((icFloatNumber)(fy - a3/200.0));
    }
    return true;
  }
};



/**
******************************************************************************
* Name: SIccCalcOp::Describe
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
void SIccCalcOp::Describe(std::string &desc, int /* nVerboseness */)
{
  const size_t bufSize = 300;
  char buf[bufSize];
  if (sig==icSigDataOp) {
    snprintf(buf, bufSize, "%.8g", data.num);
    desc = buf;
    return;
  }

  char name[10];
  int i;

  icGetSig(name, 10, sig, false);
  name[5]=0;
  for (i=4; i>0; i--)
    if (name[i]==' ')
      name[i] = 0;
  if (!name[1])
    name[1] = '?';
  desc = &name[1];

  switch (sig) {
    case icSigInputChanOp:        
    case icSigOutputChanOp:        
    case icSigTempGetChanOp:  
    case icSigTempPutChanOp:
    case icSigTempSaveChanOp:
      if (!data.select.v2)
        snprintf(buf, bufSize, "[%d]", data.select.v1);
      else
        snprintf(buf, bufSize, "[%d,%d]", data.select.v1, data.select.v2+1);
      desc += buf;
      break;

    case icSigEnvVarOp:
      {
        char varName[10];
        icGetSigStr(varName, 10, (icSignature)data.size);
        int l=(int)strlen(varName);
        if (l==9) { //Remove h at end
          varName[8]=0;
        }
        else {
          for (l--; l>0; l--) {
            if (varName[l]!=' ')
              break;
            varName[l]=0;
          }
        }
        snprintf(buf, bufSize, "(%s)", varName);
        desc += buf;
      }
      break;
    
    case icSigApplyCurvesOp:
    case icSigApplyMatrixOp:
    case icSigApplyCLutOp:
    case icSigApplyTintOp:
    case icSigApplyToJabOp:
    case icSigApplyFromJabOp:
    case icSigApplyCalcOp:
    case icSigApplyElemOp:
      snprintf(buf, bufSize, "(%d)", data.select.v1);
      desc += buf;
      break;

    case icSigPopOp:
      snprintf(buf, bufSize, "(%d)", data.select.v1+1);
      desc += buf;
      break;

    case icSigSolveOp:
    case icSigTransposeOp:
      snprintf(buf, bufSize, "(%d,%d)", data.select.v1+1, data.select.v2+1);
      desc += buf;
      break;

    case icSigRotateLeftOp:       
    case icSigRotateRightOp:
      snprintf(buf, bufSize, "(%d,%d)", data.select.v1, data.select.v2);
      break;

    case icSigCopyOp:             
    case icSigPositionDupOp:
      if (!data.select.v2) {
        if (data.select.v1) {
          snprintf(buf, bufSize, "(%d)", data.select.v1+1);
          desc += buf;
        }
      }
      else {
        snprintf(buf, bufSize, "(%d,%d)", data.select.v1+1, data.select.v2+1);
        desc += buf;
      }
      break;

    case icSigFlipOp:
    case icSigSumOp:
    case icSigProductOp:
    case icSigMinimumOp:
    case icSigMaximumOp:
    case icSigAndOp:
    case icSigOrOp:
      if (data.select.v1) {
        snprintf(buf, bufSize, "(%d)", data.select.v1+2);
        desc += buf;
      }
      break;

    case icSigGammaOp:
    case icSigScalarAddOp:
    case icSigScalarSubtractOp:
    case icSigScalarMultiplyOp:
    case icSigScalarDivideOp:
    case icSigAddOp:
    case icSigSubtractOp:
    case icSigMultiplyOp:
    case icSigDivideOp:
    case icSigModulusOp:
    case icSigPowOp:
    case icSigArcTan2Op:          
    case icSigLessThanOp:         
    case icSigLessThanEqualOp:    
    case icSigEqualOp:
    case icSigNotEqualOp:
    case icSigNearOp:
    case icSigGreaterThanEqualOp: 
    case icSigGreaterThanOp:
    case icSigSquareOp:       
    case icSigSquareRootOp:       
    case icSigCubeOp:       
    case icSigCubeRootOp:
    case icSigSignOp:
    case icSigAbsoluteValOp:
    case icSigTruncateOp:
    case icSigFloorOp:
    case icSigCeilingOp:
    case icSigRoundOp:
    case icSigRealNumberOp:
    case icSigNegOp:      
    case icSigExpOp:              
    case icSigLogrithmOp:         
    case icSigNaturalLogOp:       
    case icSigSineOp:             
    case icSigCosineOp:             
    case icSigTangentOp:
    case icSigArcSineOp:          
    case icSigArcCosineOp:        
    case icSigArcTangentOp:      
    case icSigCartesianToPolarOp:
    case icSigPolarToCartesianOp:
    case icSigNotOp:
    case icSigToLabOp:
    case icSigToXYZOp:
    case icSigVectorMinimumOp:
    case icSigVectorMaximumOp:
    case icSigVectorAndOp:
    case icSigVectorOrOp:
      if (data.select.v1) {
        snprintf(buf, bufSize, "[%d]", data.select.v1+1);
        desc += buf;
      }
      break;
    
    default:
      break;
  }
}

typedef std::map<icSigCalcOp, IIccOpDef*> icCalcOpMap;

class CIccCalcOpMgr
{
  CIccCalcOpMgr();
  static CIccCalcOpMgr* m_inst;
public:
  static CIccCalcOpMgr* GetInstance();
  static IIccOpDef *getOpDef(icSigCalcOp opSig);

protected:
  icCalcOpMap m_map;
  CIccOpDefInvalid m_invalid;
};
CIccCalcOpMgr *CIccCalcOpMgr::m_inst = NULL;

CIccCalcOpMgr::CIccCalcOpMgr()
{
  m_map[icSigDataOp] = new CIccOpDefData();            
  m_map[icSigInputChanOp] = new CIccOpDefInputChan();        
  m_map[icSigOutputChanOp] = new CIccOpDefOutputChan(); 
  m_map[icSigTempGetChanOp] = new CIccOpDefTempGetChan();
  m_map[icSigTempPutChanOp] = new CIccOpDefTempPutChan();
  m_map[icSigTempSaveChanOp] = new CIccOpDefTempSaveChan();
  m_map[icSigEnvVarOp] = new CIccOpDefEnvVar();
  m_map[icSigApplyCurvesOp] = new CIccOpDefSubElement();
  m_map[icSigApplyMatrixOp] = new CIccOpDefSubElement();
  m_map[icSigApplyCLutOp] = new CIccOpDefSubElement();
  m_map[icSigApplyTintOp] = new CIccOpDefSubElement();
  m_map[icSigApplyToJabOp] = new CIccOpDefSubElement();
  m_map[icSigApplyFromJabOp] = new CIccOpDefSubElement();
  m_map[icSigApplyCalcOp] = new CIccOpDefSubElement();
  m_map[icSigApplyElemOp] = new CIccOpDefSubElement();
  m_map[icSigCopyOp] = new CIccOpDefCopy();             
  m_map[icSigRotateLeftOp] = new CIccOpDefRotateLeft();       
  m_map[icSigRotateRightOp] = new CIccOpDefRotateRight();
  m_map[icSigPositionDupOp] = new CIccOpDefPositionDup();
  m_map[icSigFlipOp] = new CIccOpDefFlip();
  m_map[icSigPopOp] = new CIccOpDefPop();
  m_map[icSigSolveOp] = new CIccOpDefSolve();
  m_map[icSigTransposeOp] = new CIccOpDefTranspose();
  m_map[icSigPiOp] = new CIccOpDefPi(); 
  m_map[icSigPosInfinityOp] = new CIccOpDefPosInfinity();
  m_map[icSigNegInfinityOp] = new CIccOpDefNegInfinity();
  m_map[icSigNotaNumberOp] = new CIccOpDefNotANumber(); 
  m_map[icSigSumOp] = new CIccOpDefSum();
  m_map[icSigProductOp] = new CIccOpDefProduct();
  m_map[icSigAddOp] = new CIccOpDefAdd();
  m_map[icSigSubtractOp] = new CIccOpDefSubtract();         
  m_map[icSigMultiplyOp] = new CIccOpDefMultiply();         
  m_map[icSigDivideOp] = new CIccOpDefDivide();           
  m_map[icSigModulusOp] = new CIccOpDefModulus();           
  m_map[icSigPowOp] = new CIccOpDefPow();
  m_map[icSigGammaOp] = new CIccOpDefGamma();
  m_map[icSigScalarAddOp] = new CIccOpDefScalarAdd();
  m_map[icSigScalarSubtractOp] = new CIccOpDefScalarSubtract();
  m_map[icSigScalarMultiplyOp] = new CIccOpDefScalarMultiply();
  m_map[icSigScalarDivideOp] = new CIccOpDefScalarDivide();
  m_map[icSigSquareOp] = new CIccOpDefSquare();       
  m_map[icSigSquareRootOp] = new CIccOpDefSquareRoot();       
  m_map[icSigCubeOp] = new CIccOpDefCube();       
  m_map[icSigCubeRootOp] = new CIccOpDefCubeRoot();       
  m_map[icSigSignOp] = new CIccOpDefSign();
  m_map[icSigAbsoluteValOp] = new CIccOpDefAbsoluteVal();
  m_map[icSigTruncateOp] = new CIccOpDefTruncate();      
  m_map[icSigFloorOp] = new CIccOpDefFloor();      
  m_map[icSigCeilingOp] = new CIccOpDefCeiling();      
  m_map[icSigRoundOp] = new CIccOpDefRound();      
  m_map[icSigNegOp] = new CIccOpDefNeg();
  m_map[icSigExpOp] = new CIccOpDefExp();              
  m_map[icSigLogrithmOp] = new CIccOpDefLogrithm();         
  m_map[icSigNaturalLogOp] = new CIccOpDefNaturalLog();       
  m_map[icSigSineOp] = new CIccOpDefSine();             
  m_map[icSigCosineOp] = new CIccOpDefCosine();             
  m_map[icSigTangentOp] = new CIccOpDefTangent();          
  m_map[icSigArcSineOp] = new CIccOpDefArcSine();          
  m_map[icSigArcCosineOp] = new CIccOpDefArcCosine();        
  m_map[icSigArcTangentOp] = new CIccOpDefArcTangent();      
  m_map[icSigArcTan2Op] = new CIccOpDefArcTan2();
  m_map[icSigCartesianToPolarOp] = new CIccOpDefCartesianToPolar();
  m_map[icSigPolarToCartesianOp] = new CIccOpDefPolarToCartesian();
  m_map[icSigVectorMinimumOp] = new CIccOpDefVectorMinimum();
  m_map[icSigVectorMaximumOp] = new CIccOpDefVectorMaximum();
  m_map[icSigVectorAndOp] = new CIccOpDefVectorAnd();
  m_map[icSigVectorOrOp] = new CIccOpDefVectorOr();
  m_map[icSigMinimumOp] = new CIccOpDefMinimum();          
  m_map[icSigMaximumOp] = new CIccOpDefMaximum();          
  m_map[icSigRealNumberOp] = new CIccOpDefRealNumber();
  m_map[icSigLessThanOp] = new CIccOpDefLessThan();         
  m_map[icSigLessThanEqualOp] = new CIccOpDefLessThanEqual();    
  m_map[icSigEqualOp] = new CIccOpDefEqual();
  m_map[icSigNotEqualOp] = new CIccOpDefNotEqual();
  m_map[icSigNearOp] = new CIccOpDefNear();
  m_map[icSigGreaterThanEqualOp] = new CIccOpDefGreaterThanEqual(); 
  m_map[icSigGreaterThanOp] = new CIccOpDefGreaterThan();
  m_map[icSigAndOp] = new CIccOpDefAnd();
  m_map[icSigOrOp] = new CIccOpDefOr();
  m_map[icSigNotOp] = new CIccOpDefNot();
  m_map[icSigToLabOp] = new CIccOpDefToLab();
  m_map[icSigToXYZOp] = new CIccOpDefFromLab();
}

CIccCalcOpMgr *CIccCalcOpMgr::GetInstance()
{
  if (!m_inst)
    m_inst = new CIccCalcOpMgr();
  return m_inst;
}


/**
******************************************************************************
* Name: SIccCalcOp::IsValidOp
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool SIccCalcOp::IsValidOp(icSigCalcOp sig)
{
  switch (sig) {
    case icSigDataOp:          
    case icSigInputChanOp:        
    case icSigOutputChanOp: 
    case icSigTempGetChanOp:
    case icSigTempPutChanOp:
    case icSigTempSaveChanOp:
    case icSigEnvVarOp:
    case icSigApplyCurvesOp:
    case icSigApplyMatrixOp:
    case icSigApplyCLutOp:
    case icSigApplyTintOp:
    case icSigApplyToJabOp:
    case icSigApplyFromJabOp:
    case icSigApplyCalcOp:
    case icSigApplyElemOp:
    case icSigCopyOp:             
    case icSigRotateLeftOp:       
    case icSigRotateRightOp:
    case icSigPositionDupOp:
    case icSigFlipOp:
    case icSigPopOp:
    case icSigSolveOp:
    case icSigTransposeOp:
    case icSigPiOp: 
    case icSigPosInfinityOp:
    case icSigNegInfinityOp:
    case icSigNotaNumberOp:
    case icSigSumOp:
    case icSigProductOp:
    case icSigAddOp:
    case icSigSubtractOp:         
    case icSigMultiplyOp:         
    case icSigDivideOp:
    case icSigModulusOp:
    case icSigPowOp:
    case icSigGammaOp:
    case icSigScalarAddOp:
    case icSigScalarSubtractOp:
    case icSigScalarMultiplyOp:
    case icSigScalarDivideOp:
    case icSigSquareOp:       
    case icSigSquareRootOp:       
    case icSigCubeOp:       
    case icSigCubeRootOp:
    case icSigSignOp:
    case icSigAbsoluteValOp:
    case icSigTruncateOp:
    case icSigFloorOp:
    case icSigCeilingOp:
    case icSigRoundOp:
    case icSigNegOp:
    case icSigExpOp:              
    case icSigLogrithmOp:         
    case icSigNaturalLogOp:       
    case icSigSineOp:             
    case icSigCosineOp:             
    case icSigTangentOp:          
    case icSigArcSineOp:          
    case icSigArcCosineOp:        
    case icSigArcTangentOp:      
    case icSigArcTan2Op:
    case icSigCartesianToPolarOp:
    case icSigPolarToCartesianOp:
    case icSigVectorMinimumOp:
    case icSigVectorMaximumOp:
    case icSigVectorAndOp:
    case icSigVectorOrOp:
    case icSigMinimumOp:          
    case icSigMaximumOp:          
    case icSigRealNumberOp:
    case icSigLessThanOp:         
    case icSigLessThanEqualOp:    
    case icSigEqualOp:
    case icSigNotEqualOp:
    case icSigNearOp:
    case icSigGreaterThanEqualOp: 
    case icSigGreaterThanOp:
    case icSigAndOp:
    case icSigOrOp:
    case icSigNotOp:
    case icSigToLabOp:
    case icSigToXYZOp:
    case icSigIfOp:
    case icSigElseOp:
    case icSigSelectOp:
    case icSigCaseOp:
    case icSigDefaultOp:
      return true;

    default:
      return false;
  }
}

IIccOpDef *CIccCalcOpMgr::getOpDef(icSigCalcOp opSig)
{
  icCalcOpMap::iterator pos;
  CIccCalcOpMgr *inst = GetInstance();
  pos = inst->m_map.find(opSig);
  if (pos==inst->m_map.end())
    return &inst->m_invalid;
  return pos->second;
}



/******************************************************************************
* Name: SIccCalcOp::IsValidOp
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool SIccCalcOp::IsValidOp(CIccMpeCalculator *pCalc)
{
  switch(sig) {
    case icSigApplyCurvesOp:
    case icSigApplyMatrixOp:
    case icSigApplyCLutOp:
    case icSigApplyTintOp:
    case icSigApplyToJabOp:
    case icSigApplyFromJabOp:
    case icSigApplyCalcOp:
    case icSigApplyElemOp:
      {
        CIccMultiProcessElement *pMpe = pCalc->GetElem(sig, data.select.v1);
        return pMpe != NULL;
      }

    default:
      return IsValidOp(sig);
  }
}

/**
******************************************************************************
* Name: SIccCalcOp::ArgsUsed
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icUInt16Number SIccCalcOp::ArgsUsed(CIccMpeCalculator *pCalc)
{
  // make sure our max value fits in our result type
  static_assert( icMaxDataStackSize <= std::numeric_limits<icUInt16Number>::max() );
  
  switch (sig) {
    case icSigDataOp:
    case icSigPiOp:
    case icSigPosInfinityOp:
    case icSigNegInfinityOp:
    case icSigNotaNumberOp:
    case icSigInputChanOp:        
    case icSigTempGetChanOp:
    case icSigEnvVarOp:
      return 0;

    case icSigApplyCurvesOp:
    case icSigApplyMatrixOp:
    case icSigApplyCLutOp:
    case icSigApplyTintOp:
    case icSigApplyToJabOp:
    case icSigApplyFromJabOp:
    case icSigApplyCalcOp:
    case icSigApplyElemOp:
      {
        CIccMultiProcessElement *pMpe = pCalc->GetElem(sig, data.select.v1);
        return (pMpe ? pMpe->NumInputChannels() : 0);
      }

    case icSigOutputChanOp:
    case icSigTempPutChanOp:
    case icSigTempSaveChanOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v2+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigCopyOp:
    case icSigRotateLeftOp:       
    case icSigRotateRightOp:
    case icSigPositionDupOp:
    case icSigPopOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigSolveOp:
      {
      uint64_t fullResult = ((uint64_t)data.select.v1+1)
                            *((uint64_t)data.select.v2+1)
                            + ((uint64_t)data.select.v1+1);
      if (fullResult > icMaxDataStackSize)
        fullResult = icMaxDataStackSize;
      return (icUInt16Number)fullResult;
      }

    case icSigTransposeOp:
      {
      uint64_t fullResult = ((uint64_t)data.select.v1+1)
                            *((uint64_t)data.select.v2+1);
      if (fullResult > icMaxDataStackSize)
        fullResult = icMaxDataStackSize;
      return (icUInt16Number)fullResult;
      }

    case icSigFlipOp:
    case icSigSumOp:              
    case icSigProductOp:         
    case icSigMinimumOp:          
    case icSigMaximumOp:          
    case icSigAndOp:
    case icSigOrOp:
    case icSigGammaOp:
    case icSigScalarAddOp:
    case icSigScalarSubtractOp:
    case icSigScalarMultiplyOp:
    case icSigScalarDivideOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v1+2);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigAddOp:
    case icSigSubtractOp:
    case icSigMultiplyOp:
    case icSigDivideOp:
    case icSigModulusOp:
    case icSigPowOp:
    case icSigArcTan2Op:          
    case icSigLessThanOp:         
    case icSigLessThanEqualOp:    
    case icSigEqualOp:
    case icSigNotEqualOp:
    case icSigNearOp:
    case icSigGreaterThanEqualOp: 
    case icSigGreaterThanOp:
    case icSigCartesianToPolarOp:
    case icSigPolarToCartesianOp:
    case icSigVectorMinimumOp:
    case icSigVectorMaximumOp:
    case icSigVectorAndOp:
    case icSigVectorOrOp:
      {
      uint32_t fullSum = 2*((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigSquareOp:       
    case icSigSquareRootOp:       
    case icSigCubeOp:       
    case icSigCubeRootOp:
    case icSigSignOp:
    case icSigAbsoluteValOp:
    case icSigTruncateOp:
    case icSigFloorOp:
    case icSigCeilingOp:
    case icSigRoundOp:
    case icSigRealNumberOp:
    case icSigNegOp:      
    case icSigExpOp:              
    case icSigLogrithmOp:         
    case icSigNaturalLogOp:       
    case icSigSineOp:             
    case icSigCosineOp:             
    case icSigTangentOp:          
    case icSigArcSineOp:          
    case icSigArcCosineOp:        
    case icSigArcTangentOp:      
    case icSigNotOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }
    
    case icSigToLabOp:
    case icSigToXYZOp:
      {
      uint32_t fullSum = 3*((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigIfOp:
    case icSigSelectOp:
      return 1;

    default:
      return 0;
  }
}


/**
******************************************************************************
* Name: SIccCalcOp::ArgsPushed
* 
* Purpose: Checking for stack over/underflow
*
* Args: calculator object pointer
*
* Return: number of stack spaces pushed OR icMaxDataStackSize if too large
******************************************************************************/
icUInt16Number SIccCalcOp::ArgsPushed(CIccMpeCalculator *pCalc)
{
  // make sure our max value fits in our result type
  static_assert( icMaxDataStackSize <= std::numeric_limits<icUInt16Number>::max() );

  switch (sig) {
    case icSigOutputChanOp:
    case icSigTempPutChanOp:

    case icSigPopOp:
    case icSigIfOp:
    case icSigSelectOp:
      /*case icSigElseOp:*/     //This is only valid after an icSigIfOp is parsed
      /*case icSigCaseOp:*/     //This is only valid after an icSigSelectOp is parsed
      /*case icSigDefaultOp:*/  //This is only valid after an icSigSelectOp is parsed
      return 0;

    case icSigDataOp:            
    case icSigPiOp:
    case icSigPosInfinityOp:
    case icSigNegInfinityOp:
    case icSigNotaNumberOp:
      return 1;

    case icSigEnvVarOp:
      return 2;

    case icSigInputChanOp:        
    case icSigTempGetChanOp:
    case icSigTempSaveChanOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v2+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigApplyCurvesOp:
    case icSigApplyMatrixOp:
    case icSigApplyCLutOp:
    case icSigApplyTintOp:
    case icSigApplyToJabOp:
    case icSigApplyFromJabOp:
    case icSigApplyCalcOp:
    case icSigApplyElemOp:
      {
        CIccMultiProcessElement *pMpe = pCalc->GetElem(sig, data.select.v1);
        return (pMpe ? pMpe->NumOutputChannels() : 0);
      }

    case icSigCopyOp:
      {
      uint64_t fullResult = ((uint64_t)data.select.v1+1)
                            *((uint64_t)data.select.v2+2);
      if (fullResult > icMaxDataStackSize)
        fullResult = icMaxDataStackSize;
      return (icUInt16Number)fullResult;
      }
    
    case icSigRotateLeftOp:
    case icSigRotateRightOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigFlipOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v1+2);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigPositionDupOp:
      {
      uint32_t fullSum = (uint32_t)data.select.v1+2+(uint32_t)data.select.v2;
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigSolveOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v2+2);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigTransposeOp:
      {
      uint64_t fullResult = ((uint64_t)data.select.v1+1)
                            *((uint64_t)data.select.v2+1);
      if (fullResult > icMaxDataStackSize)
        fullResult = icMaxDataStackSize;
      return (icUInt16Number)fullResult;
      }

    case icSigSumOp:
    case icSigProductOp:         
    case icSigMinimumOp:          
    case icSigMaximumOp:          
    case icSigAndOp:
    case icSigOrOp:
      return 1;

    case icSigAddOp:              
    case icSigSubtractOp:         
    case icSigMultiplyOp:         
    case icSigDivideOp:
    case icSigModulusOp:
    case icSigPowOp:              
    case icSigGammaOp:
    case icSigScalarAddOp:
    case icSigScalarSubtractOp:
    case icSigScalarMultiplyOp:
    case icSigScalarDivideOp:
    case icSigSquareOp:       
    case icSigSquareRootOp:       
    case icSigCubeOp:       
    case icSigCubeRootOp:
    case icSigSignOp:
    case icSigAbsoluteValOp:
    case icSigTruncateOp:
    case icSigFloorOp:
    case icSigCeilingOp:
    case icSigRoundOp:
    case icSigRealNumberOp:
    case icSigNegOp:
    case icSigExpOp:              
    case icSigLogrithmOp:         
    case icSigNaturalLogOp:       
    case icSigSineOp:             
    case icSigCosineOp:             
    case icSigTangentOp:          
    case icSigArcSineOp:          
    case icSigArcCosineOp:        
    case icSigArcTangentOp:      
    case icSigArcTan2Op:
    case icSigLessThanOp:         
    case icSigLessThanEqualOp:    
    case icSigEqualOp:
    case icSigNotEqualOp:
    case icSigNearOp:
    case icSigGreaterThanEqualOp: 
    case icSigGreaterThanOp:
    case icSigNotOp:
    case icSigVectorMinimumOp:
    case icSigVectorMaximumOp:
    case icSigVectorAndOp:
    case icSigVectorOrOp:
      {
      uint32_t fullSum = ((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigCartesianToPolarOp:
    case icSigPolarToCartesianOp:
      {
      uint32_t fullSum = 2*((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    case icSigToLabOp:
    case icSigToXYZOp:
      {
      uint32_t fullSum = 3*((uint32_t)data.select.v1+1);
      if (fullSum > icMaxDataStackSize)
        fullSum = icMaxDataStackSize;
      return (icUInt16Number)fullSum;
      }

    default:
      return 0;
  }
}

CIccFuncTokenizer::CIccFuncTokenizer(const char *szText, bool bAllowNamedReferences)
{
  m_token = new std::string();
  m_text = szText;
  m_bUseRefs = bAllowNamedReferences;
}

CIccFuncTokenizer::~CIccFuncTokenizer()
{
  delete m_token;
}


bool CIccFuncTokenizer::GetNext(bool bForceNoRefs)
{
  m_token->clear();

try_again:

  while (*m_text && IsWhiteSpace())
    m_text++;

  if (!*m_text) {
    return false;
  }
  
  if (!m_bUseRefs || bForceNoRefs) {
    if (*m_text == '{') {
      m_text++;
      *m_token = "{";
      return true;
    }

    if (*m_text == '}') {
      m_text++;
      *m_token = "}";
      return true;
    }

    if (*m_text == '[') {
      m_text++;
      *m_token = "[";
      while (*m_text && *m_text != ']') {
        *m_token += *m_text;
        m_text++;
      }
    }

    if (*m_text == '(') {
      m_text++;
      *m_token = "(";
      while (*m_text && *m_text != ')') {
        *m_token += *m_text;
        m_text++;
      }
      if (!*m_text) {
        return false;
      }
    }
  }

  if (!*m_text) {
    return false;
  }

  if (IsComment()) {
    SkipComment();
    goto try_again;
  }

  if (m_bUseRefs && !bForceNoRefs) {
    while (!IsWhiteSpace() && *m_text && !IsComment()) {
      *m_token += *m_text;
      m_text++;
    }
  }
  else {
    while (!IsWhiteSpace() && *m_text && *m_text != '{' && *m_text != '[' &&
           *m_text != '(' &&  *m_text != '}' && !IsComment()) {
      *m_token += *m_text;
      m_text++;
    }
  }
  return true;
}


icSigCalcOp CIccFuncTokenizer::GetSig()
{
  const unsigned char *szToken = (unsigned char*) m_token->c_str();
  int i;

  if ((szToken[0]>='0' && szToken[0]<='9') || szToken[0]=='.' || (szToken[0]=='-' && szToken[1]!='I')) {
    return icSigDataOp;
  }

  if (szToken[0]=='{')
    return icSigBeginBlockOp;

  if (szToken[0]=='}')
    return icSigEndBlockOp;

  if (szToken[0]=='[')
    return icSigBadOp; 

  icUInt32Number sig = 0;
  for (i=0; i<4 && szToken[i]; i++) {
    sig <<= 8;
    sig |= szToken[i];
  }

  for (;i<4; i++) {
    sig <<= 8;
    sig |= 0x20;
  }

  return (icSigCalcOp)sig;
}

std::string CIccFuncTokenizer::GetName() const
{
  std::string rv;
  const char *pos = m_token->c_str();

  while (*pos && *pos != '{' && *pos != '[' && *pos != '(') {
    rv += *pos;
    pos++;
  }

  return rv;
}

std::string CIccFuncTokenizer::GetReference() const
{
  const char *pos = strchr(m_token->c_str(), '{');
  if (!m_bUseRefs || !pos)
    return "";

  std::string rv;
  pos++;
  while (*pos && *pos != '}') {
    rv += *pos;
    pos++;
  }

  return rv;
}

bool CIccFuncTokenizer::GetIndex(icUInt16Number &v1, icUInt16Number &v2, icUInt16Number initV1, icUInt16Number initV2)
{
  unsigned int iv1, iv2;
  const char *pos = GetPos();

  if (!GetNext(true))
    return false;
  iv1=initV1;
  iv2=initV2;
  const char *szToken = m_token->c_str();
  if (*szToken=='[' || *szToken=='(') {
    if (strchr(szToken, ',')) {
      if (*szToken=='(') 
        sscanf(m_token->c_str(), "(%u,%u)", &iv1, &iv2);
      else 
        sscanf(m_token->c_str(), "[%u,%u]", &iv1, &iv2);
    }
    else {
      if (*szToken=='(')
        sscanf(m_token->c_str(), "(%u)", &iv1);
      else 
        sscanf(m_token->c_str(), "[%u]", &iv1);
    }
  }
  else {
    SetPos(pos); //Undo get token
  }
  v1 = (icUInt16Number)iv1 - initV1;
  v2 = (icUInt16Number)iv2 - initV2;

  return true;
}


icFloat32Number CIccFuncTokenizer::GetValue()
{
  const char * ascii = m_token->c_str();
  char * p = NULL;
  double d = strtod( ascii, &p );
  return (icFloat32Number)d;
}


bool CIccFuncTokenizer::GetEnvSig(icSigCmmEnvVar &envSig)
{
  const char *pos = GetPos();

  if (!GetNext())
    return false;
  const char *szToken = m_token->c_str();
  int l=(int)strlen(szToken);
  if ((*szToken=='[' && szToken[l-1]==']') ||
      (*szToken=='(' && szToken[l-1]==')')) {

    int i;

    icUInt32Number sig = 0;
    if (l==10) {
      sscanf(szToken+1, "%x", &sig);

      envSig = (icSigCmmEnvVar)sig;
      return true;
    }

    for (i=1; i<=4 && i<l-1; i++) {
      sig <<= 8;
      sig |= szToken[i];
    }

    for (;i<=4; i++) {
      sig <<= 8;
      sig |= 0x20;
    }

    envSig = (icSigCmmEnvVar)sig;
  }
  else {
    SetPos(pos); //Undo get token
    envSig = (icSigCmmEnvVar)0;
    return false;
  }

  return true;
}


bool CIccFuncTokenizer::IsWhiteSpace()
{
  if (*m_text==' ' || *m_text=='\n' || *m_text=='\r' || *m_text=='\t')
    return true;
  return false;
}

bool CIccFuncTokenizer::IsComment()
{
  if (*m_text=='%' || *m_text==';' || *m_text=='/')
    return true;
  return false;
}

void CIccFuncTokenizer::SkipComment()
{
  while(*m_text && !(*m_text=='\n' || *m_text=='\r')) {
    m_text++;
  }
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::CIccCalculatorFunc
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccCalculatorFunc::CIccCalculatorFunc(CIccMpeCalculator *pCalc)
{
  m_pCalc = pCalc;
  m_nReserved = 0;

  m_nOps = 0;
  m_Op = NULL;
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::CIccCalculatorFunc
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccCalculatorFunc::CIccCalculatorFunc(const CIccCalculatorFunc &func)
{
  m_pCalc = func.m_pCalc;

  m_nReserved= func.m_nReserved;

  m_nOps = func.m_nOps;

  if (m_nOps) {
    m_Op = (SIccCalcOp*)malloc(m_nOps * sizeof(SIccCalcOp));
    memcpy(m_Op, func.m_Op, m_nOps*sizeof(SIccCalcOp));
  }
  else
    m_Op = NULL;
}

/**
 ******************************************************************************
 * Name: &CIccCalculatorFunc::operator=
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccCalculatorFunc &CIccCalculatorFunc::operator=(const CIccCalculatorFunc &func)
{
  m_pCalc = func.m_pCalc;

  m_nReserved= func.m_nReserved;

  if (m_Op)
    free(m_Op);

  m_nOps = func.m_nOps;

  if (m_nOps) {
    m_Op = (SIccCalcOp*)malloc(m_nOps * sizeof(SIccCalcOp));
    memcpy(m_Op, func.m_Op, m_nOps*sizeof(SIccCalcOp));
  }
  else
    m_Op = NULL;

  return (*this);
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::~CIccChannelFunc
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccCalculatorFunc::~CIccCalculatorFunc()
{
  if (m_Op) {
    free(m_Op);
  }
}

void CIccCalculatorFunc::InsertBlanks(std::string &sDescription, int nBlanks)
{
  char blanks[21]="                    ";
  int i;

  while (nBlanks>0) {
    i =  nBlanks < 20 ? nBlanks : 20;
    blanks[i] = 0;
    sDescription += blanks;
    nBlanks -= i;
  }
}

void CIccCalculatorFunc::DescribeSequence(std::string &sDescription,
                                       icUInt32Number nOps, SIccCalcOp *op, int nBlanks)
{
  std::string opName;
  std::string funcDesc;

  InsertBlanks(funcDesc, nBlanks);
  funcDesc += "{ ";

  int pos = nBlanks + 2;

  for (icUInt32Number i=0; i<nOps;) {
    if (pos > (nBlanks + 65)) {
      funcDesc += "\n";
      InsertBlanks(funcDesc, nBlanks + 2);
      pos = nBlanks + 2;
    }

    op->Describe(opName, 100); // TODO - propogate nVerboseness
    funcDesc += opName;
    funcDesc += " ";
    pos += (int)opName.size() + 1;

    if (op->sig == icSigIfOp) {
      SIccCalcOp *ifop = op;

      if  ((i+1)<nOps && op[1].sig == icSigElseOp) {
        SIccCalcOp *elseop = &op[1];
        // TODO - nOps-i probably should be nOps-i-2 as in CIccCalculatorFunc::CheckUnderflowOverflow
        icUInt32Number nSubOps = (icUInt32Number)icIntMin(nOps-i, ifop->data.size);
        op++;
        i++;
        funcDesc += "\n";
        DescribeSequence(funcDesc, nSubOps, &op[1], nBlanks+2);
        op += nSubOps;
        i += nSubOps;

        InsertBlanks(funcDesc, nBlanks+2);
        funcDesc += "else\n";
        //pos = 0;  // value overwritten below
        
        // TODO - nOps-i probably should be nOps-i-2 as in CIccCalculatorFunc::CheckUnderflowOverflow
        nSubOps = (icUInt32Number)icIntMin(nOps-i, elseop->data.size);
        DescribeSequence(funcDesc, nSubOps, &op[1], nBlanks+2);
        op += nSubOps;
        i += nSubOps;
      }
      else {
        icUInt32Number remaining = nOps - i - 1;
        icUInt32Number nSubOps = (icUInt32Number)icIntMin(remaining, ifop->data.size);
        funcDesc += "\n";
        DescribeSequence(funcDesc, nSubOps, &op[1], nBlanks+2);
        op += nSubOps;
        i += nSubOps;
      }

      InsertBlanks(funcDesc, nBlanks + 2);
      pos = nBlanks + 2;
    }
    else if (op->sig == icSigSelectOp) {
      op++;
      i++;
      funcDesc += "\n";

      int j, n;

      for (n=0; i+n<nOps; ) {
        if (op[n].sig==icSigCaseOp)
          n++;
        else if (n>1 && op[n].sig==icSigDefaultOp) {
          n++;
          break;
        }
        else
          break;
      }

      int p = n;
      for (j=0; j<n; j++) {
        icUInt32Number nSubOps = (icUInt32Number)icIntMin(nOps-(i+p), op[j].data.size);
        InsertBlanks(funcDesc, nBlanks+2);
        if (op[j].sig==icSigCaseOp)
          funcDesc += "case\n";
        else
          funcDesc += "dflt\n";

        DescribeSequence(funcDesc, nSubOps, &op[p], nBlanks+2);
        p += nSubOps;
      }

      op += p-1;
      i += p-1;

      InsertBlanks(funcDesc, nBlanks + 2);
      pos = nBlanks + 2;
    }
    op++;
    i++;
  }
  
  funcDesc += "}\n";

  sDescription += funcDesc;
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::Describe
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccCalculatorFunc::Describe(std::string &sDescription, int /* nVerboseness */, int nBlanks)
{
  if (m_nOps) {
    DescribeSequence(sDescription, m_nOps, m_Op, nBlanks);
  }
  else {
    sDescription += "Undefined Function!\n";
  }
}

static void AppendOpList(CIccCalcOpList &toList, const CIccCalcOpList &fromList)
{
  CIccCalcOpList::const_iterator o;

  for (o=fromList.begin(); o!=fromList.end(); o++) {
    toList.push_back(*o);
  }
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::ParseFuncDef
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
const char *CIccCalculatorFunc::ParseFuncDef(const char *szFuncDef, CIccCalcOpList &scanList, std::string &sReport)
{
  SIccCalcOp op;
  op.extra = 0;
  op.def=NULL;

  scanList.clear();

  if (!szFuncDef)
    return szFuncDef;

  CIccFuncTokenizer scan(szFuncDef);
  if (!scan.GetNext())
    return NULL;

  if (scan.GetSig() != icSigBeginBlockOp)
    return NULL;

  while(scan.GetNext()) {
    op.sig = scan.GetSig();

    if (op.sig == icSigEndBlockOp)
      return scan.GetPos();

    switch(op.sig) {
      case icSigDataOp:
        op.data.num = scan.GetValue();
        scanList.push_back(op);
        break;

      case icSigIfOp:
        {
          CIccCalcOpList trueList, falseList;
          bool bHasElse;

          szFuncDef = ParseFuncDef(scan.GetPos(), trueList, sReport);
          if (!szFuncDef)
            return NULL;

          scan.SetPos(szFuncDef);
          bHasElse = false;
          if (scan.GetNext() && scan.GetSig()==icSigElseOp) {
            bHasElse = true;
            szFuncDef = ParseFuncDef(scan.GetPos(), falseList, sReport);

            if (!szFuncDef)
              return NULL;
          }

          scan.SetPos(szFuncDef);
          op.data.size = (icUInt32Number)trueList.size();
          scanList.push_back(op);

          if (bHasElse) {
            op.sig = icSigElseOp;
            op.data.size = (icUInt32Number)falseList.size();
            scanList.push_back(op);

            AppendOpList(scanList, trueList);
            AppendOpList(scanList, falseList);
          }
          else {
            AppendOpList(scanList, trueList);
          }
        }
        break;

      case icSigSelectOp:
        {
          op.sig = icSigSelectOp;
          op.data.size = 0;
          scanList.push_back(op);

          int c;
          CIccCalcOpListPtrList selList;
          for (c=0; ; c++) {
            szFuncDef = scan.GetPos();
            scan.GetNext();
            if (scan.GetSig()==icSigCaseOp) {
              CIccCalcOpListPtr pCaseList = new CIccCalcOpList();

              szFuncDef = ParseFuncDef(scan.GetPos(), *pCaseList, sReport);

              if (!szFuncDef) {
                delete pCaseList;
                return NULL;
              }

              scan.SetPos(szFuncDef);
              op.sig = icSigCaseOp;
              op.data.size = (icUInt32Number)pCaseList->size();
              scanList.push_back(op);

              selList.push_back(pCaseList);
            }
            else if (c && scan.GetSig()==icSigDefaultOp) {
              CIccCalcOpListPtr pDefaultList = new CIccCalcOpList();

              szFuncDef = ParseFuncDef(scan.GetPos(), *pDefaultList, sReport);

              if (!szFuncDef) {
                delete pDefaultList;
                return NULL;
              }

              scan.SetPos(szFuncDef);
              op.sig = icSigDefaultOp;
              op.data.size = (icUInt32Number)pDefaultList->size();
              scanList.push_back(op);

              selList.push_back(pDefaultList);
              break;
            }
            else {
              scan.SetPos(szFuncDef);
              break;
            }
          }
          CIccCalcOpListPtrList::iterator l;
          for (l=selList.begin(); l!=selList.end(); l++) {
            CIccCalcOpListPtr pCaseList = *l;

            AppendOpList(scanList, *pCaseList);
            delete pCaseList;
          }
        }
        break;

      case icSigInputChanOp:
      case icSigOutputChanOp:
      case icSigTempGetChanOp:
      case icSigTempPutChanOp:
      case icSigTempSaveChanOp:
        if (!scan.GetIndex(op.data.select.v1, op.data.select.v2, 0, 1))
          return NULL;
        scanList.push_back(op);
        break;

      case icSigEnvVarOp:
        {
          icSigCmmEnvVar envSig;
          if (!scan.GetEnvSig(envSig))
            return NULL;
          op.data.size = (icUInt32Number)envSig;
          scanList.push_back(op);
        }
        break;

      case icSigCopyOp:
      case icSigRotateLeftOp:
      case icSigRotateRightOp:
      case icSigPositionDupOp:
      case icSigSolveOp:
      case icSigTransposeOp:
        if (!scan.GetIndex(op.data.select.v1, op.data.select.v2, 1, 1))
          return NULL;
        scanList.push_back(op);
        break;

      case icSigPopOp:
        if (!scan.GetIndex(op.data.select.v1, op.data.select.v2, 1))
          return NULL;
        op.data.select.v2 = 0;
        scanList.push_back(op);
        break;

      case icSigFlipOp:
      case icSigSumOp:
      case icSigProductOp:
      case icSigMinimumOp:
      case icSigMaximumOp:
      case icSigAndOp:
      case icSigOrOp:
        if (!scan.GetIndex(op.data.select.v1, op.data.select.v2, 2))
          return NULL;
        op.data.select.v2 = 0;
        scanList.push_back(op);
        break;

      case icSigApplyCurvesOp:
      case icSigApplyMatrixOp:
      case icSigApplyCLutOp:
      case icSigApplyTintOp:
      case icSigApplyToJabOp:
      case icSigApplyFromJabOp:
      case icSigApplyCalcOp:
      case icSigApplyElemOp:
        if (!scan.GetIndex(op.data.select.v1, op.data.select.v2))
          return NULL;
        op.data.select.v2 = 0;
        scanList.push_back(op);
        break;

      case icSigGammaOp:
      case icSigScalarAddOp:
      case icSigScalarSubtractOp:
      case icSigScalarMultiplyOp:
      case icSigScalarDivideOp:
      case icSigAddOp:
      case icSigSubtractOp:
      case icSigMultiplyOp:
      case icSigDivideOp:
      case icSigModulusOp:
      case icSigPowOp:
      case icSigArcTan2Op:          
      case icSigLessThanOp:         
      case icSigLessThanEqualOp:    
      case icSigEqualOp:
      case icSigNotEqualOp:
      case icSigNearOp:
      case icSigGreaterThanEqualOp: 
      case icSigGreaterThanOp:
      case icSigSquareOp:       
      case icSigSquareRootOp:       
      case icSigCubeOp:       
      case icSigCubeRootOp:
      case icSigSignOp:
      case icSigAbsoluteValOp:
      case icSigTruncateOp:
      case icSigFloorOp:
      case icSigCeilingOp:
      case icSigRoundOp:
      case icSigRealNumberOp:
      case icSigNegOp:      
      case icSigExpOp:              
      case icSigLogrithmOp:         
      case icSigNaturalLogOp:       
      case icSigSineOp:             
      case icSigCosineOp:             
      case icSigTangentOp:          
      case icSigArcSineOp:          
      case icSigArcCosineOp:        
      case icSigArcTangentOp:
      case icSigCartesianToPolarOp:
      case icSigPolarToCartesianOp:
      case icSigNotOp:
      case icSigToLabOp:
      case icSigToXYZOp:
      case icSigVectorMinimumOp:
      case icSigVectorMaximumOp:
      case icSigVectorAndOp:
      case icSigVectorOrOp:
        if (!scan.GetIndex(op.data.select.v1, op.data.select.v2, 1))
          return NULL;
        op.data.select.v2 = 0;
        scanList.push_back(op);
        break;

      default:
        if (!SIccCalcOp::IsValidOp(op.sig)) {
          std::string opDesc;

          op.Describe(opDesc, 100); // TODO - propogate nVerboseness
          sReport += "Invalid Operator \"";
          sReport += opDesc;
          sReport += "\"\n";

          if (scanList.rbegin()!=scanList.rend()) {
            sReport += "Last Few operators in reverse:\n";

            CIccCalcOpList::reverse_iterator opi;
            int i;
            for (i=0, opi=scanList.rbegin(); i<10 && opi!=scanList.rend(); i++, opi++) {
              opi->Describe(opDesc, 100); // TODO - propogate nVerboseness
              sReport += " ";
              sReport += opDesc;
            }
            sReport += "\n";
          }
          return NULL;
        }
        op.data.select.v1 = 0;
        op.data.select.v2 = 0;
        scanList.push_back(op);
        break;
    }
  }
  return NULL;
}


/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::SetFunction
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icFuncParseStatus CIccCalculatorFunc::SetFunction(const char *szFuncDef, std::string &sReport)
{
  CIccCalcOpList opList;

  if (ParseFuncDef(szFuncDef, opList, sReport)) {
    return SetFunction(opList, sReport);
  }
  return icFuncParseSyntaxError;
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::SetFunction
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icFuncParseStatus CIccCalculatorFunc::SetFunction(CIccCalcOpList &opList, std::string &sReport)
{
  if (m_Op) {
    free(m_Op);
    m_Op = NULL;
  }

  m_nOps = (icUInt32Number)opList.size();

  if (m_nOps) {
    CIccCalcOpList::iterator i;
    int j;

    m_Op = (SIccCalcOp*)calloc(m_nOps , sizeof(SIccCalcOp));

    for (i=opList.begin(), j=0; i!= opList.end(); i++, j++) {
      m_Op[j] = *i;
    }
  }
  else {
    return icFuncParseEmptyFunction;
  }

  if (!HasValidOperations(sReport))
    return icFuncParseInvalidOperation;

  return DoesStackUnderflowOverflow(sReport);
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::Read
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccCalculatorFunc::Read(icUInt32Number size, CIccIO *pIO)
{
  icUInt32Number headerSize = sizeof(icUInt32Number);

  headerSize += sizeof(icChannelFuncSignature) + sizeof(icUInt32Number);

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  icChannelFuncSignature sig;

  if (!pIO->Read32(&sig))
    return false;

  if (sig!= GetType())
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read32(&m_nOps))
    return false;

  if ((icUInt64Number)m_nOps * sizeof(icUInt32Number) * 2 > (icUInt64Number)size - headerSize)
    return false;

  if (m_Op) {
    free(m_Op);
  }

  if (m_nOps) {
    m_Op = (SIccCalcOp*)calloc(m_nOps, sizeof(SIccCalcOp));

    if (!m_Op) {
      m_nOps = 0;
      return false;
    }

    icUInt32Number i;
    for (i=0; i<m_nOps; i++) {
      if (!pIO->Read32(&m_Op[i].sig))
        return false;
      if (!pIO->Read32(&m_Op[i].data.num))
        return false;
    }
  }

  return true;
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::Write
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccCalculatorFunc::Write(CIccIO *pIO)
{
  if (!pIO)
    return false;

  icChannelFuncSignature sig;
  sig = GetType();
  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write32(&m_nOps))
    return false;

  icUInt32Number i;
  for (i=0; i<m_nOps; i++)  {
    if (!pIO->Write32(&m_Op[i].sig))
      return false;
    if (!pIO->Write32(&m_Op[i].data.num))
      return false;
  }

  return true;
}


/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::SetOpDefs
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccCalculatorFunc::SetOpDefs()
{
  if (!m_Op)
    return false;

  icUInt32Number i;
  for (i=0; i<m_nOps; i++) {
    m_Op[i].def = CIccCalcOpMgr::getOpDef(m_Op[i].sig);
    if (!m_Op[i].def)
      return false;
  }
  return true;
}


/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccCalculatorFunc::Begin(const CIccMpeCalculator *pChannelCalc, CIccTagMultiProcessElement * /* pMPE */)
{
  if (!pChannelCalc)
    return false;
  std::string sReport;

  if (!InitSelectOps())
    return false;

  if (!HasValidOperations(sReport))
    return false;

  if (!SetOpDefs())
    return false;

  if (DoesOverflowInput(pChannelCalc->NumInputChannels()))
    return false;

  if (DoesOverflowOutput(pChannelCalc->NumOutputChannels()))
    return false;

  if (DoesStackUnderflowOverflow(sReport)!=icFuncParseNoError)
    return false;

  return true;
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::InitSelectOps
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccCalculatorFunc::InitSelectOps()
{
  icUInt32Number i;

  for (i=0; i<m_nOps; i++) {
    if (m_Op[i].sig==icSigSelectOp) {
      if (!InitSelectOp(&m_Op[i], m_nOps-i))
        return false;
    }
  }

  return true;
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::InitSelectOp
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccCalculatorFunc::InitSelectOp(SIccCalcOp *ops, icUInt32Number nOps)
{

  if (ops->sig != icSigSelectOp)
    return false;

  if (ops->extra)
    return true;


  icUInt32Number i, n, pos;
  for (n=0; n<nOps && ops[n+1].sig==icSigCaseOp; n++);
  ops->extra=n;
  if (ops[n+1].sig==icSigDefaultOp) {
    n++;
  }
  pos = n;
  for (i=1; i<=n && pos<nOps; i++) {
    ops[i].extra = pos;
    pos += ops[i].data.size;
  }

  if (i<=n)
    return false;

  return true;
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::ApplySequence
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccCalculatorFunc::ApplySequence(CIccApplyMpeCalculator *pApply, icUInt32Number nOps, SIccCalcOp *ops) const
{
  SIccCalcOp *op;
  SIccOpState os;

  os.pApply = pApply;
  os.pStack = pApply->GetStack();
  os.pScratch = pApply->GetScratch();
  os.temp = pApply->GetTemp();
  os.pixel = pApply->GetInput();
  os.output = pApply->GetOutput();
  os.nOps = nOps;

  for (os.idx=0; os.idx<os.nOps; os.idx++) {
    op = &ops[os.idx];

    if (g_pDebugger)
      g_pDebugger->BeforeOp(op, os, ops);

    if (op->sig==icSigIfOp) {
      icFloatNumber a1;
      OsPopArg(a1);

      if (os.idx+1<nOps && ops[os.idx+1].sig==icSigElseOp) {
        os.idx++;
        if (a1>=0.5) {
          if (os.idx+1 + op->data.size >= nOps)
            return false;

          if (!ApplySequence(pApply, op->data.size, &ops[os.idx+1]))
            return false;
        }
        else {
          if (os.idx+1 + op->data.size + ops[os.idx].data.size > nOps)
            return false;

          if (!ApplySequence(pApply, ops[os.idx].data.size, &ops[os.idx+1 + op->data.size]))
            return false;
        }
        os.idx += op->data.size + ops[os.idx].data.size;
      }
      else {
        if (a1>=0.5) {
          if (os.idx+op->data.size >= nOps)
            return false;

          if (!ApplySequence(pApply, op->data.size, &ops[os.idx+1]))
            return false;
        }
        os.idx+= op->data.size;
      }
    }
    else if (op->sig==icSigSelectOp) {
      icFloatNumber a1;
      OsPopArg(a1);
      if (isnan(a1))
        a1 = 0.0;
        
      icInt32Number nSel;
      if (isinf(a1)) {
        nSel = (a1 < 0.0) ? std::numeric_limits<icInt32Number>::lowest() : std::numeric_limits<icInt32Number>::max();
      }
      else
        nSel = (a1 >= 0.0) ? (icInt32Number)(a1+0.5f) : (icInt32Number)(a1-0.5f);

      if (!op->extra) {
        return false;
      }
      
      icUInt32Number nDefOff = (icUInt32Number)(os.idx+1 + op->extra);
      if (nDefOff >= nOps)
        return false;

      if (nSel<0 || (icUInt32Number)nSel>=op->extra) {

        if (ops[nDefOff].sig==icSigDefaultOp) {
          if (os.idx+1 + ops[nDefOff].extra >= nOps)
            return false;

          if (!ApplySequence(pApply, ops[nDefOff].data.size, &ops[os.idx+1 + ops[nDefOff].extra]))
            break;
        }
      }
      else {
        icUInt32Number nOff = os.idx+1 + nSel;

        if (nOff >= nOps) 
          return false;

        if (!ApplySequence(pApply, ops[nOff].data.size, &ops[os.idx+1 + ops[nOff].extra]))
          break;
      }

      if (ops[nDefOff].sig==icSigDefaultOp) {
        if (os.idx+1 + ops[nDefOff].extra + ops[nDefOff].data.size >nOps)
          return false;

        os.idx = (icUInt32Number)(os.idx + ops[nDefOff].extra + ops[nDefOff].data.size);
      }
      else if (op->extra) {
        unsigned long nOff = os.idx + op->extra;

        if (os.idx+1 + ops[nOff].extra + ops[nOff].data.size > nOps)
          return false;

        os.idx = (icUInt32Number)(os.idx + ops[nOff].extra + ops[nOff].data.size);
      }
      else 
        return false;
    }
    else {
      if (!op->def->Exec(op, os))
        return false;
    }

    if (g_pDebugger) {
      g_pDebugger->AfterOp(op, os, ops);
    }
  }
  return true;
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::Apply
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccCalculatorFunc::Apply(CIccApplyMpeCalculator *pApply) const
{
  CIccFloatVector *pStack = pApply->GetStack();

  pStack->clear();

  if (!ApplySequence(pApply, m_nOps, m_Op)) {
    icFloatNumber *pOut = pApply->GetOutput();
    icUInt32Number i;
    for (i=0; i<m_pCalc->NumOutputChannels(); i++)
      pOut[i] = -1;
    return false;
  }

  return true;
}

/**
 ******************************************************************************
 * Name: CIccCalculatorFunc::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccCalculatorFunc::Validate(std::string sigPath, std::string &sReport,
                                           const CIccMpeCalculator* pChannelCalc/*=NULL*/,
                                           const CIccProfile *pProfile/*=NULL*/) const
{
  CIccInfo Info;
  std::string sSigPathName = Info.GetSigPathName(sigPath);

  icValidateStatus rv = icValidateOK;
  if (m_nReserved) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " function has non zero reserved data.\n";
    rv = icValidateWarning;
  }

  if (GetMaxTemp()>65535) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " accesses illegal temporary channels.\n";
    return icValidateCriticalError;
  }

  if (!HasValidOperations(sReport)) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " function has invalid operations.\n";
    return icValidateCriticalError;
  }

  if (HasUnsupportedOperations(sReport, pProfile)) {
    sReport += icMsgValidateWarning;
    sReport += sSigPathName;
    sReport += " function has unsupported operations.\r\n";
    rv = icValidateWarning;
  }

  if (DoesOverflowInput(pChannelCalc->NumInputChannels())) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " accesses illegal input channels.\n";
    return icValidateCriticalError;
  }

  if (DoesOverflowOutput(pChannelCalc->NumOutputChannels())) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " accesses illegal output channels.\n";
    return icValidateCriticalError;
  }

  icFuncParseStatus parseStat = DoesStackUnderflowOverflow(sReport);
  if (parseStat!=icFuncParseNoError) {
    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    if (parseStat==icFuncParseStackUnderflow)
      sReport += " causes an evaluation stack underflow.\n";
    else
      sReport += " causes an evaluation stack overflow.\n";
    return icValidateCriticalError;
  }

  return rv;
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::GetMaxTemp
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icUInt32Number CIccCalculatorFunc::GetMaxTemp() const
{
  icUInt32Number i, nMaxTemp = 0;

  for (i=0; i<m_nOps; i++) {
    if (m_Op[i].sig == icSigTempGetChanOp || m_Op[i].sig == icSigTempPutChanOp || m_Op[i].sig == icSigTempSaveChanOp ) {
      icUInt32Number last = (icUInt32Number)(m_Op[i].data.select.v1) + (icUInt32Number)(m_Op[i].data.select.v2);
      if (last>nMaxTemp)
        nMaxTemp = last;
    }
  }
  return nMaxTemp;
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::NeedTempReset
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccCalculatorFunc::NeedTempReset(icUInt8Number *tempUsage, icUInt32Number nMaxTemp)
{
  return SequenceNeedTempReset(m_Op, m_nOps, tempUsage, nMaxTemp);
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::SequenceNeedTempReset
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccCalculatorFunc::SequenceNeedTempReset(SIccCalcOp *op, icUInt32Number nOps, icUInt8Number *tempUsage, icUInt32Number nMaxTemp)
{
  icUInt32Number i, j;

  for (i=0; i<nOps; i++) {
    icSigCalcOp sig = op[i].sig;
    if (sig==icSigTempGetChanOp) {
      icUInt32Number p = op[i].data.select.v1;
      icUInt32Number n = op[i].data.select.v2+1;
      for (j=0; j<n; j++) {
        if (!tempUsage[p+j]) {
          return true;
        }
      }
    }
    else if (sig==icSigTempPutChanOp) {
      icUInt32Number p = op[i].data.select.v1;
      icUInt32Number n = op[i].data.select.v2+1;
      memset(tempUsage+p, 1, n);
    }
    else if (sig==icSigTempSaveChanOp) {
      icUInt32Number p = op[i].data.select.v1;
      icUInt32Number n = op[i].data.select.v2+1;
      memset(tempUsage+p, 1, n);
    }
    else if (sig==icSigIfOp) {
      bool rv = false;
      icUInt8Number *ifTemps = (icUInt8Number *)malloc(nMaxTemp);
      icUInt32Number p;
      if (!ifTemps)
        return true;

      memcpy(ifTemps, tempUsage, nMaxTemp);

      p=i+2;
      if (p > nOps) {
        free(ifTemps);
        return true;
      }
      rv = rv || SequenceNeedTempReset(&op[p], icIntMin(nOps-p, op[i].data.size), ifTemps, nMaxTemp);

      if (i<nOps && op[i+1].sig==icSigElseOp) {
        icUInt8Number *elseTemps = (icUInt8Number *)malloc(nMaxTemp);
        if (!elseTemps) {
          free(ifTemps);
          return true;
        }

        memcpy(elseTemps, tempUsage, nMaxTemp);

        p=i+2+op[i].data.size;
        if (p > nOps) {
          free(ifTemps);
          free(elseTemps);
          return true;
        }
        rv = rv || SequenceNeedTempReset(&op[p], icIntMin(nOps-p, op[i+1].data.size), elseTemps, nMaxTemp);

        if (!rv) {
          for (j=0; j<nMaxTemp; j++) {
            tempUsage[j] |= (ifTemps[j] && elseTemps[j]);
          }
        }

        free(elseTemps);

        i += 1 + op[i].data.size + op[i+1].data.size;
      }
      else {
        i += op[i].data.size;
      }

      free(ifTemps);

      if (rv)
        return true;
    }
  }
  return false;
}


/**
******************************************************************************
* Name: CIccCalculatorFunc::CheckUnderflow
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
int CIccCalculatorFunc::CheckUnderflowOverflow(SIccCalcOp *op, icUInt32Number nOps, int nArgs, bool bCheckUnderflow, std::string &sReport) const
{
  icUInt32Number i, p;
  int nIfArgs, nElseArgs, nSelArgs, nCaseArgs;


  for (i=0; i<nOps; i++) {
    int nArgsUsed = op[i].ArgsUsed(m_pCalc);

#if 0
    std::string opstr;
    op[i].Describe(opstr);
    printf("%s : %d %d\n", opstr.c_str(), nArgsUsed, nArgs);
#endif

    if (nArgsUsed > nArgs) {
      std::string opDesc;
      icUInt32Number j, l;
      icInt32Number f;

      op[i].Describe(opDesc, 100); // TODO - propogate nVerboseness
      sReport += "Stack underflow at operation \"" + opDesc + "\" in \"";
      f=i-2;
      if (f<0) f=0;
      l=i+2;
      if (l>nOps-1)
        l=nOps-1;
      for (j=(icUInt32Number)f; j<=l; j++) {
        op[j].Describe(opDesc, 100); // TODO - propogate nVerboseness
        if (j!=(icUInt32Number)f)
          sReport += " ";
        sReport += opDesc;
      }
      sReport += "\"\n";
      return -1;
    }

    nArgs -= nArgsUsed;
    nArgs += op[i].ArgsPushed(m_pCalc);

    if (nArgs>icMaxDataStackSize)
      return -2;

    if (op[i].sig == icSigIfOp) {
      int incI = 0;
      if (i+1<nOps && op[i+1].sig==icSigElseOp) {
        p = i+2; 
        if (p > nOps)
          return -1;
        nIfArgs = CheckUnderflowOverflow(&op[p], icIntMin(nOps-p, op[i].data.size), nArgs, bCheckUnderflow, sReport);
        if (nIfArgs<0)
          return -1;
        incI =op[i].data.size;

        p = i+2+op[i].data.size;
        if (p > nOps)
          return -1;
        nElseArgs = CheckUnderflowOverflow(&op[p], icIntMin(nOps-p, op[i+1].data.size), nArgs, bCheckUnderflow, sReport);
        if (nElseArgs<0)
          return -1;
        incI += op[i+1].data.size;

        nArgs = bCheckUnderflow ? icIntMin(nIfArgs, nElseArgs) : icIntMax(nIfArgs, nElseArgs) ;
        i++;
        i+=incI;
      }
      else {
        p = i+1; 
        if (p > nOps)
          return -1;
        nIfArgs = CheckUnderflowOverflow(&op[p], icIntMin(nOps-p, op[i].data.size), nArgs, bCheckUnderflow, sReport);
        if (nIfArgs<0)
          return -1;
        nArgs = bCheckUnderflow ? icIntMin(nArgs, nIfArgs) : icIntMax(nArgs, nIfArgs);

        i+= op[i].data.size;
      }
    }
    else if (op[i].sig == icSigSelectOp) {
      icUInt32Number n;
      nSelArgs = nArgs;

      for (n=1; i+n<nOps; ) {
        if (op[i+n].sig==icSigCaseOp)
          n++;
        else if (n>1 && op[i+n].sig==icSigDefaultOp) {
          n++;
          break;
        }
        else
          break;
      }
      n--;

      if (!n)
        return -1;

      icUInt32Number pos = i+1 + n;
      for (p=0; p<n; p++) {
        SIccCalcOp *sop = &op[i+1+p];
        int len = sop->data.size;
        if (pos>=nOps)
          return -1;

        if (pos > nOps)
          return -1;
        nCaseArgs = CheckUnderflowOverflow(&op[pos], icIntMin(nOps-pos, len), nArgs, bCheckUnderflow, sReport);
        if (nCaseArgs<0)
          return -1;

        if (!p)
          nSelArgs = nCaseArgs;
        else
          nSelArgs = bCheckUnderflow ? icIntMin(nSelArgs, nCaseArgs) : icIntMax(nSelArgs, nCaseArgs);

        pos += sop->data.size;
      }
      nArgs = nSelArgs;

      i = pos - 1;
    }
  }

  return nArgs;
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::DoesStackUnderflowOverflow
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icFuncParseStatus CIccCalculatorFunc::DoesStackUnderflowOverflow(std::string &sReport) const
{
  int rv = CheckUnderflowOverflow(m_Op, m_nOps, 0, true, sReport); 

  if (rv==-1)
    return icFuncParseStackUnderflow;
  else if (rv<0)
    return icFuncParseStackOverflow;

  rv = CheckUnderflowOverflow(m_Op, m_nOps, 0, false, sReport); 

  if (rv==-1)
    return icFuncParseStackUnderflow;
  else if (rv<0)
    return icFuncParseStackOverflow;

  return icFuncParseNoError;
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::HasValidOperations
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccCalculatorFunc::HasValidOperations(std::string &sReport) const
{
  icUInt32Number i;
  for (i=0; i<m_nOps; i++) {
    if (!m_Op[i].IsValidOp(m_pCalc)) {
      std::string opDesc;
      m_Op[i].Describe(opDesc, 100); // TODO - propogate nVerboseness 
      sReport += "Invalid Operation (" + opDesc + ")\n";
      return false;
    }
    if (m_Op[i].sig==icSigSelectOp && i+1<m_nOps && m_Op[i+1].sig!=icSigCaseOp) {
      sReport += "A case operator does not follow a sel operator\n";
      return false;
    }
    if (i) {
      if (m_Op[i].sig==icSigElseOp) {
        if (m_Op[i-1].sig!=icSigIfOp) {
          sReport += "An else operator has no proceeding if operator\n";
          return false;
        }
      }
      else if (m_Op[i].sig==icSigCaseOp) {
        if (m_Op[i-1].sig!=icSigSelectOp && m_Op[i-1].sig!=icSigCaseOp) {
          sReport += "A case operator has no proceeding sel or case operator\n";
          return false;
        }
      }
      else if (m_Op[i].sig==icSigDefaultOp) {
        if (m_Op[i-1].sig!=icSigCaseOp) {
          sReport += "An dflt operator has no proceeding case operator\n";
          return false;
        }
      }
    }
  }
  return true;
}

/**
******************************************************************************
* Name: CIccCalculatorFunc::HasUnsupportedOperations
*
* Purpose:
*
* Args:
*
* Return:
******************************************************************************/
bool CIccCalculatorFunc::HasUnsupportedOperations(std::string &sReport, const CIccProfile *pProfile) const
{
  icUInt32Number i;
  if (pProfile) {
    icUInt32Number version = pProfile->m_Header.version;

    icCalcOpMap map;
    for (i = 0; i < m_nOps; i++) {
      if (version < icVersionNumberV5_1 && 
          (m_Op[i].sig == icSigNotOp ||
           m_Op[i].sig == icSigNotEqualOp)) {
        map[m_Op[i].sig] = NULL;
      }
    }
    if (map.size()) {
      icCalcOpMap::iterator sig;
      sReport += "Calculator operator(s) not supported by profile version:";
      for (sig = map.begin(); sig != map.end(); sig++) {
        SIccCalcOp op;
        op.sig = sig->first;
        std::string opname;
        op.Describe(opname, 100);
        sReport += std::string(" (") + opname + ")";
      }
      sReport += "\n";

      return true;
    }
  }
  return false;
}


/**
******************************************************************************
* Name: CIccChannelFunc::DoesOverflowInput
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccCalculatorFunc::DoesOverflowInput(icUInt16Number nInputChannels) const
{
  icUInt32Number i;
  for (i=0; i<m_nOps; i++) {
    if (m_Op[i].sig == icSigInputChanOp) {
      if (m_Op[i].data.select.v1+m_Op[i].data.select.v2 >= nInputChannels)
        return true;
    }
  }
  return false;
}


/**
******************************************************************************
* Name: CIccChannelFunc::DoesOverflowOutput
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccCalculatorFunc::DoesOverflowOutput(icUInt16Number nOutputChannels) const
{
  icUInt32Number i;
  for (i=0; i<m_nOps; i++) {
    if (m_Op[i].sig == icSigOutputChanOp) {
      if (m_Op[i].data.select.v1 + m_Op[i].data.select.v2 >= nOutputChannels)
        return true;
    }
  }
  return false;
}


/**
 ******************************************************************************
 * Name: CIccMpeCalculator::CIccMpeCalculator
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeCalculator::CIccMpeCalculator(icUInt16Number nInputChannels /*=0*/,
                                     icUInt16Number nOutputChannels /*=0*/)
{
  m_nReserved = 0;
  m_nInputChannels = nInputChannels;
  m_nOutputChannels = nOutputChannels;
  m_nTempChannels = 0;
  m_bNeedTempReset = true;
  m_nSubElem = 0;
  m_SubElem = NULL;
  m_calcFunc = NULL;
  m_pCmmEnvVarLookup = NULL;
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::CIccMpeCalculator
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeCalculator::CIccMpeCalculator(const CIccMpeCalculator &channelGen)
{
  m_nReserved = channelGen.m_nReserved;

  m_nInputChannels = channelGen.m_nInputChannels;
  m_nOutputChannels = channelGen.m_nOutputChannels;
  m_nTempChannels = channelGen.m_nTempChannels;

  m_pCmmEnvVarLookup = channelGen.m_pCmmEnvVarLookup;

  m_nSubElem = channelGen.m_nSubElem;
  m_bNeedTempReset = channelGen.m_bNeedTempReset;

  m_pCmmEnvVarLookup = channelGen.m_pCmmEnvVarLookup;

  icCalculatorFuncPtr ptr = channelGen.m_calcFunc;

  if (ptr)
    m_calcFunc = ptr->NewCopy();
  else
    m_calcFunc = NULL;

  if (channelGen.m_nSubElem) {
    icUInt32Number i;

    m_SubElem = (CIccMultiProcessElement**)calloc(m_nSubElem, sizeof(CIccMultiProcessElement*));
    if (m_SubElem) {
      for (i=0; i<m_nSubElem; i++) {
        if (channelGen.m_SubElem[i])
          m_SubElem[i] = channelGen.m_SubElem[i]->NewCopy();
      }
    }
    else {
      m_nSubElem = 0;
      m_SubElem = NULL;
    }
  }
  else {
    m_nSubElem = 0;
    m_SubElem = NULL;
  }
}

/**
 ******************************************************************************
 * Name: &CIccMpeCalculator::operator=
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeCalculator &CIccMpeCalculator::operator=(const CIccMpeCalculator &channelGen)
{
  m_nReserved = channelGen.m_nReserved;

  SetSize(0,0);

  m_nInputChannels= channelGen.m_nInputChannels;
  m_nOutputChannels = channelGen.m_nOutputChannels;

  m_pCmmEnvVarLookup = channelGen.m_pCmmEnvVarLookup;

  icCalculatorFuncPtr ptr = channelGen.m_calcFunc;

  if (ptr)
    m_calcFunc = ptr->NewCopy();
  else
    m_calcFunc = NULL;

  if (channelGen.m_nSubElem) {
    icUInt32Number i;

    m_SubElem = (CIccMultiProcessElement**)calloc(m_nSubElem, sizeof(CIccMultiProcessElement*));
    if (m_SubElem) {
      for (i=0; i<m_nSubElem; i++) {
        if (channelGen.m_SubElem[i])
          m_SubElem[i] = channelGen.m_SubElem[i]->NewCopy();
      }
    }
    else {
      m_nSubElem = 0;
      m_SubElem = NULL;
    }
  }
  else {
    m_nSubElem = 0;
    m_SubElem = NULL;
  }

  return *this;
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::~CIccMpeCalculator
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
CIccMpeCalculator::~CIccMpeCalculator()
{
  SetSize(0,0);
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::SetSize
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeCalculator::SetSize(icUInt16Number nInputChannels, icUInt16Number nOutputChannels)
{
  if (m_calcFunc) {
    delete m_calcFunc;
    m_calcFunc = NULL;
  }
  icUInt32Number i;

  if (m_SubElem) {
    for (i=0; i<m_nSubElem; i++) {
      if (m_SubElem[i])
        delete m_SubElem[i];
    }
    free(m_SubElem);
    m_SubElem = NULL;
    m_nSubElem = 0;
  }

  m_nInputChannels = nInputChannels;
  m_nOutputChannels = nOutputChannels;
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::SetCalcFunc
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icFuncParseStatus CIccMpeCalculator::SetCalcFunc(icCalculatorFuncPtr newChannelFunc) 
{
  if (m_calcFunc) {
    delete m_calcFunc;
  }
  m_calcFunc = newChannelFunc;
  
  return icFuncParseNoError;
}

/**
******************************************************************************
* Name: CIccMpeCalculator::SetCalcFunc
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
icFuncParseStatus CIccMpeCalculator::SetCalcFunc(const char *szFuncDef, std::string &sReport)
{

  if (m_calcFunc) {
    delete m_calcFunc;
    m_calcFunc = NULL;
  }

  CIccCalculatorFunc *pFunc = new CIccCalculatorFunc(this);
  icFuncParseStatus rv = pFunc->SetFunction(szFuncDef, sReport);
  
  if (rv!=icFuncParseNoError) {
    delete pFunc;
    m_calcFunc = NULL;

    return rv;
  }

  m_calcFunc = pFunc;

  return rv;
}


/**
 ******************************************************************************
 * Name: CIccMpeCalculator::Describe
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeCalculator::Describe(std::string &sDescription, int nVerboseness)
{
  if (m_calcFunc) {
    const size_t bufSize = 81;
    icChar buf[bufSize];

    snprintf(buf, bufSize, "BEGIN_CALC_ELEMENT %u %u\n", m_nInputChannels, m_nOutputChannels);
    sDescription += buf;

    if (m_nSubElem && m_SubElem) {
      icUInt32Number i;
      for (i=0; i<m_nSubElem; i++) {
        snprintf(buf, bufSize, "BEGIN_SUBCALCELEM %u\n", i);
        sDescription += buf;
        m_SubElem[i]->Describe(sDescription, nVerboseness);
        snprintf(buf, bufSize, "END_SUBCALCELEM %u\n\n", i);
        sDescription += buf;
      }
    }

    if (m_calcFunc) {
      sDescription += "BEGIN_CALC_FUNCTION\n";
      m_calcFunc->Describe(sDescription, nVerboseness);
      sDescription += "END_CALC_FUNCTION\n";
    }

    snprintf(buf, bufSize, "END_CALC_ELEMENT\n");
    sDescription += buf;

  }
}

typedef std::map<icUInt32Number, CIccCalculatorFunc*> icChannelFuncOffsetMap;
typedef std::map<CIccCalculatorFunc*, icPositionNumber> icChannelFuncPtrMap;

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::Read
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeCalculator::Read(icUInt32Number size, CIccIO *pIO)
{
  icElemTypeSignature sig;

  size_t startPos = pIO->Tell();
  
  size_t headerSize = sizeof(icTagTypeSignature) + // typeSig
    sizeof(icUInt32Number) +    // reserved
    sizeof(icUInt16Number) +    // inputChannels
    sizeof(icUInt16Number) +    // outputChannels
    sizeof(icUInt32Number) +    // SubElement count
    sizeof(icPositionNumber);   // at least one icPositionNumber

  if (headerSize > size)
    return false;

  if (!pIO) {
    return false;
  }

  icUInt16Number nInputChannels, nOutputChannels;

  if (!pIO->Read32(&sig))
    return false;

  if (!pIO->Read32(&m_nReserved))
    return false;

  if (!pIO->Read16(&nInputChannels))
    return false;

  if (!pIO->Read16(&nOutputChannels))
    return false;

  SetSize(nInputChannels, nOutputChannels);

  icUInt32Number nSubElem;

  if (!pIO->Read32(&nSubElem))
    return false;

  icUInt32Number nPos = nSubElem + 1;

  if (headerSize + (icUInt64Number)nPos*sizeof(icPositionNumber) > size) {
    return false;
  }

  icPositionNumber *pos, *posvals = (icPositionNumber*)malloc(nPos*sizeof(icPositionNumber));
  if (!posvals) {
    return false;
  }

  icUInt32Number n = nPos * (sizeof(icPositionNumber)/sizeof(icUInt32Number));
  if (pIO->Read32(posvals, n)!=n) {
    free(posvals);
    return false;
  }

  pos = &posvals[1];
  if (nSubElem) {
    icElemTypeSignature elemSig;

    SetSubElem(nSubElem-1, NULL); //Initialize array

    for (n=0; n<nSubElem; n++) {
      // No, you may not make circular references in the tag,
      // reference back into the tag header,
      // or try to load more data than we have available.
      if ( (pos->offset < headerSize) || ((pos->offset + pos->size) > size) ) {
        free(posvals);
        return false;
      }
      
      pIO->Seek(startPos + pos->offset, icSeekSet);

      if (!pIO->Read32(&elemSig)) {
        free(posvals);
        return false;
      }

      CIccMultiProcessElement *pElem = CIccMultiProcessElement::Create(elemSig);
      if (!pElem) {
        free(posvals);
        return false;
      }

      pIO->Seek(startPos + pos->offset, icSeekSet);
      if (!pElem->Read(pos->size, pIO)) {
        free(posvals);
        return false;
      }
      SetSubElem((icUInt16Number)n, pElem);
      pos++;
    }
  }  

  m_calcFunc = new CIccCalculatorFunc(this);
  pos = posvals;
  
  // overreading, references into the header, or not having a calc func would be bad
  if ( !m_calcFunc || (pos->offset < headerSize) || ((pos->offset + pos->size) > size) ) {
    free(posvals);
    return false;
  }

  pIO->Seek(startPos + pos->offset, icSeekSet);

  if (!m_calcFunc->Read(pos->size, pIO)) {
    free(posvals);
    return false;
  }
  
  free(posvals);

  pIO->Seek(startPos + size, icSeekSet);

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::Write
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeCalculator::Write(CIccIO *pIO)
{
  icElemTypeSignature sig = GetType();

  if (!pIO)
    return false;

  size_t elemStart = pIO->Tell();

  if (!pIO->Write32(&sig))
    return false;

  if (!pIO->Write32(&m_nReserved))
    return false;

  if (!pIO->Write16(&m_nInputChannels))
    return false;

  if (!pIO->Write16(&m_nOutputChannels))
    return false;

  if (!pIO->Write32(&m_nSubElem))
    return false;

  icUInt32Number nPos = m_nSubElem + 1;

  icPositionNumber *pos, *posvals = (icPositionNumber*)calloc(nPos, sizeof(icPositionNumber));
  if (!posvals) {
    return false;
  }
  size_t nPositionStart = pIO->Tell();

  size_t n, np = nPos * (sizeof(icPositionNumber)/sizeof(icUInt32Number));
  if (pIO->Write32(posvals, np)!=np) {
    free(posvals);
    return false;
  }

  if (m_calcFunc) {
    posvals[0].offset = (icUInt32Number)(pIO->Tell() - elemStart);
    if (!m_calcFunc->Write(pIO)) {
      free(posvals);
      return false;
    }
    posvals[0].size = (icUInt32Number)(pIO->Tell() - elemStart - posvals[nPos-1].offset );
    pIO->Align32();
  }

  pos = &posvals[1];

  if (m_nSubElem) {
    for(n=0; n<m_nSubElem; n++) {
      if (m_SubElem[n]) {
        pos->offset = (icUInt32Number)( pIO->Tell()-elemStart );
        if (!m_SubElem[n]->Write(pIO)) {
          free(posvals);
          return false;
        }
        pos->size = (icUInt32Number)(pIO->Tell()-elemStart - pos->offset);
        pIO->Align32();
      }
      pos++;
    }
  }
  size_t endPos = pIO->Tell();

  pIO->Seek(nPositionStart, icSeekSet);

  if (pIO->Write32(posvals, np)!=np) {
    free(posvals);
    return false;
  }
  free(posvals);

  pIO->Seek(endPos, icSeekSet);

  return true;
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::Begin
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeCalculator::Begin(icElemInterp nInterp, CIccTagMultiProcessElement *pMPE)
{
  if (!m_calcFunc)
    return false;

  m_nTempChannels = m_calcFunc->GetMaxTemp()+1;

  m_pCmmEnvVarLookup = pMPE->GetCmmEnvLookup();

  if (m_nTempChannels>65536)
    return false;

  if (m_nTempChannels) {
    icUInt8Number *tempUsage = (icUInt8Number *)calloc(m_nTempChannels, sizeof(icUInt8Number));
    if (tempUsage) {
      m_bNeedTempReset = m_calcFunc->NeedTempReset(tempUsage, m_nTempChannels);
      free(tempUsage);
    }
    else
      m_bNeedTempReset = true;
  }
  else {
    m_bNeedTempReset = false;
  }

  if (!m_calcFunc->Begin(this, pMPE))
    return false;

  icUInt32Number n;
  for (n=0; n<m_nSubElem; n++) {
    if (m_SubElem[n] && !m_SubElem[n]->Begin(nInterp, pMPE))
      return false;
  }

  return true;
}

/**
******************************************************************************
* Name: CIccMpeCalculator::Begin
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccApplyMpe *CIccMpeCalculator::GetNewApply(CIccApplyTagMpe *pApplyTag)
{
  //CIccApplyTagMpe *pApplyTagEx = (CIccApplyTagMpe*)pApplyTag;

  CIccApplyMpeCalculator *pApply = new CIccApplyMpeCalculator(this);

  if (!pApply)
    return NULL;

  if (m_nTempChannels) {
    pApply->m_temp = (icFloatNumber*)malloc(m_nTempChannels*sizeof(icFloatNumber));
  }
  pApply->m_stack = new CIccFloatVector;
  pApply->m_scratch = new CIccFloatVector;
  pApply->m_scratch->resize(50);
  pApply->m_pCmmEnvVarLookup = m_pCmmEnvVarLookup;

  icUInt32Number i;

  pApply->m_nSubElem = m_nSubElem;
  if(m_nSubElem) {
    pApply->m_SubElem = (CIccSubCalcApply **)calloc(m_nSubElem, sizeof(CIccSubCalcApply*));

    if (m_SubElem) {
      for (i=0; i<m_nSubElem; i++) {
        if (m_SubElem[i]) {
          pApply->m_SubElem[i] = new CIccSubCalcApply(m_SubElem[i]->GetNewApply(pApplyTag));
        }
      }
    }
  }
  else {
    m_SubElem=NULL;
  }
  return pApply;
}


/**
 ******************************************************************************
 * Name: CIccMpeCalculator::Apply
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
void CIccMpeCalculator::Apply(CIccApplyMpe *pApply, icFloatNumber *pDestPixel, const icFloatNumber *pSrcPixel) const
{
  CIccApplyMpeCalculator *pApplyCalc = (CIccApplyMpeCalculator*)pApply;
  icFloatNumber *pSrcTemp = pApplyCalc->m_temp;
  bool rv;

  pApplyCalc->m_temp = pSrcTemp;
  pApplyCalc->m_input = pSrcPixel;
  pApplyCalc->m_output = pDestPixel;

  if (m_bNeedTempReset) {
    memset(pSrcTemp, 0, m_nTempChannels*sizeof(icFloatNumber));
  }

  if (g_pDebugger) {
    g_pDebugger->BeginApply();
    rv = m_calcFunc->Apply(pApplyCalc);
    if (!rv)
      g_pDebugger->Error("Calc Function Apply Terminated with an error!");

    g_pDebugger->EndApply();
  }
  else {
    (void) m_calcFunc->Apply(pApplyCalc);
  }
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::Validate
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
icValidateStatus CIccMpeCalculator::Validate(std::string sigPath, std::string &sReport, const CIccTagMultiProcessElement* pMPE/*=NULL*/, const CIccProfile *pProfile/*=NULL*/) const
{
  std::string mpeSigPath = sigPath + icGetSigPath(GetType());
  icValidateStatus rv = CIccMultiProcessElement::Validate(sigPath, sReport, pMPE, pProfile);

  icUInt32Number i;

  if (m_SubElem) {
    for (i=0; i<m_nSubElem; i++) {
      if (m_SubElem[i])
        rv = icMaxStatus(rv, m_SubElem[i]->Validate(mpeSigPath, sReport, pMPE, pProfile));
    }
  }

  bool empty=false;
  if (m_calcFunc) {
    rv = icMaxStatus(rv, m_calcFunc->Validate(mpeSigPath, sReport, this, pProfile));
  }
  else
    empty = true;

  if (empty) {
    CIccInfo Info;
    std::string sSigPathName = Info.GetSigPathName(sigPath);

    sReport += icMsgValidateCriticalError;
    sReport += sSigPathName;
    sReport += " - Has an Empty Calculator Functions!\n";
    rv = icMaxStatus(rv, icValidateCriticalError);
  }

  return rv;
}


/**
 ******************************************************************************
 * Name: CIccMpeCalculator::IsLateBinding
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeCalculator::IsLateBinding() const
{
  icUInt32Number i;

  if (m_SubElem) {
    for (i=0; i<m_nSubElem; i++) {
      if (m_SubElem[i] && m_SubElem[i]->IsLateBinding())
        return true;
    }
  }

  return false;
}

/**
 ******************************************************************************
 * Name: CIccMpeCalculator::IsLateBindingReflectance
 * 
 * Purpose: 
 * 
 * Args: 
 * 
 * Return: 
 ******************************************************************************/
bool CIccMpeCalculator::IsLateBindingReflectance() const
{
  icUInt32Number i;

  if (m_SubElem) {
    for (i=0; i<m_nSubElem; i++) {
      if (m_SubElem[i] && m_SubElem[i]->IsLateBindingReflectance())
        return true;
    }
  }

  return false;
}


/**
******************************************************************************
* Name: CIccMpeCalculator::GetSubApply
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccMultiProcessElement *CIccMpeCalculator::GetElem(icSigCalcOp opsig, icUInt16Number index)
{
  if (m_SubElem && index<m_nSubElem) {
    CIccMultiProcessElement *pMpe = m_SubElem[index];
    if (opsig==icSigApplyElemOp)
      return pMpe;

    if ((opsig==icSigApplyCurvesOp && pMpe->GetType() != icSigCurveSetElemType) ||
        (opsig==icSigApplyMatrixOp && pMpe->GetType() != icSigMatrixElemType) ||
        (opsig==icSigApplyCLutOp && !(pMpe->GetType() == icSigCLutElemType || pMpe->GetType() == icSigExtCLutElemType)) ||
        (opsig==icSigApplyTintOp && pMpe->GetType() != icSigTintArrayElemType) ||
        (opsig==icSigApplyToJabOp && pMpe->GetType() != icSigXYZToJabElemType) ||
        (opsig==icSigApplyFromJabOp && pMpe->GetType() != icSigJabToXYZElemType) ||
        (opsig==icSigApplyCalcOp && pMpe->GetType() != icSigCalculatorElemType))
      return NULL;

    return pMpe;
  }

  return NULL;
}


/**
******************************************************************************
* Name: CIccMpeCalculator::GetSubApply
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccMpeCalculator::SetElem(icUInt32Number idx, CIccMultiProcessElement *pElem, icUInt32Number &count, CIccMultiProcessElement ***pArray)
{
  bool rv = true;

  if (idx + 1 > count) {
    if (*pArray) {
      *pArray = (CIccMultiProcessElement**)icRealloc(*pArray, (idx + 1) * sizeof(CIccMultiProcessElement*));

      if (!(*pArray))
        return false;

      icUInt32Number i;
      for (i = count; i <= idx; i++) {
        (*pArray)[i] = NULL;
      }
    }
    else {
      *pArray = (CIccMultiProcessElement**)calloc(idx + 1, sizeof(CIccMultiProcessElement*));

      if (!(*pArray))
        return false;
    }
    count = idx + 1;
  }
  else if (!(*pArray))
    return false;

  if ((*pArray)[idx]) {
    delete (*pArray)[idx];
    rv = false;
  }

  (*pArray)[idx] = pElem;

  return rv;
}

/**
******************************************************************************
* Name: CIccMpeCalculator::CIccApplyMpeCalculator
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccApplyMpeCalculator::CIccApplyMpeCalculator(CIccMultiProcessElement *pElem) : CIccApplyMpe(pElem)
{
  m_temp = NULL;

  m_input = NULL;
  m_output = NULL;
  
  m_stack = NULL;
  m_scratch = NULL;

  m_nSubElem = 0;
  m_SubElem = NULL;

}


/**
******************************************************************************
* Name: CIccMpeCalculator::~CIccApplyMpeCalculator
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccApplyMpeCalculator::~CIccApplyMpeCalculator()
{
  if (m_stack) {
    delete m_stack;
  }
  if (m_scratch) {
    delete m_scratch;
  }

  if (m_temp) {
    free(m_temp);
  }

  icUInt32Number i;

  if (m_SubElem) {
    for (i=0; i<m_nSubElem; i++) {
      if (m_SubElem[i])
        delete m_SubElem[i];
    }
  }
}

/**
******************************************************************************
* Name: CIccApplyMpeCalculator::GetSubApply
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
CIccSubCalcApply *CIccApplyMpeCalculator::GetApply(icUInt16Number index)
{
  if (m_SubElem && index<m_nSubElem)
    return m_SubElem[index];

  return NULL;
}


/**
******************************************************************************
* Name: CIccApplyMpeCalculator::GetSubApply
* 
* Purpose: 
* 
* Args: 
* 
* Return: 
******************************************************************************/
bool CIccApplyMpeCalculator::GetEnvVar(icSigCmmEnvVar sigEnv, icFloatNumber &val)
{
  if (!m_pCmmEnvVarLookup) {
    val = 0;
    return false;
  }
  return m_pCmmEnvVarLookup->GetEnvVar(sigEnv, val);
}



