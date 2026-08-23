#include "WindowsWindowProvider.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

QRect overlayLocalWindowRect(const QRect &physicalWindowRect,
                             const QVector<CaptureMonitorGeometry> &monitors,
                             const QRect &virtualDesktopRect)
{
    if (!physicalWindowRect.isValid() || !virtualDesktopRect.isValid())
        return {};

    const QRect globalLogical = displayRectFromPhysical(physicalWindowRect, monitors);
    const QRect local = globalLogical.translated(-virtualDesktopRect.topLeft());
    return local.intersected(QRect(QPoint(0, 0), virtualDesktopRect.size()));
}

QVector<QRect> windowsForCaptureOverlay(quintptr overlayWindowId,
                                        const QVector<CaptureMonitorGeometry> &monitors,
                                        const QRect &virtualDesktopRect)
{
#ifdef Q_OS_WIN
    struct EnumerationContext {
        HWND overlay = nullptr;
        DWORD processId = 0;
        const QVector<CaptureMonitorGeometry> *monitors = nullptr;
        const QRect *virtualDesktopRect = nullptr;
        QVector<QRect> windows;
    } context;

    context.overlay = reinterpret_cast<HWND>(overlayWindowId);
    context.processId = GetCurrentProcessId();
    context.monitors = &monitors;
    context.virtualDesktopRect = &virtualDesktopRect;

    EnumWindows([](HWND hwnd, LPARAM parameter) -> BOOL {
        auto *ctx = reinterpret_cast<EnumerationContext *>(parameter);
        if (!ctx || hwnd == ctx->overlay || !IsWindowVisible(hwnd) || IsIconic(hwnd))
            return TRUE;

        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        if (processId == ctx->processId)
            return TRUE;

        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED,
                                            &cloaked, sizeof(cloaked))) && cloaked != 0) {
            return TRUE;
        }

        const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (((exStyle & WS_EX_TOOLWINDOW) && (exStyle & WS_EX_NOACTIVATE))
            || (exStyle & WS_EX_TRANSPARENT))
            return TRUE;

        wchar_t className[128] = {};
        GetClassNameW(hwnd, className, 128);
        if (lstrcmpW(className, L"Progman") == 0
            || lstrcmpW(className, L"WorkerW") == 0
            || lstrcmpW(className, L"Shell_TrayWnd") == 0
            || lstrcmpW(className, L"Shell_SecondaryTrayWnd") == 0) {
            return TRUE;
        }

        RECT nativeRect = {};
        HRESULT frameResult = DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS,
                                                    &nativeRect, sizeof(nativeRect));
        if (FAILED(frameResult) || nativeRect.right <= nativeRect.left
            || nativeRect.bottom <= nativeRect.top) {
            if (!GetWindowRect(hwnd, &nativeRect))
                return TRUE;
        }

        const QRect physical(nativeRect.left, nativeRect.top,
                             nativeRect.right - nativeRect.left,
                             nativeRect.bottom - nativeRect.top);
        if (physical.width() < 16 || physical.height() < 16)
            return TRUE;

        const QRect local = overlayLocalWindowRect(physical, *ctx->monitors,
                                                   *ctx->virtualDesktopRect);
        if (local.width() >= 16 && local.height() >= 16)
            ctx->windows.append(local);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));

    return context.windows;
#else
    Q_UNUSED(overlayWindowId)
    Q_UNUSED(monitors)
    Q_UNUSED(virtualDesktopRect)
    return {};
#endif
}
