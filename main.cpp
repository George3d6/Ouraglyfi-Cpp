#include "lock_free_queue.h"

#include <string>
#include <iostream>
#include <cassert>
#include <future>
#include <thread>
#include <atomic>
#include <memory>
#include <cstddef>
#include <unordered_set>

class TestValue {
public:
    //Meta info for testing purposes
    static std::atomic<uint64_t> move_count;
    static std::atomic<uint64_t> copy_count;
    static std::atomic<uint64_t> ctor_count;

    static void reset() {
        move_count = 0;
        copy_count = 0;
        ctor_count = 0;
    }

    static void print_state() {
        std::cout << "TestValue was constructed: " << TestValue::ctor_count << " times" << std::endl;
        std::cout << "TestValue was copied: " << TestValue::copy_count << " times" << std::endl;
        std::cout << "TestValue was moved: " << TestValue::move_count << " times" << std::endl;
    }

    //The actual type
    TestValue(std::string info = ""): info(info){
        ctor_count++;
    };
    TestValue(const TestValue& copy_from) : info(copy_from.info) {
        copy_count++;
    };
    TestValue(TestValue&& move_from) noexcept : info(std::move(move_from.info)) {
        move_count++;
    };

    std::string info;
    std::vector<std::string> extra_garbage = {"extra", "garbage", "member"};

    TestValue& operator=(const TestValue& copy_from) {
        this->info = copy_from.info;
        copy_count++;
        return *this;
    };

    TestValue& operator=(const TestValue&& move_from) noexcept {
        this->info = std::move(move_from.info);
        move_count++;
        return *this;
    };
};

std::atomic<uint64_t> TestValue::move_count = 0;
std::atomic<uint64_t> TestValue::copy_count = 0;
std::atomic<uint64_t> TestValue::ctor_count = 0;

void test_ScspFixedLockFreeQueue() {
    std::cout << "\n\n------------------------------------------------------------------\n-  Testing the Single Consumer Single Producer Queue-\n"   <<
    "-------------------------------------------------------------------\n\n";
    static constexpr auto nr_elements_to_add = 300000;
    static constexpr auto queue_size = 500;
    ouraglyfi::FixedQueue<TestValue, false, false> queue(queue_size);
    std::atomic<uint64_t> semaphor = 0;
    constexpr uint64_t nr_threads = 2;

    TestValue::reset();
    auto t1 = std::thread([&]() -> void {
                   semaphor++;
                   while(semaphor != nr_threads) {}
                   for(auto n = 0; n < nr_elements_to_add; n++) {
                       if(n % 2 == 0) {
                           auto to_add = TestValue(std::to_string(n));
                           while(queue.enqueue(std::move(to_add)) != ouraglyfi::ReturnCode::Done);
                       } else {
                           while(queue.enqueue(TestValue(std::to_string(n)))  != ouraglyfi::ReturnCode::Done);
                       }
                   }
    });
    auto t2 = std::thread([&]() -> void {
                   semaphor++;
                   while(semaphor != nr_threads) {}
                   for(auto n = 0; n < nr_elements_to_add; n++) {
                       TestValue value;
                       while(queue.dequeue(value)  != ouraglyfi::ReturnCode::Done);
                       assert(std::atoi(value.info.c_str()) < nr_elements_to_add);
                   }
    });
    t1.join();
    t2.join();
    TestValue::print_state();
}

/*
    Test for Multi Consumer Single Producer queue
*/
void test_McspFixedLockFreeQueue() {
    std::cout << "\n\n------------------------------------------------------------------\n-  Testing the Multi Consumer Multi Producer Queue   -\n"
    << "-------------------------------------------------------------------\n\n";
    static constexpr auto nr_elements_to_add = 10000;
    static constexpr auto queue_size = 5;
    ouraglyfi::FixedQueue<TestValue, true, true> queue(queue_size);

    std::atomic<uint64_t> semaphor = 0;
    constexpr size_t nr_readers = 10;
    constexpr size_t nr_writers = 20;
    constexpr uint64_t nr_threads = nr_writers + nr_readers;

    std::vector<std::thread> reader_threads = {};
    std::atomic<uint64_t> total = 0;
    for(size_t n = 0; n < nr_readers; n++) {
        reader_threads.emplace_back(std::thread([&]() -> void {
            static std::atomic<uint64_t> this_print = 1;
            semaphor++;
            while(semaphor != nr_threads) {}
            auto has_read = 0;
            for(uint64_t iter = 0; iter < nr_elements_to_add*nr_writers; iter++) {
                TestValue value;
                has_read++;
                while(queue.dequeue(value) != ouraglyfi::ReturnCode::Done)  {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1000 * 3));
                }
                assert(uint64_t(std::atoi(value.info.c_str())) < nr_elements_to_add*nr_readers*nr_writers);
                total.fetch_add(uint64_t(std::atoi(value.info.c_str())));
            }
            auto output = std::to_string(this_print.fetch_add(1)) + " | :This reader has dequeued: " + std::to_string(has_read) +  " times\n";
            std::cout << output << std::endl;
        }));
    }

    std::vector<std::thread> writer_threads = {};
    for(size_t n = 0; n < nr_writers; n++) {
        std::atomic<int> my_number_is = 0;
        writer_threads.emplace_back(std::thread([&]() -> void {
            const auto writers_numbers = my_number_is.fetch_add(1);
            static std::atomic<uint64_t> this_print = 1;
            semaphor++;
            while(semaphor != nr_threads) {}
            uint64_t has_wrote = 0;
            for(uint64_t n = writers_numbers*nr_elements_to_add*nr_readers; n < nr_elements_to_add*nr_readers*(writers_numbers + 1); n++) {
            while(queue.enqueue(TestValue(std::to_string(n))) != ouraglyfi::ReturnCode::Done ) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(1000 * 3));
            }
            has_wrote++;
            }
            auto output =  std::to_string(this_print.fetch_add(1)) + " | This writer has enqueued: " + std::to_string(has_wrote) +  " times\n";
            std::cout << output << std::endl;
        }));
    }

    for(auto& thr : writer_threads) {
        thr.join();
    }
    for(auto& thr : reader_threads) {
        thr.join();
    }

    uint64_t expected = 0;
    for(uint64_t n = 0; n < nr_elements_to_add*nr_readers*nr_writers; n++) {
        expected += n;
    }

    std::cout << "The sum of the dequeued elements ios: " << total  << std::endl;
    std::cout << "The expected sum for the dequeued elements is: " << expected << std::endl;
    TestValue::print_state();
}

int main() {
    test_ScspFixedLockFreeQueue();
    test_McspFixedLockFreeQueue();
}
