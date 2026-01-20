#include "framework.h"
#include "hooks.h"
#include "helpers.h"
#include <detours/detours.h>

#pragma comment(lib, "Ws2_32.lib")

HMODULE OriginalDLL;

extern "C" {
	uintptr_t OrignalDWriteCreateFactory{ 0 };
}

DwormConfig config{};

void LoadOriginalLib() {
    char path[MAX_PATH]{};
    memcpy(path + GetSystemDirectoryA(path, MAX_PATH - 12), "\\DWrite.dll", 12);
    OriginalDLL = LoadLibraryA(path);
}

BOOL APIENTRY DllMain(
	HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
	if (DetourIsHelperProcess()) {
		return TRUE;
	}

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        LoadOriginalLib();
        if (OriginalDLL == NULL)
        {
            MessageBoxA(0, "Cannot load original dll", "Dworm Proxy", MB_ICONERROR);
            return false;
        }

        OrignalDWriteCreateFactory = (uintptr_t)GetProcAddress(OriginalDLL, "DWriteCreateFactory");

        if (!ParseConf(config)) {
            MessageBoxA(0, "Error with config file dwormconf.txt", "Dworm Proxy", MB_ICONERROR);
            return false;
        }

        DetourRestoreAfterWith();
        HooksAttach();
        break;

    case DLL_PROCESS_DETACH:
        FreeLibrary(OriginalDLL);
        HooksDetach();
        break;
    }
    return TRUE;
}

