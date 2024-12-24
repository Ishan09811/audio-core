// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/pool/pool.hpp>
#include <audio_core/common/common_types.h>

namespace KernelShim {

class KTransferMemory {
public:
    explicit KTransferMemory(size_t size) 
        : m_size(size), m_pool(size) {
        AllocateMemory();
    }

    ~KTransferMemory() {
        FreeMemory();
    }

    VAddr GetSourceAddress() const {
        return reinterpret_cast<VAddr>(m_data);
    }

private:
    void AllocateMemory() {
        m_data = static_cast<u8*>(m_pool.malloc());
        if (!m_data) {
            throw std::runtime_error("Failed to allocate memory using Boost.Pool");
        }
    }

    void FreeMemory() {
        if (m_data) {
            m_pool.free(m_data);
            m_data = nullptr;
        }
    }

private:
    size_t m_size;
    boost::pool<> m_pool;  // Boost pool for memory allocation
    u8* m_data{nullptr};   // Pointer to allocated memory
};

} // namespace KernelShim
