#include "qboxcursor.h"
#include "qboxserver.h"
#include <qwseat.h>

QBoxCursor::QBoxCursor(QBoxServer *server):
    m_service(server),
    QObject(server)
{
    m_cursor = new QWCursor(server);
    m_cursor->attachOutputLayout(server->output->outputLayout);
    bool sizeIsOk = false;
    uint32_t xcursor_size = qEnvironmentVariableIntValue("XCURSOR_SIZE", &sizeIsOk);
    if (!sizeIsOk)
        xcursor_size = 24;
    m_cursorManager = QWXCursorManager::create(getenv("XCURSOR_THEME"), xcursor_size);
    m_cursorManager->load(1);
    connect(m_cursor, &QWCursor::motion, this, &QBoxCursor::onCursorMotion);
    connect(m_cursor, &QWCursor::motionAbsolute, this, &QBoxCursor::onCursorMotionAbsolute);
    connect(m_cursor, &QWCursor::button, this, &QBoxCursor::onCursorButton);
    connect(m_cursor, &QWCursor::axis, this, &QBoxCursor::onCursorAxis);
    connect(m_cursor, &QWCursor::frame, this, &QBoxCursor::onCursorFrame);
}

void QBoxCursor::setCursorState(CursorState state)
{
    cursorState = state;
}

void QBoxCursor::onCursorMotion(wlr_pointer_motion_event *event)
{
    m_cursor->move(QWPointer::from(event->pointer), QPointF(event->delta_x, event->delta_y));
    processCursorMotion(event->time_msec);
}

void QBoxCursor::onCursorMotionAbsolute(wlr_pointer_motion_absolute_event *event)
{
    m_cursor->warpAbsolute(QWPointer::from(event->pointer), QPointF(event->x, event->y));
    processCursorMotion(event->time_msec);
}

void QBoxCursor::onCursorButton(wlr_pointer_button_event *event)
{
    /* This event is forwarded by the cursor when a pointer emits a button
     * event. */
    auto *xdgShell = m_service->xdgShell;
    /* Notify the client with pointer focus that a button press has occurred */
    getSeat()->pointerNotifyButton(event->time_msec, event->button, event->state);
    QPointF spos;
    wlr_surface *surface = nullptr;
    auto view = xdgShell->viewAt(m_cursor->position(), &surface, &spos);
    if (event->state == WLR_BUTTON_RELEASED) {
        /* If you released any buttons, we exit interactive move/resize mode. */
        cursorState = CursorState::Normal;
    } else { /// WLR_BUTTON_PRESSED
        /* Focus that client if the button was _pressed_ */
        xdgShell->focusView(view, surface);
    }

    // TODO: wlr_idle_notifier_v1_notify_activity
}

void QBoxCursor::onCursorAxis(wlr_pointer_axis_event *event)
{
    /* This event is forwarded by the cursor when a pointer emits an axis event,
     * for example when you move the scroll wheel. */

    /* Notify the client with pointer focus of the axis event. */
    getSeat()->pointerNotifyAxis(event->time_msec, event->orientation,
                                 event->delta, event->delta_discrete, event->source);
}

void QBoxCursor::onCursorFrame()
{
    /* This event is forwarded by the cursor when a pointer emits an frame
     * event. Frame events are sent after regular pointer events to group
     * multiple events together. For instance, two axis events may happen at the
     * same time, in which case a frame event won't be sent in between. */

    /* Notify the client with pointer focus of the frame event. */
    getSeat()->pointerNotifyFrame();
}

void QBoxCursor::processCursorMotion(uint32_t time)
{
    /* If the mode is non-passthrough, delegate to those functions. */
    if (cursorState == CursorState::MovingWindow) {
        processCursorMove();
        return;
    } else if (cursorState == CursorState::ResizingWindow) {
        processCursorResize();
        return;
    }

    /* Otherwise, find the view under the pointer and send the event along. */
    wlr_surface *surface = nullptr;
    QPointF spos;
    auto view = m_service->xdgShell->viewAt(m_cursor->position(), &surface, &spos);

    /* If there's no view under the cursor, set the cursor image to a
     * default. This is what makes the cursor image appear when you move it
     * around the screen, not over any views. */
    if (!view)
        m_cursorManager->setCursor("default", m_cursor);

    if (surface) {
        /*
         * "Enter" the surface if necessary. This lets the client know that the
         * cursor has entered one of its surfaces.
         *
         * Note that wlroots will avoid sending duplicate enter/motion events if
         * the surface has already has pointer focus or if the client is already
         * aware of the coordinates passed.
         */
        getSeat()->pointerNotifyEnter(QWSurface::from(surface), spos.x(), spos.y());
        getSeat()->pointerNotifyMotion(time, spos.x(), spos.y());
    } else {
        /* Clear pointer focus so future button events and such are not sent to
         * the last client to have the cursor over it. */
        getSeat()->pointerClearFocus();
    }

    // TODO: wlr_idle_notifier_v1_notify_activity
}

void QBoxCursor::processCursorMove()
{
    /* Move the grabbed view to the new position. */
    auto *grabbedView = m_service->grabbedView;
    QRectF grabGeoBox = m_service->grabGeoBox;

    if (grabbedView->sceneTree->handle()->node.type == WLR_SCENE_NODE_TREE) {
        grabbedView->geometry.setTopLeft((grabGeoBox.topLeft() + m_cursor->position() - m_service->grabCursorPos).toPoint());
        grabbedView->sceneTree->setPosition(grabbedView->geometry.topLeft());
    };
}

void QBoxCursor::processCursorResize()
{
    auto *grabbedView = m_service->grabbedView;
    const QPointF &cursorPos = m_cursor->position();
    QRectF newGeoBox = m_service->grabGeoBox;
    const int minimumSize = 10;

    // FIXME: Left May > Right
    if (m_service->resizingEdges & WLR_EDGE_TOP) {
        newGeoBox.setTop(cursorPos.y());
    } else if (m_service->resizingEdges & WLR_EDGE_BOTTOM) {
        newGeoBox.setBottom(cursorPos.y());
    }
    if (m_service->resizingEdges & WLR_EDGE_LEFT) {
        newGeoBox.setLeft(cursorPos.x());
    } else if (m_service->resizingEdges & WLR_EDGE_RIGHT) {
        newGeoBox.setRight(cursorPos.x());
    }

    QSize minSize(grabbedView->xdgToplevel->handle()->current.min_width,
                  grabbedView->xdgToplevel->handle()->current.min_height);
    QSize maxSize(grabbedView->xdgToplevel->handle()->current.max_width,
                  grabbedView->xdgToplevel->handle()->current.max_height);

    if (maxSize.width() == 0)
        maxSize.setWidth(99999);
    if (maxSize.height() == 0)
        maxSize.setHeight(99999);

    auto currentGeoBox = grabbedView->xdgToplevel->getGeometry();
    currentGeoBox.moveTopLeft(grabbedView->geometry.topLeft() + currentGeoBox.topLeft());
    if (newGeoBox.width() < qMax(minimumSize, minSize.width()) || newGeoBox.width() > maxSize.width()) {
        newGeoBox.setLeft(currentGeoBox.left());
        newGeoBox.setRight(currentGeoBox.right());
    }

    if (newGeoBox.height() < qMax(minimumSize, minSize.height()) || newGeoBox.height() > maxSize.height()) {
        newGeoBox.setTop(currentGeoBox.top());
        newGeoBox.setBottom(currentGeoBox.bottom());
    }
    grabbedView->geometry.setTopLeft(newGeoBox.topLeft().toPoint());

    grabbedView->sceneTree->setPosition(grabbedView->geometry.topLeft());
    grabbedView->xdgToplevel->setSize(newGeoBox.size().toSize());
}

QWSeat *QBoxCursor::getSeat()
{
    return m_service->seat->m_seat;
}
