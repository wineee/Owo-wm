#ifndef QBOXDECORATION_H
#define QBOXDECORATION_H

#include <qwxdgdecorationmanagerv1.h>

class QBoxServer;

using QW_NAMESPACE::QWXdgDecorationManagerV1;
using QW_NAMESPACE::QWXdgToplevelDecorationV1;

class QBoxDecoration : public QObject
{
    Q_OBJECT
public:
    QBoxDecoration(QBoxServer *m_server);

private Q_SLOTS:
    void onNewToplevelDecoration(QWXdgToplevelDecorationV1 *decorat);
    void onXdgDecorationMode();

private:
    QWXdgDecorationManagerV1 *decoratManager;
    QBoxServer *server;
};

#endif // QBOXDECORATION_H
