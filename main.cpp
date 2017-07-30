#include "scsp_fixed_queue.h"

#include <string>
#include <iostream>
#include <cassert>
#include <future>
#include <thread>
#include <atomic>
#include <memory>
#include <cstddef>

class TestValue {
public:
    //Meta info for testing purposes
    static std::atomic<uint64_t> move_count;
    static std::atomic<uint64_t> copy_count;
    static std::atomic<uint64_t> ctor_count;

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
    static void reset() {
        move_count = 0;
        copy_count = 0;
        ctor_count = 0;
    }

    TestValue& operator=(const TestValue& copy_from) {
        this->info = copy_from.info;
        copy_count++;
        return *this;
    };
};

std::atomic<uint64_t> TestValue::move_count = 0;
std::atomic<uint64_t> TestValue::copy_count = 0;
std::atomic<uint64_t> TestValue::ctor_count = 0;

void test_ScspFixedLockFreeQueue() {
    auto nr_elements_to_add = 480;
    ScspFixedLockFreeQueue<TestValue> queue(500);

    std::atomic<uint64_t> semaphor = 0;
    constexpr uint64_t nr_threads = 2;

    TestValue::reset();
    auto t1 = std::thread([&]() -> void {
                   semaphor++;
                   while(semaphor != nr_threads) {}
                   for(auto n = 0; n < nr_elements_to_add; n++) {
                       while(!queue.enqueue(TestValue(std::to_string(n))));
                   }
    });
    auto t2 = std::thread([&]() -> void {
                   semaphor++;
                   while(semaphor != nr_threads) {}
                   for(auto n = 0; n < nr_elements_to_add; n++) {
                       TestValue value;
                       while(!queue.dequeue(value));
                       assert(std::atoi(value.info.c_str()) < nr_elements_to_add);
                   }
    });
    t1.join();
    t2.join();
    std::cout << "During the test " << nr_elements_to_add << " were inserted and dequeued" << std::endl;
    std::cout << "TestValue was constructed: " << TestValue::ctor_count << " times" << std::endl;
    std::cout << "TestValue was copied: " << TestValue::copy_count << " times" << std::endl;
    std::cout << "TestValue was moved: " << TestValue::move_count << " times" << std::endl;
}

int main() {
    test_ScspFixedLockFreeQueue();
}

/*
LockFreeQueue<long> lock_free_queue{};

long nr_elements = 50000;

for(long n = 0; n < nr_elements; n++) {
    std::async(std::launch::async, [&lock_free_queue, n]() -> void {
                   std::cout << "Placing in: " << n << std::endl;
                   while(!lock_free_queue.try_enqueue(std::move(n)));
               });
}

for(long n = 0; n < nr_elements; n++) {
    long* result = nullptr;
    lock_free_queue.try_dequeue(result);
    if(result != nullptr) {
        std::cout << "Result was: " << *result << std::endl;
        //assert(*result == n);
    }
}
*/
