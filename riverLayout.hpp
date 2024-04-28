#pragma once

#define WLR_USE_UNSTABLE

#include "globals.hpp"
#include "RiverLayoutProtocolManager.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <vector>
#include <list>
#include <deque>
#include <any>

enum eFullscreenMode : int8_t;

struct SRiverNodeData {

    uint32_t riverSerial;
    bool     riverDone;
    PHLWINDOWREF pWindow;

    Vector2D position;
    Vector2D size;

    float    percSize = 1.f; // size multiplier for resizing children

    int      workspaceID = -1;
		bool		 ignoreFullscreenChecks = false;


    bool     operator==(const SRiverNodeData& rhs) const {
        return pWindow.lock() == rhs.pWindow.lock();
    }
};

struct SRiverWorkspaceData {
    int                workspaceID = -1;
    bool               operator==(const SRiverWorkspaceData& rhs) const {
        return workspaceID == rhs.workspaceID;
    }
};


struct SRiverResource {
  wl_resource *resource;
  uint64_t monitorID;
};
class CRiverLayout : public IHyprLayout {
  public:

    CRiverLayout(const char *r_namespace);
    std::string			     m_sRiverNamespace = "";
    virtual void                     onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(PHLWINDOW);
    virtual bool                     isWindowTiled(PHLWINDOW);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(PHLWINDOW);
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner, PHLWINDOW pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(PHLWINDOW, eFullscreenMode, bool);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(PHLWINDOW);
    virtual void                     switchWindows(PHLWINDOW, PHLWINDOW);
		virtual void 										 moveWindowTo(PHLWINDOW, const std::string& dir, bool silent);
    virtual void                     alterSplitRatio(PHLWINDOW, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to);

    virtual void                     onEnable();
    virtual void                     onDisable();
    void 			     riverViewDimensions(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t serial);
    void			     riverCommit(const char *layout_name, uint32_t serial);
    bool                              addRiverLayoutResource(wl_resource *resource, uint64_t monitorID); 
    int                              removeRiverLayoutResource(wl_resource *resource);
    int                              getRiverLayoutResourceCount();
    virtual Vector2D 								 predictSizeForNewWindowTiled();

        



  private:
    std::list<SRiverNodeData>        m_lMasterNodesData;
    std::vector<SRiverWorkspaceData> m_lMasterWorkspacesData;
    std::list<SRiverResource>      m_lRiverLayoutResources;
    Vector2D 			   m_vRemovedWindowVector;
    

    bool                              m_bForceWarps = false;

    int                               getNodesOnWorkspace(const int&);
    void                              applyNodeDataToWindow(SRiverNodeData*);
    void                              resetNodeSplits(const int&);
    SRiverNodeData*                  getNodeFromWindow(PHLWINDOW);
    SRiverWorkspaceData*             getMasterWorkspaceData(const int&);
    void                              calculateWorkspace(PHLWORKSPACE);
    PHLWINDOW                          getNextWindow(PHLWINDOW, bool);
    int                               getMastersOnWorkspace(const int&);
    bool                              prepareLoseFocus(PHLWINDOW);
    void                              prepareNewFocus(PHLWINDOW, bool inherit_fullscreen);

    friend struct SRiverNodeData;
    friend struct SRiverWorkspaceData;
};

template <typename CharT>
struct std::formatter<SRiverNodeData*, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const SRiverNodeData* const& node, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!node)
            return std::format_to(out, "[Node nullptr]");
        std::format_to(out, "[Node {:x}: workspace: {}, pos: {:j2}, size: {:j2}", (uintptr_t)node, node->workspaceID, node->position, node->size);
        if (!node->pWindow.expired())
            std::format_to(out, ", window: {:x}", node->pWindow.lock());
        return std::format_to(out, "]");
    }
};
