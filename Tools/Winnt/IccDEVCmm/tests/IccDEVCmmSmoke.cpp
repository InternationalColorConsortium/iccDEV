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
#include <string>
#include <vector>

typedef DWORD(WINAPI *CMGetInfoFn)(DWORD);
typedef BOOL(WINAPI *CMIsProfileValidFn)(HPROFILE, LPBOOL);
typedef HCMTRANSFORM(WINAPI *CMCreateMultiProfileTransformFn)(HPROFILE *, DWORD, PDWORD, DWORD, DWORD);
typedef BOOL(WINAPI *CMTranslateColorsFn)(HCMTRANSFORM, LPCOLOR, DWORD, COLORTYPE, LPCOLOR, COLORTYPE);
typedef BOOL(WINAPI *CMDeleteTransformFn)(HCMTRANSFORM);

struct CmmApi
{
    HMODULE module;
    CMGetInfoFn get_info;
    CMIsProfileValidFn is_profile_valid;
    CMCreateMultiProfileTransformFn create_multi_profile_transform;
    CMTranslateColorsFn translate_colors;
    CMDeleteTransformFn delete_transform;
};

static void print_last_error(const char *what)
{
    std::printf("[FAIL] %s failed, GetLastError=%lu\n", what, GetLastError());
}

static std::wstring widen_ascii_path(const char *path)
{
    return std::wstring(path, path + std::strlen(path));
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
    api.translate_colors = reinterpret_cast<CMTranslateColorsFn>(require_proc(api.module, "CMTranslateColors"));
    api.delete_transform = reinterpret_cast<CMDeleteTransformFn>(require_proc(api.module, "CMDeleteTransform"));

    return api.get_info &&
           api.is_profile_valid &&
           api.create_multi_profile_transform &&
           api.translate_colors &&
           api.delete_transform;
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

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::printf("usage: IccDEVCmmSmoke.exe <IccDEVCmm.dll> <rgb-profile.icc>\n");
        return 2;
    }

    const std::wstring dll_path = widen_ascii_path(argv[1]);
    const std::wstring profile_path = widen_ascii_path(argv[2]);
    CmmApi api = {};

    if (!load_cmm(dll_path.c_str(), api))
        return 1;

    if (!check_cmm_info(api))
        return 1;

    HPROFILE profile = open_profile(profile_path.c_str());
    if (!profile)
        return 1;

    bool ok = check_profile_valid(api, profile) &&
              check_rgb_roundtrip_transform(api, profile);

    CloseColorProfile(profile);

    if (!ok)
        return 1;

    std::printf("[OK] IccDEVCmm smoke test passed\n");
    return 0;
}
