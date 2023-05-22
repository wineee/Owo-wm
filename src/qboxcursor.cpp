#include "qboxcursor.h"
#include "qboxserver.h"
#include <qwseat.h>

QBoxCursor::QBoxCursor(QBoxServer *server):
    m_service(server),
    QObject(nullptr)
{
    m_cursor = new QWCursor(server);
    m_cursor->attachOutputLayout(server->outputLayout);
    m_cursorManager = QWXCursorManager::create(nullptr, 24);
    m_cursorManager->load(1);
    connect(m_cursor, &QWCursor::motion, this, &QBoxCursor::onCursorMotion);
    connect(m_cursor, &QWCursor::motionAbsolute, this, &QBoxCursor::onCursorMotionAbsolute);
    connect(m_cursor, &QWCursor::button, this, &QBoxCursor::onCursorButton);
    connect(m_cursor, &QWCursor::axis, this, &QBoxCursor::onCursorAxis);
    connect(m_cursor, &QWCursor::frame, this, &QBoxCursor::onCursorFrame);
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
    getSeat()->pointerNotifyButton(event->time_msec, event->button, event->state);
    QPointF spos;
    wlr_surface *surface = nullptr;
    auto view = m_service->viewAt(m_cursor->position(), &surface, &spos);
    if (event->state == WLR_BUTTON_RELEASED) {
        cursorState = CursorState::Normal;
    } else {
        m_service->focusView(view, surface);
    }
}

void QBoxCursor::onCursorAxis(wlr_pointer_axis_event *event)
{
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
    auto *grabbedView = m_service->grabbedView;
    QRectF grabGeoBox = m_service->grabGeoBox;
    if (cursorState == CursorState::MovingWindow) {
        grabbedView->geometry.setTopLeft(
                    (grabGeoBox.topLeft()
                    + (m_cursor->position() - m_service->grabCursorPos)).toPoint());
        grabbedView->sceneTree->setPosition(grabbedView->geometry.topLeft());
        return;
    } else if (cursorState == CursorState::ResizingWindow) {
        const QPointF &cursorPos = m_cursor->position();
        QRectF newGeoBox = grabGeoBox;
        const int minimumSize = 10;

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
        return;
    }

    wlr_surface *surface = nullptr;
    QPointF spos;
    auto view = m_service->viewAt(m_cursor->position(), &surface, &spos);
    if (!view)
        m_cursorManager->setCursor("left_ptr", m_cursor);

    if (surface) {
        getSeat()->pointerNotifyEnter(surface, spos.x(), spos.y());
        getSeat()->pointerNotifyMotion(time, spos.x(), spos.y());
    } else {
        getSeat()->pointerClearFocus();
    }
}

QWSeat *QBoxCursor::getSeat()
{
    return m_service->seat->m_seat;
}
