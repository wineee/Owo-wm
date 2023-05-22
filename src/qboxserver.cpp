#include "qboxserver.h"

#include <QGuiApplication>
#include <QLoggingCategory>

QBoxServer::QBoxServer()
{
    display = new QWDisplay(this);
    backend = QWBackend::autoCreate(display, this);
    if (!backend)
        qFatal("failed to create wlr_backend");

    renderer = QWRenderer::autoCreate(backend);
    if (!renderer)
        qFatal("failed to create wlr_renderer");
    renderer->initWlDisplay(display);

    allocator = QWAllocator::autoCreate(backend, renderer);
    if (!allocator)
        qFatal("failed to create wlr_allocator");

    compositor = QWCompositor::create(display, renderer);
    subcompositor = QWSubcompositor::create(display);
    dataDeviceManager = QWDataDeviceManager::create(display);

    outputLayout = new QWOutputLayout(this);
    connect(backend, &QWBackend::newOutput, this, &QBoxServer::onNewOutput);

    scene = new QWScene(this);
    scene->attachOutputLayout(outputLayout);
    xdgShell = QWXdgShell::create(display, 3);
    connect(xdgShell, &QWXdgShell::newSurface, this, &QBoxServer::onNewXdgSurface);

    decoration = new QBoxDecoration(this);

    cursor = new QBoxCursor(this);

    connect(backend, &QWBackend::newInput, this, &QBoxServer::onNewInput);

    seat = QWSeat::create(display, "seat0");
    connect(seat, &QWSeat::requestSetCursor, this, &QBoxServer::onRequestSetCursor);
    connect(seat, &QWSeat::requestSetSelection, this, &QBoxServer::onRequestSetSelection);
}

QBoxServer::~QBoxServer()
{
    delete backend;
}

bool QBoxServer::start()
{
    const char *socket = display->addSocketAuto();
    if (!socket) {
        return false;
    }

    if (!backend->start())
        return false;

    qputenv("WAYLAND_DISPLAY", QByteArray(socket));
    qInfo("Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);

    display->start(qApp->thread());
    return true;
}

void QBoxServer::onNewOutput(QWOutput *output)
{
    Q_ASSERT(output);
    outputs.append(output);

    output->initRender(allocator, renderer);
    if (!wl_list_empty(&output->handle()->modes)) {
        auto *mode = output->preferredMode();
        output->setMode(mode);
        output->enable(true);
        if (!output->commit())
            return;
    }

    connect(output, &QWOutput::frame, this, &QBoxServer::onOutputFrame);
    outputLayout->addAuto(output->handle());
}

void QBoxServer::onNewXdgSurface(wlr_xdg_surface *surface)
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
    view->server = this;
    auto s = QWXdgToplevel::from(surface->toplevel);
    view->xdgToplevel = s;
    view->sceneTree = QWScene::xdgSurfaceCreate(scene, s);
    view->sceneTree->handle()->node.data = view;
    surface->data = view->sceneTree;
    connect(s, &QWXdgSurface::map, this, &QBoxServer::onXdgToplevelMap);
    connect(s, &QWXdgSurface::unmap, this, &QBoxServer::onXdgToplevelUnmap);
    connect(s, &QWXdgToplevel::requestMove, this, &QBoxServer::onXdgToplevelRequestMove);
    connect(s, &QWXdgToplevel::requestResize, this, &QBoxServer::onXdgToplevelRequestResize);
    connect(s, &QWXdgToplevel::requestMaximize, this, &QBoxServer::onXdgToplevelRequestMaximize);
    connect(s, &QWXdgToplevel::requestMinimize, this, &QBoxServer::onXdgToplevelRequestMinimize);
    connect(s, &QWXdgToplevel::requestFullscreen, this, &QBoxServer::onXdgToplevelRequestRequestFullscreen);
    connect(s, &QWXdgToplevel::destroyed, this, [this, view] {
        views.removeOne(view);
        if (grabbedView == view)
            grabbedView = nullptr;
        delete view;
    });
}

void QBoxServer::onXdgToplevelMap()
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

    views.append(view);
    focusView(view, surface->handle()->surface);
}

void QBoxServer::onXdgToplevelUnmap()
{
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    views.removeOne(view);
}

void QBoxServer::onXdgToplevelRequestMove(wlr_xdg_toplevel_move_event *)
{
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    beginInteractive(view, QBoxCursor::CursorState::MovingWindow, 0);
}

void QBoxServer::onXdgToplevelRequestResize(wlr_xdg_toplevel_resize_event *event)
{
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    auto view = getView(surface);
    Q_ASSERT(view);
    beginInteractive(view, QBoxCursor::CursorState::ResizingWindow, event->edges);
}

QWOutput *QBoxServer::getActiveOutput(View *view)
{
    wlr_output *output = nullptr;
    const QPointF center_p = view->geometry.toRectF().center();
    QPointF closest_p = outputLayout->closestPoint(output, center_p);
    return QWOutput::from(outputLayout->outputAt(closest_p));
}

QRect QBoxServer::getUsableArea(View *view)
{
    QRect usable_area{0, 0, 0, 0};
    QWOutput *output = getActiveOutput(view);
    usable_area.setSize(output->effectiveResolution());
    return usable_area;
}

void QBoxServer::onXdgToplevelRequestMaximize(bool maximize)
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

void QBoxServer::onXdgToplevelRequestMinimize(bool minimize)
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

void QBoxServer::onXdgToplevelRequestRequestFullscreen(bool fullscreen)
{
    /* Just as with request_maximize, we must send a configure here. */
    Q_UNUSED(fullscreen);
    auto surface = qobject_cast<QWXdgSurface*>(sender());
    surface->scheduleConfigure();
}


void QBoxServer::onNewInput(QWInputDevice *device)
{
    if (QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(device)) {

        xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        xkb_keymap *keymap = xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

        keyboard->setKeymap(keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        keyboard->setRepeatInfo(25, 600);

        connect(keyboard, &QWKeyboard::modifiers, this, &QBoxServer::onKeyboardModifiers);
        connect(keyboard, &QWKeyboard::key, this, &QBoxServer::onKeyboardKey);
        connect(keyboard, &QWKeyboard::destroyed, this, &QBoxServer::onKeyboardDestroy);

        seat->setKeyboard(keyboard);

        Q_ASSERT(!keyboards.contains(keyboard));
        keyboards.append(keyboard);
    } else if (device->handle()->type == WLR_INPUT_DEVICE_POINTER) {
        cursor->m_cursor->attachInputDevice(device);
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!keyboards.isEmpty()) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    seat->setCapabilities(caps);
}

void QBoxServer::onRequestSetCursor(wlr_seat_pointer_request_set_cursor_event *event)
{
    if (seat->handle()->pointer_state.focused_client == event->seat_client)
        cursor->m_cursor->setSurface(event->surface, QPoint(event->hotspot_x, event->hotspot_y));
}

void QBoxServer::onRequestSetSelection(wlr_seat_request_set_selection_event *event)
{
    seat->setSelection(event->source, event->serial);
}

void QBoxServer::onKeyboardModifiers()
{
    QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(QObject::sender());
    seat->setKeyboard(keyboard);
    seat->keyboardNotifyModifiers(&keyboard->handle()->modifiers);
}

void QBoxServer::onKeyboardKey(wlr_keyboard_key_event *event)
{
    QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(QObject::sender());
    /* Translate libinput keycode -> xkbcommon */
    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->handle()->xkb_state, keycode, &syms);

    bool handled = false;
    uint32_t modifiers = keyboard->getModifiers();
    if ((modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_CTRL))
            && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int i = 0; i < nsyms; i++)
            handled = handleKeybinding(syms[i]);
    }

    if (!handled) {
        seat->setKeyboard(keyboard);
        seat->keyboardNotifyKey(event->time_msec, event->keycode, event->state);
    }
}

void QBoxServer::onKeyboardDestroy()
{
    QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(QObject::sender());
    keyboards.removeOne(keyboard);
}

void QBoxServer::onOutputFrame()
{
    auto output = qobject_cast<QWOutput*>(sender());
    Q_ASSERT(output);
    auto sceneOutput = QWSceneOutput::from(scene, output);
    sceneOutput->commit();

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    sceneOutput->sendFrameDone(&now);
}

QBoxServer::View *QBoxServer::getView(const QWXdgSurface *surface)
{
    auto sceneTree = reinterpret_cast<QWSceneTree*>(surface->handle()->data);
    return reinterpret_cast<View*>(sceneTree->handle()->node.data);
}

QBoxServer::View *QBoxServer::viewAt(const QPointF &pos, wlr_surface **surface, QPointF *spos) const
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

void QBoxServer::focusView(View *view, wlr_surface *surface)
{
    if (!view)
        return;

    wlr_surface *prevSurface = seat->handle()->keyboard_state.focused_surface;
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
    views.move(views.indexOf(view), 0);
    view->xdgToplevel->setActivated(true);

    if (QWKeyboard *keyboard = seat->getKeyboard()) {
        seat->keyboardNotifyEnter(view->xdgToplevel->handle()->base->surface,
                                       keyboard->handle()->keycodes, keyboard->handle()->num_keycodes, &keyboard->handle()->modifiers);
    }
}

void QBoxServer::beginInteractive(View *view, QBoxCursor::CursorState state, uint32_t edges)
{
    wlr_surface *focusedSurface = seat->handle()->pointer_state.focused_surface;
    if (view->xdgToplevel->handle()->base->surface !=
            wlr_surface_get_root_surface(focusedSurface)) {
        return;
    }
    grabbedView = view;
    cursor->cursorState = state;
    grabCursorPos = cursor->m_cursor->position();
    grabGeoBox = view->xdgToplevel->getGeometry();
    grabGeoBox.moveTopLeft(view->geometry.topLeft() + grabGeoBox.topLeft());
    resizingEdges = edges;
}

bool QBoxServer::handleKeybinding(xkb_keysym_t sym)
{
    switch (sym) {
    case XKB_KEY_Escape:
        display->terminate();
        qApp->exit();
        break;
    case XKB_KEY_F1:
        if (views.size() < 2)
            break;

        focusView(views.at(1), views.at(1)->xdgToplevel->handle()->base->surface);
        break;
    default:
        return false;
    }
    return true;
}

