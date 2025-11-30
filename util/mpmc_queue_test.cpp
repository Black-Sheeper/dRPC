#include <gtest/gtest.h>

#include "mpmc_queue.h"

class MPMCQueueTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 基础功能测试
TEST_F(MPMCQueueTest, BasicPushPop)
{
    MPMCQueue<int> queue;

    queue.push(1);
    queue.push(2);
    queue.push(3);

    auto result1 = queue.pop();
    auto result2 = queue.pop();
    auto result3 = queue.pop();
    auto result4 = queue.pop(); // 空队列

    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 1);

    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), 2);

    EXPECT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value(), 3);

    EXPECT_FALSE(result4.has_value());
}

// 测试字符串类型
TEST_F(MPMCQueueTest, StringType)
{
    MPMCQueue<std::string> queue;

    queue.push("hello");
    queue.push("world");

    auto result1 = queue.pop();
    auto result2 = queue.pop();

    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), "hello");

    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), "world");
}

// 单生产者单消费者测试
TEST_F(MPMCQueueTest, SingleProducerSingleConsumer)
{
    MPMCQueue<int> queue;
    const int NUM_ITEMS = 1000;

    std::thread producer([&]()
                         {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            queue.push(i);
        } });

    std::vector<int> consumed;
    std::thread consumer([&]()
                         {
        for (int i = 0; i < NUM_ITEMS; ++i) {
            auto item = queue.pop();
            while (!item.has_value()) {
                std::this_thread::yield();
                item = queue.pop();
            }
            consumed.push_back(item.value());
        } });

    producer.join();
    consumer.join();

    EXPECT_EQ(consumed.size(), NUM_ITEMS);

    // 验证所有值都被正确消费（顺序可能不同）
    std::set<int> consumed_set(consumed.begin(), consumed.end());
    for (int i = 0; i < NUM_ITEMS; ++i)
    {
        EXPECT_TRUE(consumed_set.count(i));
    }
}

// 多生产者单消费者测试
TEST_F(MPMCQueueTest, MultipleProducersSingleConsumer)
{
    MPMCQueue<int> queue;
    const int NUM_PRODUCERS = 4;
    const int ITEMS_PER_PRODUCER = 250;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> produced_count{0};
    std::vector<std::thread> producers;

    for (int i = 0; i < NUM_PRODUCERS; ++i)
    {
        producers.emplace_back([&, producer_id = i]()
                               {
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                int value = producer_id * ITEMS_PER_PRODUCER + j;
                queue.push(value);
                produced_count.fetch_add(1, std::memory_order_relaxed);
            } });
    }

    std::vector<int> consumed;
    std::thread consumer([&]()
                         {
        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            auto item = queue.pop();
            while (!item.has_value()) {
                std::this_thread::yield();
                item = queue.pop();
            }
            consumed.push_back(item.value());
        } });

    for (auto &producer : producers)
    {
        producer.join();
    }
    consumer.join();

    EXPECT_EQ(consumed.size(), TOTAL_ITEMS);

    // 验证所有值都被正确消费
    std::set<int> consumed_set(consumed.begin(), consumed.end());
    for (int i = 0; i < TOTAL_ITEMS; ++i)
    {
        EXPECT_TRUE(consumed_set.count(i));
    }
}

// 单生产者多消费者测试
TEST_F(MPMCQueueTest, SingleProducerMultipleConsumers)
{
    MPMCQueue<int> queue;
    const int NUM_CONSUMERS = 4;
    const int TOTAL_ITEMS = 1000;

    std::thread producer([&]()
                         {
        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            queue.push(i);
        } });

    std::atomic<int> consumed_count{0};
    std::vector<std::thread> consumers;
    std::vector<std::vector<int>> consumed_lists(NUM_CONSUMERS);

    for (int i = 0; i < NUM_CONSUMERS; ++i)
    {
        consumers.emplace_back([&, consumer_id = i]()
                               {
            while (consumed_count.load() < TOTAL_ITEMS) {
                auto item = queue.pop();
                if (item.has_value()) {
                    consumed_lists[consumer_id].push_back(item.value());
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            } });
    }

    producer.join();
    for (auto &consumer : consumers)
    {
        consumer.join();
    }

    // 验证所有值都被正确消费
    std::set<int> all_consumed;
    int total_consumed = 0;
    for (const auto &list : consumed_lists)
    {
        total_consumed += list.size();
        all_consumed.insert(list.begin(), list.end());
    }

    EXPECT_EQ(total_consumed, TOTAL_ITEMS);
    for (int i = 0; i < TOTAL_ITEMS; ++i)
    {
        EXPECT_TRUE(all_consumed.count(i));
    }
}

// 多生产者多消费者测试
TEST_F(MPMCQueueTest, MultipleProducersMultipleConsumers)
{
    MPMCQueue<int> queue;
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_PER_PRODUCER = 250;
    const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;

    std::atomic<int> produced_count{0};
    std::atomic<int> consumed_count{0};
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::vector<std::vector<int>> consumed_lists(NUM_CONSUMERS);

    // 启动生产者
    for (int i = 0; i < NUM_PRODUCERS; ++i)
    {
        producers.emplace_back([&, producer_id = i]()
                               {
            for (int j = 0; j < ITEMS_PER_PRODUCER; ++j) {
                int value = producer_id * ITEMS_PER_PRODUCER + j;
                queue.push(value);
                produced_count.fetch_add(1, std::memory_order_relaxed);
            } });
    }

    // 启动消费者
    for (int i = 0; i < NUM_CONSUMERS; ++i)
    {
        consumers.emplace_back([&, consumer_id = i]()
                               {
            while (consumed_count.load() < TOTAL_ITEMS) {
                auto item = queue.pop();
                if (item.has_value()) {
                    consumed_lists[consumer_id].push_back(item.value());
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            } });
    }

    // 等待所有线程完成
    for (auto &producer : producers)
    {
        producer.join();
    }
    for (auto &consumer : consumers)
    {
        consumer.join();
    }

    // 验证所有值都被正确消费
    std::set<int> all_consumed;
    int total_consumed = 0;
    for (const auto &list : consumed_lists)
    {
        total_consumed += list.size();
        all_consumed.insert(list.begin(), list.end());
    }

    EXPECT_EQ(total_consumed, TOTAL_ITEMS);
    for (int i = 0; i < TOTAL_ITEMS; ++i)
    {
        EXPECT_TRUE(all_consumed.count(i));
    }
}

// 性能测试 - 高并发场景
TEST_F(MPMCQueueTest, HighConcurrencyPerformance)
{
    MPMCQueue<int> queue; 
    const int NUM_OPERATIONS = 10000;
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;

    std::atomic<int> completed_operations{0};
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // 预热
    for (int i = 0; i < 100; ++i)
    {
        queue.push(i);
        queue.pop();
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // 生产者线程
    for (int i = 0; i < NUM_PRODUCERS; ++i)
    {
        producers.emplace_back([&, i]()
                               {
            for (int j = 0; j < NUM_OPERATIONS / NUM_PRODUCERS; ++j) {
                queue.push(i * 1000 + j);
                push_count.fetch_add(1, std::memory_order_relaxed);
                completed_operations.fetch_add(1, std::memory_order_relaxed);
            } });
    }

    // 消费者线程
    for (int i = 0; i < NUM_CONSUMERS; ++i)
    {
        consumers.emplace_back([&]()
                               {
            for (int j = 0; j < NUM_OPERATIONS / NUM_CONSUMERS; ++j) {
                if (queue.pop()) {
                    pop_count.fetch_add(1, std::memory_order_relaxed);
                    completed_operations.fetch_add(1, std::memory_order_relaxed);
                }
            } });
    }

    // 等待所有线程完成
    for (auto &thread : producers)
        thread.join();
    for (auto &thread : consumers)
        thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "High concurrency test completed:" << std::endl;
    std::cout << "  Operations: " << completed_operations.load() << std::endl;
    std::cout << "  Push count: " << push_count.load() << std::endl;
    std::cout << "  Pop count: " << pop_count.load() << std::endl;
    std::cout << "  Time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Ops/sec: " << (completed_operations.load() * 1000.0 / duration.count()) << std::endl;
}

// 内存使用测试 - 大量数据
TEST_F(MPMCQueueTest, MemoryUsageWithLargeData)
{
    MPMCQueue<std::vector<int>> queue;
    const int NUM_OPERATIONS = 1000;

    for (int i = 0; i < NUM_OPERATIONS; ++i)
    {
        std::vector<int> data(1000, i); // 每个vector包含1000个元素
        queue.push(std::move(data));
    }

    int popped_count = 0;
    for (int i = 0; i < NUM_OPERATIONS; ++i)
    {
        auto item = queue.pop();
        if (item.has_value())
        {
            EXPECT_EQ(item.value().size(), 1000);
            popped_count++;
        }
    }

    EXPECT_EQ(popped_count, NUM_OPERATIONS);
}

// 边界条件测试
TEST_F(MPMCQueueTest, BoundaryConditions)
{
    MPMCQueue<int> queue;

    // 测试空队列pop
    auto result = queue.pop();
    EXPECT_FALSE(result.has_value());

    // 测试push/pop交替
    for (int i = 0; i < 100; ++i)
    {
        queue.push(i);
        auto item = queue.pop();
        EXPECT_TRUE(item.has_value());
        EXPECT_EQ(item.value(), i);
    }

    // 再次测试空队列
    result = queue.pop();
    EXPECT_FALSE(result.has_value());
}

// 长时间运行测试
TEST_F(MPMCQueueTest, LongRunningTest)
{
    MPMCQueue<int> queue;
    const int DURATION_MS = 5000; // 运行5秒
    std::atomic<bool> stop{false};

    std::thread producer([&]()
                         {
        int value = 0;
        auto start = std::chrono::steady_clock::now();
        while (!stop.load()) {
            queue.push(value++);
            // 短暂休息以避免过度生产
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        } });

    std::thread consumer([&]()
                         {
        int last_value = -1;
        auto start = std::chrono::steady_clock::now();
        while (!stop.load() || 
               std::chrono::steady_clock::now() - start < std::chrono::milliseconds(DURATION_MS + 100)) {
            auto item = queue.pop();
            if (item.has_value()) {
                EXPECT_GT(item.value(), last_value);
                last_value = item.value();
            } else {
                std::this_thread::yield();
            }
        } });

    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    stop.store(true);

    producer.join();
    consumer.join();

    std::cout << "Long running test completed successfully" << std::endl;
}