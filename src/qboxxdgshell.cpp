#include "qboxxdgshell.h"
#include "qboxserver.h"
#include "qwconfig.h"

#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwxdgshell.h>

QBoxXdgShell::QBoxXdgShell(QBoxServer *server):
    m_server(server),
    QObject(server)
{
    scene = new QWScene(server);
    scene->attachOutputLayout(m_server->output->outputLayout);
    xdgShell = QWXdgShell::create(server->display, 3);
    connect(xdgShell, &QWXdgShell::newSurface, this, &QBoxXdgShell::onNewXdgSurface);
}

void QBoxXdgShell::focusView(View *view, wlr_surface *surface)
{
    if (!view)
        return;

    auto *seat = m_server->seat->m_seat;
    wlr_surface *prevSurface = seat->handle()->keyboard_state.focused_surface;
    if (prevSurface == surface) {
        /* Don't re-focus an already focused surface. */
        return;
    }
    if (prevSurface) {
        /*
         * Deactivate the previously focused surface. This lets the client know
         * it no longer has focus and the client will repaint accordingly, e.g.
         * stop displaying a caret.
         */
        auto previous = QWXdgSurface::from(QWSurface::from(prevSurface));
        auto toplevel = qobject_cast<QWXdgToplevel*>(previous);
        Q_ASSERT(toplevel);
        toplevel->setActivated(false);
    }

    /* Move the view to the front */
//   if (!seat->focused_layer) {
        view->sceneTree->raiseToTop();
//   }
    m_server->views.move(m_server->views.indexOf(view), 0);
    /* Activate the new surface */
    view->xdgToplevel->setActivated(true);

    /*
     * Tell the seat to have the keyboard enter this surface. wlroots will keep
     * track of this and automatically send key events to the appropriate
     * clients without additional work on your part.
     */
    if (QWKeyboard *keyboard = seat->getKeyboard()) {
        seat->keyboardNotifyEnter(QWSurface::from(view->xdgToplevel->handle()->base->surface),
                                       keyboard->handle()->keycodes, keyboard->handle()->num_keycodes, &keyboard->handle()->modifiers);
    }
}

QWOutput *QBoxXdgShell::getActiveOutput(View *view)
{
    const QPointF center_p = view->geometry.toRectF().center();
    QPointF closest_p = m_server->output->outputLayout->closestPoint(nullptr, center_p);
    return m_server->output->outputLayout->outputAt(closest_p);
}

QBoxXdgShell::View *QBoxXdgShell::viewAt(const QPointF &pos, wlr_surface **surface, QPointF *spos) const
{
    /* This returns the topmost node in the scene at the given layout coords.
     * we only care about surface nodes as we are specifically looking for a
     * surface in the surface tree of a qboxview. */
    auto node = scene->at(pos, spos);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
        return nullptr;
    }
    auto *sceneBuffer = QWSceneBuffer::from(node);
#if WLR_VERSION_MINOR > 16
    auto *sceneSurface = wlr_scene_surface_try_from_buffer(sceneBuffer->handle());
#else
    auto *sceneSurface = wlr_scene_surface_from_buffer(sceneBuffer->handle());
#endif
    if (!sceneSurface)
        return nullptr;

    *surface = sceneSurface->surface;
    /* Find the node corresponding to the `view` at the root of this
     * surface tree, it is the only one for which we set the data field. */
    wlr_scene_tree *tree = node->parent;
    while (tree && !tree->node.data) {
        tree = tree->node.parent;
    }

    return tree ? reinterpret_cast<View*>(tree->node.data) : nullptr;
}

void QBoxXdgShell::onNewXdgSurface(wlr_xdg_surface *surface)
{
    /* This event is raised when wlr_xdg_shell receives a new xdg surface from a
     * client, either a toplevel (application window) or popup. */

    /* We must add xdg popups to the scene graph so they get rendered. The
     * wlroots scene graph provides a helper for this, but to use it we must
     * provide the proper parent scene node of the xdg popup. To enable this,
     * we always set the user data field of xdg_surfaces to the corresponding
     * scene node. */
    if (surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        auto *s = QWXdgPopup::from(surface->popup);
        auto parent = QWXdgSurface::from(QWSurface::from(surface->popup->parent));
        QWSceneTree *parentTree = reinterpret_cast<QWSceneTree*>(parent->handle()->data);
        surface->data = QWScene::xdgSurfaceCreate(parentTree, s);
        // TODO:: map/unmap for popups?
        return;
    }
    Q_ASSERT(surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

    /* Allocate a View for this surface */
    auto view = new View();
    view->server = m_server;
    auto s = QWXdgToplevel::from(surface->toplevel);
    view->xdgToplevel = s;
    view->sceneTree = QWScene::xdgSurfaceCreate(scene, s);
    view->sceneTree->handle()->node.data = view;
    surface->data = view->sceneTree;
    /* Listen to the various events it can emit */
    connect(s->surface(), &QWSurface::map, this, &QBoxXdgShell::onMap);
    connect(s->surface(), &QWSurface::unmap, this, &QBoxXdgShell::onUnmap);
    connect(s->toPopup(), &QWXdgPopup::newPopup, this, &QBoxXdgShell::onXdgToplevelNewPopup);
    connect(s, &QWXdgToplevel::requestMove, this, &QBoxXdgShell::onXdgToplevelRequestMove);
    connect(s, &QWXdgToplevel::requestResize, this, &QBoxXdgShell::onXdgToplevelRequestResize);
    connect(s, &QWXdgToplevel::requestMaximize, this, &QBoxXdgShell::onXdgToplevelRequestMaximize);
    connect(s, &QWXdgToplevel::requestMinimize, this, &QBoxXdgShell::onXdgToplevelRequestMinimize);
    connect(s, &QWXdgToplevel::requestFullscreen, this, &QBoxXdgShell::onXdgToplevelRequestRequestFullscreen);
    connect(s, &QWXdgToplevel::destroyed, this, [this, view] {
        m_server->views.removeOne(view);
        if (m_server->grabbedView == view)
            m_server->grabbedView = nullptr;
        delete view;
    });
}

void QBoxXdgShell::onMap()
{
    /* Called when the surface is mapped, or ready to display on-screen. */
    auto *surface = QWXdgSurface::from(qobject_cast<QWSurface*>(sender()));
    if (!surface)
        return;
    auto view = getView(surface);
    Q_ASSERT(view);
    if (view->xdgToplevel->handle()->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;
    auto geoBox = view->xdgToplevel->getGeometry();
    auto usableArea = getUsableArea(view);

    /*
     * TODO: config geometry
     */
    view->geometry = {
      0, // x
      0, // y
      std::min(geoBox.width(), usableArea.width()), // width
      std::min(geoBox.height(), usableArea.height()) // height
    };

    m_server->views.append(view); // ?

    /* A view no larger than a title bar shouldn't be sized or focused */
    if (view->geometry.height() > TITLEBAR_HEIGHT &&
            view->geometry.height() > TITLEBAR_HEIGHT * (usableArea.width()/usableArea.height())) {
        view->xdgToplevel->setSize(view->geometry.size());
        focusView(view, surface->handle()->surface);
    }
    view->sceneTree->setPosition(view->geometry.topLeft());
}

void QBoxXdgShell::onUnmap()
{
    auto *surface = QWXdgSurface::from(qobject_cast<QWSurface*>(sender()));
    if (!surface)
        return;
    auto view = getView(surface);
    Q_ASSERT(view);
    if (view->xdgToplevel->handle()->base->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL)
        return;

    qsizetype viewid = m_server->views.indexOf(view);
    m_server->views.removeAt(viewid);

    /* Focus the next view, if any. */
    if (viewid >= m_server->views.size())
        return;
    auto *nextView = m_server->views.at(viewid);
    if (nextView && nextView->sceneTree && nextView->sceneTree->handle()->node.enabled) {
        wlr_log(WLR_INFO, "%s: %s", "Focusing next view",
            nextView->xdgToplevel->handle()->app_id);
         focusView(nextView, nextView->xdgToplevel->handle()->base->surface);
    }
}

void QBoxXdgShell::onXdgToplevelNewPopup(QWXdgPopup *popup)
{
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    QPointF outputPos = view->geometry.topLeft() + popup->getGeometry().topLeft();
    auto *woutput = m_server->output->outputLayout->outputAt(outputPos); // TODO

    if (!woutput) {
        return;
    }
    auto *qoutput = reinterpret_cast<QBoxOutPut*>(woutput->handle()->data);

    int topMargin = 0; // TODO: read from config

    QRect outputToplevelBox = qoutput->geometry;
    outputToplevelBox.moveTopLeft(outputToplevelBox.topLeft() - view->geometry.topLeft());
    outputToplevelBox.setHeight(outputToplevelBox.height() - topMargin);
    popup->unconstrainFromBox(outputToplevelBox);
}


void QBoxXdgShell::onXdgToplevelRequestMove(wlr_xdg_toplevel_move_event *)
{
    /* This event is raised when a client would like to begin an interactive
     * move, typically because the user clicked on their client-side
     * decorations. */
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    beginInteractive(view, QBoxCursor::CursorState::MovingWindow, 0);
}

void QBoxXdgShell::onXdgToplevelRequestResize(wlr_xdg_toplevel_resize_event *event)
{
    /* This event is raised when a client would like to begin an interactive
     * resize, typically because the user clicked on their client-side
     * decorations. */
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    beginInteractive(view, QBoxCursor::CursorState::ResizingWindow, event->edges);
}

QRect QBoxXdgShell::getUsableArea(View *view)
{
    QRect usable_area{0, 0, 0, 0};
    QWOutput *output = getActiveOutput(view);
    usable_area.setSize(output->effectiveResolution());
    return usable_area;
}

void QBoxXdgShell::onXdgToplevelRequestMaximize(bool maximize)
{
    /* This event is raised when a client would like to maximize itself,
     * typically because the user clicked on the maximize button on
     * client-side decorations.
    */
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);

    QRect usable_area = getUsableArea(view);

    bool is_maximized = view->xdgToplevel->handle()->current.maximized;
    if (!is_maximized) {
         view->previous_geometry = view->geometry;
         // FIXME: should not set this
         view->previous_geometry.setWidth(view->xdgToplevel->handle()->current.width);
         view->previous_geometry.setHeight(view->xdgToplevel->handle()->current.height);
         view->geometry.setX(0);
         view->geometry.setY(0);
    } else {
         usable_area = view->previous_geometry;
         view->geometry.setTopLeft(view->previous_geometry.topLeft());
    }

    view->xdgToplevel->setSize(usable_area.size());
    view->xdgToplevel->setMaximized(!is_maximized);
    view->sceneTree->setPosition(view->geometry.topLeft());
//    surface->scheduleConfigure();
}

void QBoxXdgShell::onXdgToplevelRequestMinimize(bool minimize)
{
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    bool minimize_requested = view->xdgToplevel->handle()->requested.minimized;

    if (minimize_requested) {
        view->previous_geometry = view->geometry;
        // FIXME: should not set this
        view->previous_geometry.setWidth(view->xdgToplevel->handle()->current.width);
        view->previous_geometry.setHeight(view->xdgToplevel->handle()->current.height);

        view->geometry.setY(-view->previous_geometry.height());

        //auto *next_view = view.
        //struct wb_view *next_view = wl_container_of(view->link.next, next_view, link);

        //if (wl_list_length(&view->link) > 1)
        //	focus_view(next_view, next_view->xdg_toplevel->base->surface);
        //else
        //	focus_view(view, view->xdg_toplevel->base->surface);
    } else {
        view->geometry = view->previous_geometry;
    }

    view->sceneTree->setPosition(view->geometry.topLeft());
}

void QBoxXdgShell::onXdgToplevelRequestRequestFullscreen(bool fullscreen) // TODO
{
    /* This event is raised when a client would like to set itself to
     * fullscreen. qwlbox currently doesn't support fullscreen, but to
     * conform to xdg-shell protocol we still must send a configure.
     * wlr_xdg_surface_schedule_configure() is used to send an empty reply.
     */
    Q_UNUSED(fullscreen);
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    surface->scheduleConfigure();
}

void QBoxXdgShell::beginInteractive(View *view, QBoxCursor::CursorState state, uint32_t edges)
{
    /* This function sets up an interactive move or resize operation, where the
     * compositor stops propagating pointer events to clients and instead
     * consumes them itself, to move or resize windows. */
    wlr_surface *focusedSurface = m_server->seat->m_seat->handle()->pointer_state.focused_surface;

    /* Deny move/resize requests from unfocused clients. */
    if (view->xdgToplevel->handle()->base->surface !=
            wlr_surface_get_root_surface(focusedSurface)) {
        return;
    }
    m_server->grabbedView = view;
    m_server->cursor->setCursorState(state);

    m_server->grabCursorPos = m_server->cursor->getCursor()->position();
    m_server->grabGeoBox = view->xdgToplevel->getGeometry();
    m_server->grabGeoBox.moveTopLeft(view->geometry.topLeft() + m_server->grabGeoBox.topLeft());
    m_server->resizingEdges = edges;
}

QBoxXdgShell::View *QBoxXdgShell::getView(const QWXdgSurface *surface)
{
    auto sceneTree = reinterpret_cast<QWSceneTree*>(surface->handle()->data);
    return reinterpret_cast<View*>(sceneTree->handle()->node.data);
}
