#include "qboxlayershell.h"
#include "qboxserver.h"
#include "qwconfig.h"

#include <qwdisplay.h>
#include <qwlayershellv1.h>
#include <qwoutput.h>
#include <qwxdgshell.h>

QBoxLayerShell::QBoxLayerShell(QBoxServer *server):
    m_server(server),
    QObject(server)
{
    //scene = new QWScene(server);
    //scene->attachOutputLayout(m_server->output->outputLayout);
    layerShell = QWLayerShellV1::create(server->display, 4);
    connect(layerShell, &QWLayerShellV1::newSurface, this, &QBoxLayerShell::onNewXdgSurface);
}

void QBoxLayerShell::onNewXdgSurface(wlr_layer_surface_v1 *layerSurface)
{
    qDebug() << "new layer surface";
}
