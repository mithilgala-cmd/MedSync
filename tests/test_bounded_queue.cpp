#include "medsync/BoundedQueue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

namespace medsync {
namespace {

TEST(BoundedQueueTest, PreservesFifoOrder) {
    BoundedQueue<int> queue(4);

    ASSERT_TRUE(queue.tryPush(1));
    ASSERT_TRUE(queue.tryPush(2));
    ASSERT_TRUE(queue.tryPush(3));

    int value = 0;
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 2);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 3);
}

TEST(BoundedQueueTest, RejectsWhenFull) {
    BoundedQueue<int> queue(2);

    EXPECT_TRUE(queue.tryPush(10));
    EXPECT_TRUE(queue.tryPush(20));
    EXPECT_FALSE(queue.tryPush(30));
    EXPECT_EQ(queue.size(), 2u);
}

TEST(BoundedQueueTest, CloseRejectsNewItemsAndDrainsExisting) {
    BoundedQueue<int> queue(4);
    ASSERT_TRUE(queue.tryPush(7));
    queue.close();

    EXPECT_FALSE(queue.tryPush(8));
    EXPECT_TRUE(queue.isClosed());

    int value = 0;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 7);
    EXPECT_FALSE(queue.pop(value));
}

TEST(BoundedQueueTest, ClearDiscardsPendingItems) {
    BoundedQueue<int> queue(4);
    ASSERT_TRUE(queue.tryPush(1));
    ASSERT_TRUE(queue.tryPush(2));

    queue.clear();

    EXPECT_EQ(queue.size(), 0u);
    EXPECT_TRUE(queue.tryPush(3));
    int value = 0;
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 3);
}

TEST(BoundedQueueTest, ConsumerReceivesItemsFromAnotherThread) {
    BoundedQueue<int> queue(4);
    std::atomic<bool> started{false};
    std::vector<int> received;

    std::thread consumer([&] {
        started.store(true, std::memory_order_release);
        int value = 0;
        while (queue.pop(value)) {
            received.push_back(value);
        }
    });

    while (!started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    ASSERT_TRUE(queue.tryPush(1));
    ASSERT_TRUE(queue.tryPush(2));
    ASSERT_TRUE(queue.tryPush(3));
    queue.close();

    consumer.join();

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], 1);
    EXPECT_EQ(received[1], 2);
    EXPECT_EQ(received[2], 3);
}

TEST(BoundedQueueTest, PopForTimesOutOnEmptyQueue) {
    BoundedQueue<int> queue(2);

    int value = 0;
    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(queue.popFor(value, std::chrono::milliseconds(20)));
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_GE(elapsed, std::chrono::milliseconds(20));
}

TEST(BoundedQueueTest, SupportsMoveOnlyValues) {
    BoundedQueue<std::unique_ptr<int>> queue(2);

    ASSERT_TRUE(queue.tryPush(std::make_unique<int>(42)));

    std::unique_ptr<int> value;
    ASSERT_TRUE(queue.pop(value));
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
}

TEST(BoundedQueueTest, ManyItemsAreDeliveredExactlyOnce) {
    constexpr int kItemCount = 1000;
    BoundedQueue<int> queue(8);
    std::vector<int> received;

    std::thread consumer([&] {
        int value = 0;
        while (queue.pop(value)) {
            received.push_back(value);
        }
    });

    for (int i = 0; i < kItemCount; ++i) {
        while (!queue.tryPush(i)) {
            std::this_thread::yield();
        }
    }
    queue.close();
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<std::size_t>(kItemCount));
    EXPECT_EQ(std::accumulate(received.begin(), received.end(), 0),
              (kItemCount - 1) * kItemCount / 2);
}

} // namespace
} // namespace medsync
