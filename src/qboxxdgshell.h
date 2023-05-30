#ifndef QBOXXDGSHELL_H
#define QBOXXDGSHELL_H

#include "qboxoutput.h"
#include "qboxcursor.h"
#include <qwscene.h>
#include <qwxdgshell.h>
#include <QObject>

using QW_NAMESPACE::QWScene, QW_NAMESPACE::QWXdgShell, QW_NAMESPACE::QWXdgSurface;

class QBoxServer;

class QBoxXdgShell : public QObject
{
    Q_OBJECT
    friend class QBoxDecoration;
    friend class QBoxCursor;
    friend class QBoxSeat;
//    friend class QBoxOutPut;
    using View = QBoxOutPut::View;

public:
    explicit QBoxXdgShell(QBoxServer *parent);

    void focusView(View *view, wlr_surface *surface);
    QWOutput *getActiveOutput(View *view);
    View *viewAt(const QPointF &pos, wlr_surface **surface, QPointF *spos) const;
    QWScene *getScene() {
        return scene;
    }

private Q_SLOTS:
    void onNewXdgSurface(wlr_xdg_surface *surface);
    void onXdgToplevelMap();
    void onXdgToplevelUnmap();
    void onXdgToplevelRequestMove(wlr_xdg_toplevel_move_event *);
    void onXdgToplevelRequestResize(wlr_xdg_toplevel_resize_event *event);
    void onXdgToplevelRequestMaximize(bool maximize);
    void onXdgToplevelRequestMinimize(bool minimize);
    void onXdgToplevelRequestRequestFullscreen(bool fullscreen);

private:
    static inline View *getView(const QWXdgSurface *surface);
    void beginInteractive(View *view, QBoxCursor::CursorState state, uint32_t edges);
    QRect getUsableArea(View *view);

    QWScene *scene;
    QWXdgShell *xdgShell;

    QBoxServer *m_server;
};

#endif // QBOXXDGSHELL_H
