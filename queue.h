#include <vector>
#include <atomic>
#include <memory>
#include <thread>
#include <iostream>
#include <iostream>

template<class T>
class LockFreeQueue {
public:

    //Some pre-defined values
    const int buffer_state_ready = 0;
    const int buffer_state_used = 1;
    const int in_position_invalid = -2;

    static const int part_size = 100;
    const int pool_size = 2;

private:

class Buffer {
    public:
    T* buffer;
    // 0 == free, 1 == used
    std::atomic<int> state;
    Buffer() {
        state.store(0);
        buffer = (T*) malloc(part_size * sizeof(T));
    }

    Buffer(Buffer&& old): buffer(old.buffer) {
        state.store(old.state);
    }
};

public:

std::atomic<int> in_position = -1;
std::atomic<int> out_position = -1;

T* in;
std::vector<T*> transition;
std::vector<Buffer> pool;

LockFreeQueue() {
        for(auto n = 0; n < pool_size; n++) {
                pool.emplace_back(Buffer{});
        }
        pool[0].state = buffer_state_used;
        in = pool[0].buffer;
}

void clean_pool() {

}

bool try_enqueue(T obj) {
        //This is when we fail... shouldn't be often or at all unless we are very unlucky
        if(in_position == in_position_invalid) { return false; }
        //Have we reached the second to last index ?, stop insertion by making in_position == -2
        int compare = 99;
        if(in_position.compare_exchange_strong(compare, in_position_invalid)) {
                if(compare == in_position_invalid) {
                    return false;
                }
                bool found_buffer = false;
                //This takes a bit of extra time to allocate, since I don't want to have a separate allocation thread
                //Maybe in the future a new clean_in_non_blocking_alloc() that launches an async alloc should be made
                for(auto& buffer: pool) {
                        int buffer_state_ready_compare = buffer_state_ready;
                        if(buffer.state.compare_exchange_strong(buffer_state_ready_compare, buffer_state_used)) {
                                transition.emplace_back(std::move(in));
                                in = buffer.buffer;
                                in_position = -1;
                                found_buffer = true;
                                pool.emplace_back(Buffer{});
                        }
                }
                if(!found_buffer) {
                    pool.emplace_back(Buffer{});
                    transition.emplace_back(std::move(in));
                    in = pool.end()->buffer;
                    in_position = -1;
                }
        }
        if(compare == in_position_invalid) { return false; }
        //Add to the queue
        auto index = in_position.fetch_add(1);
        in[index] = std::move(obj);
        return true;
}

bool try_dequeue(T*& item) {
        auto index = out_position.fetch_add(1);
        //Check the item in the transition blocks based on the index, the index / 100 is the index of the block, % 100 is the pos in the block
        const size_t first_access = index/100;
        if(first_access >= transition.size()) {
            return false;
        }
        const size_t second_access = index%100;
        item = &(transition[first_access][second_access]);
        return true;
}
};
