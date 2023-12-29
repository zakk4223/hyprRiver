#include "RiverLayoutProtocolManager.hpp"
#include <hyprland/src/Compositor.hpp>
#include "river-layout-v3-protocol.h"





static void handleDestroy(wl_client *client, wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void bindManagerInt(wl_client *client, void *data, uint32_t version, uint32_t id) {
	g_pRiverLayoutProtocolManager->bindManager(client, data, version, id);
}

static void handleDisplayDestroy(struct wl_listener *listener, void *data) {
	g_pRiverLayoutProtocolManager->displayDestroy();
}


void handleGetLayout(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *output, const char *r_namespace) {
	g_pRiverLayoutProtocolManager->getLayout(client, resource, id, output, r_namespace);
}

void handlePushViewDimensions(wl_client *client, wl_resource *resource, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t serial) {

	g_pRiverLayoutProtocolManager->pushViewDimensions(client, resource, x, y, width, height, serial);
}


void handleCommit(wl_client *client, wl_resource *resource, const char *layout_name, uint32_t serial) {

	g_pRiverLayoutProtocolManager->commit(client, resource, layout_name, serial);
}



static const struct river_layout_manager_v3_interface riverLayoutManagerImpl = {
	.destroy = handleDestroy,
	.get_layout = handleGetLayout,

};

static const struct river_layout_v3_interface riverLayoutImpl = {
	.destroy = handleDestroy, 
	.push_view_dimensions = handlePushViewDimensions, 
	.commit = handleCommit,

};

CRiverLayout *layoutFromResource(wl_resource *resource) {
	ASSERT(wl_resource_instance_of(resource, &river_layout_v3_interface, &riverLayoutImpl));
	return (CRiverLayout *)wl_resource_get_user_data(resource);
}

static void handleLayoutDestroy(wl_resource *resource) {
	const auto LAYOUT = layoutFromResource(resource);
	if (LAYOUT) {
		wl_resource_set_user_data(resource, nullptr);
	}
  if (LAYOUT->removeRiverLayoutResource(resource) == 0) {
  	g_pRiverLayoutProtocolManager->removeLayout(LAYOUT);
  }
}


CRiverLayoutProtocolManager::~CRiverLayoutProtocolManager() {
	wl_list_remove(&m_liDisplayDestroy.link);
}


CRiverLayoutProtocolManager::CRiverLayoutProtocolManager() {
	m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, &river_layout_manager_v3_interface, 2, this, bindManagerInt);
	if (!m_pGlobal) {
		Debug::log(LOG, "RiverLayout could not start!");
		return;
	}

	m_liDisplayDestroy.notify = handleDisplayDestroy;
	wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);
	Debug::log(LOG, "RiverLayout started successfully!");
}


void CRiverLayoutProtocolManager::pushViewDimensions(wl_client *client, wl_resource *resource, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t serial) {
	const auto LAYOUT = layoutFromResource(resource);
	if (LAYOUT) {
		LAYOUT->riverViewDimensions(x, y, width, height, serial);
	}
}


void CRiverLayoutProtocolManager::commit(wl_client *client, wl_resource *resource, const char *layout_name, uint32_t serial) {
	const auto LAYOUT = layoutFromResource(resource);
	if (LAYOUT) {
		LAYOUT->riverCommit(layout_name, serial);
	}
}

void CRiverLayoutProtocolManager::bindManager(wl_client *client, void *data, uint32_t version, uint32_t id) {
	const auto RESOURCE = wl_resource_create(client, &river_layout_manager_v3_interface, version, id);
	wl_resource_set_implementation(RESOURCE, &riverLayoutManagerImpl, this, nullptr);
	Debug::log(LOG, "RiverLayoutManager bound successfully!");
}

//River tilers create a layout resource per output, and the server is supposed to enforce a uniqueness of (namespace,output). 
//River has no real 'workspace' concept, but instead allows numeric tagging of windows and the ability to show hide tags as you wish. 
//Tilers may or may not keep state based on tags send to them and various tiler user messages. 
//Since this is just a POC use one global layout per namespace

void CRiverLayoutProtocolManager::getLayout(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *output, const char *r_namespace) {
  const auto OMONITOR = g_pCompositor->getMonitorFromOutput((wlr_output *)output->data);
  
	const auto LAYOUT = std::find_if(m_vRiverLayouts.begin(), m_vRiverLayouts.end(), [&](const auto &layout) { return layout->m_sRiverNamespace == r_namespace; });

	wl_resource *newResource = wl_resource_create(client, &river_layout_v3_interface, wl_resource_get_version(resource), id);
	if (LAYOUT == m_vRiverLayouts.end()) { //Didn't find it
		m_vRiverLayouts.emplace_back(std::make_unique<CRiverLayout>(r_namespace));
		wl_resource_set_implementation(newResource, &riverLayoutImpl, m_vRiverLayouts.back().get(), &handleLayoutDestroy);
    m_vRiverLayouts.back().get()->addRiverLayoutResource(newResource, OMONITOR->ID);
    		HyprlandAPI::addLayout(PHANDLE, m_vRiverLayouts.back()->m_sRiverNamespace, m_vRiverLayouts.back().get());
	} else {
  //TODO: namespace collision event
		wl_resource_set_implementation(newResource, &riverLayoutImpl, (*LAYOUT).get(), &handleLayoutDestroy);
    (*LAYOUT)->addRiverLayoutResource(newResource, OMONITOR->ID );
	}
}

void CRiverLayoutProtocolManager::removeLayout(CRiverLayout *rmLayout) {
	HyprlandAPI::removeLayout(PHANDLE, rmLayout);
	std::erase_if(m_vRiverLayouts, [&](const auto& l) { return l.get() == rmLayout; });
}

void CRiverLayoutProtocolManager::displayDestroy() {
	wl_global_destroy(m_pGlobal);
}


void CRiverLayoutProtocolManager::sendLayoutDemand(wl_resource *resource, uint32_t view_count, uint32_t usable_width, uint32_t usable_height, uint32_t tags, uint32_t serial) {
	//Some of the river tilers get angry if you send a zero view count request, so just ignore any that come through here...
	if (view_count > 0) {
		river_layout_v3_send_layout_demand(resource, view_count, usable_width, usable_height, tags, serial);
	}
}

void CRiverLayoutProtocolManager::sendUserCommand(wl_resource *resource, const char *command) {
	river_layout_v3_send_user_command(resource, command);
}

void CRiverLayoutProtocolManager::sendUserCommandTags(wl_resource *resource, uint32_t tags) {
	if (wl_resource_get_version(resource) >= 2 )
		river_layout_v3_send_user_command_tags(resource, tags);
}
