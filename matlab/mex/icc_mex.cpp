/** @file
    File:       icc_mex.cpp

    Contains:   MATLAB MEX gateway for IccProfLib C Wrapper API.

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
 */

/*
 * Single-file MEX gateway with command dispatch.
 *
 * MATLAB usage:
 *   handle = icc_mex('profile_open', filename)
 *   handle = icc_mex('profile_read', filename)
 *   header = icc_mex('profile_header', handle)
 *   icc_mex('profile_free', handle)
 *
 *   handle = icc_mex('cmm_create', srcSpace, dstSpace, firstIsInput)
 *   status = icc_mex('cmm_attach', handle, filename, intent, interp, lutType, useD2B, useBPC)
 *   status = icc_mex('cmm_begin', handle)
 *   info   = icc_mex('cmm_info', handle)
 *   result = icc_mex('cmm_apply', handle, pixels)
 *   icc_mex('cmm_free', handle)
 *
 *   applyHandle = icc_mex('apply_create', cmmHandle)
 *   result      = icc_mex('apply_apply', applyHandle, pixels)
 *   icc_mex('apply_free', applyHandle)
 *
 * Handles are stored as uint64 scalars in MATLAB (opaque pointers).
 * Compatible with MATLAB R2015b+ and GNU Octave 6+.
 */

#include "mex.h"
#include "IccWrapper.h"

#include <cstring>
#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <mutex>
#include <vector>

/* -----------------------------------------------------------------------
 * Typed handle registry - prevents use-after-free, double-free, forgery,
 * and cross-type confusion (e.g., passing a profile handle to cmm_free).
 * Thread-safe via mutex for concurrent parfor/spmd usage.
 * ----------------------------------------------------------------------- */
enum HandleType : uint8_t { kProfile = 0, kCmm = 1, kApply = 2 };

static std::unordered_map<uint64_t, HandleType> g_liveHandles;
static std::unordered_map<uint64_t, uint64_t> g_applyParents;
static std::mutex g_handleMutex;

static void register_handle(uint64_t h, HandleType t) {
  std::lock_guard<std::mutex> lk(g_handleMutex);
  g_liveHandles[h] = t;
}

static bool is_valid_handle(uint64_t h, HandleType expected) {
  std::lock_guard<std::mutex> lk(g_handleMutex);
  auto it = g_liveHandles.find(h);
  return it != g_liveHandles.end() && it->second == expected;
}

static void unregister_handle(uint64_t h) {
  std::lock_guard<std::mutex> lk(g_handleMutex);
  g_liveHandles.erase(h);
  g_applyParents.erase(h);
}

static bool unregister_handle_if_live(uint64_t h, HandleType expected, HandleType *actual) {
  std::lock_guard<std::mutex> lk(g_handleMutex);
  auto it = g_liveHandles.find(h);
  if (it == g_liveHandles.end())
    return false;
  if (actual)
    *actual = it->second;
  if (it->second != expected)
    return false;
  g_liveHandles.erase(it);
  g_applyParents.erase(h);
  return true;
}

static std::vector<uint64_t> unregister_apply_children(uint64_t cmmHandle) {
  std::vector<uint64_t> applyHandles;
  std::lock_guard<std::mutex> lk(g_handleMutex);
  for (auto it = g_applyParents.begin(); it != g_applyParents.end(); ) {
    if (it->second == cmmHandle) {
      uint64_t applyHandle = it->first;
      auto live = g_liveHandles.find(applyHandle);
      if (live != g_liveHandles.end() && live->second == kApply) {
        applyHandles.push_back(applyHandle);
        g_liveHandles.erase(live);
      }
      it = g_applyParents.erase(it);
    } else {
      ++it;
    }
  }
  return applyHandles;
}

/* MEX cleanup - free all outstanding handles on 'clear mex' */
static void cleanup_handles() {
  std::vector<uint64_t> applyHandles;
  std::vector<uint64_t> profileHandles;
  std::vector<uint64_t> cmmHandles;
  {
    std::lock_guard<std::mutex> lk(g_handleMutex);
    for (auto &kv : g_liveHandles) {
      switch (kv.second) {
        case kProfile: profileHandles.push_back(kv.first); break;
        case kCmm:     cmmHandles.push_back(kv.first); break;
        case kApply:   applyHandles.push_back(kv.first); break;
      }
    }
    g_liveHandles.clear();
    g_applyParents.clear();
  }
  for (uint64_t h : applyHandles)
    IccApplyFree(reinterpret_cast<CIccApplyHandle*>(h));
  for (uint64_t h : cmmHandles)
    IccCmmFree(reinterpret_cast<CIccCmmHandle*>(h));
  for (uint64_t h : profileHandles)
    IccProfileFree(reinterpret_cast<CIccProfileHandle*>(h));
}

/* Cast between void* and uint64 for MATLAB handle passing */
static inline uint64_t ptr_to_handle(void *p) {
  return reinterpret_cast<uint64_t>(p);
}

static inline void* handle_to_ptr(uint64_t h) {
  return reinterpret_cast<void*>(h);
}

/* Extract a uint64 handle from a MATLAB mxArray and validate type */
static uint64_t get_typed_handle(const mxArray *arg, const char *name, HandleType expected) {
  if (!mxIsUint64(arg) || mxGetNumberOfElements(arg) != 1)
    mexErrMsgIdAndTxt("iccdev:invalidHandle",
                      "%s: expected a uint64 scalar handle.", name);
  uint64_t h = *static_cast<uint64_t*>(mxGetData(arg));
  if (h == 0) {
    mexErrMsgIdAndTxt("iccdev:invalidHandle",
                      "%s: null handle.", name);
  }
  bool found = false;
  HandleType actual = expected;
  {
    std::lock_guard<std::mutex> lk(g_handleMutex);
    auto it = g_liveHandles.find(h);
    if (it != g_liveHandles.end()) {
      found = true;
      actual = it->second;
    }
  }
  if (!found)
    mexErrMsgIdAndTxt("iccdev:invalidHandle",
                      "%s: invalid or already-freed handle.", name);
  if (actual != expected)
    mexErrMsgIdAndTxt("iccdev:typeMismatch",
                      "%s: wrong handle type (expected %d, got %d).",
                      name, static_cast<int>(expected), static_cast<int>(actual));
  return h;
}

/* Extract a string from a MATLAB mxArray (caller must mxFree) */
static char* get_string(const mxArray *arg, const char *name) {
  if (!mxIsChar(arg))
    mexErrMsgIdAndTxt("iccdev:invalidArg",
                      "%s: expected a string argument.", name);
  char *str = mxArrayToString(arg);
  if (!str)
    mexErrMsgIdAndTxt("iccdev:outOfMemory",
                      "%s: failed to convert string.", name);
  return str;
}

/* Extract a scalar int32 from MATLAB - validates NaN/Inf/range */
static int get_int(const mxArray *arg, const char *name) {
  if (mxIsDouble(arg) && mxGetNumberOfElements(arg) == 1) {
    double v = mxGetScalar(arg);
    if (!std::isfinite(v) || v < static_cast<double>(INT32_MIN) || v > static_cast<double>(INT32_MAX))
      mexErrMsgIdAndTxt("iccdev:invalidArg",
                        "%s: value out of int32 range or not finite.", name);
    return static_cast<int>(v);
  }
  if (mxIsInt32(arg) && mxGetNumberOfElements(arg) == 1)
    return *static_cast<int32_t*>(mxGetData(arg));
  mexErrMsgIdAndTxt("iccdev:invalidArg",
                    "%s: expected a numeric scalar.", name);
  return 0;
}

/* Return a uint64 scalar to MATLAB */
static mxArray* make_handle(uint64_t h) {
  mxArray *out = mxCreateNumericMatrix(1, 1, mxUINT64_CLASS, mxREAL);
  *static_cast<uint64_t*>(mxGetData(out)) = h;
  return out;
}

/* -----------------------------------------------------------------------
 * Command: profile_open / profile_read
 * ----------------------------------------------------------------------- */
static void cmd_profile_open(int nlhs, mxArray *plhs[],
                              int nrhs, const mxArray *prhs[],
                              bool lazy) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('profile_open', filename)");

  char *fname = get_string(prhs[1], "profile_open");
  CIccProfileHandle *h;
  if (lazy)
    h = reinterpret_cast<CIccProfileHandle*>(IccProfileOpenHandle(fname));
  else
    h = reinterpret_cast<CIccProfileHandle*>(IccProfileReadHandle(fname));
  mxFree(fname);

  if (!h)
    mexErrMsgIdAndTxt("iccdev:profileError", "Failed to open ICC profile.");

  uint64_t handle = ptr_to_handle(h);
  register_handle(handle, kProfile);
  plhs[0] = make_handle(handle);
}

/* -----------------------------------------------------------------------
 * Command: profile_header
 * ----------------------------------------------------------------------- */
static void cmd_profile_header(int nlhs, mxArray *plhs[],
                                int nrhs, const mxArray *prhs[]) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('profile_header', handle)");

  uint64_t h = get_typed_handle(prhs[1], "profile_header", kProfile);
  CIccProfileHandle *pH = reinterpret_cast<CIccProfileHandle*>(handle_to_ptr(h));

  icHeader hdr;
  memset(&hdr, 0, sizeof(hdr));
  if (!IccProfileGetHeader(pH, &hdr))
    mexErrMsgIdAndTxt("iccdev:headerError", "Failed to read profile header.");

  /* Return as a MATLAB struct */
  const char *fields[] = {
    "size", "cmmId", "version", "deviceClass", "colorSpace",
    "pcs", "magic", "platform", "flags", "manufacturer", "model",
    "attributes", "renderingIntent", "creator",
    "illuminantX", "illuminantY", "illuminantZ",
    "dateYear", "dateMonth", "dateDay",
    "dateHours", "dateMinutes", "dateSeconds",
    "versionString", "profileId"
  };
  mxArray *s = mxCreateStructMatrix(1, 1, 25, fields);

  mxSetField(s, 0, "size",            mxCreateDoubleScalar(hdr.size));
  mxSetField(s, 0, "cmmId",           mxCreateDoubleScalar(hdr.cmmId));
  mxSetField(s, 0, "version",         mxCreateDoubleScalar(hdr.version));
  mxSetField(s, 0, "deviceClass",     mxCreateDoubleScalar(hdr.deviceClass));
  mxSetField(s, 0, "colorSpace",      mxCreateDoubleScalar(hdr.colorSpace));
  mxSetField(s, 0, "pcs",             mxCreateDoubleScalar(hdr.pcs));
  mxSetField(s, 0, "magic",           mxCreateDoubleScalar(hdr.magic));
  mxSetField(s, 0, "platform",        mxCreateDoubleScalar(hdr.platform));
  mxSetField(s, 0, "flags",           mxCreateDoubleScalar(hdr.flags));
  mxSetField(s, 0, "manufacturer",    mxCreateDoubleScalar(hdr.manufacturer));
  mxSetField(s, 0, "model",           mxCreateDoubleScalar(hdr.model));

  /* attributes: handle platform divergence of icUInt64Number */
#ifdef ICUINT64TYPE
  /* GCC/Clang: icUInt64Number is a scalar unsigned long long */
  double attr = static_cast<double>(hdr.attributes);
#else
  /* MSVC: icUInt64Number is icUInt32Number[2] */
  double attr = static_cast<double>(hdr.attributes[0])
              + static_cast<double>(hdr.attributes[1]) * 4294967296.0;
#endif
  mxSetField(s, 0, "attributes",      mxCreateDoubleScalar(attr));

  mxSetField(s, 0, "renderingIntent", mxCreateDoubleScalar(hdr.renderingIntent));
  mxSetField(s, 0, "creator",         mxCreateDoubleScalar(hdr.creator));

  /* Illuminant as s15Fixed16 (divide by 65536) */
  mxSetField(s, 0, "illuminantX",
             mxCreateDoubleScalar(static_cast<double>(hdr.illuminant.X) / 65536.0));
  mxSetField(s, 0, "illuminantY",
             mxCreateDoubleScalar(static_cast<double>(hdr.illuminant.Y) / 65536.0));
  mxSetField(s, 0, "illuminantZ",
             mxCreateDoubleScalar(static_cast<double>(hdr.illuminant.Z) / 65536.0));

  /* Date */
  mxSetField(s, 0, "dateYear",    mxCreateDoubleScalar(hdr.date.year));
  mxSetField(s, 0, "dateMonth",   mxCreateDoubleScalar(hdr.date.month));
  mxSetField(s, 0, "dateDay",     mxCreateDoubleScalar(hdr.date.day));
  mxSetField(s, 0, "dateHours",   mxCreateDoubleScalar(hdr.date.hours));
  mxSetField(s, 0, "dateMinutes", mxCreateDoubleScalar(hdr.date.minutes));
  mxSetField(s, 0, "dateSeconds", mxCreateDoubleScalar(hdr.date.seconds));

  /* Version string */
  char verStr[16];
  snprintf(verStr, sizeof(verStr), "%u.%u.%u",
           (hdr.version >> 24) & 0xFF,
           (hdr.version >> 20) & 0xF,
           (hdr.version >> 16) & 0xF);
  mxSetField(s, 0, "versionString", mxCreateString(verStr));

  /* Profile ID as uint8 array */
  mxArray *pid = mxCreateNumericMatrix(1, 16, mxUINT8_CLASS, mxREAL);
  memcpy(mxGetData(pid), hdr.profileID.ID8, 16);
  mxSetField(s, 0, "profileId", pid);

  plhs[0] = s;
}

/* -----------------------------------------------------------------------
 * Command: profile_free
 * ----------------------------------------------------------------------- */
static void cmd_profile_free(int nlhs, mxArray *plhs[],
                              int nrhs, const mxArray *prhs[]) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('profile_free', handle)");

  uint64_t h = get_typed_handle(prhs[1], "profile_free", kProfile);
  CIccProfileHandle *pH = reinterpret_cast<CIccProfileHandle*>(handle_to_ptr(h));
  unregister_handle(h);
  IccProfileFree(pH);
}

/* -----------------------------------------------------------------------
 * Command: cmm_create
 * ----------------------------------------------------------------------- */
static void cmd_cmm_create(int nlhs, mxArray *plhs[],
                            int nrhs, const mxArray *prhs[]) {
  icColorSpaceSignature srcSpace = icSigUnknownData;
  icColorSpaceSignature dstSpace = icSigUnknownData;
  icBoolean firstIsInput = 1;

  if (nrhs >= 2)
    srcSpace = static_cast<icColorSpaceSignature>(get_int(prhs[1], "cmm_create:srcSpace"));
  if (nrhs >= 3)
    dstSpace = static_cast<icColorSpaceSignature>(get_int(prhs[2], "cmm_create:dstSpace"));
  if (nrhs >= 4)
    firstIsInput = static_cast<icBoolean>(get_int(prhs[3], "cmm_create:firstIsInput"));

  CIccCmmHandle *cmm = reinterpret_cast<CIccCmmHandle*>(
    IccCmmCreate(srcSpace, dstSpace, firstIsInput));
  if (!cmm)
    mexErrMsgIdAndTxt("iccdev:cmmError", "Failed to create CMM.");

  uint64_t handle = ptr_to_handle(cmm);
  register_handle(handle, kCmm);
  plhs[0] = make_handle(handle);
}

/* -----------------------------------------------------------------------
 * Command: cmm_attach
 * ----------------------------------------------------------------------- */
static void cmd_cmm_attach(int nlhs, mxArray *plhs[],
                            int nrhs, const mxArray *prhs[]) {
  if (nrhs < 3)
    mexErrMsgIdAndTxt("iccdev:badArgs",
      "Usage: icc_mex('cmm_attach', cmmHandle, filename, [intent], [interp], [lutType], [useD2B], [useBPC])");

  uint64_t h = get_typed_handle(prhs[1], "cmm_attach:handle", kCmm);
  CIccCmmHandle *cmm = reinterpret_cast<CIccCmmHandle*>(handle_to_ptr(h));

  char *fname = get_string(prhs[2], "cmm_attach:filename");

  icRenderingIntent intent = icPerceptual;
  icXformInterp interp = icInterpLinear;
  icXformLutType lutType = icXformLutColor;
  icBoolean useD2B = 1;
  icBoolean useBPC = 0;

  if (nrhs >= 4) {
    int v = get_int(prhs[3], "intent");
    if (v < 0 || v > 3)
      mexErrMsgIdAndTxt("iccdev:badArgs", "intent must be 0..3, got %d.", v);
    intent = static_cast<icRenderingIntent>(v);
  }
  if (nrhs >= 5) {
    int v = get_int(prhs[4], "interp");
    if (v < 0 || v > 1)
      mexErrMsgIdAndTxt("iccdev:badArgs", "interp must be 0..1, got %d.", v);
    interp = static_cast<icXformInterp>(v);
  }
  if (nrhs >= 6) {
    int v = get_int(prhs[5], "lutType");
    if (v < 0 || v > 0xA)
      mexErrMsgIdAndTxt("iccdev:badArgs", "lutType must be 0..10, got %d.", v);
    lutType = static_cast<icXformLutType>(v);
  }
  if (nrhs >= 7) useD2B  = static_cast<icBoolean>(get_int(prhs[6], "useD2B"));
  if (nrhs >= 8) useBPC  = static_cast<icBoolean>(get_int(prhs[7], "useBPC"));

  icStatusCMM stat = IccCmmAttachProfileFile(cmm, fname, intent, interp,
                                              NULL, lutType, useD2B, useBPC, NULL);
  mxFree(fname);

  if (nlhs >= 1)
    plhs[0] = mxCreateDoubleScalar(static_cast<double>(stat));

  if (stat != icCmmStatOk)
    mexWarnMsgIdAndTxt("iccdev:attachWarning",
                       "IccCmmAttachProfileFile returned status %d.", static_cast<int>(stat));
}

/* -----------------------------------------------------------------------
 * Command: cmm_begin
 * ----------------------------------------------------------------------- */
static void cmd_cmm_begin(int nlhs, mxArray *plhs[],
                           int nrhs, const mxArray *prhs[]) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('cmm_begin', cmmHandle)");

  uint64_t h = get_typed_handle(prhs[1], "cmm_begin:handle", kCmm);
  CIccCmmHandle *cmm = reinterpret_cast<CIccCmmHandle*>(handle_to_ptr(h));

  icStatusCMM stat = IccCmmBegin(cmm);

  if (nlhs >= 1)
    plhs[0] = mxCreateDoubleScalar(static_cast<double>(stat));

  if (stat != icCmmStatOk)
    mexErrMsgIdAndTxt("iccdev:beginError",
                      "IccCmmBegin failed with status %d.", static_cast<int>(stat));
}

/* -----------------------------------------------------------------------
 * Command: cmm_info
 * ----------------------------------------------------------------------- */
static void cmd_cmm_info(int nlhs, mxArray *plhs[],
                          int nrhs, const mxArray *prhs[]) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('cmm_info', cmmHandle)");

  uint64_t h = get_typed_handle(prhs[1], "cmm_info:handle", kCmm);
  CIccCmmHandle *cmm = reinterpret_cast<CIccCmmHandle*>(handle_to_ptr(h));

  SIccCmmStruct info;
  icStatusCMM stat = IccCmmGetInfo(cmm, &info);

  if (stat != icCmmStatOk)
    mexErrMsgIdAndTxt("iccdev:infoError",
                      "IccCmmGetInfo failed with status %d.", static_cast<int>(stat));

  const char *fields[] = {"srcSpace", "dstSpace", "srcSamples", "dstSamples"};
  mxArray *s = mxCreateStructMatrix(1, 1, 4, fields);
  mxSetField(s, 0, "srcSpace",   mxCreateDoubleScalar(info.srcSpace));
  mxSetField(s, 0, "dstSpace",   mxCreateDoubleScalar(info.dstSpace));
  mxSetField(s, 0, "srcSamples", mxCreateDoubleScalar(info.srcSamples));
  mxSetField(s, 0, "dstSamples", mxCreateDoubleScalar(info.dstSamples));
  plhs[0] = s;
}

/* -----------------------------------------------------------------------
 * Command: cmm_apply
 * ----------------------------------------------------------------------- */
static void cmd_cmm_apply(int nlhs, mxArray *plhs[],
                           int nrhs, const mxArray *prhs[]) {
  if (nrhs < 3)
    mexErrMsgIdAndTxt("iccdev:badArgs",
      "Usage: result = icc_mex('cmm_apply', cmmHandle, pixels)");

  uint64_t h = get_typed_handle(prhs[1], "cmm_apply:handle", kCmm);
  CIccCmmHandle *cmm = reinterpret_cast<CIccCmmHandle*>(handle_to_ptr(h));

  /* Get pipeline info */
  SIccCmmStruct info;
  if (IccCmmGetInfo(cmm, &info) != icCmmStatOk)
    mexErrMsgIdAndTxt("iccdev:cmmError", "CMM not initialized. Call cmm_begin first.");

  /* Validate input: must be double or single matrix */
  const mxArray *pixArg = prhs[2];
  if (!mxIsDouble(pixArg) && !mxIsSingle(pixArg))
    mexErrMsgIdAndTxt("iccdev:badArgs",
                      "Pixels must be a double or single matrix (N x channels).");

  mwSize nRows = mxGetM(pixArg);
  mwSize nCols = mxGetN(pixArg);

  if (nRows == 0) {
    plhs[0] = mxCreateDoubleMatrix(0, info.dstSamples, mxREAL);
    return;
  }

  if (nCols != static_cast<mwSize>(info.srcSamples))
    mexErrMsgIdAndTxt("iccdev:channelMismatch",
                      "Expected %d source channels, got %d.",
                      static_cast<int>(info.srcSamples),
                      static_cast<int>(nCols));

  /* Allocate float buffers (IccProfLib uses float internally) */
  if (nRows > UINT32_MAX)
    mexErrMsgIdAndTxt("iccdev:overflow", "Too many pixels (max 4 billion).");

  icUInt32Number nPixels = static_cast<icUInt32Number>(nRows);
  size_t srcSize = static_cast<size_t>(nPixels) * info.srcSamples * sizeof(icFloatNumber);
  size_t dstSize = static_cast<size_t>(nPixels) * info.dstSamples * sizeof(icFloatNumber);
  if (nPixels > 0 && srcSize / nPixels / sizeof(icFloatNumber) != static_cast<size_t>(info.srcSamples))
    mexErrMsgIdAndTxt("iccdev:overflow", "Source buffer size overflow.");
  if (nPixels > 0 && dstSize / nPixels / sizeof(icFloatNumber) != static_cast<size_t>(info.dstSamples))
    mexErrMsgIdAndTxt("iccdev:overflow", "Destination buffer size overflow.");

  icFloatNumber *srcBuf = static_cast<icFloatNumber*>(mxMalloc(srcSize));
  icFloatNumber *dstBuf = static_cast<icFloatNumber*>(mxMalloc(dstSize));

  /* Copy input to row-major float buffer.
   * MATLAB stores column-major, so we transpose during copy. */
  if (mxIsDouble(pixArg)) {
    double *data = mxGetPr(pixArg);
    for (mwSize r = 0; r < nRows; r++)
      for (mwSize c = 0; c < nCols; c++)
        srcBuf[r * info.srcSamples + c] =
          static_cast<icFloatNumber>(data[c * nRows + r]);
  } else {
    float *data = static_cast<float*>(mxGetData(pixArg));
    for (mwSize r = 0; r < nRows; r++)
      for (mwSize c = 0; c < nCols; c++)
        srcBuf[r * info.srcSamples + c] = data[c * nRows + r];
  }

  /* Apply transform */
  icStatusCMM stat;
  if (nPixels == 1)
    stat = IccCmmApplyFloat(cmm, dstBuf, srcBuf);
  else
    stat = IccCmmApplyFloatMulti(cmm, dstBuf, srcBuf, nPixels);

  if (stat != icCmmStatOk) {
    mxFree(srcBuf);
    mxFree(dstBuf);
    mexErrMsgIdAndTxt("iccdev:applyError",
                      "IccCmmApply failed with status %d.", static_cast<int>(stat));
  }

  /* Create output: double matrix (N x dstChannels), transpose back */
  plhs[0] = mxCreateDoubleMatrix(nRows, info.dstSamples, mxREAL);
  double *out = mxGetPr(plhs[0]);
  for (mwSize r = 0; r < nRows; r++)
    for (int c = 0; c < info.dstSamples; c++)
      out[c * nRows + r] = static_cast<double>(dstBuf[r * info.dstSamples + c]);

  mxFree(srcBuf);
  mxFree(dstBuf);
}

/* -----------------------------------------------------------------------
 * Command: cmm_free
 * ----------------------------------------------------------------------- */
static void cmd_cmm_free(int nlhs, mxArray *plhs[],
                          int nrhs, const mxArray *prhs[]) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('cmm_free', cmmHandle)");

  uint64_t h = get_typed_handle(prhs[1], "cmm_free:handle", kCmm);
  CIccCmmHandle *cmm = reinterpret_cast<CIccCmmHandle*>(handle_to_ptr(h));
  std::vector<uint64_t> applyHandles = unregister_apply_children(h);
  for (uint64_t applyHandle : applyHandles)
    IccApplyFree(reinterpret_cast<CIccApplyHandle*>(handle_to_ptr(applyHandle)));
  unregister_handle(h);
  IccCmmFree(cmm);
}

/* -----------------------------------------------------------------------
 * Command: apply_create
 * ----------------------------------------------------------------------- */
static void cmd_apply_create(int nlhs, mxArray *plhs[],
                              int nrhs, const mxArray *prhs[]) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('apply_create', cmmHandle)");

  uint64_t h = get_typed_handle(prhs[1], "apply_create:handle", kCmm);
  CIccCmmHandle *cmm = reinterpret_cast<CIccCmmHandle*>(handle_to_ptr(h));

  CIccApplyHandle *ah = reinterpret_cast<CIccApplyHandle*>(IccCmmGetApply(cmm));
  if (!ah)
    mexErrMsgIdAndTxt("iccdev:applyError", "Failed to create apply handle.");

  uint64_t applyHandle = ptr_to_handle(ah);
  register_handle(applyHandle, kApply);
  {
    std::lock_guard<std::mutex> lk(g_handleMutex);
    g_applyParents[applyHandle] = h;
  }
  plhs[0] = make_handle(applyHandle);
}

/* -----------------------------------------------------------------------
 * Command: apply_apply
 * ----------------------------------------------------------------------- */
static void cmd_apply_apply(int nlhs, mxArray *plhs[],
                             int nrhs, const mxArray *prhs[]) {
  if (nrhs < 5)
    mexErrMsgIdAndTxt("iccdev:badArgs",
      "Usage: result = icc_mex('apply_apply', applyHandle, pixels, srcChannels, dstChannels)");

  uint64_t h = get_typed_handle(prhs[1], "apply_apply:handle", kApply);
  CIccApplyHandle *ah = reinterpret_cast<CIccApplyHandle*>(handle_to_ptr(h));

  const mxArray *pixArg = prhs[2];
  if (!mxIsDouble(pixArg) && !mxIsSingle(pixArg))
    mexErrMsgIdAndTxt("iccdev:badArgs", "Pixels must be double or single matrix.");

  int srcCh = get_int(prhs[3], "srcChannels");
  int dstCh = get_int(prhs[4], "dstChannels");

  if (srcCh <= 0 || srcCh > 16)
    mexErrMsgIdAndTxt("iccdev:badArgs", "srcChannels must be 1..16, got %d.", srcCh);
  if (dstCh <= 0 || dstCh > 16)
    mexErrMsgIdAndTxt("iccdev:badArgs", "dstChannels must be 1..16, got %d.", dstCh);

  mwSize nRows = mxGetM(pixArg);
  mwSize nCols = mxGetN(pixArg);

  if (nRows == 0) {
    plhs[0] = mxCreateDoubleMatrix(0, dstCh, mxREAL);
    return;
  }

  if (nCols != static_cast<mwSize>(srcCh))
    mexErrMsgIdAndTxt("iccdev:channelMismatch",
                      "Expected %d source channels, got %d.", srcCh, static_cast<int>(nCols));

  if (nRows > UINT32_MAX)
    mexErrMsgIdAndTxt("iccdev:overflow", "Too many pixels (max 4 billion).");

  icUInt32Number nPixels = static_cast<icUInt32Number>(nRows);

  size_t srcSize = static_cast<size_t>(nPixels) * srcCh * sizeof(icFloatNumber);
  size_t dstSize = static_cast<size_t>(nPixels) * dstCh * sizeof(icFloatNumber);
  if (nPixels > 0 && srcSize / nPixels / sizeof(icFloatNumber) != static_cast<size_t>(srcCh))
    mexErrMsgIdAndTxt("iccdev:overflow", "Source buffer size overflow.");
  if (nPixels > 0 && dstSize / nPixels / sizeof(icFloatNumber) != static_cast<size_t>(dstCh))
    mexErrMsgIdAndTxt("iccdev:overflow", "Destination buffer size overflow.");

  icFloatNumber *srcBuf = static_cast<icFloatNumber*>(mxMalloc(srcSize));
  icFloatNumber *dstBuf = static_cast<icFloatNumber*>(mxMalloc(dstSize));

  if (mxIsDouble(pixArg)) {
    double *data = mxGetPr(pixArg);
    for (mwSize r = 0; r < nRows; r++)
      for (mwSize c = 0; c < nCols; c++)
        srcBuf[r * srcCh + c] = static_cast<icFloatNumber>(data[c * nRows + r]);
  } else {
    float *data = static_cast<float*>(mxGetData(pixArg));
    for (mwSize r = 0; r < nRows; r++)
      for (mwSize c = 0; c < nCols; c++)
        srcBuf[r * srcCh + c] = data[c * nRows + r];
  }

  icStatusCMM stat;
  if (nPixels == 1)
    stat = IccApplyApplyFloat(ah, dstBuf, srcBuf);
  else
    stat = IccApplyApplyFloatMulti(ah, dstBuf, srcBuf, nPixels);

  if (stat != icCmmStatOk) {
    mxFree(srcBuf);
    mxFree(dstBuf);
    mexErrMsgIdAndTxt("iccdev:applyError",
                      "IccApplyApplyFloat failed with status %d.", static_cast<int>(stat));
  }

  plhs[0] = mxCreateDoubleMatrix(nRows, dstCh, mxREAL);
  double *out = mxGetPr(plhs[0]);
  for (mwSize r = 0; r < nRows; r++)
    for (int c = 0; c < dstCh; c++)
      out[c * nRows + r] = static_cast<double>(dstBuf[r * dstCh + c]);

  mxFree(srcBuf);
  mxFree(dstBuf);
}

/* -----------------------------------------------------------------------
 * Command: apply_free
 * ----------------------------------------------------------------------- */
static void cmd_apply_free(int nlhs, mxArray *plhs[],
                            int nrhs, const mxArray *prhs[]) {
  if (nrhs < 2)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Usage: icc_mex('apply_free', applyHandle)");

  if (!mxIsUint64(prhs[1]) || mxGetNumberOfElements(prhs[1]) != 1)
    mexErrMsgIdAndTxt("iccdev:invalidHandle",
                      "apply_free:handle: expected a uint64 scalar handle.");
  uint64_t h = *static_cast<uint64_t*>(mxGetData(prhs[1]));
  HandleType actual = kApply;
  if (h == 0 || !unregister_handle_if_live(h, kApply, &actual)) {
    if (h != 0 && actual != kApply)
      mexErrMsgIdAndTxt("iccdev:typeMismatch",
                        "apply_free: wrong handle type (expected %d, got %d).",
                        static_cast<int>(kApply), static_cast<int>(actual));
    return;
  }
  CIccApplyHandle *ah = reinterpret_cast<CIccApplyHandle*>(handle_to_ptr(h));
  IccApplyFree(ah);
}

/* =======================================================================
 * MEX Gateway
 * ======================================================================= */
void mexFunction(int nlhs, mxArray *plhs[],
                 int nrhs, const mxArray *prhs[]) {
  static bool initialized = false;
  if (!initialized) {
    mexAtExit(cleanup_handles);
    initialized = true;
  }

  if (nrhs < 1 || !mxIsChar(prhs[0]))
    mexErrMsgIdAndTxt("iccdev:badArgs",
      "First argument must be a command string.\n"
      "Commands: profile_open, profile_read, profile_header, profile_free,\n"
      "          cmm_create, cmm_attach, cmm_begin, cmm_info, cmm_apply, cmm_free,\n"
      "          apply_create, apply_apply, apply_free");

  char cmd[64];
  if (mxGetString(prhs[0], cmd, sizeof(cmd)) != 0)
    mexErrMsgIdAndTxt("iccdev:badArgs", "Command string too long (max 63 chars).");

  if (strcmp(cmd, "profile_open") == 0)        cmd_profile_open(nlhs, plhs, nrhs, prhs, true);
  else if (strcmp(cmd, "profile_read") == 0)    cmd_profile_open(nlhs, plhs, nrhs, prhs, false);
  else if (strcmp(cmd, "profile_header") == 0)  cmd_profile_header(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "profile_free") == 0)    cmd_profile_free(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "cmm_create") == 0)      cmd_cmm_create(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "cmm_attach") == 0)      cmd_cmm_attach(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "cmm_begin") == 0)       cmd_cmm_begin(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "cmm_info") == 0)        cmd_cmm_info(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "cmm_apply") == 0)       cmd_cmm_apply(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "cmm_free") == 0)        cmd_cmm_free(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "apply_create") == 0)    cmd_apply_create(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "apply_apply") == 0)     cmd_apply_apply(nlhs, plhs, nrhs, prhs);
  else if (strcmp(cmd, "apply_free") == 0)      cmd_apply_free(nlhs, plhs, nrhs, prhs);
  else
    mexErrMsgIdAndTxt("iccdev:unknownCommand",
                      "Unknown command '%s'.", cmd);
}
