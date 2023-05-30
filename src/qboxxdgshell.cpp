#include "qboxxdgshell.h"
#include "qboxserver.h"

#include <qwdisplay.h>

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

    wlr_surface *prevSurface = m_server->seat->m_seat->handle()->keyboard_state.focused_surface;
    if (prevSurface == surface) {
        return;
    }
    if (prevSurface) {
        auto previous = QWXdgSurface::from(prevSurface);
        auto toplevel = qobject_cast<QWXdgToplevel*>(previous);
        Q_ASSERT(toplevel);
        toplevel->setActivated(false);
    }

    view->sceneTree->raiseToTop();
    m_server->views.move(m_server->views.indexOf(view), 0);
    view->xdgToplevel->setActivated(true);

    if (QWKeyboard *keyboard = m_server->seat->m_seat->getKeyboard()) {
        m_server->seat->m_seat->keyboardNotifyEnter(view->xdgToplevel->handle()->base->surface,
                                       keyboard->handle()->keycodes, keyboard->handle()->num_keycodes, &keyboard->handle()->modifiers);
    }
}

QWOutput *QBoxXdgShell::getActiveOutput(View *view)
{
    wlr_output *output = nullptr;
    const QPointF center_p = view->geometry.toRectF().center();
    QPointF closest_p = m_server->output->outputLayout->closestPoint(output, center_p);
    return QWOutput::from(m_server->output->outputLayout->outputAt(closest_p));
}

QBoxXdgShell::View *QBoxXdgShell::viewAt(const QPointF &pos, wlr_surface **surface, QPointF *spos) const
{
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
    wlr_scene_tree *tree = node->parent;
    while (tree && !tree->node.data) {
        tree = tree->node.parent;
    }

    return tree ? reinterpret_cast<View*>(tree->node.data) : nullptr;
}

void QBoxXdgShell::onNewXdgSurface(wlr_xdg_surface *surface)
{
    if (surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        auto *s = QWXdgPopup::from(surface->popup);
        auto parent = QWXdgSurface::from(surface->popup->parent);
        QWSceneTree *parentTree = reinterpret_cast<QWSceneTree*>(parent->handle()->data);
        surface->data = QWScene::xdgSurfaceCreate(parentTree, s);
        return;
    }
    Q_ASSERT(surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);

    auto view = new View();
    view->server = m_server;
    auto s = QWXdgToplevel::from(surface->toplevel);
    view->xdgToplevel = s;
    view->sceneTree = QWScene::xdgSurfaceCreate(scene, s);
    view->sceneTree->handle()->node.data = view;
    surface->data = view->sceneTree;
    connect(s, &QWXdgSurface::map, this, &QBoxXdgShell::onXdgToplevelMap);
    connect(s, &QWXdgSurface::unmap, this, &QBoxXdgShell::onXdgToplevelUnmap);
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

void QBoxXdgShell::onXdgToplevelMap()
{
    /* Called when the surface is mapped, or ready to display on-screen. */
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);

    auto geoBox = view->xdgToplevel->getGeometry();
    auto usableArea = getUsableArea(view);

    view->geometry = {
      0, // x
      0, // y
      std::min(geoBox.width(), usableArea.width()), // width
      std::min(geoBox.height(), usableArea.height()) // height
    };

    m_server->views.append(view);
    focusView(view, surface->handle()->surface);
}

void QBoxXdgShell::onXdgToplevelUnmap()
{
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    m_server->views.removeOne(view);
}

void QBoxXdgShell::onXdgToplevelRequestMove(wlr_xdg_toplevel_move_event *)
{
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    beginInteractive(view, QBoxCursor::CursorState::MovingWindow, 0);
}

void QBoxXdgShell::onXdgToplevelRequestResize(wlr_xdg_toplevel_resize_event *event)
{
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

void QBoxXdgShell::onXdgToplevelRequestRequestFullscreen(bool fullscreen)
{
    /* Just as with request_maximize, we must send a configure here. */
    Q_UNUSED(fullscreen);
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    surface->scheduleConfigure();
}

void QBoxXdgShell::beginInteractive(View *view, QBoxCursor::CursorState state, uint32_t edges)
{
    wlr_surface *focusedSurface = m_server->seat->m_seat->handle()->pointer_state.focused_surface;
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
