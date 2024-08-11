// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include <DbgHelp.h>
#include <Psapi.h>
#include <tchar.h>
#include <winsock.h>
#include "../ExplorerPatcher/def.h"
#include <ShlObj_core.h>
#include <valinet/pdb/pdb.h>
#include <detours.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "detours.lib")

HMODULE hModule = NULL;

/*
BOOL PsymEnumeratesymbolsCallback_u(
           PSYMBOL_INFO pSymInfo,
           ULONG SymbolSize,
           PVOID UserContext
)
{
    printf("[InputSwitchDisabler] Symbol enumeration: %s\n", pSymInfo->Name);
    return TRUE;
}*/

__int64 dummyOnHotkey()
{
    printf("[InputSwitchDisabler] Overriding hotkey detection!\n");
    return 0;
}

extern "C" __declspec(dllexport) void clean() {

}

extern "C" __declspec(dllexport) int setup() {
    HANDLE hProcess = GetCurrentProcess();
    HMODULE hMods[1024];
    DWORD cbNeeded;
    unsigned int i;

    // Get a list of all the modules in this process.

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        printf("[InputSwitchDisabler] Starting module enumeration\n");
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
        {
            TCHAR szModName[MAX_PATH + 1];

            // Get the full path to the module's file.
            if (GetModuleFileNameEx(hProcess, hMods[i], szModName,
                sizeof(szModName) / sizeof(TCHAR)))
            {
                printf("[InputSwitchDisabler] Module %s\n", szModName);
                TCHAR inputSwitchName[] = _T("inputswitch.dll");
                size_t isLen = _tcslen(inputSwitchName);
                size_t modLen = _tcslen(szModName);
                if (modLen >= isLen)
                {
                    TCHAR* modNameLower = _tcslwr(&szModName[modLen - isLen]);
                    if (_tcscmp(modNameLower, inputSwitchName) == 0)
                    {
                        printf("[InputSwitchDisabler] InputSwitch Module found!\n");

                        char szSettingsPath[MAX_PATH];
                        ZeroMemory(
                            szSettingsPath,
                            MAX_PATH * sizeof(char)
                        );
                        SHGetFolderPathA(
                            NULL,
                            SPECIAL_FOLDER_LEGACY,
                            NULL,
                            SHGFP_TYPE_CURRENT,
                            szSettingsPath
                        );
                        strcat_s(
                            szSettingsPath,
                            MAX_PATH,
                            APP_RELATIVE_PATH
                        );
                        strcat_s(
                            szSettingsPath,
                            MAX_PATH,
                            "\\"
                        );

                        MODULEINFO modInfo;

                        if (!GetModuleInformation(hProcess, hMods[i], &modInfo, sizeof(modInfo)))
                        {
                            printf("[InputSwitchDisabler] GetModuleInformation Failure!\n");
                            return 1;
                        }

                        SymSetOptions(SYMOPT_CASE_INSENSITIVE | SYMOPT_EXACT_SYMBOLS | SYMOPT_UNDNAME);
                        if (!SymInitialize(hProcess, szSettingsPath, false))
                        {
                            printf("[InputSwitchDisabler] SymInitialize Failure!\n");
                            return 1;
                        }

                        int res = VnDownloadSymbols(NULL,
                            szModName,
                            szSettingsPath,
                            MAX_PATH
                        );

                        //VOID* ptr = DetourFindFunction(szModName, "CTsfHandler::_OnHotkey");

                        //MessageBoxA(NULL, "Test", "Test", MB_OK);
                        if (!SymLoadModuleEx(hProcess, NULL, szSettingsPath, inputSwitchName, (DWORD64)modInfo.lpBaseOfDll, modInfo.SizeOfImage, NULL, 0))
                        {
                            printf("[InputSwitchDisabler] SymLoadModuleEx Failure = %i!\n", GetLastError());
                            return 1;
                        }

                        SYMBOL_INFO symInfo = { 0 };
                        symInfo.SizeOfStruct = sizeof(SYMBOL_INFO);
                        symInfo.MaxNameLen = 0; // We don't need the name
                        if (!SymFromName(hProcess, _T("CTsfHandler::_HandleLegacyShortcutPressed"), &symInfo))
                        {
                            printf("[InputSwitchDisabler] SymFromName Failure = %i!\n", GetLastError());
                            return 1;
                        }

                        if (NO_ERROR != DetourTransactionBegin())
                        {
                            printf("[InputSwitchDisabler] DetourTransactionBegin Failure!\n");
                            DetourTransactionAbort();
                            return 1;
                        }

                        if (NO_ERROR != DetourUpdateThread(GetCurrentThread()))
                        {
                            printf("[InputSwitchDisabler] DetourUpdateThread Failure!\n");
                            DetourTransactionAbort();
                            return 1;
                        }

                        if (NO_ERROR != DetourAttach((void**)&symInfo.Address, dummyOnHotkey))
                        {
                            printf("[InputSwitchDisabler] DetourAttach Failure!\n");
                            DetourTransactionAbort();
                            return 1;
                        }
                        if (NO_ERROR != DetourTransactionCommit())
                        {
                            printf("[InputSwitchDisabler] DetourTransactionCommit Failure!\n");
                            DetourTransactionAbort();
                            return 1;
                        }
                        printf("[InputSwitchDisabler] Hotkey detection hooked!\n");
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
) {
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        hModule = hinstDLL;
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

