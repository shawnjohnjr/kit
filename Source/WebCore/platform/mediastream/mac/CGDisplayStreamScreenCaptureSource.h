/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#include "CGDisplayStreamCaptureSource.h"
#include "IOSurface.h"
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <CoreGraphics/CGDisplayStream.h>
#include <wtf/Lock.h>
#include <wtf/OSObjectPtr.h>

typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CGDisplayStreamScreenCaptureSource final : public CGDisplayStreamCaptureSource {
public:
    static Expected<UniqueRef<DisplayCaptureSourceMac::Capturer>, String> create(const String&);

    explicit CGDisplayStreamScreenCaptureSource(uint32_t);
    ~CGDisplayStreamScreenCaptureSource() = default;

    static std::optional<CaptureDevice> screenCaptureDeviceWithPersistentID(const String&);
    static void screenCaptureDevices(Vector<CaptureDevice>&);

private:

    // DisplayCaptureSourceMac::Capturer
    CaptureDevice::DeviceType deviceType() const final { return CaptureDevice::DeviceType::Screen; }
    RealtimeMediaSourceSettings::DisplaySurfaceType surfaceType() const final { return RealtimeMediaSourceSettings::DisplaySurfaceType::Monitor; }
    IntSize intrinsicSize() const final;
#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "CGDisplayStreamScreenCaptureSource"; }
#endif

    // CGDisplayStreamCaptureSource
    RetainPtr<CGDisplayStreamRef> createDisplayStream(FrameAvailableCallback, dispatch_queue_t) final;
    bool checkDisplayStream() final;

    class DisplaySurface {
    public:
        DisplaySurface() = default;
        explicit DisplaySurface(IOSurfaceRef surface)
            : m_surface(surface)
        {
            if (m_surface)
                IOSurfaceIncrementUseCount(m_surface.get());
        }

        ~DisplaySurface()
        {
            if (m_surface)
                IOSurfaceDecrementUseCount(m_surface.get());
        }

        DisplaySurface& operator=(IOSurfaceRef surface)
        {
            if (m_surface)
                IOSurfaceDecrementUseCount(m_surface.get());
            if (surface)
                IOSurfaceIncrementUseCount(surface);
            m_surface = surface;
            return *this;
        }

        IOSurfaceRef ioSurface() const { return m_surface.get(); }

    private:
        RetainPtr<IOSurfaceRef> m_surface;
    };

    RetainPtr<CGDisplayStreamRef> m_displayStream;

    uint32_t m_displayID { 0 };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
