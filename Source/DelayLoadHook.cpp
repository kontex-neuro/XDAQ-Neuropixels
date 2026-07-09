// Resolves the DELAY_LOAD_DLL_NAME dependency relative to this module's own
// directory instead of relying on the host application's DLL search path
// (see /DELAYLOAD linker flag). If the DLL is already loaded elsewhere in the
// process, that instance is reused instead of loading a second copy, so all
// consumers in the same process share one instance.
#ifdef _WIN32

#include <windows.h>

#include <delayimp.h>

#include <cstring>
#include <string>

// Directory containing the dependency, relative to this module's own
// directory (e.g. L"../shared/XDAQ-Neuropixels"). Defaults to the same directory.
#ifndef DELAY_LOAD_RELATIVE_DIR
#define DELAY_LOAD_RELATIVE_DIR L"."
#endif

namespace
{

std::wstring moduleDirectory()
{
    HMODULE module = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory),
        &module
    );
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring result(path, len);
    auto pos = result.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : result.substr(0, pos);
}

FARPROC WINAPI delayLoadHook(unsigned reason, PDelayLoadInfo info)
{
    if (reason != dliNotePreLoadLibrary || _stricmp(info->szDll, DELAY_LOAD_DLL_NAME) != 0)
        return nullptr;

    // Reuse an already-loaded copy (e.g. loaded by another XDAQ plugin in the
    // same process) so both share a single instance / device lock state.
    if (HMODULE existing = GetModuleHandleA(info->szDll))
        return reinterpret_cast<FARPROC>(existing);

    std::wstring dllName(info->szDll, info->szDll + std::strlen(info->szDll));
    std::wstring fullPath = moduleDirectory() + L"/" + DELAY_LOAD_RELATIVE_DIR + L"/" + dllName;
    return reinterpret_cast<FARPROC>(
        LoadLibraryExW(fullPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR)
    );
}

} // namespace

extern "C" PfnDliHook __pfnDliNotifyHook2 = delayLoadHook;

#endif // _WIN32
