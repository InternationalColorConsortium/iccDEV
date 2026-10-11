/** @file
    File:       IccXformFactory.cpp

    Contains:   Implementation of the CIccXform class and creation factories

    Version:    V1

    Copyright:  (c) see ICC Software License
*/

/*
 * Copyright (c) International Color Consortium.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *
 *     3. Neither the name of International Color Consortium (ICC) nor the names of
 *        its contributors may be used to endorse or promote products derived from
 *        this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Oct 30, 2005
// Added CICCXform Creation using factory support
//
//////////////////////////////////////////////////////////////////////

#include "IccXformFactory.h"
#include <new>

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

CIccXform* CIccBaseXformFactory::CreateXform(icXformType xformSig, CIccTag *pTag/*=NULL*/, CIccCreateXformHintManager *pHintManager/*=NULL*/)
{
  //We generally ignore pHint in the base creator (used by others to determine what form of xform to create)
  switch(xformSig) {
   case icXformTypeMatrixTRC:
     return new(std::nothrow) CIccXformMatrixTRC();

   case icXformType3DLut:
     return new(std::nothrow) CIccXform3DLut(pTag);

   case icXformType4DLut:
     return new(std::nothrow) CIccXform4DLut(pTag);

   case icXformTypeNDLut:
     return new(std::nothrow) CIccXformNDLut(pTag);

   case icXformTypeNamedColor:
     if (pHintManager) {
			 IIccCreateXformHint* pHint = pHintManager->GetHint("CIccCreateNamedColorXformHint");
			 if (pHint) {
				 CIccCreateNamedColorXformHint *pNCHint = (CIccCreateNamedColorXformHint*)pHint;
				 return new(std::nothrow) CIccXformNamedColor(pTag, pNCHint->csPcs, pNCHint->csDevice,
                                        pNCHint->csSpectralPcs,
                                        &pNCHint->spectralRange,
                                        &pNCHint->biSpectralRange,
                                        pNCHint->nOverprintType);
			 }
     }
		 return NULL;

   case icXformTypeMpe:
     return new(std::nothrow) CIccXformMpe(pTag);

	 case icXformTypeMonochrome:
		 return new(std::nothrow) CIccXformMonochrome();

    default:
      return NULL;
  }
}

std::unique_ptr<CIccXformCreator> CIccXformCreator::theXformCreator;

CIccXformCreator::~CIccXformCreator()
{
  IIccXformFactory *pFactory = DoPopFactory(true);

  while (pFactory) {
    delete pFactory;
    pFactory = DoPopFactory(true);
  }
}

CIccXformCreator* CIccXformCreator::GetInstance()
{
  if (!theXformCreator.get()) {
    theXformCreator = CIccXformCreatorPtr(new CIccXformCreator);

    theXformCreator->DoPushFactory(new CIccBaseXformFactory);
  }

  return theXformCreator.get();
}

CIccXform* CIccXformCreator::DoCreateXform(icXformType xformTypeSig, CIccTag *pTag/*=NULL*/, CIccCreateXformHintManager *pHintManager/*=NULL*/)
{
  CIccXformFactoryList::iterator i;
  CIccXform *rv = NULL;

  for (i=factoryStack.begin(); i!=factoryStack.end(); i++) {
    rv = (*i)->CreateXform(xformTypeSig, pTag, pHintManager);
    if (rv)
      break;
  }
  return rv;
}

void CIccXformCreator::DoPushFactory(IIccXformFactory *pFactory)
{
  factoryStack.push_front(pFactory);
}

IIccXformFactory* CIccXformCreator::DoPopFactory(bool /* bAll =false */)
{
  //int nNum = (bAll ? 0 : 1);

  if (factoryStack.size()>0) {
    CIccXformFactoryList::iterator i=factoryStack.begin();
    IIccXformFactory* rv = (*i);
    factoryStack.pop_front();
    return rv;
  }
  return NULL;
}

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif
