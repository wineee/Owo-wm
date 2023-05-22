#include "qboxdecoration.h"
#include "qboxserver.h"

extern "C" {
#include <wlr/types/wlr_xdg_decoration_v1.h>
}

QBoxDecoration::QBoxDecoration(QBoxServer *m_server):
    server(m_server),
    decoratManager(QWXdgDecorationManagerV1::create(m_server->display)),
    QObject(nullptr)
{
    if (decoratManager != nullptr) {
        connect(decoratManager, &QWXdgDecorationManagerV1::newToplevelDecoration, this, &QBoxDecoration::onNewToplevelDecoration);
    }
}

void QBoxDecoration::onNewToplevelDecoration(QWXdgToplevelDecorationV1 *toplevel_decoration)
{
    connect(toplevel_decoration,
            &QWXdgToplevelDecorationV1::requestMode,
            this,
            &QBoxDecoration::onXdgDecorationMode);
    connect(decoratManager, &QObject::destroyed, this, &QObject::deleteLater);
    /* For some reason, a lot of clients don't emit the request_mode signal. */
    Q_EMIT toplevel_decoration->requestMode();
}

void QBoxDecoration::onXdgDecorationMode()
{
    auto *toplevel_decoration = qobject_cast<QWXdgToplevelDecorationV1*>(QObject::sender());
    // do not support server-side decorations yet
    toplevel_decoration->setMode(WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    if (!server->views.empty())
      server->views.last()->decoration = toplevel_decoration;
}