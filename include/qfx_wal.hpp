#pragma once

#include "qfx.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace qfx {

struct alignas(8) WalEntry {
    uint64_t seq = 0;
    uint64_t timestamp_ns = 0;
    Order order{};
    uint64_t checksum = 0;
};

static_assert(sizeof(WalEntry) == 80, "WalEntry must be exactly 80 bytes");

class BinaryWal {
public:
    int fd = -1;
    void* mmap_addr = nullptr;
    size_t file_size = 0;
    size_t capacity = 0;
    uint64_t next_seq = 1;
    uint64_t current_index = 0;

    ~BinaryWal() {
        close_wal();
    }

    bool open_wal(const char* filepath, size_t record_capacity = 65536) {
        close_wal();
        capacity = record_capacity;
        file_size = sizeof(WalEntry) * capacity;

        fd = open(filepath, O_RDWR | O_CREAT, 0666);
        if (fd < 0) return false;

        struct stat st{};
        if (fstat(fd, &st) != 0) return false;

        if (static_cast<size_t>(st.st_size) < file_size) {
            if (ftruncate(fd, static_cast<off_t>(file_size)) != 0) return false;
        }

        mmap_addr = mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mmap_addr == MAP_FAILED) {
            mmap_addr = nullptr;
            return false;
        }

        current_index = 0;
        next_seq = 1;
        auto* entries = static_cast<WalEntry*>(mmap_addr);
        for (size_t i = 0; i < capacity; ++i) {
            if (entries[i].seq >= next_seq) {
                next_seq = entries[i].seq + 1;
                current_index = i + 1;
            }
        }
        return true;
    }

    void close_wal() {
        if (mmap_addr && mmap_addr != MAP_FAILED) {
            msync(mmap_addr, file_size, MS_SYNC);
            munmap(mmap_addr, file_size);
            mmap_addr = nullptr;
        }
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }

    uint64_t append(const Order& order, uint64_t ts_ns) noexcept {
        if (!mmap_addr) return 0;

        uint64_t idx = current_index % capacity;
        auto* entries = static_cast<WalEntry*>(mmap_addr);

        WalEntry entry{};
        entry.seq = next_seq++;
        entry.timestamp_ns = ts_ns;
        entry.order = order;
        entry.checksum = entry.seq ^ entry.timestamp_ns ^ order.price ^ order.quantity;

        entries[idx] = entry;
        current_index++;
        return entry.seq;
    }

    size_t recover(std::vector<WalEntry>& out) const {
        out.clear();
        if (!mmap_addr) return 0;

        auto* entries = static_cast<WalEntry*>(mmap_addr);
        for (size_t i = 0; i < capacity; ++i) {
            if (entries[i].seq > 0) {
                uint64_t expected = entries[i].seq ^ entries[i].timestamp_ns ^ entries[i].order.price ^ entries[i].order.quantity;
                if (entries[i].checksum == expected) {
                    out.push_back(entries[i]);
                }
            }
        }
        std::sort(out.begin(), out.end(), [](const WalEntry& a, const WalEntry& b) {
            return a.seq < b.seq;
        });
        return out.size();
    }
};

}
