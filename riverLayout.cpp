#include "riverLayout.hpp"
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/decorations/DecorationPositioner.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/render/Renderer.hpp>




CRiverLayout::CRiverLayout(const char *r_namespace) {

	m_sRiverNamespace = r_namespace;
}
SRiverNodeData* CRiverLayout::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd.pWindow.lock() == pWindow)
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



bool CRiverLayout::addRiverLayoutResource(wl_resource *resource, MONITORID monitorID) {
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

void CRiverLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
		if (!pWindow) return;
    if (!isDirection(dir))
        return;

    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(pWindow, dir[0]);
		if (pWindow->m_workspace != PWINDOW2->m_workspace) {
 			// if different monitors, send to monitor
			onWindowRemovedTiling(pWindow);
			pWindow->moveToWorkspace(PWINDOW2->m_workspace);
			pWindow->m_monitor = PWINDOW2->m_monitor;
			if (!silent) {
				const auto pMonitor = g_pCompositor->getMonitorFromID(pWindow->monitorID());
				g_pCompositor->setActiveMonitor(pMonitor);
			}
			onWindowCreatedTiling(pWindow);
    } else {
        // if same monitor, switch windows
        switchWindows(pWindow, PWINDOW2);
				if (silent)
					g_pCompositor->focusWindow(PWINDOW2);
		}
}


void CRiverLayout::onWindowCreatedTiling(PHLWINDOW pWindow, eDirection direction) {
    m_vRemovedWindowVector = Vector2D(0.f, 0.f);
    if (pWindow->m_isFloating)
        return;

    //static auto* const PNEWTOP = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:new_on_top")->intValue;

    const auto         PMONITOR = pWindow->m_monitor.lock();

    //const auto         PNODE = *PNEWTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();
    const auto PNODE = &m_lMasterNodesData.emplace_front();

    PNODE->workspaceID = pWindow->workspaceID();
    PNODE->pWindow     = pWindow;

    //static auto* const PNEWISMASTER = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:new_is_master")->intValue;

    const auto         WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);

    if (WINDOWSONWORKSPACE == 1) {

        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize(); MAXSIZE.x < PMONITOR->m_size.x || MAXSIZE.y < PMONITOR->m_size.y) {
            // we can't continue. make it floating.
            pWindow->m_isFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize();
            MAXSIZE.x < PMONITOR->m_size.x || MAXSIZE.y < PMONITOR->m_size.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_isFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    // recalc
    recalculateMonitor(pWindow->monitorID());
}

void CRiverLayout::onWindowRemovedTiling(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    m_vRemovedWindowVector = Vector2D(0.f, 0.f);
    pWindow->unsetWindowData(PRIORITY_LAYOUT);
    pWindow->updateWindowData();

    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    m_lMasterNodesData.remove(*PNODE);

    m_vRemovedWindowVector = pWindow->m_realPosition->goal() + pWindow->m_realSize->goal() / 2.f;


    recalculateMonitor(pWindow->monitorID());
}

void CRiverLayout::recalculateMonitor(const MONITORID& monid) {
    const auto PMONITOR   = g_pCompositor->getMonitorFromID(monid);
    if (!PMONITOR || !PMONITOR->m_activeWorkspace)
      return;

    const auto PWORKSPACE = PMONITOR->m_activeWorkspace;

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->m_activeSpecialWorkspace) {
        calculateWorkspace(PMONITOR->m_activeSpecialWorkspace);
    }

    if (PWORKSPACE->m_hasFullscreenWindow) {
        if (PWORKSPACE->m_fullscreenMode == FSMODE_FULLSCREEN)
            return;

        // massive hack from the fullscreen func
        const auto      PFULLWINDOW = PWORKSPACE->getFullscreenWindow();

        SRiverNodeData fakeNode;
        fakeNode.pWindow         = PFULLWINDOW;
        fakeNode.position        = PMONITOR->m_position + PMONITOR->m_reservedTopLeft;
        fakeNode.size            = PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight;
        fakeNode.workspaceID     = PWORKSPACE->m_id;
        PFULLWINDOW->m_position = fakeNode.position;
        PFULLWINDOW->m_size     = fakeNode.size;

        applyNodeDataToWindow(&fakeNode);

        return;
    }

    // calc the WS
    calculateWorkspace(PWORKSPACE);
}

void CRiverLayout::calculateWorkspace(PHLWORKSPACE pWorkspace) {
    static uint32_t use_serial = 1;
    uint32_t new_serial = use_serial++;

    if (!pWorkspace)
        return;

    const auto         PMONITOR = pWorkspace->m_monitor.lock();
    const uint32_t num_nodes = getNodesOnWorkspace(pWorkspace->m_id);
    //record serial so we know if we're done
    for (auto& nd : m_lMasterNodesData) {
	    if (nd.workspaceID != pWorkspace->m_id)
		    continue;
	    nd.riverSerial = new_serial;
	    nd.riverDone = false;
    }



  const auto &RESOURCE = std::find_if(m_lRiverLayoutResources.begin(), m_lRiverLayoutResources.end(), [&](const auto &rres) { return rres.monitorID == PMONITOR->m_id;});
  if (RESOURCE != m_lRiverLayoutResources.end()) {
     g_pRiverLayoutProtocolManager->sendLayoutDemand(RESOURCE->resource, num_nodes, PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x - PMONITOR->m_reservedTopLeft.x, PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y - PMONITOR->m_reservedTopLeft.y, 1, new_serial);
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

		const auto PMONITOR = nd.pWindow.lock()->m_monitor.lock();
		nd.position = PMONITOR->m_position  + PMONITOR->m_reservedTopLeft + Vector2D(x+0.0f, y+0.0f);
		nd.size = Vector2D(width+0.0f, height+0.0f);
		nd.riverDone = true;
		break;
	}
}

void CRiverLayout::riverCommit(const char *layout_name, uint32_t serial) {

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
		const auto FOCUSCANDIDATE = g_pCompositor->vectorToWindowUnified(m_vRemovedWindowVector, FULL_EXTENTS | ALLOW_FLOATING);
		if (FOCUSCANDIDATE) {
			g_pCompositor->focusWindow(FOCUSCANDIDATE);
		}

		m_vRemovedWindowVector = Vector2D(0.f, 0.f);
	}
}


void CRiverLayout::applyNodeDataToWindow(SRiverNodeData* pNode) {

    PHLMONITOR PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto& m : g_pCompositor->m_monitors) {
            if (m->activeSpecialWorkspaceID() == pNode->workspaceID) {
                PMONITOR = m;
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_monitor.lock();
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node {} (workspace ID: {})!!", static_cast<void *>(pNode), pNode->workspaceID);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);

    const auto PWINDOW = pNode->pWindow.lock();
		const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(PWINDOW->workspaceID()));

		if (PWINDOW->isFullscreen() && !pNode->ignoreFullscreenChecks)
			return;

    PWINDOW->unsetWindowData(PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();
		
    static auto* const PGAPSINDATA     = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_in");
    static auto* const PGAPSOUTDATA    = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_out");
    auto* const        PGAPSIN         = (CCssGapData*)(*PGAPSINDATA)->getData();
    auto* const        PGAPSOUT        = (CCssGapData*)(*PGAPSOUTDATA)->getData();
		static auto* const PANIMATE = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes");

		auto gapsIn = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
		auto gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);



    if (!validMapped(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid window {}!!", pNode, PWINDOW);
        return;
    }


    PWINDOW->m_size     = pNode->size;
    PWINDOW->m_position = pNode->position;

    auto       calcPos  = PWINDOW->m_position;
    auto       calcSize = PWINDOW->m_size;

    const auto OFFSETTOPLEFT = Vector2D((double)(DISPLAYLEFT ? gapsOut.m_left : gapsIn.m_left), (double)(DISPLAYTOP ? gapsOut.m_top : gapsIn.m_top));

    const auto OFFSETBOTTOMRIGHT = Vector2D((double)(DISPLAYRIGHT ? gapsOut.m_right : gapsIn.m_right), (DISPLAYBOTTOM ? gapsOut.m_bottom : gapsIn.m_bottom));

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->workspaceID())) {
        static auto* const PSCALEFACTOR = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:special_scale_factor")->getDataStaticPtr();

        CBox               wb = {calcPos + (calcSize - calcSize * **PSCALEFACTOR) / 2.f, calcSize * **PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realPosition = wb.pos();
        *PWINDOW->m_realSize     = wb.size();
    } else {
				CBox wb = {calcPos, calcSize};
				wb.round();
        *PWINDOW->m_realSize     = wb.size(); 
        *PWINDOW->m_realPosition = wb.pos(); 
    }

    if (m_bForceWarps && !**PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_realPosition->warp();
        PWINDOW->m_realSize->warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CRiverLayout::isWindowTiled(PHLWINDOW pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CRiverLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, PHLWINDOW pWindow) {

  //River's tiling paradigm has no concept of being able to manually resize windows in a stack/area etc. 
  //If you try to resize a window it just forces it to float. Do the same thing here
  const auto PNODE = getNodeFromWindow(pWindow);
  pWindow->m_isFloating = true;
  m_lMasterNodesData.remove(*PNODE);
  g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
  recalculateMonitor(pWindow->monitorID());

}

void CRiverLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) {
    const auto PMONITOR   = pWindow->m_monitor.lock();
    const auto PWORKSPACE = pWindow->m_workspace;

    // save position and size if floating
    if (pWindow->m_isFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
        pWindow->m_lastFloatingSize     = pWindow->m_realSize->goal();
        pWindow->m_lastFloatingPosition = pWindow->m_realPosition->goal();
        pWindow->m_position             = pWindow->m_realPosition->goal();
        pWindow->m_size                 = pWindow->m_realSize->goal();
    }

    if (EFFECTIVE_MODE == FSMODE_NONE) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            *pWindow->m_realPosition = pWindow->m_lastFloatingPosition;
            *pWindow->m_realSize     = pWindow->m_lastFloatingSize;

            pWindow->unsetWindowData(PRIORITY_LAYOUT);
            pWindow->updateWindowData();
        }
    } else {
        // apply new pos and size being monitors' box
        if (EFFECTIVE_MODE == FSMODE_FULLSCREEN) {
            *pWindow->m_realPosition = PMONITOR->m_position;
            *pWindow->m_realSize     = PMONITOR->m_size;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SRiverNodeData fakeNode;
            fakeNode.pWindow                = pWindow;
            fakeNode.position               = PMONITOR->m_position + PMONITOR->m_reservedTopLeft;
            fakeNode.size                   = PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight;
            fakeNode.workspaceID            = pWindow->workspaceID();
            pWindow->m_position            = fakeNode.position;
            pWindow->m_size                = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->changeWindowZOrder(pWindow, true);
}

void CRiverLayout::recalculateWindow(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->monitorID());
}

SWindowRenderLayoutHints CRiverLayout::requestRenderHints(PHLWINDOW pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesnt have any hints
}

void CRiverLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_monitor, pWindow->m_monitor);
        std::swap(pWindow2->m_workspace, pWindow->m_workspace);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    recalculateMonitor(pWindow->monitorID());
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->monitorID());

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);

}

Vector2D  CRiverLayout::predictSizeForNewWindowTiled() {
	return {}; //whatever
}

void CRiverLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
    recalculateMonitor(pWindow->monitorID());
}

PHLWINDOW CRiverLayout::getNextWindow(PHLWINDOW pWindow, bool next) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    if (next) {
            for (auto n : m_lMasterNodesData) {
                if (n.pWindow.lock() != pWindow && n.workspaceID == pWindow->workspaceID()) {
                    return n.pWindow.lock();
                }
            }
            // focus next
            bool reached = false;
            for (auto n : m_lMasterNodesData) {
                if (n.pWindow.lock() == pWindow) {
                    reached = true;
                    continue;
                }

                if (n.workspaceID == pWindow->workspaceID() && reached) {
                    return n.pWindow.lock();
                }
            }
    } else {
            // focus previous
            bool reached = false;
            for (auto it = m_lMasterNodesData.rbegin(); it != m_lMasterNodesData.rend(); it++) {
                if (it->pWindow.lock() == pWindow) {
                    reached = true;
                    continue;
                }

                if (it->workspaceID == pWindow->workspaceID() && reached) {
                    return it->pWindow.lock();
                }
            }
        }

    return nullptr;
}

std::any CRiverLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    if (!header.pWindow)
	    return 0;
    CVarList vars(message, 0, ' ');

    if (vars.size() < 1 || vars[0].empty()) {
        Debug::log(ERR, "layoutmsg called without params");
        return 0;
    }

  const auto RESOURCE = std::find_if(m_lRiverLayoutResources.begin(), m_lRiverLayoutResources.end(), [&](const auto &rres) { return rres.monitorID == header.pWindow->monitorID();});
  if (RESOURCE != m_lRiverLayoutResources.end()) {
   		g_pRiverLayoutProtocolManager->sendUserCommandTags(RESOURCE->resource, 1);
      g_pRiverLayoutProtocolManager->sendUserCommand(RESOURCE->resource, message.c_str());
      recalculateMonitor(header.pWindow->monitorID());
  }
    return 0;
}

void CRiverLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE);
}

void CRiverLayout::onEnable() {
    for (auto& w : g_pCompositor->m_windows) {
        if (w->m_isFloating ||  !w->m_isMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w);
    }
}

void CRiverLayout::onDisable() {
    m_lMasterNodesData.clear();
}
