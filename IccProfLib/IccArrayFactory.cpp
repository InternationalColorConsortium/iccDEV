/** @file
    File:       IccArrayFactory.cpp

    Contains:   Implementation of the IIccArray handler creation factories

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * The ICC Software License, Version 0.2
 *
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
// -July 27, 2011
// Added IIccArray Creation using factory support
//
//////////////////////////////////////////////////////////////////////

#include "IccTagComposite.h"
#include "IccArrayBasic.h"
#include "IccArrayFactory.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include <string.h>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

IIccArray* CIccBasicArrayFactory::CreateArray(icArraySignature arrayTypeSig, CIccTagArray *pTagArray)
{
  switch(arrayTypeSig) {
    case icSigNamedColorArray:
      return new CIccArrayNamedColor(pTagArray);

    case icSigColorantInfoArray:
      return new CIccArrayColorantInfo(pTagArray);

    default:
      return new CIccArrayUnknown(pTagArray, arrayTypeSig);
  }
}


static struct {
  icArraySignature sig;
  const icChar *szArrayName;
} g_icArrayNames[] = {
  { icSigColorantInfoArray, "colorantInfoArray" },
  { icSigNamedColorArray, "namedColorArray" },
  { icSigUtf8TextTypeArray, "UTF8TextArray" },
  { (icArraySignature)0, "" },
};

bool CIccBasicArrayFactory::GetArraySigName(std::string &arrayName, icArraySignature arrayTypeSig, bool bFindUnknown)
{
  int i;
  for (i = 0; g_icArrayNames[i].sig; i++) {
    if (g_icArrayNames[i].sig == arrayTypeSig) {
      arrayName = g_icArrayNames[i].szArrayName;
      return true;
    }
  }

  if (!bFindUnknown) {
    char sig[20];
    arrayName = "UnknownStruct_";
    icGetSigStr(sig, arrayTypeSig);
    arrayName += sig;
  }
  else {
    arrayName = "";
  }

  return false;
}

icArraySignature CIccBasicArrayFactory::GetArraySig(const icChar *szArrayName)
{
  int i;
  for (i = 0; g_icArrayNames[i].sig; i++) {
    if (!strcmp(g_icArrayNames[i].szArrayName, szArrayName)) {
      return g_icArrayNames[i].sig;
    }
  }
  return (icArraySignature)0;
}


std::unique_ptr<CIccArrayCreator> CIccArrayCreator::theArrayCreator;

CIccArrayCreator::~CIccArrayCreator()
{
  IIccArrayFactory *pFactory = DoPopFactory(true);

  while (pFactory) {
    delete pFactory;
    pFactory = DoPopFactory(true);
  }
}

CIccArrayCreator* CIccArrayCreator::GetInstance()
{
  if (!theArrayCreator.get()) {
    theArrayCreator = CIccArrayCreatorPtr(new CIccArrayCreator);

    theArrayCreator->DoPushFactory(new CIccBasicArrayFactory);
  }

  return theArrayCreator.get();
}

IIccArray* CIccArrayCreator::DoCreateArray(icArraySignature arrayTypeSig, CIccTagArray *pTagArray)
{
  CIccArrayFactoryList::iterator i;
  IIccArray *rv = NULL;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    rv = (*i)->CreateArray(arrayTypeSig, pTagArray);
    if (rv)
      break;
  }
  return rv;
}

bool CIccArrayCreator::DoGetArraySigName(std::string &arrayName, icArraySignature arrayTypeSig, bool bFillUnknown)
{
  CIccArrayFactoryList::iterator i;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    if ((*i)->GetArraySigName(arrayName, arrayTypeSig, bFillUnknown))
      return true;
  }

  return false;
}

icArraySignature CIccArrayCreator::DoGetArraySig(const icChar* structName)
{
  CIccArrayFactoryList::iterator i;

  for (i = factoryStack.begin(); i != factoryStack.end(); i++) {
    icArraySignature rv = (*i)->GetArraySig(structName);
    if (rv)
      return rv;
  }

  return (icArraySignature)0;
}


void CIccArrayCreator::DoPushFactory(IIccArrayFactory *pFactory)
{
  factoryStack.push_front(pFactory);
}

IIccArrayFactory* CIccArrayCreator::DoPopFactory(bool bAll /*=false*/)
{
  if (factoryStack.size()>0) {
    CIccArrayFactoryList::iterator i=factoryStack.begin();
    IIccArrayFactory* rv = (*i);
    factoryStack.pop_front();
    return rv;
  }
  return NULL;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
