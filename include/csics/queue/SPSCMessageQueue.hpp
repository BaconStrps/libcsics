#pragma once
#include <csics/queue/SPSCQueue.hpp>
#include <cstring>



namespace csics::queue {

template<typename Message>
concept MessageType = requires(Message msg) {
    std::is_trivially_copyable_v<Message>;
    std::is_trivially_destructible_v<Message>;
};

    template<MessageType Message>
    class SPSCMessageQueue {
        public:
            explicit SPSCMessageQueue(size_t capacity) noexcept
                : queue_(capacity * sizeof(Message)) {}
            ~SPSCMessageQueue() noexcept = default;

            bool enqueue(const Message& msg) noexcept {
                SPSCQueue::WriteSlot slot;
                bool acquired = queue_.acquire_write(slot, sizeof(Message));
                if (!acquired) {
                    return false;
                }
                std::memcpy(slot.data, &msg, sizeof(Message));
                return queue_.commit_write(slot);
            }

            bool dequeue(Message& msg) noexcept {
                SPSCQueue::ReadSlot slot;
                bool acquired = queue_.acquire_read(slot);
                if (!acquired) {
                    return false;
                }
                std::memcpy(&msg, slot.data, sizeof(Message));
                return queue_.commit_read(slot);
            }

            bool has_pending_data() const noexcept {
                return queue_.has_pending_data();
            }
        private:
            SPSCQueue queue_;
    };
};
