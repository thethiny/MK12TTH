#pragma once

// dllmain.cpp : Defines the entry point for the DLL application.
#include "../pch.h"
#include "mk12sdk/sdk.h"

#define PLUGIN_API __declspec(dllexport)

namespace MK12HookPlugin {
    // Plugin name to use when loading and printing errors to log
    extern "C" PLUGIN_API const char* GetPluginName();

    // Hook project name that this plugin is compatible with
    extern "C" PLUGIN_API const char* GetPluginProject();

    // GUI tab name that will be used in the Plugins section
    extern "C" PLUGIN_API const char* GetPluginTabName();

    // Initialization
    extern "C" PLUGIN_API void OnInitialize(HMODULE hMod);

    // Shutdown
    extern "C" PLUGIN_API void OnShutdown();

    // Called every game tick
    extern "C" PLUGIN_API void OnFrameTick();

    // Called on match/fight start
    extern "C" PLUGIN_API void OnFightStartup();

    // Tab data for menu, remove this if you don't want a plugin tab
    extern "C" PLUGIN_API void TabFunction();

    /*const char* GetPluginName()
    {
        return ::GetPluginName();
    }

    const char* GetPluginProject()
    {
        return ::GetPluginProject();
    }

    const char* GetPluginTabName()
    {
        return ::GetPluginTabName();
    }

    void OnInitialize(HMODULE hMod)
    {
        ::OnInitialize(hMod);
    }

    void OnShutdown()
    {
        ::OnShutdown();
    }

    void OnFrameTick()
    {
        ::OnFrameTick();
    }

    void OnFightStartup()
    {
        ::OnFightStartup();
    }

    void TabFunction()
    {
        ::TabFunction();
    }*/
}
