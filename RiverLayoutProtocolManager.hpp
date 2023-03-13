#pragma once
#define WLR_USE_UNSTABLE
#include "globals.hpp"
#include <src/defines.hpp>
#include "riverLayout.hpp"



class CRiverLayout;

class CRiverLayoutProtocolManager {
	public:
		CRiverLayoutProtocolManager();
		void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id);
		void displayDestroy();
		void getLayout(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *output, const char *r_namespace);
		void pushViewDimensions(wl_client *client, wl_resource *resource, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t serial);
		void commit(wl_client *client, wl_resource *resource, const char *layout_name, uint32_t serial);
		void sendLayoutDemand(wl_resource *resource, uint32_t view_count, uint32_t usable_width, uint32_t usable_height, uint32_t tags, uint32_t serial);
		void sendUserCommand(wl_resource *resource, const char *command);
		void sendUserCommandTags(wl_resource *resource, uint32_t tags);
		void removeLayout(CRiverLayout *rmLayout); 


	private:
		wl_global *m_pGlobal = nullptr;
		wl_listener m_liDisplayDestroy;
		wl_resource *m_pClientResource;
		std::vector<std::unique_ptr<CRiverLayout>> m_vRiverLayouts;

};

    inline std::unique_ptr<CRiverLayoutProtocolManager> g_pRiverLayoutProtocolManager;


