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
        ATLTRACE("Error: Could not load plugin %s\n", pluginPath);
        return false;
    }

    // Get pointer to the factory function
    CreatePlugin createFunc = (CreatePlugin)GET_FUNCTION_POINTER(libHandle, "createInstance");
    if (!createFunc) {
        ATLTRACE("Error: Could not find 'createPluginInstance' in %s\n", pluginPath);
        FREE_LIBRARY(libHandle);
        return false;
    }

    // Get pointer to the destroyer function
    DestroyPlugin destroyFunc = (DestroyPlugin)GET_FUNCTION_POINTER(libHandle, "destroyInstance");
    if (!destroyFunc) {
        ATLTRACE("Warning: Could not find 'destroyPluginInstance' in '%s. Memory leaks might occur if plugin is not properly managed\n", pluginPath);
    }

    // Call the factory function to create a plugin instance
    IPotatoPlugin* pluginRawPtr = createFunc();
    if (!pluginRawPtr) {
        ATLTRACE("Error: Factory function returned null plugin instance from %s\n", pluginPath);
        FREE_LIBRARY(libHandle);
        return false;
    }

    ATLTRACE("Successfully loaded plugin %s from %s\n", pluginRawPtr->getName(), pluginPath);

    loadedPlugins.emplace_back(std::unique_ptr<IPotatoPlugin>(pluginRawPtr), libHandle, destroyFunc);
    return true;
}

void PotatoPluginManager::unloadAllPlugins() {
    for (auto& info : loadedPlugins) {
        if (info.pluginInstance) {
            ATLTRACE("Unloading plugin instance: %s\n", info.pluginInstance->getName());
            if (info.destroyerFunc) {
                info.destroyerFunc(info.pluginInstance.release());
            }
            // If destroyerFunc is null, unique_ptr's destructor will call delete, which is usually fine
            // but explicitly using the destroyer is safer for cross-DLL allocations.
        }
        if (info.libraryHandle) {
            FREE_LIBRARY(info.libraryHandle);
            ATLTRACE("Freed library handle\n");
        }
    }
    loadedPlugins.clear();
    std::cout << "All plugins unloaded." << std::endl;
}

std::vector<IPotatoPlugin*> PotatoPluginManager::getAllPlugins() const {
    std::vector<IPotatoPlugin*> plugins;
    for (const auto& info : loadedPlugins) {
        if (info.pluginInstance) {
            plugins.push_back(info.pluginInstance.get());
        }
    }
    return plugins;
}