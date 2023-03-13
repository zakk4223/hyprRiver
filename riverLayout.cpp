#include "riverLayout.hpp"
#include <src/Compositor.hpp>




CRiverLayout::CRiverLayout(const char *r_namespace) {

	m_sRiverNamespace = r_namespace;
}
SRiverNodeData* CRiverLayout::getNodeFromWindow(CWindow* pWindow) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd.pWindow == pWindow)
            return &nd;
    }

    return nullptr;
}

int CRiverLayout::getNodesOnWorkspace(const int& ws) {
    int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n.workspaceID == ws)
            no++;
    }

    return no;
}



bool CRiverLayout::addRiverLayoutResource(wl_resource *resource, uint64_t monitorID) {
  for (auto &resm : m_lRiverLayoutResources) {
    if (monitorID == resm.monitorID) {
      return false;
    }
  }

  const auto LRES = &m_lRiverLayoutResources.emplace_back();
  LRES->monitorID = monitorID;
  LRES->resource = resource;
  return true;
}

int CRiverLayout::removeRiverLayoutResource(wl_resource *resource) {
  
  std::erase_if(m_lRiverLayoutResources, [&](const auto &rr) { return rr.resource == resource;});

  return m_lRiverLayoutResources.size();
}


int CRiverLayout::getRiverLayoutResourceCount() {
  return m_lRiverLayoutResources.size();
}

SRiverWorkspaceData* CRiverLayout::getMasterWorkspaceData(const int& ws) {
    for (auto& n : m_lMasterWorkspacesData) {
        if (n.workspaceID == ws)
            return &n;
    }

    //create on the fly if it doesn't exist yet
    const auto PWORKSPACEDATA   = &m_lMasterWorkspacesData.emplace_back();
    PWORKSPACEDATA->workspaceID = ws;
    return PWORKSPACEDATA;
}

std::string CRiverLayout::getLayoutName() {
    return m_sRiverNamespace; 
}

void CRiverLayout::onWindowCreatedTiling(CWindow* pWindow) {
    m_vRemovedWindowVector = Vector2D(0.f, 0.f);
    if (pWindow->m_bIsFloating)
        return;

    //static auto* const PNEWTOP = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:new_on_top")->intValue;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    //const auto         PNODE = *PNEWTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();
    const auto PNODE = &m_lMasterNodesData.emplace_front();

    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow     = pWindow;

    //static auto* const PNEWISMASTER = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:new_is_master")->intValue;

    const auto         WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);

    auto               OPENINGON = isWindowTiled(g_pCompositor->m_pLastWindow) && g_pCompositor->m_pLastWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID ?
                      getNodeFromWindow(g_pCompositor->m_pLastWindow) :
                      getNodeFromWindow(g_pCompositor->m_pLastWindow); 

    if (OPENINGON && OPENINGON->pWindow->m_sGroupData.pNextWindow && OPENINGON != PNODE && !g_pKeybindManager->m_bGroupsLocked) {
        m_lMasterNodesData.remove(*PNODE);

        OPENINGON->pWindow->insertWindowToGroup(pWindow);

        pWindow->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(pWindow));

        return;
    }

    if (WINDOWSONWORKSPACE == 1) {

        // first, check if it isn't too big.
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow); MAXSIZE.x < PMONITOR->vecSize.x || MAXSIZE.y < PMONITOR->vecSize.y) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        // first, check if it isn't too big.
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow);
            MAXSIZE.x < PMONITOR->vecSize.x || MAXSIZE.y < PMONITOR->vecSize.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
        g_pCompositor->setWindowFullscreen(PFULLWINDOW, false, FULLSCREEN_FULL);
    }

    // recalc
    recalculateMonitor(pWindow->m_iMonitorID);
}

void CRiverLayout::onWindowRemovedTiling(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    m_vRemovedWindowVector = Vector2D(0.f, 0.f);
    pWindow->m_sSpecialRenderData.rounding = true;
    pWindow->m_sSpecialRenderData.border   = true;
    pWindow->m_sSpecialRenderData.decorate = true;

    if (pWindow->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

    m_lMasterNodesData.remove(*PNODE);

    m_vRemovedWindowVector = pWindow->m_vRealPosition.goalv() + pWindow->m_vRealSize.goalv() / 2.f;


    recalculateMonitor(pWindow->m_iMonitorID);
}

void CRiverLayout::recalculateMonitor(const int& monid) {
    const auto PMONITOR   = g_pCompositor->getMonitorFromID(monid);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->specialWorkspaceID) {
        calculateWorkspace(PMONITOR->specialWorkspaceID);
    }

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL)
            return;

        // massive hack from the fullscreen func
        const auto      PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        SRiverNodeData fakeNode;
        fakeNode.pWindow         = PFULLWINDOW;
        fakeNode.position        = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        fakeNode.size            = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        fakeNode.workspaceID     = PWORKSPACE->m_iID;
        PFULLWINDOW->m_vPosition = fakeNode.position;
        PFULLWINDOW->m_vSize     = fakeNode.size;

        applyNodeDataToWindow(&fakeNode);

        return;
    }

    // calc the WS
    calculateWorkspace(PWORKSPACE->m_iID);
}

void CRiverLayout::calculateWorkspace(const int& ws) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(ws);
    static uint32_t use_serial = 1;
    uint32_t new_serial = use_serial++;

    if (!PWORKSPACE)
        return;

    const auto         PWORKSPACEDATA = getMasterWorkspaceData(ws);
    const auto         PMONITOR = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);
    const uint32_t num_nodes = getNodesOnWorkspace(PWORKSPACE->m_iID);
    //record serial so we know if we're done
    for (auto& nd : m_lMasterNodesData) {
	    if (nd.workspaceID != PWORKSPACE->m_iID)
		    continue;
	    nd.riverSerial = new_serial;
	    nd.riverDone = false;
    }



  const auto &RESOURCE = std::find_if(m_lRiverLayoutResources.begin(), m_lRiverLayoutResources.end(), [&](const auto &rres) { return rres.monitorID == PMONITOR->ID;});
  if (RESOURCE != m_lRiverLayoutResources.end()) {
     g_pRiverLayoutProtocolManager->sendLayoutDemand(RESOURCE->resource, num_nodes, PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y, 1, new_serial);
  }
}

void CRiverLayout::riverViewDimensions(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t serial)
{
	//These come in order, so use the first one with proper serial AND not flagged as done
	for (auto &nd : m_lMasterNodesData) {
		if (nd.riverSerial != serial)
			continue;
		if (nd.riverDone)
			continue;

		const auto PMONITOR = g_pCompositor->getMonitorFromID(nd.pWindow->m_iMonitorID);
		nd.position = PMONITOR->vecPosition  + PMONITOR->vecReservedTopLeft + Vector2D(x+0.0f, y+0.0f);
		nd.size = Vector2D(width+0.0f, height+0.0f);
		nd.riverDone = true;
		break;
	}
}

void CRiverLayout::riverCommit(const char *layout_name, uint32_t serial) {

	CMonitor *PMONITOR = nullptr;
	for (auto &nd : m_lMasterNodesData) {
		if (nd.riverSerial != serial)
			continue;
		if (!nd.riverDone)
			continue;

		nd.riverDone = false;
		nd.riverSerial = 0;
		applyNodeDataToWindow(&nd);
	}
        
	if (m_vRemovedWindowVector != Vector2D(0.f, 0.f)) {
		const auto FOCUSCANDIDATE = g_pCompositor->vectorToWindowIdeal(m_vRemovedWindowVector);
		if (FOCUSCANDIDATE) {
			g_pCompositor->focusWindow(FOCUSCANDIDATE);
		}

		m_vRemovedWindowVector = Vector2D(0.f, 0.f);
	}
}


void CRiverLayout::applyNodeDataToWindow(SRiverNodeData* pNode) {
    CMonitor* PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->specialWorkspaceID == pNode->workspaceID) {
                PMONITOR = m.get();
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_iMonitorID);
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node %x (workspace ID: %i)!!", pNode, pNode->workspaceID);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;
    const auto PGAPSIN     = &g_pConfigManager->getConfigValuePtr("general:gaps_in")->intValue;
    const auto PGAPSOUT    = &g_pConfigManager->getConfigValuePtr("general:gaps_out")->intValue;

    const auto PWINDOW = pNode->pWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW)) {
        Debug::log(ERR, "Node %x holding invalid window %x!!", pNode, PWINDOW);
        return;
    }

    PWINDOW->m_vSize     = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    auto calcPos  = PWINDOW->m_vPosition + Vector2D(*PBORDERSIZE, *PBORDERSIZE);
    auto calcSize = PWINDOW->m_vSize - Vector2D(2 * *PBORDERSIZE, 2 * *PBORDERSIZE);

    if (!g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID) &&
        (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) == 1 ||
         (PWINDOW->m_bIsFullscreen && g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_efFullscreenMode == FULLSCREEN_MAXIMIZED))) {
        PWINDOW->m_vRealPosition = calcPos - Vector2D(*PBORDERSIZE, *PBORDERSIZE);
        PWINDOW->m_vRealSize     = calcSize + Vector2D(2 * *PBORDERSIZE, 2 * *PBORDERSIZE);

        PWINDOW->updateWindowDecos();

        PWINDOW->m_sSpecialRenderData.rounding = false;
        PWINDOW->m_sSpecialRenderData.border   = false;
        PWINDOW->m_sSpecialRenderData.decorate = false;

        return;
    }

    PWINDOW->m_sSpecialRenderData.rounding = true;
    PWINDOW->m_sSpecialRenderData.border   = true;
    PWINDOW->m_sSpecialRenderData.decorate = true;

    /*
    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? *PGAPSOUT : *PGAPSIN, DISPLAYTOP ? *PGAPSOUT : *PGAPSIN);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? *PGAPSOUT : *PGAPSIN, DISPLAYBOTTOM ? *PGAPSOUT : *PGAPSIN);

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;
    */

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos = calcPos + RESERVED.topLeft;
    calcSize = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID)) {
        static auto const PSCALEFACTOR = 0.8f; 

        PWINDOW->m_vRealPosition = calcPos + (calcSize - calcSize * PSCALEFACTOR) / 2.f;
        PWINDOW->m_vRealSize     = calcSize * PSCALEFACTOR;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize * PSCALEFACTOR);
    } else {
        PWINDOW->m_vRealSize     = calcSize;
        PWINDOW->m_vRealPosition = calcPos;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize);
    }

    if (m_bForceWarps) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CRiverLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CRiverLayout::resizeActiveWindow(const Vector2D& pixResize, CWindow* pWindow) {

  //River's tiling paradigm has no concept of being able to manually resize windows in a stack/area etc. 
  //If you try to resize a window it just forces it to float. Do the same thing here
  const auto PNODE = getNodeFromWindow(pWindow);
  pWindow->m_bIsFloating = true;
  m_lMasterNodesData.remove(*PNODE);
  g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
  recalculateMonitor(pWindow->m_iMonitorID);

}

void CRiverLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen || g_pCompositor->isWorkspaceSpecial(pWindow->m_iWorkspaceID))
        return; // ignore

    const auto PMONITOR   = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen           = on;
    PWORKSPACE->m_bHasFullscreenWindow = !PWORKSPACE->m_bHasFullscreenWindow;

    g_pEventManager->postEvent(SHyprIPCEvent{"fullscreen", std::to_string((int)on)});
    EMIT_HOOK_EVENT("fullscreen", pWindow);

    if (!pWindow->m_bIsFullscreen) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            pWindow->m_vRealPosition = pWindow->m_vLastFloatingPosition;
            pWindow->m_vRealSize     = pWindow->m_vLastFloatingSize;

            pWindow->m_sSpecialRenderData.rounding = true;
            pWindow->m_sSpecialRenderData.border   = true;
            pWindow->m_sSpecialRenderData.decorate = true;
        }
    } else {
        // if it now got fullscreen, make it fullscreen

        PWORKSPACE->m_efFullscreenMode = fullscreenMode;

        // save position and size if floating
        if (pWindow->m_bIsFloating) {
            pWindow->m_vLastFloatingSize     = pWindow->m_vRealSize.goalv();
            pWindow->m_vLastFloatingPosition = pWindow->m_vRealPosition.goalv();
            pWindow->m_vPosition             = pWindow->m_vRealPosition.goalv();
            pWindow->m_vSize                 = pWindow->m_vRealSize.goalv();
        }

        // apply new pos and size being monitors' box
        if (fullscreenMode == FULLSCREEN_FULL) {
            pWindow->m_vRealPosition = PMONITOR->vecPosition;
            pWindow->m_vRealSize     = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SRiverNodeData fakeNode;
            fakeNode.pWindow     = pWindow;
            fakeNode.position    = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size        = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID = pWindow->m_iWorkspaceID;
            pWindow->m_vPosition = fakeNode.position;
            pWindow->m_vSize     = fakeNode.size;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv());

    g_pCompositor->moveWindowToTop(pWindow);

    recalculateMonitor(PMONITOR->ID);
}

void CRiverLayout::recalculateWindow(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->m_iMonitorID);
}

SWindowRenderLayoutHints CRiverLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesnt have any hints
}

void CRiverLayout::switchWindows(CWindow* pWindow, CWindow* pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    const auto inheritFullscreen = prepareLoseFocus(pWindow);

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_iMonitorID, pWindow->m_iMonitorID);
        std::swap(pWindow2->m_iWorkspaceID, pWindow->m_iWorkspaceID);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    recalculateMonitor(pWindow->m_iMonitorID);
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->m_iMonitorID);

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);

    prepareNewFocus(pWindow2, inheritFullscreen);
}

void CRiverLayout::alterSplitRatio(CWindow* pWindow, float ratio, bool exact) {
    recalculateMonitor(pWindow->m_iMonitorID);
}

CWindow* CRiverLayout::getNextWindow(CWindow* pWindow, bool next) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    const auto PNODE = getNodeFromWindow(pWindow);

    if (next) {
            for (auto n : m_lMasterNodesData) {
                if (n.pWindow != pWindow && n.workspaceID == pWindow->m_iWorkspaceID) {
                    return n.pWindow;
                }
            }
            // focus next
            bool reached = false;
            bool found   = false;
            for (auto n : m_lMasterNodesData) {
                if (n.pWindow == pWindow) {
                    reached = true;
                    continue;
                }

                if (n.workspaceID == pWindow->m_iWorkspaceID && reached) {
                    return n.pWindow;
                }
            }
    } else {
            // focus previous
            bool reached = false;
            bool found   = false;
            for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
                if (it->pWindow == pWindow) {
                    reached = true;
                    continue;
                }

                if (it->workspaceID == pWindow->m_iWorkspaceID && reached) {
                    return it->pWindow;
                }
            }
        }

    return nullptr;
}

bool CRiverLayout::prepareLoseFocus(CWindow* pWindow) {
    if (!pWindow)
        return false;

    //if the current window is fullscreen, make it normal again if we are about to lose focus
    if (pWindow->m_bIsFullscreen) {
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);
        //static auto* const INHERIT = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:inherit_fullscreen")->intValue;
        //return *INHERIT == 1;
    }

    return false;
}

void CRiverLayout::prepareNewFocus(CWindow* pWindow, bool inheritFullscreen) {
    if (!pWindow)
        return;

    if (inheritFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, true, g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID)->m_efFullscreenMode);
}

std::any CRiverLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    if (!header.pWindow)
	    return 0;
    CVarList vars(message, 0, ' ');

    if (vars.size() < 1 || vars[0].empty()) {
        Debug::log(ERR, "layoutmsg called without params");
        return 0;
    }

  const auto RESOURCE = std::find_if(m_lRiverLayoutResources.begin(), m_lRiverLayoutResources.end(), [&](const auto &rres) { return rres.monitorID == header.pWindow->m_iMonitorID;});
  if (RESOURCE != m_lRiverLayoutResources.end()) {
	if (m_sRiverNamespace != "kile") {
      		g_pRiverLayoutProtocolManager->sendUserCommandTags(RESOURCE->resource, 1);
	}
      g_pRiverLayoutProtocolManager->sendUserCommand(RESOURCE->resource, message.c_str());
      recalculateMonitor(header.pWindow->m_iMonitorID);
  }
    return 0;
}

void CRiverLayout::replaceWindowDataWith(CWindow* from, CWindow* to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE);
}

void CRiverLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bMappedX11 || !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CRiverLayout::onDisable() {
    m_lMasterNodesData.clear();
}
