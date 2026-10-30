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
/*
    Local smoke tests for the IccDEVCmm Windows ICM module.

    This harness intentionally loads IccDEVCmm.dll directly instead of calling
    RegisterCMM().  It verifies the DLL exports and exercises profile
    validation plus a two-profile RGB transform without changing system CMM
    registration.
*/

#include <windows.h>
#include <icm.h>

#include <cstring>
#include <cstdio>
#include <cwchar>
#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <vector>

typedef DWORD(WINAPI *CMGetInfoFn)(DWORD);
typedef BOOL(WINAPI *CMIsProfileValidFn)(HPROFILE, LPBOOL);
typedef HCMTRANSFORM(WINAPI *CMCreateMultiProfileTransformFn)(HPROFILE *, DWORD, PDWORD, DWORD, DWORD);
typedef HCMTRANSFORM(WINAPI *CMCreateTransformExtFn)(LPLOGCOLORSPACEA, LPDEVCHARACTER,
                                                     LPDEVCHARACTER, DWORD);
typedef HCMTRANSFORM(WINAPI *CMCreateTransformExtWFn)(LPLOGCOLORSPACEW, LPDEVCHARACTER,
                                                      LPDEVCHARACTER, DWORD);
typedef BOOL(WINAPI *CMTranslateColorsFn)(HCMTRANSFORM, LPCOLOR, DWORD, COLORTYPE, LPCOLOR, COLORTYPE);
typedef BOOL(WINAPI *CMTranslateRGBsExtFn)(HCMTRANSFORM, LPVOID, BMFORMAT, DWORD, DWORD, DWORD,
                                           LPVOID, BMFORMAT, DWORD, LPBMCALLBACKFN, LPARAM);
typedef BOOL(WINAPI *CMDeleteTransformFn)(HCMTRANSFORM);
typedef BOOL(WINAPI *CMConvertColorNameToIndexFn)(HPROFILE, PCOLOR_NAME, PDWORD, DWORD);
typedef BOOL(WINAPI *CMConvertIndexToColorNameFn)(HPROFILE, PDWORD, PCOLOR_NAME, DWORD);

struct CmmApi
{
    HMODULE module;
    CMGetInfoFn get_info;
    CMIsProfileValidFn is_profile_valid;
    CMCreateMultiProfileTransformFn create_multi_profile_transform;
    CMCreateTransformExtFn create_transform_ext;
    CMCreateTransformExtWFn create_transform_ext_w;
    CMTranslateColorsFn translate_colors;
    CMTranslateRGBsExtFn translate_rgbs_ext;
    CMDeleteTransformFn delete_transform;
    CMConvertColorNameToIndexFn convert_color_name_to_index;
    CMConvertIndexToColorNameFn convert_index_to_color_name;
};

static void print_last_error(const char *what)
{
    std::printf("[FAIL] %s failed, GetLastError=%lu\n", what, GetLastError());
}

static std::wstring normalize_windows_path(const char *path)
{
    std::wstring wide(path, path + std::strlen(path));
    for (wchar_t &ch : wide) {
        if (ch == L'/')
            ch = L'\\';
    }

    DWORD needed = GetFullPathNameW(wide.c_str(), 0, NULL, NULL);
    if (!needed)
        return wide;

    std::vector<wchar_t> buffer(needed);
    DWORD written = GetFullPathNameW(wide.c_str(), needed, buffer.data(), NULL);
    if (!written || written >= needed)
        return wide;

    return std::wstring(buffer.data(), written);
}

static FARPROC require_proc(HMODULE module, const char *name)
{
    FARPROC proc = GetProcAddress(module, name);
    if (!proc)
        std::printf("[FAIL] Missing export: %s\n", name);
    else
        std::printf("[OK] Export present: %s\n", name);
    return proc;
}

static bool load_cmm(const wchar_t *dll_path, CmmApi &api)
{
    api = {};
    api.module = LoadLibraryW(dll_path);
    if (!api.module) {
        print_last_error("LoadLibraryW(IccDEVCmm.dll)");
        return false;
    }

    api.get_info = reinterpret_cast<CMGetInfoFn>(require_proc(api.module, "CMGetInfo"));
    api.is_profile_valid = reinterpret_cast<CMIsProfileValidFn>(require_proc(api.module, "CMIsProfileValid"));
    api.create_multi_profile_transform = reinterpret_cast<CMCreateMultiProfileTransformFn>(
        require_proc(api.module, "CMCreateMultiProfileTransform"));
    api.create_transform_ext = reinterpret_cast<CMCreateTransformExtFn>(
        require_proc(api.module, "CMCreateTransformExt"));
    api.create_transform_ext_w = reinterpret_cast<CMCreateTransformExtWFn>(
        require_proc(api.module, "CMCreateTransformExtW"));
    api.translate_colors = reinterpret_cast<CMTranslateColorsFn>(require_proc(api.module, "CMTranslateColors"));
    api.translate_rgbs_ext = reinterpret_cast<CMTranslateRGBsExtFn>(
        require_proc(api.module, "CMTranslateRGBsExt"));
    api.delete_transform = reinterpret_cast<CMDeleteTransformFn>(require_proc(api.module, "CMDeleteTransform"));
    api.convert_color_name_to_index = reinterpret_cast<CMConvertColorNameToIndexFn>(
        require_proc(api.module, "CMConvertColorNameToIndex"));
    api.convert_index_to_color_name = reinterpret_cast<CMConvertIndexToColorNameFn>(
        require_proc(api.module, "CMConvertIndexToColorName"));

    return api.get_info &&
           api.is_profile_valid &&
           api.create_multi_profile_transform &&
           api.create_transform_ext &&
           api.create_transform_ext_w &&
           api.translate_colors &&
           api.translate_rgbs_ext &&
           api.delete_transform &&
           api.convert_color_name_to_index &&
           api.convert_index_to_color_name;
}

static bool check_cmm_info(const CmmApi &api)
{
    const DWORD ident = api.get_info(CMM_IDENT);
    const DWORD dll_version = api.get_info(CMM_DLL_VERSION);
    const DWORD cmm_version = api.get_info(CMM_VERSION);
    const DWORD win_version = api.get_info(CMM_WIN_VERSION);

    std::printf("[INFO] CMM_IDENT=0x%08lX\n", ident);
    std::printf("[INFO] CMM_DLL_VERSION=0x%08lX\n", dll_version);
    std::printf("[INFO] CMM_VERSION=0x%08lX\n", cmm_version);
    std::printf("[INFO] CMM_WIN_VERSION=0x%08lX\n", win_version);

    if (ident != 0x49434344UL) {
        std::printf("[FAIL] Expected CMM_IDENT 0x49434344 ('ICCD')\n");
        return false;
    }

    std::printf("[OK] CMM identity matches 'ICCD'\n");
    return true;
}

static HPROFILE open_profile(const wchar_t *profile_path)
{
    PROFILE profile = {};
    profile.dwType = PROFILE_FILENAME;
    profile.pProfileData = const_cast<wchar_t *>(profile_path);
    profile.cbDataSize = static_cast<DWORD>((std::wcslen(profile_path) + 1) * sizeof(wchar_t));

    HPROFILE handle = OpenColorProfileW(&profile, PROFILE_READ, FILE_SHARE_READ, OPEN_EXISTING);
    if (!handle)
        print_last_error("OpenColorProfileW");

    return handle;
}

static bool check_profile_valid(const CmmApi &api, HPROFILE profile)
{
    BOOL valid = FALSE;
    if (!api.is_profile_valid(profile, &valid)) {
        print_last_error("CMIsProfileValid");
        return false;
    }

    if (!valid) {
        std::printf("[FAIL] CMIsProfileValid reported invalid profile\n");
        return false;
    }

    std::printf("[OK] CMIsProfileValid accepted the test profile\n");
    return true;
}

static bool check_rgb_roundtrip_transform(const CmmApi &api, HPROFILE profile)
{
    HPROFILE profiles[2] = { profile, profile };
    DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };

    HCMTRANSFORM transform = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
    if (!transform) {
        print_last_error("CMCreateMultiProfileTransform");
        return false;
    }

    COLOR input = {};
    input.rgb.red = 32768;
    input.rgb.green = 16384;
    input.rgb.blue = 49152;

    COLOR output = {};
    BOOL ok = api.translate_colors(transform, &input, 1, COLOR_RGB, &output, COLOR_RGB);
    DWORD translate_error = ok ? ERROR_SUCCESS : GetLastError();

    if (!api.delete_transform(transform)) {
        print_last_error("CMDeleteTransform");
        return false;
    }

    if (!ok) {
        SetLastError(translate_error);
        print_last_error("CMTranslateColors");
        return false;
    }

    std::printf("[INFO] RGB in=(%u,%u,%u) out=(%u,%u,%u)\n",
                input.rgb.red, input.rgb.green, input.rgb.blue,
                output.rgb.red, output.rgb.green, output.rgb.blue);
    std::printf("[OK] CMCreateMultiProfileTransform/CMTranslateColors/CMDeleteTransform succeeded\n");
    return true;
}

static bool check_legacy_transform_creation(const CmmApi &api, HPROFILE profile)
{
    DWORD size = 0;
    GetColorProfileFromHandle(profile, NULL, &size);
    if (!size) {
        print_last_error("GetColorProfileFromHandle size");
        return false;
    }
    std::vector<BYTE> data(size);
    if (!GetColorProfileFromHandle(profile, data.data(), &size)) {
        print_last_error("GetColorProfileFromHandle data");
        return false;
    }

    LPDEVCHARACTER profile_data = reinterpret_cast<LPDEVCHARACTER>(data.data());
    HCMTRANSFORM ansi = api.create_transform_ext(NULL, profile_data, NULL, NORMAL_MODE);
    HCMTRANSFORM wide = api.create_transform_ext_w(NULL, profile_data, NULL, NORMAL_MODE);
    bool passed = ansi && wide;
    if (!passed)
        print_last_error("CMCreateTransformExt A/W");
    if (ansi && !api.delete_transform(ansi))
        passed = false;
    if (wide && !api.delete_transform(wide))
        passed = false;
    if (passed)
        std::printf("[OK] Legacy CMCreateTransformExt A/W created and deleted transforms\n");
    return passed;
}

static bool check_rejected_device_link_profile(const CmmApi &api, HPROFILE profile)
{
  DWORD size = 0;
  GetColorProfileFromHandle(profile, NULL, &size);
  if (size < 40) {
    print_last_error("GetColorProfileFromHandle size (device link)");
    return false;
  }

  std::vector<BYTE> rgb_data(size);
  if (!GetColorProfileFromHandle(profile, rgb_data.data(), &size)) {
    print_last_error("GetColorProfileFromHandle data (device link)");
    return false;
  }
  rgb_data.resize(size);
  std::vector<BYTE> link_data = rgb_data;
  std::memcpy(link_data.data() + 12, "link", 4);

  LPDEVCHARACTER rgb = reinterpret_cast<LPDEVCHARACTER>(rgb_data.data());
  LPDEVCHARACTER link = reinterpret_cast<LPDEVCHARACTER>(link_data.data());
  HCMTRANSFORM ansi = api.create_transform_ext(NULL, link, rgb, NORMAL_MODE);
  HCMTRANSFORM wide = api.create_transform_ext_w(NULL, link, rgb, NORMAL_MODE);
  if (ansi || wide) {
    if (ansi)
      api.delete_transform(ansi);
    if (wide)
      api.delete_transform(wide);
    std::printf("[FAIL] Legacy A/W transform accepted a device link after a target\n");
    return false;
  }

  PROFILE link_profile = {};
  link_profile.dwType = PROFILE_MEMBUFFER;
  link_profile.pProfileData = link_data.data();
  link_profile.cbDataSize = size;
  HPROFILE link_handle = OpenColorProfileW(&link_profile, PROFILE_READ,
                                          FILE_SHARE_READ, OPEN_EXISTING);
  if (!link_handle) {
    print_last_error("OpenColorProfileW (device link)");
    return false;
  }
  HPROFILE profiles[2] = { profile, link_handle };
  DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
  HCMTRANSFORM multi = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
  CloseColorProfile(link_handle);
  if (multi) {
    api.delete_transform(multi);
    std::printf("[FAIL] Multiprofile transform accepted an output device link\n");
    return false;
  }

  // A rejected profile must not poison the CMM's subsequent allocations.
  HPROFILE valid_profiles[2] = { profile, profile };
  HCMTRANSFORM valid = api.create_multi_profile_transform(valid_profiles, 2, intents, 2, 0);
  if (!valid) {
    print_last_error("valid transform after rejected device link");
    return false;
  }
  bool deleted = api.delete_transform(valid) != FALSE;
  if (!deleted)
    print_last_error("delete valid transform after rejected device link");
  if (deleted)
    std::printf("[OK] Device-link rejection left ownership with AddXform (A/W and multi)\n");
  return deleted;
}

static bool check_transform_handle_lifecycle(const CmmApi &api, HPROFILE profile)
{
    HPROFILE profiles[2] = { profile, profile };
    DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
    HCMTRANSFORM old_handle = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
    if (!old_handle || !api.delete_transform(old_handle)) {
        print_last_error("create/delete transform (handle lifecycle)");
        return false;
    }

    HCMTRANSFORM new_handle = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
    if (!new_handle) {
        print_last_error("recreate transform (handle lifecycle)");
        return false;
    }

    COLOR input = {};
    input.rgb.red = 32768;
    COLOR output = {};
    SetLastError(ERROR_SUCCESS);
    BOOL stale_accepted = api.translate_colors(old_handle, &input, 1, COLOR_RGB,
                                                &output, COLOR_RGB);
    DWORD stale_error = GetLastError();
    bool passed = old_handle != new_handle && !stale_accepted &&
                  stale_error == ERROR_INVALID_HANDLE;
    if (!passed) {
        std::printf("[FAIL] Deleted transform handle was reused or accepted: "
                    "old=%zu new=%zu accepted=%d error=%lu\n",
                    reinterpret_cast<size_t>(old_handle),
                    reinterpret_cast<size_t>(new_handle), stale_accepted, stale_error);
    }
    if (!api.delete_transform(new_handle)) {
        print_last_error("delete recreated transform");
        passed = false;
    }

    if (passed)
        std::printf("[OK] Deleted transform handle is rejected after a new creation\n");
    return passed;
}

static bool check_concurrent_transform_creation(const CmmApi &api, HPROFILE profile)
{
    const unsigned thread_count = 12;
    HPROFILE profiles[2] = { profile, profile };
    DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
    std::vector<HCMTRANSFORM> handles(thread_count, NULL);
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    std::atomic<unsigned> ready(0);
    std::atomic<bool> start(false);

    for (unsigned n = 0; n < thread_count; ++n) {
        workers.emplace_back([&, n]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            handles[n] = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
        });
    }
    while (ready.load(std::memory_order_acquire) != thread_count)
        std::this_thread::yield();
    start.store(true, std::memory_order_release);
    for (std::thread &worker : workers)
        worker.join();

    std::set<HCMTRANSFORM> unique;
    bool passed = true;
    for (HCMTRANSFORM handle : handles) {
        size_t id = reinterpret_cast<size_t>(handle);
        if (id < 256 || id > 65535 || !unique.insert(handle).second)
            passed = false;
    }
    for (HCMTRANSFORM handle : unique) {
        if (!api.delete_transform(handle)) {
            print_last_error("delete concurrently created transform");
            passed = false;
        }
    }
    if (!passed) {
        std::printf("[FAIL] Concurrent creation returned %zu unique handles for %u requests\n",
                    unique.size(), thread_count);
    }
    else {
        std::printf("[OK] Concurrent creation returned %u distinct handles\n", thread_count);
    }
    return passed;
}

static bool check_concurrent_translation(const CmmApi &api, HPROFILE profile)
{
    HPROFILE profiles[2] = { profile, profile };
    DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
    HCMTRANSFORM transform = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
    if (!transform) {
        print_last_error("create transform (concurrent translation)");
        return false;
    }

    COLOR input = {};
    input.rgb.red = 32768;
    input.rgb.green = 16384;
    input.rgb.blue = 49152;
    COLOR expected = {};
    if (!api.translate_colors(transform, &input, 1, COLOR_RGB, &expected, COLOR_RGB)) {
        print_last_error("reference translation");
        api.delete_transform(transform);
        return false;
    }

    const unsigned thread_count = 8;
    const unsigned repetitions = 100;
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    std::atomic<unsigned> failures(0);
    for (unsigned n = 0; n < thread_count; ++n) {
        workers.emplace_back([&]() {
            for (unsigned repeat = 0; repeat < repetitions; ++repeat) {
                COLOR output = {};
                if (!api.translate_colors(transform, &input, 1, COLOR_RGB,
                                          &output, COLOR_RGB) ||
                    std::memcmp(&output, &expected, sizeof(COLOR)) != 0)
                    failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (std::thread &worker : workers)
        worker.join();
    bool deleted = api.delete_transform(transform) != FALSE;
    if (failures.load() || !deleted) {
        std::printf("[FAIL] Concurrent translations disagreed with reference: %u failures, "
                    "deleted=%d\n", failures.load(), deleted);
        return false;
    }
    std::printf("[OK] %u concurrent translations matched the reference\n",
                thread_count * repetitions);
    return true;
}

struct ReentrantCallbackContext
{
    const CmmApi *api;
    HCMTRANSFORM transform;
    bool translated;
    bool deleted;
};

static BOOL WINAPI reentrant_progress_callback(ULONG row, ULONG, LPARAM data)
{
    ReentrantCallbackContext &context =
        *reinterpret_cast<ReentrantCallbackContext *>(data);
    if (row == 0) {
        COLOR input = {};
        input.rgb.red = 32768;
        COLOR output = {};
        context.translated = context.api->translate_colors(
            context.transform, &input, 1, COLOR_RGB, &output, COLOR_RGB) != FALSE;
        context.deleted = context.api->delete_transform(context.transform) != FALSE;
    }
    return TRUE;
}

static bool check_callback_transform_lifetime(const CmmApi &api, HPROFILE profile)
{
    HPROFILE profiles[2] = { profile, profile };
    DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
    HCMTRANSFORM transform = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
    if (!transform) {
        print_last_error("create transform (callback lifetime)");
        return false;
    }

    unsigned char input[6] = { 128, 64, 192, 128, 64, 192 };
    unsigned char output[6] = {};
    ReentrantCallbackContext callback = { &api, transform, false, false };
    BOOL translated = api.translate_rgbs_ext(transform, input, BM_RGBTRIPLETS,
                                             1, 2, 3, output, BM_RGBTRIPLETS, 3,
                                             reentrant_progress_callback,
                                             reinterpret_cast<LPARAM>(&callback));
    bool passed = translated && callback.translated && callback.deleted &&
                  std::memcmp(output, output + 3, 3) == 0;
    if (!callback.deleted)
        api.delete_transform(transform);
    if (!passed) {
        std::printf("[FAIL] Reentrant callback translation/deletion: outer=%d "
                    "inner=%d deleted=%d\n", translated,
                    callback.translated, callback.deleted);
        return false;
    }
    std::printf("[OK] Progress callback reentered and deleted an in-flight transform\n");
    return true;
}

struct CrossCallbackContext
{
  const CmmApi *api;
  HCMTRANSFORM peer;
  std::atomic<unsigned> *ready;
  bool translated;
};

static BOOL WINAPI cross_transform_progress_callback(ULONG row, ULONG, LPARAM data)
{
  CrossCallbackContext &context = *reinterpret_cast<CrossCallbackContext *>(data);
  if (row != 0)
    return TRUE;

  context.ready->fetch_add(1, std::memory_order_release);
  ULONGLONG started = GetTickCount64();
  while (context.ready->load(std::memory_order_acquire) != 2) {
    if (GetTickCount64() - started > 10000)
      return FALSE;
    Sleep(1);
  }

  COLOR input = {};
  input.rgb.red = 32768;
  COLOR output = {};
  context.translated = context.api->translate_colors(context.peer, &input, 1,
                                                     COLOR_RGB, &output, COLOR_RGB) != FALSE;
  return context.translated ? TRUE : FALSE;
}

static bool check_cross_transform_callbacks(const CmmApi &api, HPROFILE profile)
{
  HPROFILE profiles[2] = { profile, profile };
  DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
  HCMTRANSFORM first = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
  HCMTRANSFORM second = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
  if (!first || !second) {
    if (first)
      api.delete_transform(first);
    if (second)
      api.delete_transform(second);
    print_last_error("create transforms (cross callbacks)");
    return false;
  }

  std::atomic<unsigned> ready(0);
  std::atomic<unsigned> finished(0);
  CrossCallbackContext callbacks[2] = {
    { &api, second, &ready, false },
    { &api, first, &ready, false }
  };
  BOOL translated[2] = { FALSE, FALSE };
  BYTE source[2][3] = { { 128, 64, 192 }, { 128, 64, 192 } };
  BYTE destination[2][3] = {};
  std::thread workers[2] = {
    std::thread([&]() {
      translated[0] = api.translate_rgbs_ext(first, source[0], BM_RGBTRIPLETS,
                                              1, 1, 3, destination[0], BM_RGBTRIPLETS,
                                              3, cross_transform_progress_callback,
                                              reinterpret_cast<LPARAM>(&callbacks[0]));
      finished.fetch_add(1, std::memory_order_release);
    }),
    std::thread([&]() {
      translated[1] = api.translate_rgbs_ext(second, source[1], BM_RGBTRIPLETS,
                                              1, 1, 3, destination[1], BM_RGBTRIPLETS,
                                              3, cross_transform_progress_callback,
                                              reinterpret_cast<LPARAM>(&callbacks[1]));
      finished.fetch_add(1, std::memory_order_release);
    })
  };

  ULONGLONG started = GetTickCount64();
  while (finished.load(std::memory_order_acquire) != 2) {
    if (GetTickCount64() - started > 15000) {
      std::fprintf(stderr, "[FAIL] Cross-transform progress callbacks deadlocked\n");
      std::fflush(stderr);
      TerminateProcess(GetCurrentProcess(), 1);
    }
    Sleep(1);
  }
  workers[0].join();
  workers[1].join();
  bool first_deleted = api.delete_transform(first) != FALSE;
  bool second_deleted = api.delete_transform(second) != FALSE;
  bool deleted = first_deleted && second_deleted;
  bool passed = translated[0] && translated[1] && callbacks[0].translated &&
                callbacks[1].translated && deleted;
  if (passed)
    std::printf("[OK] Independent progress callbacks cross-translated without deadlock\n");
  else
    std::printf("[FAIL] Cross-transform callbacks: outer=(%d,%d), inner=(%d,%d), deleted=%d\n",
                translated[0], translated[1], callbacks[0].translated,
                callbacks[1].translated, deleted);
  return passed;
}

static bool check_null_transform_intents(const CmmApi &api, HPROFILE profile)
{
    HPROFILE profiles[2] = { profile, profile };
    SetLastError(ERROR_SUCCESS);
    HCMTRANSFORM transform = api.create_multi_profile_transform(profiles, 2, NULL, 1, 0);
    if (transform) {
        api.delete_transform(transform);
        std::printf("[FAIL] CMCreateMultiProfileTransform accepted a null intents array\n");
        return false;
    }
    if (GetLastError() != ERROR_BAD_ARGUMENTS) {
        print_last_error("CMCreateMultiProfileTransform (null intents)");
        return false;
    }
    std::printf("[OK] CMCreateMultiProfileTransform rejected a null intents array\n");

    DWORD too_many_intents[3] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL,
                                  INTENT_PERCEPTUAL };
    SetLastError(ERROR_SUCCESS);
    transform = api.create_multi_profile_transform(profiles, 2, too_many_intents, 3, 0);
    if (transform || GetLastError() != ERROR_BAD_ARGUMENTS) {
        if (transform) api.delete_transform(transform);
        print_last_error("CMCreateMultiProfileTransform (invalid intent count)");
        return false;
    }

    DWORD bad_intent = 99;
    SetLastError(ERROR_SUCCESS);
    transform = api.create_multi_profile_transform(profiles, 2, &bad_intent, 1, 0);
    if (transform || GetLastError() != ERROR_BAD_ARGUMENTS) {
        if (transform) api.delete_transform(transform);
        print_last_error("CMCreateMultiProfileTransform (invalid intent value)");
        return false;
    }
    std::printf("[OK] CMCreateMultiProfileTransform rejected invalid intent count/value\n");
    return true;
}

static bool check_profile_intent_order(const CmmApi &api, HPROFILE profile)
{
    // The sRGB preference fixture has distinct perceptual and relative
    // colorimetric B2A tags. Changing only the second profile's intent must
    // change at least one translated color.
    HPROFILE profiles[2] = { profile, profile };
    DWORD perceptual[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
    DWORD mixed[2] = { INTENT_PERCEPTUAL, INTENT_RELATIVE_COLORIMETRIC };
    HCMTRANSFORM same = api.create_multi_profile_transform(profiles, 2, perceptual, 2, 0);
    HCMTRANSFORM different = api.create_multi_profile_transform(profiles, 2, mixed, 2, 0);
    if (!same || !different) {
        print_last_error("create transforms with distinct profile intents");
        if (same) api.delete_transform(same);
        if (different) api.delete_transform(different);
        return false;
    }

    COLOR inputs[4] = {};
    inputs[0].rgb = { 32768, 16384, 49152 };
    inputs[1].rgb = { 10000, 40000, 5000 };
    inputs[2].rgb = { 58000, 2000, 50000 };
    inputs[3].rgb = { 6000, 1000, 62000 };
    COLOR same_outputs[4] = {};
    COLOR different_outputs[4] = {};
    bool translated = api.translate_colors(same, inputs, 4, COLOR_RGB,
                                           same_outputs, COLOR_RGB) &&
                      api.translate_colors(different, inputs, 4, COLOR_RGB,
                                           different_outputs, COLOR_RGB);
    DWORD translate_error = translated ? ERROR_SUCCESS : GetLastError();
    api.delete_transform(same);
    api.delete_transform(different);
    if (!translated) {
        SetLastError(translate_error);
        print_last_error("translate with distinct profile intents");
        return false;
    }

    if (std::memcmp(same_outputs, different_outputs, sizeof(same_outputs)) == 0) {
        std::printf("[FAIL] Second profile intent was ignored; perceptual/perceptual "
                    "and perceptual/relative produced identical outputs\n");
        return false;
    }

    std::printf("[OK] Changing the second profile intent changes RGB output\n");
    return true;
}

static bool check_named_color_conversion(const CmmApi &api, const wchar_t *profile_path)
{
    HPROFILE profile = open_profile(profile_path);
    if (!profile)
        return false;

    COLOR_NAME names[2] = {};
    std::memcpy(names[0], "Red", sizeof("Red"));
    std::memcpy(names[1], "Blue", sizeof("Blue"));
    DWORD indices[3] = { 0xEEEEEEEEUL, 0xEEEEEEEEUL, 0xEEEEEEEEUL };
    bool passed = true;

    if (!api.convert_color_name_to_index(profile, names, indices, 2) ||
        indices[0] != 1 || indices[1] != 9 || indices[2] != 0xEEEEEEEEUL) {
        std::printf("[FAIL] CMConvertColorNameToIndex expected Red=1, Blue=9 in adjacent slots; "
                    "got %lu,%lu,%lu\n", indices[0], indices[1], indices[2]);
        passed = false;
    }

    DWORD requests[3] = { 0, 1, 2 };
    COLOR_NAME results[2] = {};
    if (!api.convert_index_to_color_name(profile, requests, results, 2) ||
        std::strcmp(results[0], "Violet-Red") != 0 ||
        std::strcmp(results[1], "Red") != 0) {
        std::printf("[FAIL] CMConvertIndexToColorName expected indices 0,1 to yield "
                    "Violet-Red,Red; got %s,%s\n", results[0], results[1]);
        passed = false;
    }

    DWORD end_index = 20;  // NamedColorV4.xml contains 20 entries, indexed 0..19.
    COLOR_NAME end_result[1] = {};
    SetLastError(ERROR_SUCCESS);
    BOOL accepted_end = api.convert_index_to_color_name(profile, &end_index, end_result, 1);
    if (accepted_end || GetLastError() != ERROR_BAD_ARGUMENTS) {
        std::printf("[FAIL] CMConvertIndexToColorName accepted index==size or returned "
                    "unexpected error %lu\n", GetLastError());
        passed = false;
    }

    CloseColorProfile(profile);
    if (passed)
        std::printf("[OK] Named-color index conversion and bounds checks passed\n");
    return passed;
}

// Bitmap translation with an explicit destination stride (#1953).
//
// CMTranslateRGBs always passes dwOutputStride 0, so only a direct
// CMTranslateRGBsExt caller reaches the strided row-advance path, and nothing
// here exercised it.  Every source row carries identical pixels, which makes
// the two regressions checkable without depending on what the transform does
// to a colour:
//
//   * rows must land on the caller's stride, so the pad bytes between rows
//     stay at the fill value -- the shipped code advanced the destination by
//     the pixel pitch instead and wrote over them;
//   * identical input rows must give identical output rows -- the shipped code
//     re-based the *source* pointer into the destination bitmap, so rows after
//     the first were converted from whatever was in the destination;
//   * a full-scale 16-bit channel must come back full-scale -- the shipped
//     BM_16b_RGB writer quantized to 255 through an icUInt8Number cast.
static bool check_rgbs_ext_output_stride(const CmmApi &api, HPROFILE profile)
{
    HPROFILE profiles[2] = { profile, profile };
    DWORD intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };

    HCMTRANSFORM transform = api.create_multi_profile_transform(profiles, 2, intents, 2, 0);
    if (!transform) {
        print_last_error("CMCreateMultiProfileTransform (RGBsExt)");
        return false;
    }

    const DWORD width = 2;
    const DWORD height = 3;
    // Explicit cast: sizeof makes the expression size_t, and narrowing it to
    // DWORD implicitly draws C4267 from MSVC.
    const DWORD row_bytes = static_cast<DWORD>(width * 3 * sizeof(unsigned short));  // BM_16b_RGB
    const DWORD input_stride = row_bytes;                        // packed source
    const DWORD pad_bytes = 4;
    const DWORD output_stride = row_bytes + pad_bytes;           // padded destination
    const unsigned char fill = 0xAA;

    std::vector<unsigned short> src(width * height * 3, 0xFFFF);
    std::vector<unsigned char> dest(output_stride * height, fill);

    BOOL ok = api.translate_rgbs_ext(transform,
                                     src.data(), BM_16b_RGB,
                                     width, height, input_stride,
                                     dest.data(), BM_16b_RGB, output_stride,
                                     NULL, 0);
    DWORD translate_error = ok ? ERROR_SUCCESS : GetLastError();

    if (!api.delete_transform(transform)) {
        print_last_error("CMDeleteTransform (RGBsExt)");
        return false;
    }

    if (!ok) {
        SetLastError(translate_error);
        print_last_error("CMTranslateRGBsExt");
        return false;
    }

    bool passed = true;

    for (DWORD row = 0; row < height; row++) {
        const unsigned char *row_base = dest.data() + static_cast<size_t>(row) * output_stride;

        for (DWORD pad = 0; pad < pad_bytes; pad++) {
            if (row_base[row_bytes + pad] != fill) {
                std::printf("[FAIL] Row %lu padding byte %lu was overwritten (0x%02X); "
                            "destination rows are not honouring dwOutputStride\n",
                            row, pad, row_base[row_bytes + pad]);
                passed = false;
                break;
            }
        }

        // Deliberately a loose bound.  The defect quantized to 255 -- 0.4% of
        // range -- so any threshold well clear of 8-bit separates the two
        // encodings, and keeping it loose stops this from becoming an
        // assertion about round-trip colour accuracy.
        const unsigned short *samples = reinterpret_cast<const unsigned short *>(row_base);
        for (DWORD sample = 0; sample < width * 3; sample++) {
            if (samples[sample] < 30000) {
                std::printf("[FAIL] Row %lu sample %lu is %u; a full-scale 16-bit channel "
                            "must stay in the 16-bit range (a value near 255 indicates the "
                            "8-bit quantization)\n",
                            row, sample, samples[sample]);
                passed = false;
                break;
            }
        }

        if (row > 0 && std::memcmp(row_base, dest.data(), row_bytes) != 0) {
            std::printf("[FAIL] Row %lu differs from row 0 although every source row is "
                        "identical; source pixels were not read from the source bitmap\n",
                        row);
            passed = false;
        }
    }

    if (!passed)
        return false;

    const unsigned short *first = reinterpret_cast<const unsigned short *>(dest.data());
    std::printf("[INFO] BM_16b_RGB %lux%lu stride=%lu first pixel=(%u,%u,%u)\n",
                width, height, output_stride, first[0], first[1], first[2]);
    std::printf("[OK] CMTranslateRGBsExt honoured dwOutputStride and 16-bit sample range\n");
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        std::printf("usage: IccDEVCmmSmoke.exe <IccDEVCmm.dll> <rgb-profile.icc> [named-profile.icc]\n");
        return 2;
    }

    const std::wstring dll_path = normalize_windows_path(argv[1]);
    const std::wstring profile_path = normalize_windows_path(argv[2]);
    CmmApi api = {};

    if (!load_cmm(dll_path.c_str(), api))
        return 1;

    if (!check_cmm_info(api))
        return 1;

    HPROFILE profile = open_profile(profile_path.c_str());
    if (!profile)
        return 1;

    bool ok = check_profile_valid(api, profile) &&
              check_rgb_roundtrip_transform(api, profile) &&
              check_legacy_transform_creation(api, profile) &&
              check_rejected_device_link_profile(api, profile) &&
              check_rgbs_ext_output_stride(api, profile) &&
              check_null_transform_intents(api, profile) &&
              check_profile_intent_order(api, profile);

    if (ok) {
        bool lifecycle_ok = check_transform_handle_lifecycle(api, profile);
        bool concurrent_ok = check_concurrent_transform_creation(api, profile);
        bool translation_ok = check_concurrent_translation(api, profile);
        bool callback_ok = check_callback_transform_lifetime(api, profile);
        bool cross_callback_ok = check_cross_transform_callbacks(api, profile);
        ok = lifecycle_ok && concurrent_ok && translation_ok && callback_ok &&
             cross_callback_ok;
    }

    CloseColorProfile(profile);

    if (ok && argc == 4) {
        const std::wstring named_profile_path = normalize_windows_path(argv[3]);
        ok = check_named_color_conversion(api, named_profile_path.c_str());
    }

    if (!ok)
        return 1;

    std::printf("[OK] IccDEVCmm smoke test passed\n");
    return 0;
}
