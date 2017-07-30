/*
*   A single producer single consumer lock free queue
*/
#pragma once

#include <atomic>
#include <vector>
#include <cstddef>
#include <algorithm>

#include <iostream>

template <class T>
class ScspFixedLockFreeQueue {
public:
    ScspFixedLockFreeQueue(size_t size): internal_size(size) {
        buffer = std::vector<T>(internal_size);
        buffer.reserve(internal_size);
        std::cout << buffer.size() << std::endl;
    }

    bool dequeue(T& read) {
        auto old_write_position = write_position.load();
        if(read_position == old_write_position)
            return false;
        read = std::move(buffer.at(read_position));
        read_position = next_position_in_buffer(read_position.load());
        return true;
    }

    bool enqueue(const T& to_write) {
        return enqueue(to_write);
    }

    bool enqueue(T&& write) {
        auto write_next = next_position_in_buffer(write_position.load());
        if(read_position.load() == write_next)
            return false;
        buffer.at(write_position.load()) = write;
        write_position.store(write_next);
        return true;
    }

private:
    inline size_t next_position_in_buffer(size_t pos) {
        return pos == (internal_size - 1) ? 0 : (pos + 1);
    }

    const size_t internal_size;
    std::vector<T> buffer;
    std::atomic<size_t> read_position = 0;
    std::atomic<size_t> write_position = 0;
};
