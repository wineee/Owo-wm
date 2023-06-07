#include "qboxseat.h"
#include "qboxserver.h"

#include <qwbackend.h>
#include <qwcursor.h>
#include <QCoreApplication>

QBoxSeat::QBoxSeat(QBoxServer *server):
    m_server(server),
    QObject(server)
{
    connect(server->backend, &QWBackend::newInput, this, &QBoxSeat::onNewInput);

    m_seat = QWSeat::create(server->display, "seat0");
    connect(m_seat, &QWSeat::requestSetCursor, this, &QBoxSeat::onRequestSetCursor);
    connect(m_seat, &QWSeat::requestSetSelection, this, &QBoxSeat::onRequestSetSelection);
}


void QBoxSeat::onRequestSetCursor(wlr_seat_pointer_request_set_cursor_event *event)
{
    if (m_seat->handle()->pointer_state.focused_client == event->seat_client)
        m_server->cursor->m_cursor->setSurface(QWSurface::from(event->surface), QPoint(event->hotspot_x, event->hotspot_y));
}

void QBoxSeat::onRequestSetSelection(wlr_seat_request_set_selection_event *event)
{
    m_seat->setSelection(event->source, event->serial);
}

void QBoxSeat::onNewInput(QWInputDevice *device)
{
    if (QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(device)) {

        xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        xkb_keymap *keymap = xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

        keyboard->setKeymap(keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        keyboard->setRepeatInfo(25, 600);

        connect(keyboard, &QWKeyboard::modifiers, this, &QBoxSeat::onKeyboardModifiers);
        connect(keyboard, &QWKeyboard::key, this, &QBoxSeat::onKeyboardKey);
        connect(keyboard, &QWKeyboard::destroyed, this, &QBoxSeat::onKeyboardDestroy);

        m_seat->setKeyboard(keyboard);

        Q_ASSERT(!m_keyboards.contains(keyboard));
        m_keyboards.append(keyboard);
    } else if (device->handle()->type == WLR_INPUT_DEVICE_POINTER) {
        Q_ASSERT(m_server);
        Q_ASSERT(m_server->cursor);
        Q_ASSERT(m_server->cursor->m_cursor);
        m_server->cursor->m_cursor->attachInputDevice(device);
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!m_keyboards.isEmpty()) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    m_seat->setCapabilities(caps);
}

void QBoxSeat::onKeyboardModifiers()
{
    QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(QObject::sender());
    m_seat->setKeyboard(keyboard);
    m_seat->keyboardNotifyModifiers(&keyboard->handle()->modifiers);
}

void QBoxSeat::onKeyboardKey(wlr_keyboard_key_event *event)
{
    QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(QObject::sender());
    /* Translate libinput keycode -> xkbcommon */
    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->handle()->xkb_state, keycode, &syms);

    bool handled = false;
    uint32_t modifiers = keyboard->getModifiers();
    if ((modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_CTRL))
            && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int i = 0; i < nsyms; i++)
            handled = handleKeybinding(syms[i]);
    }

    if (!handled) {
        m_seat->setKeyboard(keyboard);
        m_seat->keyboardNotifyKey(event->time_msec, event->keycode, event->state);
    }
}

void QBoxSeat::onKeyboardDestroy()
{
    QWKeyboard *keyboard = qobject_cast<QWKeyboard*>(QObject::sender());
    m_keyboards.removeOne(keyboard);
}

bool QBoxSeat::handleKeybinding(xkb_keysym_t sym)
{
    auto *xdgShell = m_server->xdgShell;
    switch (sym) {
    case XKB_KEY_Escape:
        m_server->display->terminate();
        qApp->exit();
        break;
    case XKB_KEY_F1:
        if (m_server->views.size() < 2)
            break;
        xdgShell->focusView(m_server->views.at(1), m_server->views.at(1)->xdgToplevel->handle()->base->surface);
        break;
    default:
        return false;
    }
    return true;
}
