#include <windows.h>
#include <stdio.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        printf("[ASIVersion] DLL_PROCESS_ATTACH\n");

        HMODULE hEHP = GetModuleHandleW(L"mk12tth.ehp");
        if (hEHP)
        {
            printf("[ASIVersion] mk12tth.ehp already loaded. Skipping LoadLibrary.\n");
        }
        else
        {
            hEHP = LoadLibraryW(L"mk12tth.ehp");
            if (hEHP)
                printf("[ASIVersion] mk12tth.ehp loaded successfully.\n");
            else
                printf("[ASIVersion] Failed to load mk12tth.ehp.\n");
        }
    }
    return TRUE;
}
