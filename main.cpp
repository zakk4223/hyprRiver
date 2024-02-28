#define WLR_USE_UNSTABLE

#include <hyprland/src/config/ConfigManager.hpp>
#include "globals.hpp"

#include "riverLayout.hpp"
#include "RiverLayoutProtocolManager.hpp"

#include <unistd.h>
#include <thread>

// Methods

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    g_pRiverLayoutProtocolManager = std::make_unique<CRiverLayoutProtocolManager>();
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:river:layout:special_scale_factor", Hyprlang::FLOAT{0.8f});
    HyprlandAPI::reloadConfig();

    return {"River Layout", "Use River layout protocol", "Zakk", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
		g_pRiverLayoutProtocolManager.reset();
	HyprlandAPI::invokeHyprctlCommand("seterror", "disable");



}
