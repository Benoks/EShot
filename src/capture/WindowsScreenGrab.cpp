#include "WindowsScreenGrab.h"

#ifdef Q_OS_WIN
#include <QDebug>
#include <QPainter>

#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>

namespace {

template <typename T>
void releaseCom(T *&pointer)
{
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}

// Copies one output's current frame into canvas. Returns false when the
// output could not be duplicated.
bool duplicateOutput(ID3D11Device *device, ID3D11DeviceContext *context, IDXGIOutput *output,
                     const DXGI_OUTPUT_DESC &description, const QRect &physicalRect,
                     QImage *canvas)
{
    IDXGIOutput1 *output1 = nullptr;
    if (FAILED(output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void **>(&output1))))
        return false;
    IDXGIOutputDuplication *duplication = nullptr;
    HRESULT hr = output1->DuplicateOutput(device, &duplication);
    releaseCom(output1);
    if (FAILED(hr)) {
        qWarning() << "[WindowsScreenGrab] DuplicateOutput failed: 0x" << Qt::hex << ulong(hr);
        return false;
    }

    bool ok = false;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
    IDXGIResource *resource = nullptr;
    // The first frame after DuplicateOutput always holds the whole desktop.
    hr = duplication->AcquireNextFrame(500, &frameInfo, &resource);
    if (SUCCEEDED(hr)) {
        ID3D11Texture2D *frame = nullptr;
        if (SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),
                                               reinterpret_cast<void **>(&frame)))) {
            D3D11_TEXTURE2D_DESC textureDescription = {};
            frame->GetDesc(&textureDescription);
            textureDescription.Usage = D3D11_USAGE_STAGING;
            textureDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            textureDescription.BindFlags = 0;
            textureDescription.MiscFlags = 0;
            textureDescription.MipLevels = 1;
            textureDescription.ArraySize = 1;
            textureDescription.SampleDesc.Count = 1;
            textureDescription.SampleDesc.Quality = 0;
            ID3D11Texture2D *staging = nullptr;
            if (textureDescription.Format == DXGI_FORMAT_B8G8R8A8_UNORM
                && SUCCEEDED(device->CreateTexture2D(&textureDescription, nullptr, &staging))) {
                context->CopyResource(staging, frame);
                D3D11_MAPPED_SUBRESOURCE mapped = {};
                if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
                    const QImage outputImage(static_cast<const uchar *>(mapped.pData),
                                             int(textureDescription.Width),
                                             int(textureDescription.Height),
                                             int(mapped.RowPitch), QImage::Format_RGB32);
                    const RECT &desktop = description.DesktopCoordinates;
                    const QRect outputRect(desktop.left, desktop.top,
                                           desktop.right - desktop.left,
                                           desktop.bottom - desktop.top);
                    QPainter painter(canvas);
                    painter.setCompositionMode(QPainter::CompositionMode_Source);
                    painter.drawImage(outputRect.topLeft() - physicalRect.topLeft(), outputImage);
                    painter.end();
                    context->Unmap(staging, 0);
                    ok = true;
                }
                releaseCom(staging);
            }
            releaseCom(frame);
        }
        releaseCom(resource);
        duplication->ReleaseFrame();
    } else {
        qWarning() << "[WindowsScreenGrab] AcquireNextFrame failed: 0x" << Qt::hex << ulong(hr);
    }
    releaseCom(duplication);
    return ok;
}

}

bool windowsExclusiveFullscreenRunning()
{
    QUERY_USER_NOTIFICATION_STATE state = QUNS_ACCEPTS_NOTIFICATIONS;
    return SUCCEEDED(SHQueryUserNotificationState(&state))
        && state == QUNS_RUNNING_D3D_FULL_SCREEN;
}

QImage grabWindowsDesktopGdi(const QRect &physicalRect)
{
    if (physicalRect.isEmpty())
        return {};
    HDC screen = GetDC(nullptr);
    if (!screen)
        return {};
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = memory
        ? CreateCompatibleBitmap(screen, physicalRect.width(), physicalRect.height()) : nullptr;
    QImage image;
    if (bitmap) {
        HGDIOBJ previous = SelectObject(memory, bitmap);
        // No CAPTUREBLT: DWM already composes layered windows into the screen
        // DC, and CAPTUREBLT makes the mouse cursor flicker.
        const BOOL copied = BitBlt(memory, 0, 0, physicalRect.width(), physicalRect.height(),
                                   screen, physicalRect.x(), physicalRect.y(), SRCCOPY);
        SelectObject(memory, previous);

        BITMAPINFOHEADER header = {};
        header.biSize = sizeof(header);
        header.biWidth = physicalRect.width();
        header.biHeight = -physicalRect.height();
        header.biPlanes = 1;
        header.biBitCount = 32;
        header.biCompression = BI_RGB;
        QImage pixels(physicalRect.size(), QImage::Format_RGB32);
        const int lines = copied && !pixels.isNull()
            ? GetDIBits(memory, bitmap, 0, UINT(physicalRect.height()), pixels.bits(),
                        reinterpret_cast<BITMAPINFO *>(&header), DIB_RGB_COLORS)
            : 0;
        if (copied && lines == physicalRect.height()) {
            image = pixels;
        } else {
            qWarning() << "[WindowsScreenGrab] GDI capture failed: BitBlt=" << bool(copied)
                       << "lines=" << lines << "of" << physicalRect.height()
                       << "error=" << GetLastError();
        }
        DeleteObject(bitmap);
    }
    if (memory)
        DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return image;
}

QImage grabWindowsDesktopDuplication(const QRect &physicalRect)
{
    if (physicalRect.isEmpty())
        return {};
    IDXGIFactory1 *factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory))))
        return {};

    QImage canvas(physicalRect.size(), QImage::Format_RGB32);
    if (canvas.isNull()) {
        releaseCom(factory);
        return {};
    }
    canvas.fill(Qt::black);
    bool capturedAny = false;
    bool capturedAll = true;
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT adapterIndex = 0;
         factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
        ID3D11Device *device = nullptr;
        ID3D11DeviceContext *context = nullptr;
        const HRESULT created = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                                  nullptr, 0, D3D11_SDK_VERSION,
                                                  &device, nullptr, &context);
        IDXGIOutput *output = nullptr;
        for (UINT outputIndex = 0;
             SUCCEEDED(created)
             && adapter->EnumOutputs(outputIndex, &output) != DXGI_ERROR_NOT_FOUND;
             ++outputIndex) {
            DXGI_OUTPUT_DESC description = {};
            if (SUCCEEDED(output->GetDesc(&description)) && description.AttachedToDesktop) {
                const RECT &desktop = description.DesktopCoordinates;
                const QRect outputRect(desktop.left, desktop.top, desktop.right - desktop.left,
                                       desktop.bottom - desktop.top);
                if (outputRect.intersects(physicalRect)) {
                    // Rotated outputs deliver unrotated frames; leave them to GDI.
                    const bool rotated = description.Rotation != DXGI_MODE_ROTATION_IDENTITY
                        && description.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED;
                    if (!rotated && duplicateOutput(device, context, output, description,
                                                    physicalRect, &canvas)) {
                        capturedAny = true;
                    } else {
                        capturedAll = false;
                    }
                }
            }
            releaseCom(output);
        }
        releaseCom(context);
        releaseCom(device);
        releaseCom(adapter);
    }
    releaseCom(factory);
    if (!capturedAny || !capturedAll)
        return {};
    return canvas;
}

#else

bool windowsExclusiveFullscreenRunning()
{
    return false;
}

QImage grabWindowsDesktopGdi(const QRect &)
{
    return {};
}

QImage grabWindowsDesktopDuplication(const QRect &)
{
    return {};
}

#endif
