#pragma once

#include <atomic>
#include <cstddef>
#include <iterator>

namespace csics::queue {

#ifdef CACHE_LINE_SIZE
constexpr size_t kCacheLineSize = CACHE_LINE_SIZE;
#else
constexpr size_t kCacheLineSize = 64;
#endif

class SPSCQueueRange;

// Single Producer Single Consumer Queue
// Uses a circular buffer with atomic indices for read and write.
// API uses acquire/commit semantics for both read and write.
// Used for low-level communication between threads with minimal overhead.
// Supports blocking and non-blocking acquire methods.
// Supports stopping the queue to unblock waiting threads.
class SPSCQueue {
   public:
    struct ReadSlot;
    struct WriteSlot;
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    explicit SPSCQueue(size_t capacity) noexcept;
    ~SPSCQueue() noexcept;

    // Acquire a read slot.
    // ReadSlot will be populated with the data pointer and size.
    // Blocks until a slot is available or the queue is stopped.
    // Returns true if successful, false otherwise.
    // Will return false if the queue is stopped and there is no data to read.
    bool acquire_read(ReadSlot& slot) noexcept;
    // Attempt to acquire a read slot without blocking.
    // Will return false if no slot is available.
    bool try_acquire_read(ReadSlot& slot) noexcept;

    // Release a previously acquired read slot.
    bool commit_read(const ReadSlot& slot) noexcept;

    // Acquire a write slot.
    // WriteSlot will be populated with the data pointer and size.
    // Blocks until a slot is available or the queue is stopped.
    bool acquire_write(WriteSlot& slot, std::size_t size) noexcept;
    // Attempt to acquire a write slot without blocking.
    // WriteSlot will be populated with the data pointer and size.
    // Returns false if no slot is available.
    bool try_acquire_write(WriteSlot& slot, std::size_t size) noexcept;

    // Release a previously acquired write slot.
    bool commit_write(const WriteSlot& slot) noexcept;

    // Stop the queue. Unblocks any waiting acquire calls.
    void stop();

    std::size_t capacity() const { return capacity_; }

    bool has_pending_data() const noexcept {
        return read_index_.load(std::memory_order_acquire) !=
               write_index_.load(std::memory_order_acquire);
    }

   private:
    std::size_t capacity_;
    std::byte* buffer_;

    struct QueueSlotHeader {  // extendable header, realistically only a size.
        uint64_t padded : 1;
        uint64_t size : 63;
    };

    alignas(kCacheLineSize) std::atomic<size_t> read_index_;
    alignas(kCacheLineSize) std::atomic<size_t> write_index_;
    alignas(kCacheLineSize) std::atomic<bool> stopped_;

   public:
    struct ReadSlot {
        const std::byte* data;
        size_t size;

        template <typename T>
        const T& as() {
            return *static_cast<const T*>(data);
        }
    };

    struct WriteSlot {
        std::byte* data;
        size_t size;

        template <typename T>
        T& as() {
            return *static_cast<T*>(data);
        }
    };
};

// Range-based iterator for reading from SPSCQueue
// Allows for easy iteration over available read slots in the queue.
// Exits when the queue is stopped, not when empty. The producer must stop the
// queue. Typically used in a consumer thread to process incoming data. Will
// manage committing read slots automatically.
class SPSCQueueRange {
   public:
    explicit SPSCQueueRange(SPSCQueue& queue) noexcept;

    struct iterator;
    struct sentinel {};

    inline iterator begin() noexcept;
    inline sentinel end() const;

   private:
    SPSCQueue* queue_;
};

struct SPSCQueueRange::iterator {
    using iterator_category = std::input_iterator_tag;
    using value_type = SPSCQueue::ReadSlot;
    using difference_type = std::ptrdiff_t;

    SPSCQueue* queue = nullptr;
    SPSCQueue::ReadSlot current_slot;
    bool end_reached = false;

    iterator() noexcept = default;
    explicit iterator(SPSCQueue* q) noexcept;

    iterator& operator++() noexcept;

    void operator++(int);

    friend bool operator==(const iterator& it, const sentinel&) noexcept {
        return it.queue == nullptr || it.end_reached;
    }

    friend bool operator!=(const iterator& it, const sentinel&) noexcept {
        return !(it == sentinel{});
    }

    SPSCQueue::ReadSlot operator*() const noexcept;

    ~iterator() noexcept;
};

/*!
 * Adapter for SPSCQueue to handle blocks with headers and data.
 * Allows for easy reading and writing of blocks consisting of a header and data
 * array. Same acquire/commit semantics as SPSCQueue. Example:
 * ```cpp
 *      struct MyHeader { uint32_t id; uint32_t timestamp; };
 *      using MyDataType = float;
 *      SPSCQueue queue(1024);
 *      SPSCQueueBlockAdapter<MyHeader, MyDataType> adapter(queue);
 *      SPSCQueueBlockAdapter<MyHeader, MyDataType>::AdaptedSlot slot;
 *      if (adapter.acquire_write(slot)) {
 *          slot.header->id = 1;
 *          slot.header->timestamp = GetTimestamp();
 *          for (size_t i = 0; i < slot.size; ++i) {
 *           slot.data[i] = ...; // fill data
 *      }
 *      adapter.commit_write(slot);
 *  }
 *  ```
 */
template <typename Header, typename Data>
class SPSCQueueBlockAdapter {
   public:
    explicit SPSCQueueBlockAdapter(SPSCQueue& queue) : queue_(queue) {}

    struct AdaptedSlot {
        Header* header;
        Data* data;
        std::size_t size;
    };

    bool acquire_write(AdaptedSlot& slot, size_t len) noexcept {
        SPSCQueue::WriteSlot raw_slot;
        bool acquired =
            queue_.acquire_write(raw_slot, sizeof(Header) + sizeof(Data) * len);
        if (!acquired) {
            return false;
        }

        slot.header = reinterpret_cast<Header*>(raw_slot.data);
        slot.data = reinterpret_cast<Data*>(raw_slot.data + sizeof(Header));
        slot.size = (raw_slot.size - sizeof(Header)) / sizeof(Data);
        return true;
    }

    bool try_acquire_write(AdaptedSlot& slot, size_t len) noexcept {
        SPSCQueue::WriteSlot raw_slot;
        bool acquired =
            queue_.try_acquire_write(raw_slot, sizeof(Header) + sizeof(Data) * len);
        if (!acquired) {
            return false;
        }

        slot.header = reinterpret_cast<Header*>(raw_slot.data);
        slot.data = reinterpret_cast<Data*>(raw_slot.data + sizeof(Header));
        slot.size = (raw_slot.size - sizeof(Header)) / sizeof(Data);
        return true;
    }

    bool commit_write(AdaptedSlot& slot) noexcept {
        SPSCQueue::WriteSlot raw_slot;
        raw_slot.data = reinterpret_cast<std::byte*>(slot.header);
        raw_slot.size = sizeof(Header) + sizeof(Data);
        return queue_.commit_write(raw_slot);
    }

    bool acquire_read(AdaptedSlot& slot) noexcept {
        SPSCQueue::ReadSlot raw_slot;
        bool acquired = queue_.acquire_read(raw_slot);
        if (!acquired) {
            return false;
        }

        slot.header = raw_slot.template as<Header>();
        slot.data = reinterpret_cast<Data*>(raw_slot.data + sizeof(Header));
        slot.size = (raw_slot.size - sizeof(Header)) / sizeof(Data);
        return true;
    }

    bool try_acquire_read(AdaptedSlot& slot) noexcept {
        SPSCQueue::ReadSlot raw_slot;
        bool acquired = queue_.try_acquire_read(raw_slot);
        if (!acquired) {
            return false;
        }

        slot.header = raw_slot.template as<Header>();
        slot.data = reinterpret_cast<Data*>(raw_slot.data + sizeof(Header));
        slot.size = (raw_slot.size - sizeof(Header)) / sizeof(Data);
        return true;
    }

    bool commit_read(AdaptedSlot& slot) noexcept {
        SPSCQueue::ReadSlot raw_slot;
        raw_slot.data = reinterpret_cast<std::byte*>(slot.header);
        raw_slot.size = sizeof(Header) + sizeof(Data);
        return queue_.commit_read(raw_slot);
    }

   private:
    SPSCQueue& queue_;
};

// Range-based iterator for reading from SPSCQueueBlockAdapter
// Allows for easy iteration over available adapted read slots in the queue.
// Exits when the queue is stopped, not when empty. The producer must stop the
// queue. Typically used in a consumer thread to process incoming data. Will
// manage committing read slots automatically.
template <typename Header, typename Data>
class SPSCQueueBlockAdapterRange {
   public:
    explicit SPSCQueueBlockAdapterRange(
        SPSCQueueBlockAdapter<Header, Data>* q) noexcept
        : queue_(q) {}

    struct iterator;
    struct sentinel {};

    inline iterator begin() noexcept { return iterator(queue_); }

    inline sentinel end() const { return sentinel{}; }

   private:
    SPSCQueueBlockAdapter<Header, Data>* queue_;
};

template <typename Header, typename Data>
struct SPSCQueueBlockAdapterRange<Header, Data>::iterator {
    using iterator_category = std::input_iterator_tag;
    using value_type =
        typename csics::queue::SPSCQueueBlockAdapter<Header, Data>::AdaptedSlot;
    struct AdaptedSlotRef {
        const Header& header;
        const Data* data;
        std::size_t size;
    };
    using ref_type = AdaptedSlotRef;
    using difference_type = std::ptrdiff_t;

    SPSCQueueBlockAdapter<Header, Data>* queue = nullptr;
    value_type current_block{nullptr, nullptr, 0};

    bool end_reached = false;

    iterator() noexcept = default;
    explicit iterator(SPSCQueueBlockAdapter<Header, Data>* q) noexcept
        : queue(q) {
        if (this->queue) {
            bool acquired = this->queue->acquire_read(current_block);
            if (!acquired) {
                end_reached = true;
                this->queue = nullptr;
            }
        } else {
            end_reached = true;
        }
    }

    iterator& operator++() noexcept {
        if (this->queue && !end_reached) {
            queue->commit_read(current_block);
            bool acquired = this->queue->acquire_read(current_block);
            if (!acquired) {
                end_reached = true;
                this->queue = nullptr;
            }
        } else {
            end_reached = true;
        }
        return *this;
    }

    void operator++(int) { ++(*this); }

    friend bool operator==(const iterator& it, const sentinel&) noexcept {
        return it.queue == nullptr || it.end_reached;
    }

    friend bool operator!=(const iterator& it, const sentinel&) noexcept {
        return !(it == sentinel{});
    }

    ref_type operator*() const noexcept {
        return ref_type{*current_block.header, current_block.data,
                        current_block.size};
    };

    ~iterator() noexcept {
        if (queue && !end_reached) {
            queue->commit_read(current_block);
        }
    };
};
};  // namespace csics::queue
