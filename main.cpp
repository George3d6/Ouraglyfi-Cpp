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
    TestValue(std::string info = "Nan"): info(info){
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
    TestValue::reset();
    std::cout << "\n\n------------------------------------------------------------------\n-  Testing the Single Consumer Single Producer Queue-\n"   <<
    "-------------------------------------------------------------------\n\n";
    static constexpr auto nr_elements_to_add = 10000;
    static constexpr auto queue_size = 500;
    ouraglyfi::FixedQueue<TestValue, false, false> queue(queue_size);
    std::atomic<uint64_t> semaphor = 0;
    constexpr uint64_t nr_threads = 2;
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
    TestValue::reset();
    std::cout << "\n\n------------------------------------------------------------------\n-  Testing the Multi Consumer Multi Producer Queue   -\n"
    << "-------------------------------------------------------------------\n\n";
    static constexpr auto queue_size = 500;
    ouraglyfi::FixedQueue<TestValue, true, true, true> queue(queue_size);

    std::atomic<uint64_t> semaphor = 0;
    constexpr size_t nr_readers = 20;
    constexpr size_t nr_writers = 10;
    constexpr uint64_t nr_threads = nr_writers + nr_readers;
    static constexpr auto nr_elements_to_add = 1000 * 60 * nr_readers * nr_writers;

    std::atomic<uint64_t> expected = 0;
    std::vector<std::thread> writer_threads = {};
    for(size_t n = 0; n < nr_writers; n++) {
        std::atomic<uint64_t> this_print = 1;
        writer_threads.emplace_back(std::thread([&]() -> void {
            auto my_nr = this_print.fetch_add(1);
            semaphor++;
            while(semaphor != nr_threads) {}
            //auto out =  "Starting writer nr: " + std::to_string(my_nr) + "\n";
            //std::cout << out;
            uint64_t has_wrote = 0;
            for(uint64_t n = 0; n < nr_elements_to_add/nr_writers; n++) {
            expected.fetch_add(uint64_t(n));
            while(queue.enqueue(TestValue(std::to_string(n))) != ouraglyfi::ReturnCode::Done ) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(1000 * 30));
            }
            has_wrote++;
            }
            auto output =  std::to_string(my_nr) + " | This writer has enqueued: " + std::to_string(has_wrote) +  " times\n";
            std::cout << output << std::endl;
        }));
    }

    std::vector<std::thread> reader_threads = {};
    std::atomic<uint64_t> total = 0;
    for(size_t n = 0; n < nr_readers; n++) {
        std::atomic<uint64_t> this_print = 1;
        reader_threads.emplace_back(std::thread([&]() -> void {
            auto my_nr = this_print.fetch_add(1);
            semaphor++;
            while(semaphor != nr_threads) {}
            //auto out =  "Starting reader nr: " + std::to_string(my_nr) + "\n";
            //std::cout << out;
            auto has_read = 0;
            for(uint64_t iter = 0; iter < nr_elements_to_add/(nr_readers); iter++) {
                TestValue value;
                has_read++;
                queue.peek(value);
                while(queue.dequeue(value) != ouraglyfi::ReturnCode::Done)  {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1000 * 30));
                }
                if(value.info == "Nan") {
                    std::cout << "NOT A NUMBER\n!" << std::endl;
                }
                assert(uint64_t(std::atoi(value.info.c_str())) < nr_elements_to_add);
                total.fetch_add(uint64_t(std::atoi(value.info.c_str())));
            }
            auto output = std::to_string(my_nr) + " | :This reader has dequeued: " + std::to_string(has_read) +  " times\n";
            std::cout << output << std::endl;
        }));
    }

    for(auto& thr : writer_threads) {
        thr.join();
    }

    for(auto& thr : reader_threads) {
        thr.join();
    }

    std::cout << "The sum of the dequeued elements is: " << total  << std::endl;
    std::cout << "The expected sum for the enqueued elements is: " << expected << std::endl;
    assert(total == expected);
    TestValue::print_state();
}

int main() {
    for(auto i = 0; i < 30; i++) {
        test_ScspFixedLockFreeQueue();
        test_McspFixedLockFreeQueue();
    }
}
