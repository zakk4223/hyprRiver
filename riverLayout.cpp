#include "riverLayout.hpp"
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/decorations/DecorationPositioner.hpp>




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

void CRiverLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
		if (!pWindow) return;
    if (!isDirection(dir))
        return;

    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(pWindow, dir[0]);
		if (pWindow->m_pWorkspace != PWINDOW2->m_pWorkspace) {
 			// if different monitors, send to monitor
			onWindowRemovedTiling(pWindow);
			pWindow->moveToWorkspace(PWINDOW2->m_pWorkspace);
			pWindow->m_iMonitorID = PWINDOW2->m_iMonitorID;
			if (!silent) {
				const auto pMonitor = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
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
    if (pWindow->m_bIsFloating)
        return;

    //static auto* const PNEWTOP = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:new_on_top")->intValue;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    //const auto         PNODE = *PNEWTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();
    const auto PNODE = &m_lMasterNodesData.emplace_front();

    PNODE->workspaceID = pWindow->workspaceID();
    PNODE->pWindow     = pWindow;

    //static auto* const PNEWISMASTER = &HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:new_is_master")->intValue;

    const auto         WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);

    auto               OPENINGON = getNodeFromWindow(g_pCompositor->m_pLastWindow.lock());

		const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();
		if (g_pInputManager->m_bWasDraggingWindow && OPENINGON) {
			if (OPENINGON->pWindow.lock()->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, pWindow))
				return;
		}

    if (OPENINGON && OPENINGON != PNODE && OPENINGON->pWindow.lock()->m_sGroupData.pNextWindow.lock() // target is group
        && pWindow->canBeGroupedInto(OPENINGON->pWindow.lock())) {

        m_lMasterNodesData.remove(*PNODE);

        static const auto* USECURRPOS = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("group:insert_after_current");
        (**USECURRPOS ? OPENINGON->pWindow.lock() : OPENINGON->pWindow.lock()->getGroupTail())->insertWindowToGroup(pWindow);

        OPENINGON->pWindow.lock()->setGroupCurrent(pWindow);
        pWindow->applyGroupRules();
        pWindow->updateWindowDecos();
        recalculateWindow(pWindow);
		    if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
			    pWindow->addWindowDeco(std::make_unique<CHyprGroupBarDecoration>(pWindow));

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

    // recalc
    recalculateMonitor(pWindow->m_iMonitorID);
}

void CRiverLayout::onWindowRemovedTiling(PHLWINDOW pWindow) {
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

    m_vRemovedWindowVector = pWindow->m_vRealPosition.goal() + pWindow->m_vRealSize.goal() / 2.f;


    recalculateMonitor(pWindow->m_iMonitorID);
}

void CRiverLayout::recalculateMonitor(const int& monid) {
    const auto PMONITOR   = g_pCompositor->getMonitorFromID(monid);
    const auto PWORKSPACE = PMONITOR->activeWorkspace;

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->activeSpecialWorkspace) {
        calculateWorkspace(PMONITOR->activeSpecialWorkspace);
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
    calculateWorkspace(PWORKSPACE);
}

void CRiverLayout::calculateWorkspace(PHLWORKSPACE pWorkspace) {
    static uint32_t use_serial = 1;
    uint32_t new_serial = use_serial++;

    if (!pWorkspace)
        return;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
    const uint32_t num_nodes = getNodesOnWorkspace(pWorkspace->m_iID);
    //record serial so we know if we're done
    for (auto& nd : m_lMasterNodesData) {
	    if (nd.workspaceID != pWorkspace->m_iID)
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

		const auto PMONITOR = g_pCompositor->getMonitorFromID(nd.pWindow.lock()->m_iMonitorID);
		nd.position = PMONITOR->vecPosition  + PMONITOR->vecReservedTopLeft + Vector2D(x+0.0f, y+0.0f);
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

    CMonitor* PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->activeSpecialWorkspaceID() == pNode->workspaceID) {
                PMONITOR = m.get();
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_iMonitorID);
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node {} (workspace ID: {})!!", static_cast<void *>(pNode), pNode->workspaceID);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PWINDOW = pNode->pWindow.lock();
		const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(PWINDOW->workspaceID()));

		if (PWINDOW->m_bIsFullscreen && !pNode->ignoreFullscreenChecks)
			return;

		PWINDOW->updateSpecialRenderData();
		
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


    PWINDOW->m_vSize     = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    auto       calcPos  = PWINDOW->m_vPosition;
    auto       calcSize = PWINDOW->m_vSize;

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? gapsOut.left : gapsIn.left, DISPLAYTOP ? gapsOut.top : gapsIn.top);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? gapsOut.right : gapsIn.right, DISPLAYBOTTOM ? gapsOut.bottom : gapsIn.bottom);

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->workspaceID())) {
        static auto* const PSCALEFACTOR = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:river:layout:special_scale_factor")->getDataStaticPtr();

        CBox               wb = {calcPos + (calcSize - calcSize * **PSCALEFACTOR) / 2.f, calcSize * **PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        PWINDOW->m_vRealPosition = wb.pos();
        PWINDOW->m_vRealSize     = wb.size();

        g_pXWaylandManager->setWindowSize(PWINDOW, wb.size());
    } else {
				CBox wb = {calcPos, calcSize};
				wb.round();
        PWINDOW->m_vRealSize     = wb.size(); 
        PWINDOW->m_vRealPosition = wb.pos(); 

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize);
    }

    if (m_bForceWarps && !**PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

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
  pWindow->m_bIsFloating = true;
  m_lMasterNodesData.remove(*PNODE);
  g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
  recalculateMonitor(pWindow->m_iMonitorID);

}

void CRiverLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!validMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen || g_pCompositor->isWorkspaceSpecial(pWindow->workspaceID()))
        return; // ignore

    const auto PMONITOR   = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->workspaceID());

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen           = on;
    PWORKSPACE->m_bHasFullscreenWindow = !PWORKSPACE->m_bHasFullscreenWindow;

		pWindow->updateDynamicRules();
		pWindow->updateWindowDecos();

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
            pWindow->m_vLastFloatingSize     = pWindow->m_vRealSize.goal();
            pWindow->m_vLastFloatingPosition = pWindow->m_vRealPosition.goal();
            pWindow->m_vPosition             = pWindow->m_vRealPosition.goal();
            pWindow->m_vSize                 = pWindow->m_vRealSize.goal();
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
            fakeNode.workspaceID = pWindow->workspaceID();
            pWindow->m_vPosition = fakeNode.position;
            pWindow->m_vSize     = fakeNode.size;
						fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goal());

    g_pCompositor->changeWindowZOrder(pWindow, true);

    recalculateMonitor(PMONITOR->ID);
}

void CRiverLayout::recalculateWindow(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->m_iMonitorID);
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

    const auto inheritFullscreen = prepareLoseFocus(pWindow);

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_iMonitorID, pWindow->m_iMonitorID);
        std::swap(pWindow2->m_pWorkspace, pWindow->m_pWorkspace);
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

Vector2D  CRiverLayout::predictSizeForNewWindowTiled() {
	return {}; //whatever
}

void CRiverLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
    recalculateMonitor(pWindow->m_iMonitorID);
}

PHLWINDOW CRiverLayout::getNextWindow(PHLWINDOW pWindow, bool next) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    const auto PNODE = getNodeFromWindow(pWindow);

    if (next) {
            for (auto n : m_lMasterNodesData) {
                if (n.pWindow.lock() != pWindow && n.workspaceID == pWindow->workspaceID()) {
                    return n.pWindow.lock();
                }
            }
            // focus next
            bool reached = false;
            bool found   = false;
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
            bool found   = false;
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

bool CRiverLayout::prepareLoseFocus(PHLWINDOW pWindow) {
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

void CRiverLayout::prepareNewFocus(PHLWINDOW pWindow, bool inheritFullscreen) {
    if (!pWindow)
        return;

    if (inheritFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, true, g_pCompositor->getWorkspaceByID(pWindow->workspaceID())->m_efFullscreenMode);
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
   		g_pRiverLayoutProtocolManager->sendUserCommandTags(RESOURCE->resource, 1);
      g_pRiverLayoutProtocolManager->sendUserCommand(RESOURCE->resource, message.c_str());
      recalculateMonitor(header.pWindow->m_iMonitorID);
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
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating ||  !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w);
    }
}

void CRiverLayout::onDisable() {
    m_lMasterNodesData.clear();
}
