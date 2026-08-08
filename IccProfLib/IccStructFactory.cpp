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
      return new (std::nothrow) CIccStructBRDF(pTagStruct);

    case icSigColorantInfoStruct:
      return new (std::nothrow) CIccStructColorantInfo(pTagStruct);

    case icSigColorEncodingParamsSruct:
      return new (std::nothrow) CIccStructColorEncodingParams(pTagStruct);

    case icSigMeasurementInfoStruct:
      return new (std::nothrow) CIccStructMeasurementInfo(pTagStruct);

    case icSigNamedColorStruct:
      return new (std::nothrow) CIccStructNamedColor(pTagStruct);

    case icSigProfileConnectionConditionsStruct:
      return new (std::nothrow) CIccStructProfileConnectionConditions(pTagStruct);

    case icSigProfileInfoStruct:
      return new (std::nothrow) CIccStructProfileInfo(pTagStruct);

    case icSigTintZeroStruct:
      return new (std::nothrow) CIccStructTintZero(pTagStruct);

    default:
      return new (std::nothrow) CIccStructUnknown(pTagStruct);
  }
}

// The published name for each struct signature: exactly one entry per signature,
// because GetStructSigName below returns the first match and any later entry for the
// same signature can never be emitted.  Three surfaces consult it -- both directions
// of CIccTagJsonStruct (IccTagJson.cpp), both directions of CIccTagXmlStruct
// (IccTagXml.cpp), and the emitted name in CIccTagStruct::Describe
// (IccTagComposite.cpp), which is what iccDumpProfile prints.
//
// #2028: icSigBRDFStruct and icSigColorantInfoStruct each appeared here twice, with
// the wrong spelling in the first -- published -- slot, which broke JSON round-tripping
// outright for brdf.  Two independent sources have to agree for a named struct tag to
// survive a JSON round trip and these two did not: CIccTagJsonStruct::ToJson writes
// pStruct->GetDisplayName() (IccStructBasic.h), while CIccTagJsonStruct::ParseJson reads
// back through GetStructSig, i.e. this table.  GetDisplayName has always returned the
// correct "brdfTransformStructure", and that string matched neither brdf entry --
// "Transfrom" in one and "brfdf" in the other -- so ParseJson left sigStruct 0, never
// called SetTagStructType, and appended nothing to parseStr: the struct type was
// silently discarded and the parse still reported success.
//
// XML broke too, and louder.  CIccTagXmlStruct::ToXml writes GetDisplayName() as the
// element name for a named struct and emits a signature only in its privateStruct
// branch, exactly as ToJson does, so it also produced <brdfTransformStructure>.
// CIccTagXmlStruct::ParseXml resolves that element name through this table and, on a
// miss, looks for a <StructureSignature> element -- a form only hand-authored documents
// carry, never the writer's own named-struct output.  So the fallback could not fire and
// the failure was total rather than silent: "Unable to find StructureSignature" and the
// whole document refused to load.  Measured both ways before the fix.
//
// Neither serialization had a fixture covering it, because there is no BRDF struct tag
// anywhere in Testing/ in either format -- the three <StructureSignature>brdf references
// are all inside commented-out examples.
//
// colorantInfoStruct was not a typo but broke the ...Structure suffix every other name
// in this table shares, and its own second entry already carried the correct form; that
// one only ever mis-published a name -- via Describe, hence iccDumpProfile -- and read
// back fine either way.
static struct {
  icStructSignature sig;
  const icChar *szStructName;
} g_icStructNames[] = {
  {icSigBRDFStruct, "brdfTransformStructure"},
  {icSigColorantInfoStruct, "colorantInfoStructure"},
  {icSigColorEncodingParamsSruct, "colorEncodingParamsStructure"},
  {icSigMeasurementInfoStruct, "measurementInfoStructure"},
  {icSigNamedColorStruct, "namedColorStructure"},
  {icSigProfileConnectionConditionsStruct, "profileConnectionConditionsStructure"},
  {icSigProfileInfoStruct, "profileInfoStructure"},
  {icSigTintZeroStruct, "tintZeroStructure"},
  {(icStructSignature)0, ""},
};

// Legacy spellings, read-only: only GetStructSig consults this table, so a document
// that still carries one of these keeps loading while nothing emits it again.  That
// asymmetry is the contract #2023 established for g_icAltTagNameTable, expressed here
// as a separate table rather than as trailing duplicates in the primary one.  With the
// duplicates removed the primary table runs one entry per signature in the order the
// icStructSignature enum declares them, which is also the order of the CreateStruct
// switch above it; while duplicates lived there, the entry that got published was
// decided by position alone, so any reordering -- something that reads as harmless
// tidying next to a switch it now matches -- silently changed which spelling iccDEV
// emitted.  Keeping the legacy names in their own table removes that coupling.
static struct {
  icStructSignature sig;
  const icChar *szStructName;
} g_icAltStructNames[] = {
  {icSigBRDFStruct, "brdfTransfromStructure"},      // published by this library before #2028
  {icSigBRDFStruct, "brfdfTransformStructure"},     // parsed but never emitted, also misspelled
  {icSigColorantInfoStruct, "colorantInfoStruct"},  // published by this library before #2028
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
    icGetSigStr(sig, 20, structTypeSig);
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

  //Allow conversion from legacy names (backwards compatibility with earlier versions).
  //Published names are searched first, so a legacy spelling cannot shadow a published one
  //of the same text -- the ordering hazard #2025 had to correct in GetTagNameSig, avoided
  //here by construction rather than by the order the two tables happen to be written in.
  for (i = 0; g_icAltStructNames[i].sig; i++) {
    if (!strcmp(g_icAltStructNames[i].szStructName, szStructName)) {
      return g_icAltStructNames[i].sig;
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
  if (!structName || !structName[0])
    return (icStructSignature)0;

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

IIccStructFactory* CIccStructCreator::DoPopFactory(bool /* bAll =false*/)
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
