#ifndef HOTKEYSEQUENCE_H
#define HOTKEYSEQUENCE_H

#include <QKeySequence>
#include "PlatformHotkey.h"

// Converts the first chord of a Qt key sequence to Win32 MOD_* / VK_* values.
// Returns false for sequences that cannot be used as a global hotkey.
bool keySequenceToWin32(const QKeySequence &seq, UINT &modifiers, UINT &vkey);
// Converts Win32 MOD_* / VK_* values back to a Qt key sequence. A zero vkey
// yields an empty sequence; unknown keys fall back to Print.
QKeySequence win32ToKeySequence(UINT modifiers, UINT vkey);

#endif
