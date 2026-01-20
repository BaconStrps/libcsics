#include <csics/queue/SPSCQueue.hpp>

#include <atomic>
#include <cstring>
#include <new>
#include <thread>
#include <iostream>

namespace csics::queue {
SPSCQueue::SPSCQueue(size_t capacity) noexcept
    : capacity_(capacity + kCacheLineSize -
                (capacity % kCacheLineSize)),  // align capacity to cache line
      buffer_(reinterpret_cast<std::byte*>(operator new(capacity_, std::align_val_t{kCacheLineSize}))
              ),
      read_index_(0),
      write_index_(0),
      stopped_(false) {}

SPSCQueue::~SPSCQueue() noexcept {
    operator delete(buffer_, std::align_val_t{kCacheLineSize});
};

bool SPSCQueue::acquire_write(WriteSlot& slot, std::size_t size) noexcept {
    if (size == 0 || size > capacity_) {
        return false;
    }

    std::size_t read_index = read_index_.load(std::memory_order_acquire);
    std::size_t write_index = write_index_.load(std::memory_order_acquire);
    auto not_available = [&]() {
        read_index = read_index_.load(std::memory_order_acquire);
        write_index = write_index_.load(std::memory_order_acquire);
        return write_index + size > read_index && (write_index != read_index) ||
               stopped_.load(std::memory_order_acquire);
    };

    while (not_available()) {
        if (stopped_.load(std::memory_order_acquire)) return false;
        std::this_thread::yield();
    }

    if (write_index + size + sizeof(QueueSlotHeader) < capacity_) {
        slot.data = buffer_ + write_index;
    } else {
        auto padding_size = capacity_ - write_index;

        QueueSlotHeader padding_header{
            .padded = 1,
            .size = padding_size - sizeof(QueueSlotHeader),
        };

        std::memcpy(buffer_ + write_index, &padding_header,
                    sizeof(QueueSlotHeader));
        write_index_.store(0, std::memory_order_release);
        write_index = 0;
    }

    while (not_available()) {
        if (stopped_.load(std::memory_order_acquire)) return false;
        std::this_thread::yield();
    }

    QueueSlotHeader header{
        .padded = false,
        .size = size,
    };

    std::memcpy(&buffer_[write_index], &header, sizeof(QueueSlotHeader));
    slot.data = &buffer_[write_index] + sizeof(QueueSlotHeader);

    slot.size = size;

    return true;
};

bool SPSCQueue::try_acquire_write(WriteSlot& slot, std::size_t size) noexcept {
    if (size == 0 || size > capacity_) {
        return false;
    }

    std::size_t read_index = read_index_.load(std::memory_order_acquire);
    std::size_t write_index = write_index_.load(std::memory_order_acquire);
    auto not_available = [&]() {
        read_index = read_index_.load(std::memory_order_acquire);
        write_index = write_index_.load(std::memory_order_acquire);
        return write_index + size > read_index && (write_index != read_index) ||
               stopped_.load(std::memory_order_acquire);
    };

    if (not_available()) {
        return false;
    }

    if (write_index + size + sizeof(QueueSlotHeader) < capacity_) {
        slot.data = buffer_ + write_index;
    } else {
        auto padding_size = capacity_ - write_index;

        QueueSlotHeader padding_header{
            .padded = 1,
            .size = padding_size - sizeof(QueueSlotHeader),
        };

        std::memcpy(buffer_ + write_index, &padding_header,
                    sizeof(QueueSlotHeader));
        write_index_.store(0, std::memory_order_release);
        write_index = 0;
    }

    if (not_available()) {
        return false;
    }

    QueueSlotHeader header{
        .padded = false,
        .size = size,
    };

    std::memcpy(&buffer_[write_index], &header, sizeof(QueueSlotHeader));
    slot.data = &buffer_[write_index] + sizeof(QueueSlotHeader);

    slot.size = size;

    return true;
};

bool SPSCQueue::acquire_read(ReadSlot& slot) noexcept {
    while (!data_available_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    auto header_ptr = reinterpret_cast<QueueSlotHeader*>(buffer_ + read_index_);
    if (header_ptr->padded) {
        read_index_.store(0, std::memory_order_release);
        header_ptr = reinterpret_cast<QueueSlotHeader*>(buffer_);
    }

    while (!data_available_.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    if (read_index_.load(std::memory_order_acquire) ==
            write_index_.load(std::memory_order_acquire) &&
        stopped_.load(std::memory_order_acquire)) {
        return false;
    }
    slot.data = buffer_ + read_index_.load(std::memory_order_acquire) +
                sizeof(QueueSlotHeader);
    slot.size = header_ptr->size;

    return true;
}

bool SPSCQueue::try_acquire_read(ReadSlot& slot) noexcept {
    if (read_index_.load(std::memory_order_acquire) ==
            write_index_.load(std::memory_order_acquire) &&
        stopped_.load(std::memory_order_acquire)) {
        return false;
    }

    auto header_ptr = reinterpret_cast<QueueSlotHeader*>(buffer_ + read_index_);
    if (header_ptr->padded) {
        read_index_.store(0, std::memory_order_release);
        header_ptr = reinterpret_cast<QueueSlotHeader*>(buffer_);
    }

    if (read_index_.load(std::memory_order_acquire) ==
            write_index_.load(std::memory_order_acquire) &&
        stopped_.load(std::memory_order_acquire)) {
        return false;
    }

    
    slot.data = buffer_ + read_index_.load(std::memory_order_acquire) +
                sizeof(QueueSlotHeader);
    slot.size = header_ptr->size;

    return true;
}

bool SPSCQueue::commit_write(const WriteSlot& slot) noexcept {
    auto new_index = write_index_.load(std::memory_order_acquire) + slot.size + sizeof(QueueSlotHeader);
    new_index = (new_index + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
    new_index = new_index % capacity_;

    write_index_.store(new_index, std::memory_order_release);
    data_available_.store(true, std::memory_order_release);

    return true;
}

bool SPSCQueue::commit_read(const ReadSlot& slot) noexcept {
    auto new_index = read_index_.load(std::memory_order_acquire) + slot.size + sizeof(QueueSlotHeader);
    new_index = (new_index + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
    new_index = new_index % capacity_;
    read_index_.store(new_index, std::memory_order_release);

    if (new_index == write_index_.load(std::memory_order_acquire)) {
        data_available_.store(false, std::memory_order_release);
    }

    return true;
}

void SPSCQueue::stop() { stopped_.store(true, std::memory_order_release); }

SPSCQueueRange::SPSCQueueRange(SPSCQueue& queue) noexcept : queue_(&queue) {}

inline SPSCQueueRange::iterator SPSCQueueRange::begin() noexcept {
    return iterator(queue_);
};

inline SPSCQueueRange::sentinel SPSCQueueRange::end() const {
    return sentinel{};
};

SPSCQueueRange::iterator::iterator(SPSCQueue* q) noexcept
    : queue(q), end_reached(false), current_slot{} {
    if (this->queue) {
        bool acquired = this->queue->try_acquire_read(current_slot);
        if (!acquired) {
            end_reached = true;
            this->queue = nullptr;
        }
    } else {
        end_reached = true;
    }
}

SPSCQueueRange::iterator& SPSCQueueRange::iterator::operator++() noexcept {
    if (queue) {
        queue->commit_read(current_slot);
        end_reached = queue->acquire_read(current_slot);
    }
    return *this;
}

void SPSCQueueRange::iterator::operator++(int) { ++(*this); }

SPSCQueue::ReadSlot SPSCQueueRange::iterator::operator*() const noexcept {
    return current_slot;
}

SPSCQueueRange::iterator::~iterator() noexcept {
    if (queue && !end_reached) {
        queue->commit_read(current_slot);
    }
}

};  // namespace csics::queue
