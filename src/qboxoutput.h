#ifndef QBOXOUTPUT_H
#define QBOXOUTPUT_H

#include <qwoutput.h>
#include <qwxdgshell.h>
#include <qwscene.h>
#include <qwxdgdecorationmanagerv1.h>
#include <QRect>

using QW_NAMESPACE::QWOutput,QW_NAMESPACE::QWOutputLayout, QW_NAMESPACE::QWXdgToplevel;
using QW_NAMESPACE::QWSceneTree, QW_NAMESPACE::QWXdgToplevelDecorationV1;
class QBoxServer;

class QBoxOutPut : public QObject
{
    Q_OBJECT
    friend class QBoxServer; // TODO : should be QBoxXdgShell
    friend class QBoxCursor;
public:
    explicit QBoxOutPut(QBoxServer *server);
    struct View
    {
        QBoxServer *server;
        QWXdgToplevel *xdgToplevel;
        QWSceneTree *sceneTree;

        QWXdgToplevelDecorationV1 *decoration;

        QRect geometry;
        QRect previous_geometry;
    };

private Q_SLOTS:
    void onNewOutput(QWOutput *output);
    void onOutputFrame();

private:
   QWOutputLayout *outputLayout;
   QList<QWOutput*> outputs;

   QBoxServer *m_server;
};

#endif // QBOXOUTPUT_H