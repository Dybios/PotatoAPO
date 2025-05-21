#pragma once

#ifndef IPROCESS_PLUGIN_H
#define IPROCESS_PLUGIN_H

#include "ProcessContext.h"

enum class PluginStatus {
    CONTINUE, // Plugin processed successfully, continue to next
    FAILURE  // Plugin encountered an error, bypass that
};

class IPotatoPlugin {
public:
    virtual ~IPotatoPlugin() = default;

    // Take reference to the context to process and return a status.
    virtual PluginStatus process(ProcessContext& context) = 0;

    // Get plugin name for easy logging
    virtual std::string getName() const = 0;
};

#endif // IPROCESS_PLUGIN_H