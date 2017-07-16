#include "queue.h"

#include <string>
#include <iostream>
#include <cassert>

int main() {

    LockFreeQueue<long> lock_free_queue{};

    long nr_elements = 12000;

    for(long n = 0; n < nr_elements; n++) {
        std::cout << "Placing in: " << n << std::endl;
        while(!lock_free_queue.try_enqueue(std::move(n)));
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
