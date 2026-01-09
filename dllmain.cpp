#include "framework.h"
#include "hooks.h"
#include <stdint.h>
#include <detours/detours.h>

#pragma comment(lib, "Ws2_32.lib")

HMODULE OriginalDLL;

extern "C" {
	uintptr_t OrignalDWriteCreateFactory{ 0 };
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

    char path[MAX_PATH]{};

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        memcpy(path + GetSystemDirectoryA(path, MAX_PATH - 12), "\\DWrite.dll", 12);
        OriginalDLL = LoadLibraryA(path);

        if (OriginalDLL == NULL)
        {
            MessageBoxA(0, "Cannot load original dll", "HIJACK", MB_ICONERROR);
            return FALSE;
        }

        OrignalDWriteCreateFactory = (uintptr_t)GetProcAddress(OriginalDLL, "DWriteCreateFactory");

        DetourRestoreAfterWith();
        HooksInit();
        break;
    case DLL_PROCESS_DETACH:
        FreeLibrary(OriginalDLL);
        HooksDetach();
        break;
    }
    return TRUE;
}

