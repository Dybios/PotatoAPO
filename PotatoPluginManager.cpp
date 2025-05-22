#include "stdafx.h"

#include "PotatoPluginManager.h"
#include <vector>
#include <iostream>
#include <memory>

PotatoPluginManager::~PotatoPluginManager() {
    unloadAllPlugins();
}

bool PotatoPluginManager::loadPlugin(const std::string& pluginPath) {
    PLUGIN_HANDLE libHandle = LOAD_LIBRARY(pluginPath.c_str());
    if (!libHandle) {
        ATLTRACE("Error: Could not load plugin %s", pluginPath.c_str());
        return false;
    }

    // Get pointer to the factory function
    CreatePlugin createFunc = (CreatePlugin)GET_FUNCTION_POINTER(libHandle, "createInstance");
    if (!createFunc) {
        ATLTRACE("Error: Could not find 'createInstance' in %s", pluginPath.c_str());
        FREE_LIBRARY(libHandle);
        return false;
    }

    // Get pointer to the destroyer function
    DestroyPlugin destroyFunc = (DestroyPlugin)GET_FUNCTION_POINTER(libHandle, "destroyInstance");
    if (!destroyFunc) {
        ATLTRACE("Warning: Could not find 'destroyInstance' in '%s. Memory leaks might occur if plugin is not properly managed", pluginPath.c_str());
    }

    // Call the factory function to create a plugin instance
    IPotatoPlugin* pluginRawPtr = createFunc();
    if (!pluginRawPtr) {
        ATLTRACE("Error: Factory function returned null plugin instance from %s", pluginPath.c_str());
        FREE_LIBRARY(libHandle);
        return false;
    }

    ATLTRACE("Successfully loaded plugin %s from %s\n", pluginRawPtr->getName(), pluginPath.c_str());
    loadedPlugins.emplace_back(pluginRawPtr, libHandle, destroyFunc);
    ATLTRACE("Successfully emplaced onto loaded plugins\n");

    return true;
}

void PotatoPluginManager::unloadAllPlugins() {
    for (auto& info : loadedPlugins) {
        if (info.pluginInstance) {
            ATLTRACE("Unloading plugin instance: %s\n", info.pluginInstance->getName());
            if (info.destroyerFunc) {
                info.destroyerFunc(info.pluginInstance);
            }
            else {
                // If destroyerFunc is null, call delete to release the raw pointer.
                // Explicit call to destroyer is safer for cross-DLL allocations. But,
                // this will have to do if NULL since we are using new in our plugins anyway.
                delete info.pluginInstance;
            }
            info.pluginInstance = nullptr;
        }
        if (info.libraryHandle) {
            FREE_LIBRARY(info.libraryHandle);
            ATLTRACE("Freed library handle\n");
        }
    }
    loadedPlugins.clear();
    ATLTRACE("All plugins unloaded.");
}

std::vector<IPotatoPlugin*> PotatoPluginManager::getAllPlugins() const {
    std::vector<IPotatoPlugin*> plugins;
    for (const auto& info : loadedPlugins) {
        if (info.pluginInstance) {
            plugins.push_back(info.pluginInstance);
        }
    }
    return plugins;
}