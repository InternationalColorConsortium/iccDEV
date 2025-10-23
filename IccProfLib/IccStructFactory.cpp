/** @file
    File:       IccStructFactory.cpp

    Contains:   Implementation of the IIccStructObject class and creation factories

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
// -June 4, 2011
// Added IIccStructObject Creation using factory support
//
//////////////////////////////////////////////////////////////////////

#include "IccTagComposite.h"
#include "IccStructBasic.h"
#include "IccStructFactory.h"
#include "IccUtil.h"
#include "IccProfile.h"
#include <cstring>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

IIccStruct* CIccBasicStructFactory::CreateStruct(icStructSignature structTypeSig, CIccTagStruct *pTagStruct)
{
  switch(structTypeSig) {
    case icSigBRDFStruct:
      return new CIccStructBRDF(pTagStruct);

    case icSigColorantInfoStruct:
      return new CIccStructColorantInfo(pTagStruct);

    case icSigColorEncodingParamsSruct:
      return new CIccStructColorEncodingParams(pTagStruct);

    case icSigMeasurementInfoStruct:
      return new CIccStructMeasurementInfo(pTagStruct);

    case icSigNamedColorStruct:
      return new CIccStructNamedColor(pTagStruct);

    case icSigProfileInfoStruct:
      return new CIccStructProfileInfo(pTagStruct);

    case icSigTintZeroStruct:
      return new CIccStructTintZero(pTagStruct);

    default:
      return new CIccStructUnknown(pTagStruct);
  }
}

static struct {
  icStructSignature sig;
  const icChar *szStructName;
} g_icStructNames[] = {
  {icSigBRDFStruct, "brdfTransfromStructure"},
  {icSigColorantInfoStruct, "colorantInfoStruct"},
  {icSigBRDFStruct, "brfdfTransformStructure"},
  {icSigColorantInfoStruct, "colorantInfoStructure"},
  {icSigColorEncodingParamsSruct, "colorEncodingParamsStructure"},
  {icSigMeasurementInfoStruct, "measurementInfoStructure"},
  {icSigNamedColorStruct, "namedColorStructure"},
  {icSigProfileInfoStruct, "profileInfoStructure"},
  {icSigTintZeroStruct, "tintZeroStructure"},
  {(icStructSignature)0, ""},
};

bool CIccBasicStructFactory::GetStructSigName(std::string &structName, icStructSignature structTypeSig, bool bFindUnknown)
{
  int i;
  for (i = 0; g_icStructNames[i].sig; i++) {
    if (g_icStructNames[i].sig == structTypeSig) {
      structName = g_icStructNames[i].szStructName;
      return true;
    }
  }

  if (!bFindUnknown) {
    char sig[20];
    structName = "UnknownStruct_";
    icGetSigStr(sig, structTypeSig);
    structName += sig;
  }
  else {
    structName = "";
  }

  return false;
}

icStructSignature CIccBasicStructFactory::GetStructSig(const icChar *szStructName)
{
  int i;
  for (i = 0; g_icStructNames[i].sig; i++) {
    if (!strcmp(g_icStructNames[i].szStructName, szStructName)) {
      return g_icStructNames[i].sig;
    }
  }
  return (icStructSignature)0;
}


std::unique_ptr<CIccStructCreator> CIccStructCreator::theStructCreator;

CIccStructCreator::~CIccStructCreator()
{
  IIccStructFactory *pFactory = DoPopFactory(true);

  while (pFactory) {
    delete pFactory;
    pFactory = DoPopFactory(true);
  }
}

CIccStructCreator* CIccStructCreator::GetInstance()
{
  if (!theStructCreator.get()) {
    theStructCreator = CIccStructCreatorPtr(new CIccStructCreator);

    theStructCreator->DoPushFactory(new CIccBasicStructFactory);
  }

  return theStructCreator.get();
}

IIccStruct* CIccStructCreator::DoCreateStruct(icStructSignature structTypeSig, CIccTagStruct *pTagStruct)
{
  CIccStructFactoryList::iterator i;
  IIccStruct *rv = NULL;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    rv = (*i)->CreateStruct(structTypeSig, pTagStruct);
    if (rv)
      break;
  }
  return rv;
}

bool CIccStructCreator::DoGetStructSigName(std::string &structName, icStructSignature structTypeSig, bool bFillUnknown)
{
  CIccStructFactoryList::iterator i;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    if ((*i)->GetStructSigName(structName, structTypeSig, bFillUnknown))
      return true;
  }

  return false;
}

icStructSignature CIccStructCreator::DoGetStructSig(const icChar* structName)
{
  CIccStructFactoryList::iterator i;

  for (i = factoryStack.begin(); i != factoryStack.end(); i++) {
    icStructSignature rv = (*i)->GetStructSig(structName);
    if (rv)
      return rv;
  }

  return (icStructSignature)0;
}

void CIccStructCreator::DoPushFactory(IIccStructFactory *pFactory)
{
  factoryStack.push_front(pFactory);
}

IIccStructFactory* CIccStructCreator::DoPopFactory(bool bAll /*=false*/)
{
  if (factoryStack.size()>0) {
    CIccStructFactoryList::iterator i=factoryStack.begin();
    IIccStructFactory* rv = (*i);
    factoryStack.pop_front();
    return rv;
  }
  return NULL;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
