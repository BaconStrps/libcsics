#include <csics/queue/SPSCQueue.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <random>
// right now, instantation testing
struct HeaderTest {
    int a;
    float b;
};

struct DataTest {
    double x;
    char y;
};

namespace csics::queue {
template class SPSCQueueBlockAdapter<HeaderTest, DataTest>;
template class SPSCQueueBlockAdapterRange<HeaderTest, DataTest>;
};

static bool binary_arr_eq(std::byte* arr1, std::byte* arr2, std::size_t size) {
    for (std::size_t i = 0; i < size; i++) {
        if (arr1[i] != arr2[i]) {
            return false;
        }
    }
    return true;
}

static std::unique_ptr<std::byte[]> generate_random_bytes(std::size_t size) {
    auto buffer = std::make_unique<std::byte[]>(size);

    // Thread-local to avoid reseeding costs if used frequently
    thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist(0, 255);

    for (std::size_t i = 0; i < size; ++i) {
        buffer[i] = static_cast<std::byte>(dist(rng));
    }

    return buffer;
}

TEST(CSICSQueueTests, BasicReadWrite) {
    using namespace csics::queue;

    SPSCQueue q(1024);

    SPSCQueue::WriteSlot ws{};

    ASSERT_TRUE(q.acquire_write(ws, 512));

    const char mystr[] = "Hello world!";

    memcpy(ws.data, mystr, sizeof(mystr));
    q.commit_write(ws);

    SPSCQueue::ReadSlot rs{};

    ASSERT_TRUE(q.acquire_read(rs));
    ASSERT_EQ(rs.size, ws.size);
    ASSERT_STREQ(reinterpret_cast<char*>(rs.data), mystr);
}

TEST(CSICSQueueTests, BasicReadWriteSmall) {
    using namespace csics::queue;
    SPSCQueue::WriteSlot ws{};
    SPSCQueue::ReadSlot rs{};
    SPSCQueue q(13);

    std::byte pattern[] = {std::byte{0}, std::byte{1}, std::byte{2},
                           std::byte{3}};

    std::size_t size = 4;

    ASSERT_TRUE(q.acquire_write(ws, size));
    std::memcpy(ws.data, pattern, size);
    ASSERT_TRUE(q.commit_write(ws));
    ASSERT_TRUE(q.acquire_read(rs));
    ASSERT_EQ(rs.size, size);
    ASSERT_PRED3(binary_arr_eq, rs.data, &pattern[0], size);
    ASSERT_TRUE(q.commit_read(rs));

    pattern[0] = std::byte{8};

    ASSERT_TRUE(q.acquire_write(ws, size));
    std::memcpy(ws.data, pattern, size);
    ASSERT_TRUE(q.commit_write(ws));
    ASSERT_TRUE(q.acquire_read(rs));
    ASSERT_PRED3(binary_arr_eq, rs.data, &pattern[0], size);
    ASSERT_EQ(rs.size, size);

}

TEST(CSICSQueueTests, FuzzReadWriteSingleThreaded) {
    using namespace csics::queue;
    SPSCQueue::WriteSlot ws{};
    SPSCQueue::ReadSlot rs{};
    SPSCQueue q(1053);
    thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(1, 300);
    std::size_t total_size = 0;

    for (std::size_t i = 0; i < 1000000; i++) {
        std::size_t size = dist(rng);
        total_size += size;
        auto pattern = generate_random_bytes(size);
        ASSERT_TRUE(q.acquire_write(ws, size));
        std::memcpy(ws.data, pattern.get(), size);
        ASSERT_TRUE(q.commit_write(ws));
        ASSERT_TRUE(q.acquire_read(rs));
        ASSERT_EQ(rs.size, size) << "Error on iteration " << i << " \nWith total size " << total_size
            << " \nws.data: " << ws.data << ", rs.data: " << rs.data;
        ASSERT_PRED3(binary_arr_eq, rs.data, &pattern[0], size);
        ASSERT_TRUE(q.commit_read(rs));
    }
}

TEST(CSICSQueueTests, FuzzReadWriteMultiThreaded) {
    #ifdef _MSC_VER
    GTEST_SKIP() << "Doesn't run in MSVC";
    #endif
    using namespace csics::queue;
    SPSCQueue::WriteSlot ws{};
    SPSCQueue::ReadSlot rs{};
    SPSCQueue q(1053);
    std::size_t iterations = 10000000;

    std::thread([&]() {
        for (std::size_t i = 0; i < iterations; i++) {
            ASSERT_TRUE(q.acquire_write(ws, sizeof(std::size_t)));
            std::memcpy(ws.data, &i, sizeof(std::size_t));
            ASSERT_TRUE(q.commit_write(ws));
        }
    });

     std::thread([&]() {
        for (std::size_t i = 0; i < iterations; i++) {
            ASSERT_TRUE(q.acquire_read(rs));
            std::size_t val = *reinterpret_cast<std::size_t*>(rs.data);
            ASSERT_EQ(val, i);
            ASSERT_TRUE(q.commit_read(rs));
        }
    });
}
