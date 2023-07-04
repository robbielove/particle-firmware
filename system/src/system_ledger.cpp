/*
 * Copyright (c) 2023 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <memory>
#include <mutex>

#include "system_ledger.h"

#include "system_ledger_internal.h"
#include "system_threading.h"
#include "check.h"

using namespace particle;
using namespace particle::system;

int ledger_get_instance(ledger_instance** ledger, const char* name, void* reserved) {
    RefCountPtr<Ledger> lr;
    CHECK(LedgerManager::instance()->getLedger(name, lr));
    *ledger = reinterpret_cast<ledger_instance*>(lr.unwrap()); // Transfer ownership to the caller
    return 0;
}

void ledger_add_ref(ledger_instance* ledger, void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    lr->addRef();
}

void ledger_release(ledger_instance* ledger, void* reserved) {
    if (ledger) {
        auto lr = reinterpret_cast<Ledger*>(ledger);
        lr->release();
    }
}

void ledger_lock(ledger_instance* ledger, void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    lr->lock();
}

void ledger_unlock(ledger_instance* ledger, void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    lr->unlock();
}

void ledger_set_callbacks(ledger_instance* ledger, const ledger_callbacks* callbacks, void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    lr->setSyncCallback(callbacks ? callbacks->sync : nullptr);
}

void ledger_set_app_data(ledger_instance* ledger, void* appData, ledger_destroy_app_data_callback destroy,
        void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    lr->setAppData(appData, destroy);
}

void* ledger_get_app_data(ledger_instance* ledger, void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    return lr->appData();
}

int ledger_get_info(ledger_instance* ledger, ledger_info* info, void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    auto srcInfo = lr->info();
    info->name = lr->name();
    info->scope = srcInfo.scope();
    info->sync_direction = srcInfo.syncDirection();
    info->last_updated = srcInfo.lastUpdated();
    info->last_synced = srcInfo.lastSynced();
    info->flags = 0;
    if (srcInfo.syncPending()) {
        info->flags |= LEDGER_INFO_SYNC_PENDING;
    }
    return 0;
}

int ledger_open(ledger_stream** stream, ledger_instance* ledger, int mode, void* reserved) {
    auto lr = reinterpret_cast<Ledger*>(ledger);
    if (mode & LEDGER_STREAM_MODE_READ) {
        // Bidirectional streams are not supported as of now
        if (mode & LEDGER_STREAM_MODE_WRITE) {
            return SYSTEM_ERROR_NOT_SUPPORTED;
        }
        std::unique_ptr<LedgerReader> r(new(std::nothrow) LedgerReader());
        if (!r) {
            return SYSTEM_ERROR_NO_MEMORY;
        }
        CHECK(lr->beginRead(*r));
        *stream = reinterpret_cast<ledger_stream*>(r.release()); // Transfer ownership to the caller
    } else if (mode & LEDGER_STREAM_MODE_WRITE) {
        if (mode & LEDGER_STREAM_MODE_READ) {
            return SYSTEM_ERROR_NOT_SUPPORTED;
        }
        std::unique_ptr<LedgerWriter> w(new(std::nothrow) LedgerWriter());
        if (!w) {
            return SYSTEM_ERROR_NO_MEMORY;
        }
        CHECK(lr->beginWrite(LedgerChangeSource::USER, *w));
        *stream = reinterpret_cast<ledger_stream*>(w.release());
    } else {
        return SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    return 0;
}

int ledger_close(ledger_stream* stream, int flags, void* reserved) {
    auto s = reinterpret_cast<LedgerStream*>(stream);
    bool flush = !(flags & LEDGER_STREAM_CLOSE_DISCARD);
    CHECK(s->close(flush));
    return 0;
}

int ledger_read(ledger_stream* stream, char* data, size_t size, void* reserved) {
    auto s = reinterpret_cast<LedgerStream*>(stream);
    size_t n = CHECK(s->read(data, size));
    return n;
}

int ledger_write(ledger_stream* stream, const char* data, size_t size, void* reserved) {
    auto s = reinterpret_cast<LedgerStream*>(stream);
    size_t n = CHECK(s->write(data, size));
    return n;
}

int ledger_purge(const char* name, void* reserved) {
    CHECK(LedgerManager::instance()->removeLedgerData(name));
    return 0;
}

int ledger_purge_all(void* reserved) {
    CHECK(LedgerManager::instance()->removeAllData());
    return 0;
}
