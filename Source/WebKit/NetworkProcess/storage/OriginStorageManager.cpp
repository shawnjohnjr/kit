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
#include "OriginStorageManager.h"

namespace WebKit {

enum class OriginStorageManager::StorageBucketMode : bool { BestEffort, Persistent };

class OriginStorageManager::StorageBucket {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StorageBucket(const String& rootPath, const String& identifier)
        : m_rootPath(rootPath)
        , m_identifier(identifier)
    {
    }
    StorageBucketMode mode() const { return m_mode; }
    void setMode(StorageBucketMode mode) { m_mode = mode; }
private:
    String m_rootPath;
    String m_identifier;
    StorageBucketMode m_mode { StorageBucketMode::BestEffort };
};

OriginStorageManager::OriginStorageManager(String&& path)
    : m_path(WTFMove(path))
{
}

OriginStorageManager::~OriginStorageManager() = default;

void OriginStorageManager::persist()
{
    m_persisted = true;
    
    if (!m_defaultBucket)
        m_defaultBucket = makeUnique<StorageBucket>(m_path, "default"_s);

    m_defaultBucket->setMode(StorageBucketMode::Persistent);
}

} // namespace WebKit

