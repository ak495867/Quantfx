#pragma once

#include "qfx.hpp"
#include <atomic>
#include <new>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace qfx {

template <size_t Capacity = 65536>
class alignas(64) SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
public:
    static constexpr size_t Mask = Capacity - 1;

    struct alignas(64) Storage {
        alignas(64) std::atomic<uint64_t> head{0};
        alignas(64) std::atomic<uint64_t> tail{0};
        alignas(64) Event buffer[Capacity];
    };

    Storage* storage = nullptr;
    bool is_owner = false;
    int shm_fd = -1;
    size_t shm_size = sizeof(Storage);

    SpscRingBuffer() = default;

    ~SpscRingBuffer() {
        if (storage && storage != MAP_FAILED) {
            munmap(storage, shm_size);
        }
        if (shm_fd >= 0) {
            close(shm_fd);
        }
    }

    bool init_shared(const char* name, bool create) {
        is_owner = create;
        int flags = O_RDWR;
        if (create) {
            flags |= O_CREAT | O_TRUNC;
        }
        shm_fd = shm_open(name, flags, 0666);
        if (shm_fd < 0) {
            return false;
        }
        if (create) {
            if (ftruncate(shm_fd, static_cast<off_t>(shm_size)) != 0) {
                close(shm_fd);
                shm_fd = -1;
                return false;
            }
        }
        void* addr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (addr == MAP_FAILED) {
            close(shm_fd);
            shm_fd = -1;
            return false;
        }
        storage = static_cast<Storage*>(addr);
        if (create) {
            new (storage) Storage();
        }
        return true;
    }

    bool push(const Event& event) noexcept {
        const uint64_t current_tail = storage->tail.load(std::memory_order_relaxed);
        const uint64_t current_head = storage->head.load(std::memory_order_acquire);
        if (current_tail - current_head >= Capacity) {
            return false;
        }
        storage->buffer[current_tail & Mask] = event;
        storage->tail.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool pop(Event& event) noexcept {
        const uint64_t current_head = storage->head.load(std::memory_order_relaxed);
        const uint64_t current_tail = storage->tail.load(std::memory_order_acquire);
        if (current_head == current_tail) {
            return false;
        }
        event = storage->buffer[current_head & Mask];
        storage->head.store(current_head + 1, std::memory_order_release);
        return true;
    }

    size_t size() const noexcept {
        const uint64_t current_head = storage->head.load(std::memory_order_relaxed);
        const uint64_t current_tail = storage->tail.load(std::memory_order_relaxed);
        return current_tail >= current_head ? current_tail - current_head : 0;
    }
};

}
