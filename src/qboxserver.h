#ifndef SERVER_H
#define SERVER_H

#include "qboxdecoration.h"

#include <QRect>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwrenderer.h>
#include <qwallocator.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwdatadevice.h>
#include <qwoutputlayout.h>
#include <qwoutput.h>
#include <qwscene.h>
#include <qwseat.h>
#include <qwxdgshell.h>
#include <qwcursor.h>
#include <qwxcursormanager.h>
#include <qwinputdevice.h>
#include <qwkeyboard.h>
#include <qwpointer.h>

extern "C" {
// avoid replace static
#include <wayland-server-core.h>
#define static
#include <wlr/util/log.h>
#include <wlr/util/edges.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_data_device.h>
#undef static
#include <wayland-server.h>
}
QW_USE_NAMESPACE

class QBoxServer : public QObject
{
    Q_OBJECT
    friend class QBoxDecoration;
public:
    QBoxServer();
    ~QBoxServer();

    bool start();

private Q_SLOTS:
    void onNewOutput(QWOutput *output);
    void onNewXdgSurface(wlr_xdg_surface *surface);
    void onXdgToplevelMap();
    void onXdgToplevelUnmap();
    void onXdgToplevelRequestMove(wlr_xdg_toplevel_move_event *);
    void onXdgToplevelRequestResize(wlr_xdg_toplevel_resize_event *event);
    void onXdgToplevelRequestMaximize(bool maximize);
    void onXdgToplevelRequestRequestFullscreen(bool fullscreen);

    void onCursorMotion(wlr_pointer_motion_event *event);
    void onCursorMotionAbsolute(wlr_pointer_motion_absolute_event *event);
    void onCursorButton(wlr_pointer_button_event *event);
    void onCursorAxis(wlr_pointer_axis_event *event);
    void onCursorFrame();

    void onNewInput(QWInputDevice *device);
    void onRequestSetCursor(wlr_seat_pointer_request_set_cursor_event *event);
    void onRequestSetSelection(wlr_seat_request_set_selection_event *event);

    void onKeyboardModifiers();
    void onKeyboardKey(wlr_keyboard_key_event *event);
    void onKeyboardDestroy();

    void onOutputFrame();

private:
    struct View
    {
        QBoxServer *server;
        QWXdgToplevel *xdgToplevel;
        QWSceneTree *sceneTree;

        QWXdgToplevelDecorationV1 *decoration;

        QRect geometry;
        QRect previous_geometry;
    };

    enum class CursorState {
        Normal,
        MovingWindow,
        ResizingWindow,
    };

    static inline View *getView(const QWXdgSurface *surface);
    View *viewAt(const QPointF &pos, wlr_surface **surface, QPointF *spos) const;
    void processCursorMotion(uint32_t time);
    void focusView(View *view, wlr_surface *surface);
    void beginInteractive(View *view, CursorState state, uint32_t edges);
    bool handleKeybinding(xkb_keysym_t sym);
    QRect getUsableArea(View *view);
    QWOutput *getActiveOutput(View *view);

    QWDisplay *display;
    QWBackend *backend;
    QWRenderer *renderer;
    QWAllocator *allocator;

    QWCompositor *compositor;
    QWSubcompositor *subcompositor;
    QWDataDeviceManager *dataDeviceManager;

    QWOutputLayout *outputLayout;
    QList<QWOutput*> outputs;

    QWScene *scene;
    QWXdgShell *xdgShell;
    QList<View*> views;
    View *grabbedView = nullptr;
    QPointF grabCursorPos;
    QRectF grabGeoBox;

    QBoxDecoration *decoration;

    QWCursor *cursor;
    QWXCursorManager *cursorManager;
    CursorState cursorState = CursorState::Normal;
    uint32_t resizingEdges = 0;

    QList<QWKeyboard*> keyboards;
    QWSeat *seat;
    QWSignalConnector sc;
};

#endif // SERVER_H
