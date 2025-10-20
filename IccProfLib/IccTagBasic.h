/** @file
    File:       IccTagBasic.h

    Contains:   Header for implementation of the CIccTag class
                and inherited classes

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
// -Initial implementation by Max Derhak 5-15-2003
//
//////////////////////////////////////////////////////////////////////

#if !defined(_ICCTAGBASIC_H)
#define _ICCTAGBASIC_H

#include <list>
#include <string>
#include "IccDefs.h"

#ifdef USEICCDEVNAMESPACE
namespace iccDEV {
#endif

class CIccIO;

class ICCPROFLIB_API CIccProfile;

class CIccSparseMatrix;

class IIccExtensionTag
{
public:
  virtual const char *GetExtClassName() const =0;
  virtual const char *GetExtDerivedClassName() const =0;
};

/**
 ***********************************************************************
 * Class: CIccTag
 *
 * Purpose:
 *  CIccTag is the base class that all Icc Tags are derived
 *  from.  It defines basic tag functionality, and provides
 *  a static function that acts as an object construction
 *  factory.
 ***********************************************************************
 */
class ICCPROFLIB_API CIccTag  
{
public:
  CIccTag();

  /**
  * Function: NewCopy(sDescription)
  *  Each derived tag will implement it's own NewCopy() function.
  *
  * Parameter(s):
  *  none
  *
  * Returns a new CIccTag object that is a copy of this object.
  */
  virtual CIccTag* NewCopy() const {return new CIccTag;}

  virtual ~CIccTag();

  /**
  * Function: GetType() 
  * 
  * Purpose: Get Tag Type.
  *  Each derived tag will implement it's own GetType() function.
  */
  virtual icTagTypeSignature GetType() const { return icMaxEnumType; }
  virtual icStructSignature GetTagStructType() const { return icSigUndefinedStruct; }
  virtual icArraySignature GetTagArrayType() const { return icSigUndefinedArray; }

  virtual bool IsArrayType() { return false; }
  virtual bool IsMBBType() { return false; } //If true then CIccTag can be cast as an CIccMBB 
  virtual bool IsNumArrayType() const { return false;} //If true then CIccTag can be cast as a CIccTagNumArray

  virtual const icChar *GetClassName() const { return "CIccTag"; }

  static CIccTag* Create(icTagTypeSignature sig);

  virtual IIccExtensionTag* GetExtension() {return NULL;}

  /**
  * Function: IsSupported(size, pIO) - Check if tag fully
  *  supported for apply purposes.  By Default inherited
  *  classes are supported.  Unknown tag types are not
  *  supported.
  *
  * Returns true if tag type is supported.
  */
  virtual bool IsSupported() { return true; }

  /**
  * Function: Read(size, pIO) - Read tag from file.
  *  Each derived tag will implement it's own Read() function.
  *
  * Parameter(s):
  * size - number of bytes in tag including the type signature.
  * pIO - IO object used to read in tag. The IO object should
  *       already be initialized to point to the begining of
  *       the tag.
  *
  * Returns true if Read is successful.
  */
  virtual bool Read(icUInt32Number size, CIccIO *pIO) { return false; }

  /**
  * Function: ReadAll() - Read All sub data for tag from file.
  *  Called by CIccProfile::ReadAll() to read all sub data for tag
  *
  * Returns true if ReadAll is successful.
  */
  virtual bool ReadAll() { return true; }

  /**
  * Function: ReadAll() - Read All sub data for tag from file.
  *  Called by CIccProfile::ReadAll() to read all sub data for tag
  *
  * Returns true if ReadAll is successful.
  */
  virtual void DetachIO() {}

  /**
  * Function: Read(size, pIO) - Read tag from file.
  *  Each derived tag will implement it's own Read() function.
  *
  * Parameter(s):
  * size - number of bytes in tag including the type signature.
  * pIO - IO object used to read in tag. The IO object should
  *       already be initialized to point to the begining of
  *       the tag.
  *
  * Returns true if Read is successful.
  */
  virtual bool Read(icUInt32Number size, CIccIO *pIO, CIccProfile *pProfile) { return Read(size, pIO); }


  /**
  * Function: Write(pIO)
  *  Each derived tag will implement it's own Write() function.
  *
  * Parameter(s):
  * pIO - IO object used to write a tag. The IO object should
  *       already be initialized to point to the begining of
  *       the tag.
  *
  * Returns true if Write is successful.
  */
  virtual bool Write(CIccIO *pIO) { return false; }

  /**
  * Function: Describe(sDescription)
  *  Each derived tag will implement it's own Describe() function.
  *
  * Parameter(s):
  * sDescription - A string to put the tag's description into.
* * verbosenss   - integer value. Default=0. The larger the value, the more verbose the output. 
  */
  virtual void Describe(std::string &sDescription, int nVerboseness=0) { sDescription.clear(); }

  /**
   ******************************************************************************
   * Function: Validate
   *  Each derived tag will implement it's own IsValid() function
   * 
   * Parameter(s):
   * sig - signature of tag being validated,
   * sDescription - A string to put tag validation report.
   */
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  //All tags start with a reserved value.  Allocate a place to put it.
  icUInt32Number m_nReserved;
};


/**
****************************************************************************
* Class: IccTagUnknown
* 
* Purpose: The general purpose I don't know tag.
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagUnknown : public CIccTag
{
public:
  CIccTagUnknown();
  CIccTagUnknown(const CIccTagUnknown &ITU);
  CIccTagUnknown &operator=(const CIccTagUnknown &UnknownTag);
  virtual CIccTag* NewCopy() const {return new CIccTagUnknown(*this);}
  virtual ~CIccTagUnknown();

  virtual bool IsSupported() { return false; }

  virtual icTagTypeSignature GetType() const { return m_nType; }
  virtual const icChar *GetClassName() const { return "CIccTagUnknown"; }

  virtual icUInt32Number GetSize() const { return m_nSize; }
  virtual const icUInt8Number* GetData() const { return m_pData; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);


protected:
  icTagTypeSignature m_nType;
  icUInt8Number *m_pData;
  icUInt32Number m_nSize;
};


/**
****************************************************************************
* Class: CIccTagText()
* 
* Purpose: The textType ICC tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagText : public CIccTag
{
public:
  CIccTagText();
  CIccTagText(const CIccTagText &ITT);
  CIccTagText &operator=(const CIccTagText &TextTag);
  virtual CIccTag* NewCopy() const {return new CIccTagText(*this);}
  virtual ~CIccTagText();

  virtual icTagTypeSignature GetType() const { return icSigTextType; }
  virtual const icChar *GetClassName() const { return "CIccTagText"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);

  const icChar *GetText() const { return m_szText; }
  void SetText(const icChar *szText);
  const icChar *operator=(const icChar *szText);

  icChar *GetBuffer(icUInt32Number nSize);
  void Release();
  icUInt32Number Capacity() const { return m_nBufSize; }
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icChar *m_szText;
  icUInt32Number m_nBufSize;
};

/**
****************************************************************************
* Class: CIccTagUtf8Text()
* 
* Purpose: The utf8TextType ICC tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagUtf8Text : public CIccTag
{
public:
  CIccTagUtf8Text();
  CIccTagUtf8Text(const CIccTagUtf8Text &ITT);
  CIccTagUtf8Text &operator=(const CIccTagUtf8Text &TextTag);
  virtual CIccTag* NewCopy() const {return new CIccTagUtf8Text(*this);}
  virtual ~CIccTagUtf8Text();

  virtual icTagTypeSignature GetType() const { return icSigUtf8TextType; }
  virtual const icChar *GetClassName() const { return "CIccUtf8TagText"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);

  const icUChar *GetText() const { return m_szText; }
  
  void SetText(const icUChar16 *szText);
  void SetText(const icUChar *szText);
  void SetText(const icChar *szText) { SetText((icUChar*)szText); }
  
  const icUChar *operator=(const icUChar *szText);
  const icChar *operator=(const icChar *szText) { return (const icChar*)operator=((icUChar*)szText); }

  icUChar *GetBuffer(icUInt32Number nSize);
  void Release();
  icUInt32Number Capacity() const { return m_nBufSize; }
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icUChar *m_szText;
  icUInt32Number m_nBufSize;
};

/**
****************************************************************************
* Class: CIccTagZipUtf8Text()
* 
* Purpose: The zipUtf8TextType ICC tag maintains compressed text data.
*  Calls to GetText will uncompress, and SetText will compress.  These
*  functions may fail if macro ICC_USE_ZLIB is not defined.
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagZipUtf8Text : public CIccTag
{
public:
  CIccTagZipUtf8Text();
  virtual ~CIccTagZipUtf8Text();
  CIccTagZipUtf8Text(const CIccTagZipUtf8Text &ITT);
  CIccTagZipUtf8Text &operator=(const CIccTagZipUtf8Text &TextTag);
  virtual CIccTag* NewCopy() const {return new CIccTagZipUtf8Text(*this);}

  virtual icTagTypeSignature GetType() const { return icSigZipUtf8TextType; }
  virtual const icChar *GetClassName() const { return "CIccZipUtf8TagText"; }

  /* GetText() may fail if ICC_USE_ZLIB is not defined*/
  virtual bool GetText(std::string &str) const;

  /* SetText() may fail if ICC_USE_ZLIB is not defined*/
  bool SetText(const icUChar16 *szText);
  virtual bool SetText(const icUChar *szText);
  bool SetText(const icChar *szText) { return SetText((icUChar*)szText); }

  /*Note: operator= is not supported because ZLIB support might not be enabled*/

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);

  /* Get access to raw zip compression buffer */
  icUChar *AllocBuffer(icUInt32Number nSize);
  icUChar *GetBuffer() const { return m_pZipBuf; }
  icUInt32Number BufferSize() const { return m_nBufSize; }
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icUChar *m_pZipBuf;
  icUInt32Number m_nBufSize;
};


/**
****************************************************************************
* Class: CIccTagZipXml()
* 
* Purpose: The zipXMLType ICC tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagZipXml : public CIccTagZipUtf8Text
{
public:
  CIccTagZipXml() : CIccTagZipUtf8Text() {}
  virtual ~CIccTagZipXml() {}
  CIccTagZipXml(const CIccTagZipXml &ITT) : CIccTagZipUtf8Text(ITT) {}
  CIccTagZipXml &operator=(const CIccTagZipXml &TextTag) {CIccTagZipUtf8Text::operator=(TextTag); return *this;}
  virtual CIccTag* NewCopy() const {return new CIccTagZipXml(*this);}

  virtual icTagTypeSignature GetType() const { return icSigZipXmlType; }
  virtual const icChar *GetClassName() const { return "CIccZipXML"; }
};


/**
****************************************************************************
* Class: CIccTagUtf16Text()
* 
* Purpose: The utf8TextType ICC tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagUtf16Text : public CIccTag
{
public:
  CIccTagUtf16Text();
  CIccTagUtf16Text(const CIccTagUtf16Text &ITT);
  CIccTagUtf16Text &operator=(const CIccTagUtf16Text &TextTag);
  virtual CIccTag* NewCopy() const {return new CIccTagUtf16Text(*this);}
  virtual ~CIccTagUtf16Text();

  virtual icTagTypeSignature GetType() const { return icSigUtf16TextType; }
  virtual const icChar *GetClassName() const { return "CIccUtf16TagText"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);

  const icChar *GetText(std::string &bufStr) const;
  const icUChar16 *GetText() const { return m_szText; }

  void SetText(const icUChar16 *szText);
  void SetText(const icUChar *szText);
  void SetText(const icChar *szText) { SetText((icUChar*)szText); }

  icUInt32Number GetLength() const;

  const icUChar16 *operator=(const icUChar16 *szText);

  icUChar16 *GetBuffer(icUInt32Number nSize);
  void Release();
  icUInt32Number Capacity() const { return m_nBufSize; }
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:

  icUChar16 *m_szText;
  icUInt32Number m_nBufSize;
};


/**
****************************************************************************
* Class: CIccTagTextDescription()
* 
* Purpose: The textType ICC tag
****************************************************************************
*/
class ICCPROFLIB_API CIccTagTextDescription : public CIccTag
{
public:
  CIccTagTextDescription();
  CIccTagTextDescription(const CIccTagTextDescription &ITTD);
  CIccTagTextDescription &operator=(const CIccTagTextDescription &TextDescTag);
  virtual CIccTag* NewCopy() const {return new CIccTagTextDescription(*this);}
  virtual ~CIccTagTextDescription();

  virtual icTagTypeSignature GetType() const { return icSigTextDescriptionType; }
  virtual const icChar *GetClassName() const { return "CIccTagTextDescription"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);

  const icChar *GetText() const { return m_szText; }
  void SetText(const icChar *szText);
  const icChar *operator=(const icChar *szText);

  icChar *GetBuffer(icUInt32Number nSize);
  void Release();
  icUInt32Number Capacity() const { return m_nASCIISize; }

  icUInt16Number *GetUnicodeBuffer(icUInt32Number nSize);
  void ReleaseUnicode();
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;


protected:
  icChar *m_szText;
  icUInt32Number m_nASCIISize;

  icUInt16Number *m_uzUnicodeText;
  icUInt32Number m_nUnicodeSize;
  icUInt32Number m_nUnicodeLanguageCode;

  icUInt8Number m_szScriptText[67];
  icUInt8Number m_nScriptSize;
  icUInt16Number m_nScriptCode;

  bool m_bInvalidScript;
};

//The icGetTagText() utility function will extract text information from a
//tag if it is one of the above text tag types.  It returns false if it the
//tag is not a text tag or it cannot extract the text from the tag.
ICCPROFLIB_API bool icGetTagText(const CIccTag *pTag, std::string &text);

/**
****************************************************************************
* Class: CIccTagSignature
* 
* Purpose:  The signatureType tag
****************************************************************************
*/
class ICCPROFLIB_API CIccTagSignature : public CIccTag
{
public:
  CIccTagSignature();
  CIccTagSignature(const CIccTagSignature &ITS);
  CIccTagSignature &operator=(const CIccTagSignature &SignatureTag);
  virtual CIccTag* NewCopy() const {return new CIccTagSignature(*this);}
  virtual ~CIccTagSignature();

  virtual icTagTypeSignature GetType() const { return icSigSignatureType; }
  virtual const icChar *GetClassName() const { return "CIccTagSignature"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  icUInt32Number GetValue() const { return m_nSig; }
  void SetValue(icUInt32Number sig) { m_nSig = sig; }
  icUInt32Number operator=(icUInt32Number sig) { SetValue(sig); return m_nSig; }
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icUInt32Number m_nSig;
};

typedef struct
{
  icChar rootName[32];
  icFloatNumber pcsCoords[3];
  icFloatNumber deviceCoords[icAny];
} SIccNamedColorEntry;

typedef struct {
  icFloatNumber lab[3];
} SIccNamedLabEntry;

/**
****************************************************************************
* Class: CIccTagNamedColor2
* 
* Purpose: the NamedColor2 tag - an array of Named Colors
****************************************************************************
*/
class ICCPROFLIB_API CIccTagNamedColor2 : public CIccTag
{
public:
  CIccTagNamedColor2(int nSize=1, int nDeviceCoords=0);
  CIccTagNamedColor2(const CIccTagNamedColor2 &ITNC);
  CIccTagNamedColor2 &operator=(const CIccTagNamedColor2 &NamedColor2Tag);
  virtual CIccTag* NewCopy() const {return new CIccTagNamedColor2(*this);}
  virtual ~CIccTagNamedColor2();

  virtual icTagTypeSignature GetType() const { return icSigNamedColor2Type; }
  virtual const icChar *GetClassName() const { return "CIccTagNamedColor2"; }

  virtual bool UseLegacyPCS() const { return true; } //Treat Lab Encoding differently?

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  const icChar *GetPrefix() const { return m_szPrefix; }
  void SetPrefix(const icChar *szPrefix);

  const icChar *GetSufix() const { return m_szSufix; }
  void SetSufix(const icChar *szSufix);

  icUInt32Number GetVendorFlags() const { return m_nVendorFlags; }
  void SetVendorFlags(icUInt32Number nVendorFlags) {m_nVendorFlags = nVendorFlags;}

  //The following Find functions return the zero based index of the color
  //or -1 to indicate that the color was not found.
  icInt32Number FindColor(const icChar *szColor) const;
  icInt32Number FindRootColor(const icChar *szRootColor) const;
  icInt32Number FindDeviceColor(icFloatNumber *pDevColor) const;
  icInt32Number FindPCSColor(icFloatNumber *pPCS, icFloatNumber dMinDE=1000.0);

  bool InitFindCachedPCSColor();
  //FindPCSColor returns the zero based index of the color or -1 to indicate that the color was not found.
  //InitFindPCSColor must be called before FindPCSColor
  icInt32Number FindCachedPCSColor(icFloatNumber *pPCS, icFloatNumber dMinDE=1000.0) const;

  ///Call ResetPCSCache() if entry values change between calls to FindPCSColor()
  void ResetPCSCache();

  bool GetColorName(std::string &sColorName, icInt32Number index) const;
  SIccNamedColorEntry &operator[](icUInt32Number index) const {return *(SIccNamedColorEntry*)((icUInt8Number*)m_NamedColor + index * m_nColorEntrySize);}
  SIccNamedColorEntry *GetEntry(icUInt32Number index) const {return (SIccNamedColorEntry*)((icUInt8Number*)m_NamedColor + index * m_nColorEntrySize);}

  icUInt32Number GetSize() const { return m_nSize; }
  icUInt32Number GetDeviceCoords() const {return m_nDeviceCoords;}
  bool SetSize(icUInt32Number nSize, icInt32Number nDeviceCoords=-1);

  virtual void SetColorSpaces(icColorSpaceSignature csPCS, icColorSpaceSignature csDevice);
  icColorSpaceSignature GetPCS() const {return m_csPCS;}
  icColorSpaceSignature GetDeviceSpace() const {return m_csDevice;}

  icFloatNumber NegClip(icFloatNumber v) const;
  icFloatNumber UnitClip(icFloatNumber v) const;

  void Lab2ToLab4(icFloatNumber *Dst, const icFloatNumber *Src) const;
  void Lab4ToLab2(icFloatNumber *Dst, const icFloatNumber *Src) const;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icChar m_szPrefix[32];
  icChar m_szSufix[32];
  
  SIccNamedColorEntry *m_NamedColor;
  SIccNamedLabEntry *m_NamedLab; ///For quick response of repeated FindPCSColor
  icUInt32Number m_nColorEntrySize;

  icUInt32Number m_nVendorFlags;
  icUInt32Number m_nDeviceCoords;
  icUInt32Number m_nSize;

  icColorSpaceSignature m_csPCS;
  icColorSpaceSignature m_csDevice;
};

/**
****************************************************************************
* Class: CIccTagXYZ
* 
* Purpose: the XYZType tag - an array of XYZ values
****************************************************************************
*/
class ICCPROFLIB_API CIccTagXYZ : public CIccTag
{
public:
  CIccTagXYZ(int nSize=1);
  CIccTagXYZ(const CIccTagXYZ &ITXYZ);
  CIccTagXYZ &operator=(const CIccTagXYZ &XYZTag);
  virtual CIccTag* NewCopy() const {return new CIccTagXYZ(*this);}
  virtual ~CIccTagXYZ();

  virtual bool IsArrayType() { return m_nSize > 1; }

  virtual icTagTypeSignature GetType() const { return icSigXYZType; }
  virtual const icChar *GetClassName() const { return "CIccTagXYZ"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  icXYZNumber &operator[](icUInt32Number index) const {return m_XYZ[index];}
  icXYZNumber *GetXYZ(icUInt32Number index) {return &m_XYZ[index];}
  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize, bool bZeroNew=true);
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icXYZNumber *m_XYZ;
  icUInt32Number m_nSize;
};

/**
****************************************************************************
* Class: CIccTagChromaticity
* 
* Purpose: the chromaticity tag - xy chromaticity values for each channel
****************************************************************************
*/
class ICCPROFLIB_API CIccTagChromaticity : public CIccTag
{
public:
  CIccTagChromaticity(int nSize=3);
  CIccTagChromaticity(const CIccTagChromaticity &ITCh);
  CIccTagChromaticity &operator=(const CIccTagChromaticity &ChromTag);
  virtual CIccTag* NewCopy() const {return new CIccTagChromaticity(*this);}
  virtual ~CIccTagChromaticity();

  virtual bool IsArrayType() { return m_nChannels > 1; }

  virtual icTagTypeSignature GetType() const { return icSigChromaticityType; }
  virtual const icChar *GetClassName() const { return "CIccTagChromaticity"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  icChromaticityNumber &operator[](icUInt32Number index) {return m_xy[index];}
  icChromaticityNumber *Getxy(icUInt32Number index) {return &m_xy[index];}
  icUInt32Number GetSize() const { return m_nChannels; }
  bool SetSize(icUInt16Number nSize, bool bZeroNew=true);

  icUInt16Number m_nColorantType;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icUInt16Number m_nChannels;
  icChromaticityNumber *m_xy;
};


/**
****************************************************************************
* Class: CIccTagCicp
*
* Purpose: the cicpType tag - metadata containing ITU-T H.273 fields
****************************************************************************
*/
class ICCPROFLIB_API CIccTagCicp : public CIccTag
{
public:
  CIccTagCicp();
  CIccTagCicp(const CIccTagCicp& ITCICP);
  CIccTagCicp& operator=(const CIccTagCicp& XYZTag);
  virtual CIccTag* NewCopy() const { return new CIccTagCicp(*this); }
  virtual ~CIccTagCicp();

  virtual icTagTypeSignature GetType() const { return icSigCicpType; }
  virtual const icChar* GetClassName() const { return "CIccTagCicp"; }

  virtual void Describe(std::string& sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO* pIO);
  virtual bool Write(CIccIO* pIO);

  void GetFields(icUInt8Number &colorPrimaries,
                 icUInt8Number &transferCharacteristics,
                 icUInt8Number &matrixCoefficients,
                 icUInt8Number &videoFullRangeFlag);

  void SetFields(icUInt8Number colorPrimaries,
                 icUInt8Number transferCharacteristics,
                 icUInt8Number matrixCoefficients,
                 icUInt8Number videoFullRangeFlag);

  virtual icValidateStatus Validate(std::string sigPath, std::string& sReport, const CIccProfile* pProfile = NULL) const;

protected:
  icUInt8Number m_nColorPrimaries;
  icUInt8Number m_nTransferCharacteristics;
  icUInt8Number m_nMatrixCoefficients;
  icUInt8Number m_nVideoFullRangeFlag;
};


/**
****************************************************************************
* Class: CIccTagNumArray
* 
* Purpose: An Interface for getting icFloatNumber values out of a tag
*  containing a number of values.
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagNumArray : public CIccTag
{
public:
  //GetValues gets the number values available in the array.
  virtual icUInt32Number GetNumValues() const = 0;

  //GetValues gets a vector of data from the number array as floating point values.
  virtual bool GetValues(icFloatNumber *DstVector, icUInt32Number nStart=0, icUInt32Number nVectorSize=1) const = 0;

  //The Interpolate function interpolates a vector from a set of vectors in the Array.  
  //The number of vectors in the array is determined by dividing GetSize() by nVectorSize.
  //The pos value should range from 0.0 to 1.0.  The last vector in the array corresponds
  //to pos=1.0.  If the zeroVals paramater is not NULL then the first vector in the array 
  //does not correspond to pos=0.0, and the pos=0.0 value comes from the zeroVals vector.
  //Otherwise, the first vector in the array corresponds to val=0.0.
  virtual bool Interpolate(icFloatNumber *DstVector, icFloatNumber pos,
                           icUInt32Number nVectorSize=1, icFloatNumber *zeroVals=NULL) const = 0;
  
  //ValuePos returns the interpolated position where val occurs in a single dimensional array.
  //The val value should range from 0.0 to 1.0.  If the first entry (entry[0]) in the array
  //is not zero and the val value is smaller than the first entry, then a zero value will
  //be assumed to proceed the first entry and the DstPos will be set to a value between -1.0
  //and 0.0.  Otherwise the DstPos value will be set to a value between 0.0 and num entries-1.
  //If val is less than 0.0 or greater than the last entry the funciton returns false.
  virtual bool ValuePos(icFloatNumber&DstPos, icFloatNumber val, bool &bNoZero) const = 0;

  virtual bool IsMatrixArray() const = 0;

};


/**
****************************************************************************
* Class: CIccTagSparseMatrixArray
* 
* Purpose: A template class for arrays of Fixed point numbers
*
* Derived Tags: CIccTagS15Fixed16 and CIccTagU16Fixed16
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagSparseMatrixArray : public CIccTagNumArray
{
public:
  CIccTagSparseMatrixArray(int nNumMatrices=1, int nChannelsPerMatrix=4);
  CIccTagSparseMatrixArray(const CIccTagSparseMatrixArray &ITSMA);
  CIccTagSparseMatrixArray &operator=(const CIccTagSparseMatrixArray &ITSMA);
  virtual CIccTag* NewCopy() { return new CIccTagSparseMatrixArray(*this); }
  virtual ~CIccTagSparseMatrixArray();

  virtual bool IsArrayType() { return m_nSize > 1; }

  virtual icTagTypeSignature GetType() const { return icSigSparseMatrixArrayType; }
  virtual const icChar *GetClassName() const { return "CIccTagChromaticity"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  /// Returns the number of matrices in the array
  icUInt32Number GetNumMatrices() const { return m_nSize; }
  icUInt32Number GetChannelsPerMatrix() const { return m_nChannelsPerMatrix; }
  icUInt32Number GetBytesPerMatrix() const { return m_nChannelsPerMatrix*sizeof(icFloatNumber); }

  // Reset destroys any existing data and lets you change matrix allocation
  bool Reset(icUInt32Number nNumMatrices, icUInt16Number nChannelsPerMatrix); 

  icSparseMatrixType GetMatrixType() const { return m_nMatrixType; }
  void SetMatrixType(icSparseMatrixType nType) { m_nMatrixType = nType; }

  // Connects CIccSparseMatrix object to index based matrix in array
  bool GetSparseMatrix(CIccSparseMatrix &mtx, int nIndex, bool bInitFromData=true);

  virtual bool IsNumArrayType() const { return true; }

  //CIccTagNumArray implementation
  virtual icUInt32Number GetNumValues() const { return GetNumMatrices(); }
  virtual bool GetValues(icFloatNumber *DstVector, icUInt32Number nStart=0, icUInt32Number nVectorSize=1) const;
  virtual bool Interpolate(icFloatNumber *DstVector, icFloatNumber val,
                           icUInt32Number nVectorSize=1, icFloatNumber *zeroVals=NULL) const; 
  virtual bool ValuePos(icFloatNumber&DstPos, icFloatNumber val, bool &bNoZero) const;

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  virtual bool IsMatrixArray() const { return true; }

protected:
  icSparseMatrixType m_nMatrixType;
  icUInt8Number *m_RawData;
  icUInt32Number m_nSize;
  icUInt16Number m_nChannelsPerMatrix;

  bool m_bNonZeroPadding;
};


/**
****************************************************************************
* Class: CIccTagFixedNum
* 
* Purpose: A template class for arrays of Fixed point numbers
*
* Derived Tags: CIccTagS15Fixed16 and CIccTagU16Fixed16
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
class ICCPROFLIB_API CIccTagFixedNum : public CIccTagNumArray
{
public:
  CIccTagFixedNum(int nSize=1);
  CIccTagFixedNum(const CIccTagFixedNum<T, Tsig> &ITFN);
  CIccTagFixedNum &operator=(const CIccTagFixedNum<T, Tsig> &FixedNumTag);
  virtual CIccTag* NewCopy() const { return new CIccTagFixedNum<T, Tsig>(*this); }
  virtual ~CIccTagFixedNum();

  virtual bool IsArrayType() { return m_nSize > 1; }

  virtual icTagTypeSignature GetType() const { return Tsig; }
  virtual const icChar *GetClassName() const;

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  T &operator[](icUInt32Number index) {return m_Num[index];}

  /// Returns the size of the data array
  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize, bool bZeroNew=true);

  virtual bool IsNumArrayType() const { return true; }

  //CIccTagNumArray implementation
  virtual icUInt32Number GetNumValues() const { return GetSize(); }
  virtual bool GetValues(icFloatNumber *DstVector, icUInt32Number nStart=0, icUInt32Number nVectorSize=1) const;
  virtual bool Interpolate(icFloatNumber *DstVector, icFloatNumber val,
                           icUInt32Number nVectorSize=1, icFloatNumber *zeroVals=NULL) const; 
  virtual bool ValuePos(icFloatNumber&DstPos, icFloatNumber val, bool &bNoZero) const;

  virtual bool IsMatrixArray() const { return false; }

protected:
  T *m_Num;
  icUInt32Number m_nSize;
};

/**
****************************************************************************
* Class: CIccTagS15Fixed16
* 
* Purpose: s15Fixed16type tag derived from CIccTagFixedNum
*****************************************************************************
*/
typedef CIccTagFixedNum<icS15Fixed16Number, icSigS15Fixed16ArrayType> CIccTagS15Fixed16;

/**
****************************************************************************
* Classe: CIccTagU16Fixed16
* 
* Purpose: u16Fixed16type tag derived from CIccTagFixedNum
*****************************************************************************
*/
typedef CIccTagFixedNum<icU16Fixed16Number, icSigU16Fixed16ArrayType> CIccTagU16Fixed16;

/**
****************************************************************************
* Class: CIccTagNum
* 
* Purpose: A template class for arrays of integers
* 
* Derived Tags: CIccTagUInt8, CIccTagUInt16, CIccTagUInt32 and CIccTagUInt64
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
class ICCPROFLIB_API CIccTagNum : public CIccTagNumArray
{
public:
  CIccTagNum(int nSize=1);
  CIccTagNum(const CIccTagNum<T, Tsig> &ITNum);
  CIccTagNum &operator=(const CIccTagNum<T, Tsig> &NumTag);
  virtual CIccTag* NewCopy() const { return new CIccTagNum<T, Tsig>(*this); }
  virtual ~CIccTagNum();

  virtual bool IsArrayType() { return m_nSize > 1; }

  virtual icTagTypeSignature GetType() const { return Tsig; }
  virtual const icChar *GetClassName() const;

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  T &operator[](icUInt32Number index) {return m_Num[index];}

  /// Returns the size of the data array
  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize, bool bZeroNew=true);

  virtual bool IsNumArrayType() const { return true; }

  //CIccTagNumArray implementation (values in m_Num will be mapped to 0.0 to 1.0 range)
  virtual icUInt32Number GetNumValues() const { return GetSize(); }
  virtual bool GetValues(icFloatNumber *DstVector, icUInt32Number nStart=0, icUInt32Number nVectorSize=1) const;
  virtual bool Interpolate(icFloatNumber *DstVector, icFloatNumber val,
                           icUInt32Number nVectorSize=1, icFloatNumber *zeroVals=NULL) const; 
  virtual bool ValuePos(icFloatNumber&DstPos, icFloatNumber val, bool &bNoZero) const;

  virtual bool IsMatrixArray() const { return false; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  T *m_Num;
  icUInt32Number m_nSize;
};


/**
****************************************************************************
* Class: CIccTagUInt8
* 
* Purpose: icUInt8Number type tag derived from CIccTagNum
*****************************************************************************
*/
typedef CIccTagNum<icUInt8Number, icSigUInt8ArrayType> CIccTagUInt8;

/**
****************************************************************************
* Class: CIccTagUInt16
* 
* Purpose: icUInt16Number type tag derived from CIccTagNum
*****************************************************************************
*/
typedef CIccTagNum<icUInt16Number, icSigUInt16ArrayType> CIccTagUInt16;

/**
****************************************************************************
* Class: CIccTagUInt32
* 
* Purpose: icUInt32Number type tag derived from CIccTagNum
*****************************************************************************
*/
typedef CIccTagNum<icUInt32Number, icSigUInt32ArrayType> CIccTagUInt32;

/**
****************************************************************************
* Class: CIccTagUInt64
* 
* Purpose: icUInt64Number type tag derived from CIccTagNum
*****************************************************************************
*/
typedef CIccTagNum<icUInt64Number, icSigUInt64ArrayType> CIccTagUInt64;



/**
****************************************************************************
* Class: CIccTagFloatNum
* 
* Purpose: A template class for arrays of floating point numbers
* 
* Derived Tags: CIccTagFloat32 and CIccTagFloat64
*****************************************************************************
*/
template <class T, icTagTypeSignature Tsig>
class ICCPROFLIB_API CIccTagFloatNum : public CIccTagNumArray
{
public:
  CIccTagFloatNum(int nSize=1);
  CIccTagFloatNum(const CIccTagFloatNum<T, Tsig> &ITNum);
  CIccTagFloatNum &operator=(const CIccTagFloatNum<T, Tsig> &NumTag);
  virtual CIccTag* NewCopy() const { return new CIccTagFloatNum<T, Tsig>(*this); }
  virtual ~CIccTagFloatNum();

  virtual bool IsArrayType() { return m_nSize > 1; }

  virtual icTagTypeSignature GetType() const { return Tsig; }
  virtual const icChar *GetClassName() const;

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  T &operator[](icUInt32Number index) {return m_Num[index];}

  /// Returns the size of the data array
  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize, bool bZeroNew=true);

  virtual bool IsNumArrayType() const { return true; }

  //CIccTagNumArray implementation
  virtual icUInt32Number GetNumValues() const { return GetSize(); }
  virtual bool GetValues(icFloatNumber *DstVector, icUInt32Number nStart=0, icUInt32Number nVectorSize=1) const;
  virtual bool Interpolate(icFloatNumber *DstVector, icFloatNumber val,
                           icUInt32Number nVectorSize=1, icFloatNumber *zeroVals=NULL) const; 
  virtual bool ValuePos(icFloatNumber&DstPos, icFloatNumber val, bool &bNoZero) const;

  virtual bool IsMatrixArray() const { return false; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  T *m_Num;
  icUInt32Number m_nSize;
};

/**
****************************************************************************
* Class: CIccTagFloat16
* 
* Purpose: icFloat16Number type tag derived from CIccTagFloatNum
*  Note: Internally icFloat16Number will be represented as icFloat32Numbers
*****************************************************************************
*/
typedef CIccTagFloatNum<icFloat32Number, icSigFloat16ArrayType> CIccTagFloat16;


/**
****************************************************************************
* Class: CIccTagFloat32
* 
* Purpose: icFloat32Number type tag derived from CIccTagFloatNum
*****************************************************************************
*/
typedef CIccTagFloatNum<icFloat32Number, icSigFloat32ArrayType> CIccTagFloat32;

/**
****************************************************************************
* Class: CIccTagFloat64
* 
* Purpose: icFloat64Number type tag derived from CIccTagFloatNum
*****************************************************************************
*/
typedef CIccTagFloatNum<icFloat64Number, icSigFloat64ArrayType> CIccTagFloat64;



/**
****************************************************************************
* Class: CIccTagMeasurement
* 
* Purpose: The measurmentType tag 
****************************************************************************
*/
class ICCPROFLIB_API CIccTagMeasurement : public CIccTag
{
public:
  CIccTagMeasurement();
  CIccTagMeasurement(const CIccTagMeasurement &ITM);
  CIccTagMeasurement &operator=(const CIccTagMeasurement &MeasTag);
  virtual CIccTag* NewCopy() const {return new CIccTagMeasurement(*this);}
  virtual ~CIccTagMeasurement();

  virtual icTagTypeSignature GetType() const { return icSigMeasurementType; }
  virtual const icChar *GetClassName() const { return "CIccTagMeasurement"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;


  icMeasurement m_Data;
};

/**
****************************************************************************
* Data Class: CIccLocalizedUnicode
* 
* Purpose: Implementation of a unicode string with language and region
*  identifiers.
*****************************************************************************
*/
class ICCPROFLIB_API CIccLocalizedUnicode
{
public: //member functions
  CIccLocalizedUnicode();
  CIccLocalizedUnicode(const CIccLocalizedUnicode& ILU);
  CIccLocalizedUnicode &operator=(const CIccLocalizedUnicode &UnicodeText);
  virtual ~CIccLocalizedUnicode();

  icUInt32Number GetLength() const { return m_nLength; }
  icUInt16Number *GetBuf() const { return m_pBuf; }

  icUInt32Number GetUtf8Size();
  const icChar *GetUtf8(icChar *szBuf, icUInt32Number nBufSize);

  bool GetText(std::string &text);
  
  bool SetSize(icUInt32Number);

  bool SetText(const icChar *szText,
               icLanguageCode nLanguageCode = icLanguageCodeEnglish,
               icCountryCode nRegionCode = icCountryCodeUSA);
  bool SetText(const icUInt16Number *sszUnicode16Text,
               icLanguageCode nLanguageCode = icLanguageCodeEnglish,
               icCountryCode nRegionCode = icCountryCodeUSA);
  bool SetText(const icUInt32Number *sszUnicode32Text,
               icLanguageCode nLanguageCode = icLanguageCodeEnglish,
               icCountryCode nRegionCode = icCountryCodeUSA);

  const icChar *operator=(const icChar *szText) { SetText(szText); return szText; }
  const icUInt16Number *operator=(const icUInt16Number *sszText) { SetText(sszText); return sszText; }
  const icUInt32Number *operator=(const icUInt32Number *sszText) { SetText(sszText); return sszText; }

  //Data
  icLanguageCode m_nLanguageCode;
  icCountryCode m_nCountryCode;

protected:
  icUInt32Number m_nLength;

  icUInt16Number *m_pBuf;

};

/**
****************************************************************************
* List Class: CIccMultiLocalizedUnicode
* 
* Purpose: List of CIccLocalizedUnicode objects
*****************************************************************************
*/
typedef std::list<CIccLocalizedUnicode> CIccMultiLocalizedUnicode;


/**
****************************************************************************
* Class: CIccTagMultiLocalizedUnicode
* 
* Purpose: The MultiLocalizedUnicode tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagMultiLocalizedUnicode : public CIccTag
{
public:
  CIccTagMultiLocalizedUnicode();
  CIccTagMultiLocalizedUnicode(const CIccTagMultiLocalizedUnicode& ITMLU);
  CIccTagMultiLocalizedUnicode &operator=(const CIccTagMultiLocalizedUnicode &MultiLocalizedTag);
  virtual CIccTag* NewCopy() const {return new CIccTagMultiLocalizedUnicode(*this);}
  virtual ~CIccTagMultiLocalizedUnicode();

  virtual icTagTypeSignature GetType() const { return icSigMultiLocalizedUnicodeType; }
  virtual const icChar *GetClassName() const { return "CIcciSigMultiLocalizedUnicode"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  CIccLocalizedUnicode *Find(icLanguageCode nLanguageCode = icLanguageCodeEnglish,
                             icCountryCode nRegionCode = icCountryCodeUSA);

  void SetText(const icChar *szText,
               icLanguageCode nLanguageCode = icLanguageCodeEnglish,
               icCountryCode nRegionCode = icCountryCodeUSA);
  void SetText(const icUInt16Number *sszUnicode16Text,
               icLanguageCode nLanguageCode = icLanguageCodeEnglish,
               icCountryCode nRegionCode = icCountryCodeUSA);
  void SetText(const icUInt32Number *sszUnicode32Text,
               icLanguageCode nLanguageCode = icLanguageCodeEnglish,
               icCountryCode nRegionCode = icCountryCodeUSA);


  CIccMultiLocalizedUnicode *m_Strings;
};


//
// MD: Moved Curve & LUT Tags to IccTagLut.h (4-30-05)
//

/**
****************************************************************************
* Class: CIccTagData
* 
* Purpose: The data type tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagData : public CIccTag
{
public:
  CIccTagData(int nSize=1);
  CIccTagData(const CIccTagData &ITD);
  CIccTagData &operator=(const CIccTagData &DataTag);
  virtual CIccTag* NewCopy() const {return new CIccTagData(*this);}
  virtual ~CIccTagData();

  virtual icTagTypeSignature GetType() const { return icSigDataType; }
  virtual const icChar *GetClassName() const { return "CIccTagData"; }

  virtual bool IsArrayType() { return m_nSize > 1; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize, bool bZeroNew=true);
  icUInt8Number &operator[] (icUInt32Number index) { return m_pData[index]; }
  icUInt8Number *GetData(icUInt32Number index=0) { return &m_pData[index];}

  virtual void SetDataType(icDataBlockType dataType=icAsciiData) { m_nDataFlag = dataType; }
  virtual bool IsTypeAscii() { return (m_nDataFlag&icDataTypeMask) == icAsciiData; }
  virtual bool IsTypeBinary() { return (m_nDataFlag&icDataTypeMask) == icBinaryData; }
  virtual bool IsTypeUtf() { return (m_nDataFlag&icDataTypeMask) == icUtfData; }
  virtual bool IsTypeCompressed() { return (m_nDataFlag & icCompressedData) != 0; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icDataBlockType m_nDataFlag;
  icUInt8Number *m_pData;
  icUInt32Number m_nSize;
};


/**
****************************************************************************
* Class: CIccTagDateTime
* 
* Purpose: The date time tag type
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagDateTime : public CIccTag
{
public:
  CIccTagDateTime();
  CIccTagDateTime(const CIccTagDateTime &ITDT);
  CIccTagDateTime &operator=(const CIccTagDateTime &DateTimeTag);
  virtual CIccTag* NewCopy() const {return new CIccTagDateTime(*this);}
  virtual ~CIccTagDateTime();

  virtual icTagTypeSignature GetType() const { return icSigDateTimeType; }
  virtual const icChar *GetClassName() const { return "CIccTagDateTime"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);

  void SetDateTime(icDateTimeNumber nDateTime) { m_DateTime = nDateTime;}
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icDateTimeNumber m_DateTime;
};

/**
****************************************************************************
* Class: CIccTagColorantOrder
* 
* Purpose: Colorant Order Tag 
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagColorantOrder : public CIccTag
{
public:
  CIccTagColorantOrder(int nsize=1);
  CIccTagColorantOrder(const CIccTagColorantOrder &ITCO);
  CIccTagColorantOrder &operator=(const CIccTagColorantOrder &ColorantOrderTag);
  virtual CIccTag* NewCopy() const {return new CIccTagColorantOrder(*this);}
  virtual ~CIccTagColorantOrder();

  virtual icTagTypeSignature GetType() const { return icSigColorantOrderType; }
  virtual const icChar *GetClassName() const { return "CIccTagColorantOrder"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  virtual void Describe(std::string &sDescription, int nVerboseness);
  icUInt8Number& operator[](int index) { return m_pData[index]; }
  icUInt8Number *GetData(int index) { return &m_pData[index]; }
  bool SetSize(icUInt16Number nsize, bool bZeronew=true);
  icUInt32Number GetSize() const {return m_nCount;}
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icUInt32Number m_nCount;
  icUInt8Number *m_pData;
};

/**
****************************************************************************
* Class: CIccTagColorantTable
* 
* Purpose: Colorant Table Tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagColorantTable : public CIccTag
{
public:
  CIccTagColorantTable(int nsize=1);
  CIccTagColorantTable(const CIccTagColorantTable &ITCT);
  CIccTagColorantTable &operator=(const CIccTagColorantTable &ColorantTableTag);
  virtual CIccTag* NewCopy() const {return new CIccTagColorantTable(*this);}
  virtual ~CIccTagColorantTable();

  virtual icTagTypeSignature GetType() const { return icSigColorantTableType; }
  virtual const icChar *GetClassName() const { return "CIccTagColorantTable"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  icColorantTableEntry &operator[](icUInt32Number index) {return m_pData[index];}
  icColorantTableEntry *GetEntry(icUInt32Number index) {return &m_pData[index];}
  icUInt32Number GetSize() const { return m_nCount; }
  bool SetSize(icUInt16Number nSize, bool bZeroNew=true);

  void SetPCS(icColorSpaceSignature sig) {m_PCS = sig;}
  icColorSpaceSignature GetPCS() const {return m_PCS;};
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

protected:
  icUInt32Number m_nCount;
  icColorantTableEntry *m_pData;
  icColorSpaceSignature m_PCS;
};

/**
****************************************************************************
* Class: CIccTagViewingConditions
* 
* Purpose: Viewing conditions tag type
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagViewingConditions : public CIccTag
{
public:
  CIccTagViewingConditions();
  CIccTagViewingConditions(const CIccTagViewingConditions &ITVC);
  CIccTagViewingConditions &operator=(const CIccTagViewingConditions &ViewCondTag);
  virtual CIccTag* NewCopy() const {return new CIccTagViewingConditions(*this);}
  virtual ~CIccTagViewingConditions();

  virtual icTagTypeSignature GetType() const { return icSigViewingConditionsType; }
  virtual const icChar *GetClassName() const { return "CIccTagViewingConditions"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  icXYZNumber m_XYZIllum;
  icXYZNumber m_XYZSurround;
  icIlluminant m_illumType;
};

/**
****************************************************************************
* Data Class: CIccProfileDescText
* 
* Purpose: Private text class for CIccProfileDescStruct.
*  Text can be either a CIccTagTextDescription or a CIccTagMultiLocalizedUnicode
*  so this class provides a single interface to both.
*****************************************************************************
*/
class ICCPROFLIB_API CIccProfileDescText
{
public:
  CIccProfileDescText();
  CIccProfileDescText(const CIccProfileDescText& IPDC);
  CIccProfileDescText &operator=(const CIccProfileDescText &ProfDescText);
  virtual ~CIccProfileDescText();

  bool SetType(icTagTypeSignature nType);
  virtual icTagTypeSignature GetType() const;

  CIccTag* GetTag() const { return m_pTag; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  bool m_bNeedsPading;

protected:
  CIccTag *m_pTag;  //either a CIccTagTextDescription or a CIccTagMultiLocalizedUnicode  
};



/**
****************************************************************************
* Data Class: CIccProfileDescStruct
* 
* Purpose: Storage class for profile description structure
*****************************************************************************
*/
class ICCPROFLIB_API CIccProfileDescStruct
{  
public:
  CIccProfileDescStruct();
  CIccProfileDescStruct(const CIccProfileDescStruct& IPDS);
  CIccProfileDescStruct &operator=(const CIccProfileDescStruct& ProfDescStruct);


  icSignature                 m_deviceMfg;      /* Device Manufacturer */
  icSignature                 m_deviceModel;    /* Device Model */
  icUInt64Number              m_attributes;     /* Device attributes */
  icTechnologySignature       m_technology;     /* Technology signature */
  CIccProfileDescText         m_deviceMfgDesc;
  CIccProfileDescText         m_deviceModelDesc;
};

/**
****************************************************************************
* List Class: CIccProfileSeqDesc
* 
* Purpose: List of profile description structures
*****************************************************************************
*/
typedef std::list<CIccProfileDescStruct> CIccProfileSeqDesc;


/**
****************************************************************************
* Class: CIccTagProfileSeqDesc
* 
* Purpose: Profile Sequence description tag  
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagProfileSeqDesc : public CIccTag
{
public:
  CIccTagProfileSeqDesc();
  CIccTagProfileSeqDesc(const CIccTagProfileSeqDesc &ITPSD);
  CIccTagProfileSeqDesc &operator=(const CIccTagProfileSeqDesc &ProfSeqDescTag);
  virtual CIccTag* NewCopy() const {return new CIccTagProfileSeqDesc(*this);}
  virtual ~CIccTagProfileSeqDesc();

  virtual icTagTypeSignature GetType() const { return icSigProfileSequenceDescType; }
  virtual const icChar *GetClassName() const { return "CIccTagProfileSeqDesc"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  CIccProfileSeqDesc *m_Descriptions;
};


/**
****************************************************************************
* List Class: CIccResponse16List
* 
* Purpose: List of response16numbers 
*****************************************************************************
*/
typedef std::list<icResponse16Number> CIccResponse16List;

/**
****************************************************************************
* Data Class: CIccResponseCurveStruct
* 
* Purpose: The base class for response curve structure
*****************************************************************************
*/
class ICCPROFLIB_API CIccResponseCurveStruct
{
  friend class ICCPROFLIB_API CIccTagResponseCurveSet16;
public: //member functions
  CIccResponseCurveStruct(icMeasurementUnitSig sig, icUInt16Number nChannels=0);
  CIccResponseCurveStruct(icUInt16Number nChannels=0);

  CIccResponseCurveStruct(const CIccResponseCurveStruct& IRCS);
  CIccResponseCurveStruct &operator=(const CIccResponseCurveStruct& RespCurveStruct);
  virtual ~CIccResponseCurveStruct();

  bool Read(icUInt32Number size, CIccIO *pIO);
  bool Write(CIccIO *pIO);
  void Describe(std::string &sDescription, int nVerboseness);

  icMeasurementUnitSig GetMeasurementType() const {return m_measurementUnitSig;}
  icUInt16Number GetNumChannels() const {return m_nChannels;}

  icXYZNumber *GetXYZ(icUInt32Number index) {return &m_maxColorantXYZ[index];}
  CIccResponse16List *GetResponseList(icUInt16Number nChannel) {return &m_Response16ListArray[nChannel];}
  CIccResponseCurveStruct* GetThis() {return this;}
  icValidateStatus Validate(std::string &sReport);

protected:
  icUInt16Number m_nChannels;
  icMeasurementUnitSig m_measurementUnitSig;
  icXYZNumber *m_maxColorantXYZ;
  CIccResponse16List *m_Response16ListArray;
};


/**
****************************************************************************
* List Class: CIccResponseCurveSet
* 
* Purpose: List of response curve structures
*****************************************************************************
*/

typedef std::list<CIccResponseCurveStruct> CIccResponseCurveSet;

class CIccResponseCurveSetIter
{
public:
  bool inited;
  CIccResponseCurveSet::iterator item;
};


/**
****************************************************************************
* Class: CIccTagResponseCurveSet16
* 
* Purpose: The responseCurveSet16 Tag type
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagResponseCurveSet16 : public CIccTag
{
public:
  CIccTagResponseCurveSet16();
  CIccTagResponseCurveSet16(const CIccTagResponseCurveSet16 &ITRCS);
  CIccTagResponseCurveSet16 &operator=(const CIccTagResponseCurveSet16 &RespCurveSet16Tag);
  virtual CIccTag* NewCopy() const {return new CIccTagResponseCurveSet16(*this);}
  virtual ~CIccTagResponseCurveSet16();

  virtual icTagTypeSignature GetType() const { return icSigResponseCurveSet16Type; }
  virtual const icChar *GetClassName() const { return "CIccTagResponseCurveSet16"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);
  virtual void Describe(std::string &sDescription, int nVerboseness);

  void SetNumChannels(icUInt16Number nChannels);
  icUInt16Number GetNumChannels() const {return m_nChannels;}

  CIccResponseCurveStruct  *NewResponseCurves(icMeasurementUnitSig sig);
  CIccResponseCurveStruct *GetResponseCurves(icMeasurementUnitSig sig);

  CIccResponseCurveStruct *GetFirstCurves();
  CIccResponseCurveStruct *GetNextCurves();

  icUInt16Number GetNumResponseCurveTypes() const;
  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;
 
protected:
  CIccResponseCurveSet *m_ResponseCurves;
  icUInt16Number m_nChannels;
  CIccResponseCurveSetIter *m_Curve;
};

/**
****************************************************************************
* Class: CIccTagSpectralDataInfo
* 
* Purpose: The responseCurveSet16 Tag type
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagSpectralDataInfo : public CIccTag
{
public:
  CIccTagSpectralDataInfo();
  CIccTagSpectralDataInfo(const CIccTagSpectralDataInfo &ITRCS);
  CIccTagSpectralDataInfo &operator=(const CIccTagSpectralDataInfo &RespCurveSet16Tag);
  virtual CIccTag* NewCopy() const {return new CIccTagSpectralDataInfo(*this);}
  virtual ~CIccTagSpectralDataInfo();

  virtual icTagTypeSignature GetType() const { return icSigSpectralDataInfoType ; }
  virtual const icChar *GetClassName() const { return "CIccTagSpectralDataInfo"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);
  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  icSignature m_nSig;
  icSpectralRange m_spectralRange;
  icSpectralRange m_biSpectralRange;
};


class CIccMatrixMath;

/**
****************************************************************************
* Class: CIccTagSpectralViewingConditions
* 
* Purpose: The responseCurveSet16 Tag type
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagSpectralViewingConditions : public CIccTag
{
public:
  CIccTagSpectralViewingConditions();
  CIccTagSpectralViewingConditions(const CIccTagSpectralViewingConditions &ITRCS);
  CIccTagSpectralViewingConditions &operator=(const CIccTagSpectralViewingConditions &RespCurveSet16Tag);
  virtual CIccTag* NewCopy() const {return new CIccTagSpectralViewingConditions(*this);}
  virtual ~CIccTagSpectralViewingConditions();

  virtual icTagTypeSignature GetType() const { return icSigSpectralViewingConditionsType ; }
  virtual const icChar *GetClassName() const { return "CIccTagSpectralViewingConditions"; }

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);
  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile=NULL) const;

  icFloatNumber *applyRangeToObserver(const icSpectralRange &newRange) const; //Caller responsible for deleting results
  CIccMatrixMath *getObserverMatrix(const icSpectralRange &newRange) const;

  icIlluminant getStdIllumiant() const { return m_stdIlluminant; }
  icFloatNumber getIlluminantCCT() const { return m_colorTemperature; }
  icStandardObserver getStdObserver() const { return m_stdObserver; }

  const icFloatNumber *getIlluminant(icSpectralRange &illumRange) const;
  bool setIlluminant(icIlluminant illumId, const icSpectralRange &illumRange, const icFloatNumber *illum, icFloatNumber illumCCT = 0.0f);
  bool setIlluminant(icFloatNumber *pWhiteXYZ);

  const icFloatNumber *getObserver(icSpectralRange &observerRange) const;
  bool setObserver(icStandardObserver observerId, const icSpectralRange &observerRange, const icFloatNumber *observer);

  bool isStandardPcc() const {
    return m_stdObserver == icStdObs1931TwoDegrees || m_stdIlluminant == icIlluminantD50;
  }
    
  icFloatXYZNumber m_illuminantXYZ;
  icFloatXYZNumber m_surroundXYZ;

  icUInt16Number m_reserved2;
  icUInt16Number m_reserved3;

protected:
  icStandardObserver m_stdObserver;

  icSpectralRange m_observerRange;
  icFloatNumber *m_observer;

  icIlluminant m_stdIlluminant;
  icFloatNumber m_colorTemperature;

  icSpectralRange m_illuminantRange;
  icFloatNumber *m_illuminant;
};



/**
****************************************************************************
* Class: CIccTagEmbeddedHeightImage
*
* Purpose: The Embedded Height map tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagEmbeddedHeightImage : public CIccTag
{
public:
  CIccTagEmbeddedHeightImage(int nSize = 1);
  CIccTagEmbeddedHeightImage(const CIccTagEmbeddedHeightImage &IEHI);
  CIccTagEmbeddedHeightImage &operator=(const CIccTagEmbeddedHeightImage &HeightTag);
  virtual CIccTag* NewCopy() const { return new CIccTagEmbeddedHeightImage(*this); }
  virtual ~CIccTagEmbeddedHeightImage();

  virtual icTagTypeSignature GetType() const { return icSigEmbeddedHeightImageType; }
  virtual const icChar *GetClassName() const { return "CIccTagEmbeddedHeightImage"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize, bool bZeroNew = true);
  icUInt8Number &operator[] (icUInt32Number index) { return m_pData[index]; }
  icUInt8Number *GetData(icUInt32Number index = 0) { return &m_pData[index]; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const;

  icUInt32Number m_nSeamlesIndicator;
  icImageEncodingType m_nEncodingFormat;
  icFloatNumber m_fMetersMinPixelValue;
  icFloatNumber m_fMetersMaxPixelValue;

protected:
  icUInt8Number *m_pData;
  icUInt32Number m_nSize;
};


/**
****************************************************************************
* Class: CIccTagEmbeddedNormalImage
*
* Purpose: The Embedded Normal tag
*****************************************************************************
*/
class ICCPROFLIB_API CIccTagEmbeddedNormalImage : public CIccTag
{
public:
  CIccTagEmbeddedNormalImage(int nSize = 1);
  CIccTagEmbeddedNormalImage(const CIccTagEmbeddedNormalImage &IENI);
  CIccTagEmbeddedNormalImage &operator=(const CIccTagEmbeddedNormalImage &NormalTag);
  virtual CIccTag* NewCopy() const { return new CIccTagEmbeddedNormalImage(*this); }
  virtual ~CIccTagEmbeddedNormalImage();

  virtual icTagTypeSignature GetType() const { return icSigEmbeddedNormalImageType; }
  virtual const icChar *GetClassName() const { return "CIccTagEmbeddedNormalImage"; }

  virtual void Describe(std::string &sDescription, int nVerboseness);

  virtual bool Read(icUInt32Number size, CIccIO *pIO);
  virtual bool Write(CIccIO *pIO);

  icUInt32Number GetSize() const { return m_nSize; }
  bool SetSize(icUInt32Number nSize, bool bZeroNew = true);
  icUInt8Number &operator[] (icUInt32Number index) { return m_pData[index]; }
  icUInt8Number *GetData(icUInt32Number index = 0) { return &m_pData[index]; }

  virtual icValidateStatus Validate(std::string sigPath, std::string &sReport, const CIccProfile* pProfile = NULL) const;

  icUInt32Number m_nSeamlesIndicator;
  icImageEncodingType m_nEncodingFormat;

protected:
  icUInt8Number *m_pData;
  icUInt32Number m_nSize;
};


#ifdef USEICCDEVNAMESPACE
} //namespace iccDEV
#endif

#endif // !defined(_ICCTAGBASIC_H)
