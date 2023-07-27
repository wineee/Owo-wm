#pragma once

#include "qboxoutput.h"
#include "qboxcursor.h"
#include <qwglobal.h>
#include <qwscene.h>
#include <qwlayershellv1.h>
#include <QObject>

QW_USE_NAMESPACE

class QBoxServer;

class QBoxLayerShell : public QObject
{
    Q_OBJECT
    friend class QBoxCursor;
    friend class QBoxSeat;

public:
    explicit QBoxLayerShell(QBoxServer *parent);

private Q_SLOTS:
    void onNewXdgSurface(wlr_layer_surface_v1 *surface);

private:
    //QWScene *scene;
    QWLayerShellV1 *layerShell;

    QBoxServer *m_server;
};
