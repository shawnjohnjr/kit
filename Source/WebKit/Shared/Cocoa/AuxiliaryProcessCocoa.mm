/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AuxiliaryProcess.h"

#import "OSStateSPI.h"
#import "WKCrashReporter.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/FloatingPointEnvironment.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <mach/task.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebKit {

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
static void initializeTimerCoalescingPolicy()
{
    // Set task_latency and task_throughput QOS tiers as appropriate for a visible application.
    struct task_qos_policy qosinfo = { LATENCY_QOS_TIER_0, THROUGHPUT_QOS_TIER_0 };
    kern_return_t kr = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
}
#endif

void AuxiliaryProcess::platformInitialize(const AuxiliaryProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    initializeTimerCoalescingPolicy();
#endif

    FloatingPointEnvironment& floatingPointEnvironment = FloatingPointEnvironment::singleton();
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    floatingPointEnvironment.enableDenormalSupport();
#endif
    floatingPointEnvironment.saveMainThreadEnvironment();

    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];

    WebCore::setApplicationBundleIdentifier(parameters.clientBundleIdentifier);
    setApplicationSDKVersion(parameters.clientSDKVersion);
}

void AuxiliaryProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName)
{
    auto errorMessage = makeString("Received invalid message: '", description(messageName), "' (", messageName, ')');
    logAndSetCrashLogMessage(errorMessage.utf8().data());
    CRASH_WITH_INFO(WTF::enumToUnderlyingType(messageName));
}

bool AuxiliaryProcess::parentProcessHasEntitlement(const char* entitlement)
{
    return WTF::hasEntitlement(m_connection->xpcConnection(), entitlement);
}

void AuxiliaryProcess::platformStopRunLoop()
{
    XPCServiceExit(WTFMove(m_priorityBoostMessage));
}

#if USE(OS_STATE)

void AuxiliaryProcess::registerWithStateDumper(ASCIILiteral title)
{
    os_state_add_handler(dispatch_get_main_queue(), [this, title] (os_state_hints_t hints) {
        @autoreleasepool {
            os_state_data_t os_state = nullptr;

            // Only gather state on faults and sysdiagnose. It's overkill for
            // general error messages.
            if (hints->osh_api == OS_STATE_API_ERROR)
                return os_state;

            auto stateDictionary = additionalStateForDiagnosticReport();

            // Submitting an empty process state object may provide an
            // indication of the existance of private sessions, which we'd like
            // to hide, so don't return empty dictionaries.
            if (![stateDictionary count])
                return os_state;

            // Serialize the accumulated process state so that we can put the
            // result in an os_state_data_t structure.
            NSError *error = nil;
            auto data = [NSPropertyListSerialization dataWithPropertyList:stateDictionary.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];

            if (!data) {
                ASSERT(data);
                return os_state;
            }

            auto neededSize = OS_STATE_DATA_SIZE_NEEDED(data.length);
            os_state = (os_state_data_t)malloc(neededSize);
            if (os_state) {
                memset(os_state, 0, neededSize);
                os_state->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
                os_state->osd_data_size = data.length;
                strlcpy(os_state->osd_title, title.characters(), sizeof(os_state->osd_title));
                memcpy(os_state->osd_data, data.bytes, data.length);
            }

            return os_state;
        }
    });
}

#endif // USE(OS_STATE)

#if ENABLE(CFPREFS_DIRECT_MODE)
id AuxiliaryProcess::decodePreferenceValue(const std::optional<String>& encodedValue)
{
    if (!encodedValue)
        return nil;
    
    auto encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:*encodedValue options:0]);
    if (!encodedData)
        return nil;
    NSError *err = nil;
    auto classes = [NSSet setWithArray:@[[NSString class], [NSNumber class], [NSDate class], [NSDictionary class], [NSArray class], [NSData class]]];
    id value = [NSKeyedUnarchiver unarchivedObjectOfClasses:classes fromData:encodedData.get() error:&err];
    ASSERT(!err);
    if (err)
        return nil;

    return value;
}

void AuxiliaryProcess::setPreferenceValue(const String& domain, const String& key, id value)
{
    if (domain.isEmpty()) {
        CFPreferencesSetValue(key.createCFString().get(), (__bridge CFPropertyListRef)value, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
#if ASSERT_ENABLED
        id valueAfterSetting = [[NSUserDefaults standardUserDefaults] objectForKey:key];
        ASSERT(valueAfterSetting == value || [valueAfterSetting isEqual:value] || key == "AppleLanguages");
#endif
    } else
        CFPreferencesSetValue(key.createCFString().get(), (__bridge CFPropertyListRef)value, domain.createCFString().get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}
#endif // ENABLE(CFPREFS_DIRECT_MODE)

} // namespace WebKit
