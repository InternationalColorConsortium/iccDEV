/** @file
    File:       IccStructFactory.h

    Contains:   Header for implementation of CIccStructFactory class and
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

////////////////////////////////////////////////////////////////////// 
// HISTORY:
//
// -Jun 10, 2011 
//  A CIccStructCreator singleton class has been added to provide general
//  support for dynamically creating struct object classes using a signature.
//  Prototype and private struct type support can be added to the system
//  by pushing additional IIccStructFactory based objects to the 
//  singleton CIccStructCreator object.
//
/////////////////////////////////////////////////////////////////////

#ifndef _ICCSTRUCTFACTORY_H
#define _ICCSTRUCTFACTORY_H

#include "IccDefs.h"
#include <memory>
#include <list>

//CIccProcessStruct factory support
#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class IIccStruct;
class CIccTagStruct;

/**
 ***********************************************************************
 * Class: IIccStructFactory
 *
 * Purpose:
 * IIccStructFactory is a factory pattern interface for CIccProcessStruct
 * creation.
 * This class is pure virtual.
 ***********************************************************************
 */
class ICCPROFLIB_API IIccStructFactory 
{
public:
  virtual ~IIccStructFactory() {}

  /**
  * Function: CreateStruct(structTypeSig)
  *  Create a element of type structTypeSig.
  *
  * Parameter(s):
  *  structTypeSig = signature of the ICC element type for the element to
  *  be created
  *
  * Returns a new IIStructObject object of the given signature type.
  * If the element factory doesn't support creation of structs of type
  * structTypeSig then it should return NULL.
  */
  virtual IIccStruct* CreateStruct(icStructSignature structTypeSig, CIccTagStruct *pTagStruct=NULL)=0;

  /**
  * Function: GeStructSigName(structTypeSig)
  *  Get display name of structTypeSig.
  *
  * Parameter(s):
  *  structName = string to put element name into, 
  *  structTypeSig = signature of the ICC element type to get a name for
  *  bFindUnknown = flag to indicate structName should be filled with unknown_sig name, (empty if false)
  *
  * Returns true if struct type is recognized by the factory, false if
  * the factory doesn't create structTypeSig elements.
  */
  virtual bool GetStructSigName(std::string &structName, icStructSignature structTypeSig, bool bFindUnknown)=0;

  /**
  * Function: GetStructSig(structName)
  *  Get signature of structure from display name.
  *
  * Parameter(s):
  *  structName = struct display name
  *
  * Returns signature of structure if it is recognized by the factory, 0 if the
  * factory doesn't create structures with such a display name.
  */
  virtual icStructSignature GetStructSig(const icChar *structName)=0;
};


//A CIccStructFactoryList is used by CIccStructCreator to keep track of element
//creation factories
typedef std::list<IIccStructFactory*> CIccStructFactoryList;


/**
 ***********************************************************************
 * Class: CIccBasicStructFactory
 *
 * Purpose:
 * CIccBasicStructFactory provides creation of CIccProcessStruct's 
 * defined by the ICC profile specification.  The CIccStructCreator always
 * creates a CIccBasicElemFactory.
 ***********************************************************************
 */
class CIccBasicStructFactory : public IIccStructFactory
{
public:
  /**
  * Function: CreateStruct(structTypeSig)
  *  Create a struct object of type structTypeSig.
  *
  * Parameter(s):
  *  structTypeSig = signature of the ICC struct type for the struct to be created
  *
  * Returns a new CIccProcessStruct object of the given signature type.
  * Unrecognized structTypeSig's will be created as a CIccStructUnknown object.
  */
  virtual IIccStruct* CreateStruct(icStructSignature structSig, CIccTagStruct *pTagStruct=NULL);

  /**
  * Function: GetStructSigName(structTypeSig)
  *  Get display name of structTypeSig.
  *
  * Parameter(s):
  *  structName = string to put struct name into, 
  *  structTypeSig = signature of the ICC struct type to get a name for
  *
  * Returns true if struct type is recognized by the factory, false if the
  * factory doesn't create structTypeSig structs.
  */
  virtual bool GetStructSigName(std::string &structName, icStructSignature structTypeSig, bool bFindUnknown=false);

  /**
  * Function: GetStructSig(structName)
  *  Get signature of structure from display name.
  *
  * Parameter(s):
  *  structName = struct display name
  *
  * Returns signature of structure if it is recognized by the factory, 0 if the
  * factory doesn't create structures with such a display name.
  */
  virtual icStructSignature GetStructSig(const icChar *structName);
};

class CIccStructCreator;

typedef std::unique_ptr<CIccStructCreator> CIccStructCreatorPtr;

/**
 ***********************************************************************
 * Class: CIccStructCreator
 *
 * Purpose:
 * CIccStructCreator uses a singleton pattern to provide dynamically
 * upgradeable IIccStructObject derived object creation based on
 * element signature.
 ***********************************************************************
 */
class CIccStructCreator 
{
public:
  ~CIccStructCreator();

  /**
  * Function: CreateStruct(structTypeSig)
  *  Create a element of type structTypeSig.
  *
  * Parameter(s):
  *  structTypeSig = signature of the ICC element type for the element to
  *  be created
  *
  * Returns a new CIccProcessStruct object of the given signature type.
  * Each factory in the factoryStack is used until a factory supports the
  * signature type.
  */
  static IIccStruct* CreateStruct(icStructSignature structTypeSig, CIccTagStruct *pTagStruct)
      { return CIccStructCreator::GetInstance()->DoCreateStruct(structTypeSig, pTagStruct); }

  /**
  * Function: GetStructSigName(structTypeSig)
  *  Get display name of structTypeSig.
  *
  * Parameter(s):
  *  structName = string to put struct name into
  *  structTypeSig = signature of the ICC struct type to get a name for
  *  bFillUnknown = flag to indicate structName is filled if not found
  *
  * Returns true if struct type is recognized by any factory, false if all
  * factories do not create structTypeSig structs. If struct type is not
  * recognized by any factories a suitable display name will be placed in
  * structName if bFillUknown is true.
  */
  static bool GetStructSigName(std::string &structName, icStructSignature structTypeSig, bool bFillUnknown=true)
      { return CIccStructCreator::GetInstance()->DoGetStructSigName(structName, structTypeSig, bFillUnknown); }

  /**
  * Function: GetStructSig(structName)
  *  Get signature of structure from display name.
  *
  * Parameter(s):
  *  structName = struct display name
  *
  * Returns signature of structure if it is recognized by any factory, 0 if the
  * all the factories don't create a structure with such a display name.
  */
  static icStructSignature GetStructSig(const icChar *structName)
  {
    return CIccStructCreator::GetInstance()->DoGetStructSig(structName);
  }

  /**
  * Function: PushFactory(pFactory)
  *  Add an IIccStructFactory to the stack of element factories tracked by
  *  the system.
  *
  * Parameter(s):
  *  pFactory = pointer to an IIccStructFactory object to add to the
  *    system.  The pFactory must be created with new, and will be owned
  *    CIccStructCreator until popped off the stack using PopFactory().
  *    Any factories not popped off will be taken care of properly on
  *    application shutdown.
  *
  */
  static void PushFactory(IIccStructFactory *pFactory)
      { CIccStructCreator::GetInstance()->CIccStructCreator::DoPushFactory(pFactory); }

  /**
  * Function: PopFactory()
  *  Remove the top IIccStructFactory from the stack of element factories
  *  tracked by the system.
  *
  * Parameter(s):
  *  None
  *
  *  Returns the top IIccStructFactory from the stack of element factories
  *  tracked by the system.  The returned element factory is no longer 
  *  owned by the system and needs to be deleted to avoid memory leaks.
  *
  *  Note: The initial CIccBasicElemFactory cannot be popped off the stack.
  */
  static IIccStructFactory* PopFactory()
      { return CIccStructCreator::GetInstance()->DoPopFactory(); }

private:
  /**Only GetInstance() can create the singleton*/
  CIccStructCreator() { }

  /**
  * Function: GetInstance()
  *  Private static function to access singleton CiccStructCreator Object.
  *
  * Parameter(s):
  *  None
  *
  * Returns the singleton CIccStructCreator object.  It will allocate
  * a new one and push a single CIccSpecStruct Factory object onto the
  * factory stack if the singleton has not been intialized.
  */
  static CIccStructCreator* GetInstance();

  IIccStruct* DoCreateStruct(icStructSignature structTypeSig, CIccTagStruct *pTagStruct=NULL);
  bool DoGetStructSigName(std::string &structName, icStructSignature structTypeSig, bool bFillUnknown=true);
  icStructSignature DoGetStructSig(const char *structName);
  void DoPushFactory(IIccStructFactory *pFactory);
  IIccStructFactory* DoPopFactory(bool bAll=false);

  static CIccStructCreatorPtr theStructCreator; 

  CIccStructFactoryList factoryStack;
};

#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif //_ICCSTRUCTFACTORY_H
