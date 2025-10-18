#include <list>
#include <string>


#ifndef _ICCCMMCONFIG_H
#define _ICCCMMCONFIG_H

#include "IccJsonUtil.h"
#include "IccCmm.h"
#include "IccEnvVar.h"

typedef enum {
	icCfgColorData,
	icCfgLegacy,
	icCfgIt8
} icCfgDataType;

class CIccCfgDataApply
{
public:
	CIccCfgDataApply();
	virtual ~CIccCfgDataApply() {}
	
	void reset();
	int fromArgs(const char **args, int nArg, bool bReset=false); //returns args used
	bool fromJson(json obj, bool bReset=false);
	void toJson(json &obj) const;

	bool m_debugCalc;
	icCfgDataType m_srcType;
  icColorSpaceSignature m_srcSpace; //Defines space to use for it8

	std::string m_srcFile; //if empty use colorData section

	icCfgDataType m_dstType;
	
	std::string m_dstFile; //if empty use stdout

	icFloatColorEncoding m_dstEncoding;
	icUInt8Number m_dstDigits;
	icUInt8Number m_dstPrecision;
};

typedef enum {
	icDst8Bit,
	icDst15Bit,
	icDstFloat,
	icDstFromSrc
} icDstEncoding;

typedef enum {
	icDstBoolFalse,
	icDstBoolTrue,
	icDstBoolFromSrc
} icDstBool;

class CIccCfgImageApply
{
public:
  CIccCfgImageApply();
	virtual ~CIccCfgImageApply() {}
	
	void reset();
	int fromArgs(const char **args, int nArg, bool bReset=false); //returns args used
	bool fromJson(json obj, bool bReset=false);
	void toJson(json& obj) const;

	std::string m_srcImgFile;
	std::string m_dstImgFile;
	icFloatColorEncoding m_dstEncoding;
	icDstBool m_dstCompression;
	icDstBool m_dstPlanar;
	icDstBool m_dstEmbedIcc;
};

typedef enum {
  icCfgCreateTextCube=0,
	icCfgCreateIccLinkV4,
	icCfgCreateIccLinkV5,
} icCfgLinkType;

class CIccCfgCreateLink
{
public:
	CIccCfgCreateLink();
  virtual ~CIccCfgCreateLink() {}

  void reset();
  int fromArgs(const char** args, int nArg, bool bReset = false); //returns args used
  bool fromJson(json obj, bool bReset = false);
  void toJson(json& obj) const;

  std::string m_linkFile;
	icCfgLinkType m_linkType;
	icUInt8Number m_linkGridSize;
	icUInt8Number m_linkPrecision;
	std::string m_linkTitle;
	icFloatNumber m_linkMinRange;
	icFloatNumber m_linkMaxRange;
	bool m_useSourceTransform;
};

class CIccCfgProfile
{
public:
	CIccCfgProfile();
	virtual ~CIccCfgProfile() {}
	
	void reset();
	bool fromJson(json obj, bool bReset=false);
	void toJson(json& obj) const;

	bool m_useEmbedded;
	std::string m_iccFile;
	int m_intent;
	icXformLutType m_transform;
	icCmmEnvSigMap m_iccEnvVars;
	std::string m_pccFile;
	icCmmEnvSigMap m_pccEnvVars;
	bool m_useD2BxB2Dx;
	bool m_adjustPcsLuminance;
	bool m_useBPC;
	bool m_useHToS;
	bool m_useV5SubProfile;
	icXformInterp m_interpolation;
};

typedef std::shared_ptr<CIccCfgProfile> CIccCfgProfilePtr;
typedef std::vector<CIccCfgProfilePtr> CIccCfgProfileArray;

class CIccCfgProfileSequence
{
public:
	CIccCfgProfileSequence();
	virtual ~CIccCfgProfileSequence() {}
	
	void reset();
	int fromArgs(const char **args, int nArg, bool bReset=false); //return args used
	bool fromJson(json obj, bool bReset=false);
	void toJson(json &obj) const;
	
	CIccCfgProfileArray m_profiles;
};

class CIccCfgPccWeight
{
public:
	CIccCfgPccWeight();
	virtual ~CIccCfgPccWeight();

	void reset();
  int fromArgs(const char** args, int nArg, bool bReset = false); //return args used
  bool fromJson(json obj, bool bReset = false);
  void toJson(json& obj) const;


	std::string m_pccPath;
	icFloatNumber m_dWeight;
};

typedef std::shared_ptr<CIccCfgPccWeight> CIccCfgPccWeightPtr;
typedef std::vector<CIccCfgPccWeightPtr> CIccCfgPccWeightArray;

class CIccCfgSearchApply
{
public:
	CIccCfgSearchApply();
  virtual ~CIccCfgSearchApply() {}

  void reset();
  int fromArgs(const char** args, int nArg, bool bReset = false); //return args used
  bool fromJson(json obj, bool bReset = false);
  void toJson(json& obj) const;

	bool isInitialized() { return m_bInitialized; }

  icRenderingIntent m_intentInitial;
  icXformLutType m_transformInitial;
  bool m_useD2BxB2DxInitial;
  bool m_adjustPcsLuminanceInitial;
  bool m_useV5SubProfileInitial;
	icXformInterp m_interpolationInitial;

	CIccCfgProfileArray m_profiles;
	CIccCfgPccWeightArray m_pccWeights;

protected:
	bool m_bInitialized = false;

	void toJsonInit(json& obj) const;
	bool fromJsonInit(json obj);

	void toJsonPccWeights(json& obj) const;
	bool fromJsonPccWeights(json obj);
	
	bool fromJsonProfiles(json j);
	void toJsonProfiles(json& obj) const;
};


class CIccCfgDataEntry
{
public:
	CIccCfgDataEntry();
	virtual ~CIccCfgDataEntry() {}

	void reset();
	bool fromJson(json obj, bool bReset=false);
	void toJson(json& obj);

	std::string m_name;
	std::vector<icFloatNumber> m_values;
  std::string m_srcName;
	std::vector<icFloatNumber> m_srcValues;
	int m_index;
	std::string m_label;
	std::list<std::string> m_debugInfo;
};

typedef std::shared_ptr<CIccCfgDataEntry> CIccCfgDataEntryPtr;
typedef std::list<CIccCfgDataEntryPtr> CIccCfgDataEntryList;

class CIccCfgColorData
{
public:
  CIccCfgColorData();
	virtual ~CIccCfgColorData() {}

	void reset();
	bool fromLegacy(const char *filename, bool bReset=false);
	bool fromIt8(const char* filename, bool bReset=false);
	bool fromJson(json obj, bool bReset=false);
	bool toLegacy(const char *filename, const CIccCfgProfileArray &profiles, icUInt8Number nDigits, icUInt8Number nPrecision, bool bShowDebug=false);
  bool toIt8(const char *filename, icUInt8Number nDigits, icUInt8Number nPrecision);

	void toJson(json& obj) const;

	icColorSpaceSignature m_space;
	icFloatColorEncoding m_encoding;

	icColorSpaceSignature m_srcSpace;
	icFloatColorEncoding m_srcEncoding;

	CIccCfgDataEntryList m_data;

protected:
	std::string spaceName(icColorSpaceSignature sig);
	void addFields(std::string& dataFormat, int& nFields, int& nSamples, icColorSpaceSignature sig, std::string prefix);
};

#endif //_ICCCMMCONFIG_H
