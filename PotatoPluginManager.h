#pragma once

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "IPotatoPlugin.h"
#include <vector>
#include <string>
#include <memory> 
#include <iostream>
#include "atltrace.h" 

// Platform-specific headers for dynamic loading
#include <windows.h>
#define PLUGIN_HANDLE HMODULE
#define LOAD_LIBRARY(path) LoadLibraryA(path)
#define GET_FUNCTION_POINTER(handle, name) GetProcAddress(handle, name)
#define FREE_LIBRARY(handle) FreeLibrary(handle)

// Function pointers for our factory and destroyer functions
// These must match the extern "C" functions in the plugins
typedef IPotatoPlugin* (*CreatePlugin)();
typedef void (*DestroyPlugin)(IPotatoPlugin*);

// Structure to hold information about each loaded plugin
struct LoadedPluginInfo {
    std::unique_ptr<IPotatoPlugin> pluginInstance; // Manages the plugin object's lifetime
    PLUGIN_HANDLE libraryHandle; // Handle to the loaded DLL
    DestroyPlugin destroyerFunc; // Pointer to the plugin's specific destroyer function

    LoadedPluginInfo(std::unique_ptr<IPotatoPlugin> instance, PLUGIN_HANDLE handle, DestroyPlugin d_func)
        : pluginInstance(std::move(instance)), libraryHandle(handle), destroyerFunc(d_func) {
    }
};

class PotatoPluginManager {
public:
    PotatoPluginManager() = default;
    // Destructor to ensure all plugins are unloaded when PluginManager goes out of scope
    ~PotatoPluginManager();

    // Dynamically loads a plugin from the specified path
    bool loadPlugin(const std::string& pluginPath);

    // Unloads all loaded plugins and frees their library handles
    void unloadAllPlugins();

    // Returns a vector of raw pointers to the loaded plugin instances
    // These pointers are owned by the PluginManager.
    std::vector<IPotatoPlugin*> getAllPlugins() const;

private:
    std::vector<LoadedPluginInfo> loadedPlugins;
};

#endif // PLUGIN_MANAGER_H