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

    output = new QBoxOutPut(this);
    xdgShell = new QBoxXdgShell(this);
    decoration = new QBoxDecoration(this);
    cursor = new QBoxCursor(this);
    seat = new QBoxSeat(this);
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

