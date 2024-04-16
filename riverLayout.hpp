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
    CWindow* pWindow = nullptr;

    Vector2D position;
    Vector2D size;

    float    percSize = 1.f; // size multiplier for resizing children

    int      workspaceID = -1;
		bool		 ignoreFullscreenChecks = false;


    bool     operator==(const SRiverNodeData& rhs) const {
        return pWindow == rhs.pWindow;
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
    virtual void                     onWindowCreatedTiling(CWindow*, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(CWindow*);
    virtual bool                     isWindowTiled(CWindow*);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(CWindow*);
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner, CWindow* pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(CWindow*, eFullscreenMode, bool);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*);
    virtual void                     switchWindows(CWindow*, CWindow*);
		virtual void 										 moveWindowTo(CWindow *, const std::string& dir);
    virtual void                     alterSplitRatio(CWindow*, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(CWindow* from, CWindow* to);

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
    SRiverNodeData*                  getNodeFromWindow(CWindow*);
    SRiverWorkspaceData*             getMasterWorkspaceData(const int&);
    void                              calculateWorkspace(PHLWORKSPACE);
    CWindow*                          getNextWindow(CWindow*, bool);
    int                               getMastersOnWorkspace(const int&);
    bool                              prepareLoseFocus(CWindow*);
    void                              prepareNewFocus(CWindow*, bool inherit_fullscreen);

    friend struct SRiverNodeData;
    friend struct SRiverWorkspaceData;
};

