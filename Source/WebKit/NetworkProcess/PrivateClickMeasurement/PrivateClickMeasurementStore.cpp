/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PrivateClickMeasurementStore.h"

#include "PrivateClickMeasurementDatabase.h"
#include "PrivateClickMeasurementDebugInfo.h"
#include "PrivateClickMeasurementManager.h"
#include <WebCore/PrivateClickMeasurement.h>
#include <wtf/RunLoop.h>
#include <wtf/SuspendableWorkQueue.h>

namespace WebKit {

namespace PCM {

static Ref<SuspendableWorkQueue> sharedWorkQueue()
{
    static NeverDestroyed<Ref<SuspendableWorkQueue>> queue(SuspendableWorkQueue::create("PrivateClickMeasurement Process Data Queue",  WorkQueue::QOS::Utility));
    return queue.get().copyRef();
}

void Store::prepareForProcessToSuspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sharedWorkQueue()->suspend(Database::interruptAllDatabases, WTFMove(completionHandler));
}

void Store::processDidResume()
{
    ASSERT(RunLoop::isMain());
    sharedWorkQueue()->resume();
}

Store::Store(const String& databaseDirectory)
    : m_queue(sharedWorkQueue())
{
    if (!databaseDirectory.isEmpty()) {
        postTask([this, protectedThis = makeRef(*this), databaseDirectory = databaseDirectory.isolatedCopy()] () mutable {
            m_database = makeUnique<Database>(WTFMove(databaseDirectory));
        });
    }
}

Store::~Store() = default;

void Store::postTask(Function<void()>&& task) const
{
    ASSERT(RunLoop::isMain());
    m_queue->dispatch(WTFMove(task));
}

void Store::postTaskReply(WTF::Function<void()>&& reply) const
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch(WTFMove(reply));
}

void Store::insertPrivateClickMeasurement(WebCore::PrivateClickMeasurement&& attribution, PrivateClickMeasurementAttributionType attributionType)
{
    postTask([this, protectedThis = makeRef(*this), attribution = WTFMove(attribution), attributionType] () mutable {
        if (m_database)
            m_database->insertPrivateClickMeasurement(WTFMove(attribution), attributionType);
    });
}

void Store::markAllUnattributedPrivateClickMeasurementAsExpiredForTesting()
{
    postTask([this, protectedThis = makeRef(*this)] {
        if (m_database)
            m_database->markAllUnattributedPrivateClickMeasurementAsExpiredForTesting();
    });
}

void Store::attributePrivateClickMeasurement(const WebCore::PrivateClickMeasurement::SourceSite& sourceSite, const WebCore::PrivateClickMeasurement::AttributionDestinationSite& destinationSite, WebCore::PrivateClickMeasurement::AttributionTriggerData&& attributionTriggerData, std::optional<WebCore::PrivateClickMeasurement>&& ephemeralMeasurement, CompletionHandler<void(std::optional<WebCore::PrivateClickMeasurement::AttributionSecondsUntilSendData>&&, DebugInfo&&)>&& completionHandler)
{
    postTask([this, protectedThis = makeRef(*this), sourceSite = sourceSite.isolatedCopy(), destinationSite = destinationSite.isolatedCopy(), attributionTriggerData = WTFMove(attributionTriggerData), ephemeralMeasurement = crossThreadCopy(ephemeralMeasurement), completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_database) {
            return postTaskReply([completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(std::nullopt, { });
            });
        }

        // Insert ephemeral measurement right before attribution.
        if (ephemeralMeasurement) {
            RELEASE_ASSERT(ephemeralMeasurement->isEphemeral());
            m_database->insertPrivateClickMeasurement(WTFMove(*ephemeralMeasurement), PrivateClickMeasurementAttributionType::Unattributed);
        }

        auto [seconds, debugInfo] = m_database->attributePrivateClickMeasurement(sourceSite, destinationSite, WTFMove(attributionTriggerData));

        postTaskReply([seconds = WTFMove(seconds), debugInfo = debugInfo.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(seconds), WTFMove(debugInfo));
        });
    });
}

void Store::privateClickMeasurementToStringForTesting(CompletionHandler<void(String)>&& completionHandler) const
{
    postTask([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        String result;
        if (m_database)
            result = m_database->privateClickMeasurementToStringForTesting();
        postTaskReply([result = result.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(result);
        });
    });
}

void Store::allAttributedPrivateClickMeasurement(CompletionHandler<void(Vector<WebCore::PrivateClickMeasurement>&&)>&& completionHandler)
{
    postTask([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        Vector<WebCore::PrivateClickMeasurement> convertedAttributions;
        if (m_database)
            convertedAttributions = m_database->allAttributedPrivateClickMeasurement();
        postTaskReply([convertedAttributions = convertedAttributions.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(convertedAttributions));
        });
    });
}

void Store::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_database)
            m_database->markAttributedPrivateClickMeasurementsAsExpiredForTesting();
        postTaskReply(WTFMove(completionHandler));
    });
}

void Store::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] () mutable {
        if (m_database)
            m_database->clearPrivateClickMeasurement(std::nullopt);
        postTaskReply(WTFMove(completionHandler));
    });
}

void Store::clearPrivateClickMeasurementForRegistrableDomain(const WebCore::RegistrableDomain& domain, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = makeRef(*this), domain = domain.isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
        if (m_database)
            m_database->clearPrivateClickMeasurement(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void Store::clearExpiredPrivateClickMeasurement()
{
    postTask([this, protectedThis = makeRef(*this)]() {
        if (m_database)
            m_database->clearExpiredPrivateClickMeasurement();
    });
}

void Store::clearSentAttribution(WebCore::PrivateClickMeasurement&& attributionToClear, WebCore::PrivateClickMeasurement::AttributionReportEndpoint attributionReportEndpoint)
{
    postTask([this, protectedThis = makeRef(*this), attributionToClear = attributionToClear.isolatedCopy(), attributionReportEndpoint]() mutable {
        if (m_database)
            m_database->clearSentAttribution(WTFMove(attributionToClear), attributionReportEndpoint);
    });
}

void Store::close(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] () mutable {
        m_database = nullptr;
        postTaskReply(WTFMove(completionHandler));
    });
}

} // namespace PCM

} // namespace WebKit
