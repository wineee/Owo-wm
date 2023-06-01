#ifndef QBOXOUTPUT_H
#define QBOXOUTPUT_H

#include <qwoutput.h>
#include <qwxdgshell.h>
#include <qwscene.h>
#include <qwxdgdecorationmanagerv1.h>
#include <QRect>

QW_USE_NAMESPACE

class QBoxServer;

class QBoxOutPut : public QObject
{
    Q_OBJECT
    friend class QBoxCursor;
    friend class QBoxXdgShell;
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

public:
    QRect geometry;
    // wlr_scene_rect *background

private:
    QWOutputLayout *outputLayout;
    QList<QWOutput*> outputs;

    QBoxServer *m_server;
};

#endif // QBOXOUTPUT_H
