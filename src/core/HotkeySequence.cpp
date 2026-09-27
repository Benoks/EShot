#include "HotkeySequence.h"

bool keySequenceToWin32(const QKeySequence &seq, UINT &modifiers, UINT &vkey)
{
    if (seq.isEmpty()) return false;

    QKeyCombination combo = seq[0];
    Qt::KeyboardModifiers qtMod = combo.keyboardModifiers();
    Qt::Key qtKey = combo.key();

    if (qtKey == Qt::Key_unknown ||
        qtKey == Qt::Key_Shift ||
        qtKey == Qt::Key_Control ||
        qtKey == Qt::Key_Alt ||
        qtKey == Qt::Key_Meta)
        return false;

    modifiers = 0;
    if (qtMod & Qt::ShiftModifier)   modifiers |= MOD_SHIFT;
    if (qtMod & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (qtMod & Qt::AltModifier)     modifiers |= MOD_ALT;
    if (qtMod & Qt::MetaModifier)    modifiers |= MOD_WIN;
    const bool keypad = qtMod.testFlag(Qt::KeypadModifier);
    qtMod &= ~Qt::KeypadModifier;

    struct { Qt::Key qt; UINT win; } mapping[] = {
        { Qt::Key_Print,      VK_SNAPSHOT },
        { Qt::Key_ScrollLock, VK_SCROLL }, { Qt::Key_Pause, VK_PAUSE },
        { Qt::Key_CapsLock,   VK_CAPITAL },{ Qt::Key_NumLock, VK_NUMLOCK },
        { Qt::Key_Menu,       VK_APPS },
        { Qt::Key_F1,         VK_F1 }, { Qt::Key_F2,  VK_F2  }, { Qt::Key_F3,  VK_F3  },
        { Qt::Key_F4,         VK_F4 }, { Qt::Key_F5,  VK_F5  }, { Qt::Key_F6,  VK_F6  },
        { Qt::Key_F7,         VK_F7 }, { Qt::Key_F8,  VK_F8  }, { Qt::Key_F9,  VK_F9  },
        { Qt::Key_F10,        VK_F10}, { Qt::Key_F11, VK_F11 }, { Qt::Key_F12, VK_F12 },
        { Qt::Key_F13,        VK_F13}, { Qt::Key_F14, VK_F14 }, { Qt::Key_F15, VK_F15 },
        { Qt::Key_F16,        VK_F16}, { Qt::Key_F17, VK_F17 }, { Qt::Key_F18, VK_F18 },
        { Qt::Key_F19,        VK_F19}, { Qt::Key_F20, VK_F20 }, { Qt::Key_F21, VK_F21 },
        { Qt::Key_F22,        VK_F22}, { Qt::Key_F23, VK_F23 }, { Qt::Key_F24, VK_F24 },
        { Qt::Key_Home,       VK_HOME },  { Qt::Key_End,    VK_END    },
        { Qt::Key_PageUp,     VK_PRIOR }, { Qt::Key_PageDown,VK_NEXT  },
        { Qt::Key_Insert,     VK_INSERT },{ Qt::Key_Delete,  VK_DELETE },
        { Qt::Key_Left,       VK_LEFT },  { Qt::Key_Right,   VK_RIGHT },
        { Qt::Key_Up,         VK_UP   },  { Qt::Key_Down,    VK_DOWN  },
        { Qt::Key_Space,      VK_SPACE }, { Qt::Key_Return,  VK_RETURN},
        { Qt::Key_Escape,     VK_ESCAPE },{ Qt::Key_Tab,     VK_TAB   },
        { Qt::Key_Backspace,  VK_BACK  },
    };
    for (auto &m : mapping) {
        if (qtKey == m.qt) { vkey = m.win; return true; }
    }

    if (keypad) {
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
            vkey = VK_NUMPAD0 + (qtKey - Qt::Key_0);
            return true;
        }
        if (qtKey == Qt::Key_Plus)     { vkey = VK_ADD; return true; }
        if (qtKey == Qt::Key_Minus)    { vkey = VK_SUBTRACT; return true; }
        if (qtKey == Qt::Key_Asterisk) { vkey = VK_MULTIPLY; return true; }
        if (qtKey == Qt::Key_Slash)    { vkey = VK_DIVIDE; return true; }
        if (qtKey == Qt::Key_Period)   { vkey = VK_DECIMAL; return true; }
        if (qtKey == Qt::Key_Enter || qtKey == Qt::Key_Return) { vkey = VK_RETURN; return true; }
    }

    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        if (qtMod == Qt::NoModifier) return false;
        vkey = 'A' + (qtKey - Qt::Key_A);
        return true;
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        if (qtMod == Qt::NoModifier) return false;
        vkey = '0' + (qtKey - Qt::Key_0);
        return true;
    }
    return false;
}

QKeySequence win32ToKeySequence(UINT modifiers, UINT vkey)
{
    if (vkey == 0) {
        Q_UNUSED(modifiers);
        return QKeySequence();
    }
    Qt::KeyboardModifiers qtMod = Qt::NoModifier;
    if (modifiers & MOD_SHIFT)   qtMod |= Qt::ShiftModifier;
    if (modifiers & MOD_CONTROL) qtMod |= Qt::ControlModifier;
    if (modifiers & MOD_ALT)     qtMod |= Qt::AltModifier;
    if (modifiers & MOD_WIN)     qtMod |= Qt::MetaModifier;

    struct { UINT win; Qt::Key qt; } mapping[] = {
        { VK_SNAPSHOT, Qt::Key_Print },
        { VK_SCROLL, Qt::Key_ScrollLock }, { VK_PAUSE, Qt::Key_Pause },
        { VK_CAPITAL, Qt::Key_CapsLock },  { VK_NUMLOCK, Qt::Key_NumLock },
        { VK_APPS, Qt::Key_Menu },
        { VK_F1,  Qt::Key_F1  }, { VK_F2,  Qt::Key_F2  }, { VK_F3,  Qt::Key_F3  },
        { VK_F4,  Qt::Key_F4  }, { VK_F5,  Qt::Key_F5  }, { VK_F6,  Qt::Key_F6  },
        { VK_F7,  Qt::Key_F7  }, { VK_F8,  Qt::Key_F8  }, { VK_F9,  Qt::Key_F9  },
        { VK_F10, Qt::Key_F10 }, { VK_F11, Qt::Key_F11 }, { VK_F12, Qt::Key_F12 },
        { VK_F13, Qt::Key_F13 }, { VK_F14, Qt::Key_F14 }, { VK_F15, Qt::Key_F15 },
        { VK_F16, Qt::Key_F16 }, { VK_F17, Qt::Key_F17 }, { VK_F18, Qt::Key_F18 },
        { VK_F19, Qt::Key_F19 }, { VK_F20, Qt::Key_F20 }, { VK_F21, Qt::Key_F21 },
        { VK_F22, Qt::Key_F22 }, { VK_F23, Qt::Key_F23 }, { VK_F24, Qt::Key_F24 },
        { VK_HOME,   Qt::Key_Home   }, { VK_END,    Qt::Key_End      },
        { VK_PRIOR,  Qt::Key_PageUp }, { VK_NEXT,   Qt::Key_PageDown  },
        { VK_INSERT, Qt::Key_Insert }, { VK_DELETE, Qt::Key_Delete    },
        { VK_LEFT,   Qt::Key_Left   }, { VK_RIGHT,  Qt::Key_Right     },
        { VK_UP,     Qt::Key_Up     }, { VK_DOWN,   Qt::Key_Down      },
        { VK_SPACE,  Qt::Key_Space  }, { VK_RETURN, Qt::Key_Return    },
        { VK_ESCAPE, Qt::Key_Escape }, { VK_TAB,    Qt::Key_Tab       },
        { VK_BACK,   Qt::Key_Backspace },
    };
    for (auto &m : mapping) {
        if (vkey == m.win) return QKeySequence(QKeyCombination(qtMod, m.qt));
    }
    if (vkey >= 'A' && vkey <= 'Z') {
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_A + (vkey - 'A'))));
    }
    if (vkey >= '0' && vkey <= '9') {
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_0 + (vkey - '0'))));
    }
    if (vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9) {
        return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, static_cast<Qt::Key>(Qt::Key_0 + (vkey - VK_NUMPAD0))));
    }
    if (vkey == VK_ADD)      return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Plus));
    if (vkey == VK_SUBTRACT) return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Minus));
    if (vkey == VK_MULTIPLY) return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Asterisk));
    if (vkey == VK_DIVIDE)   return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Slash));
    if (vkey == VK_DECIMAL)  return QKeySequence(QKeyCombination(qtMod | Qt::KeypadModifier, Qt::Key_Period));
    return QKeySequence(Qt::Key_Print);
}
