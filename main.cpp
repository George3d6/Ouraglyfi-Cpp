#include "queue_two.h"

#include <string>
#include <iostream>
#include <cassert>
#include <future>

int main() {

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

}
