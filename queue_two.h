#include <vector>
#include <atomic>
#include <memory>
#include <thread>
#include <iostream>
#include <cmath>

template<class T>
class LockFreeQueue {
private:

    //Some pre-defined values

    static const int buffer_size = 100;

    class Buffer {
    public:
        T* buffer;
        std::atomic<int> state;
        std::atomic<int> nr;
        Buffer* next;
        Buffer* previous;

        Buffer* get_next() {
            state.store(1);
            if(state == 1) {
                return;
            }
            auto new_buffer = new Buffer(nr + 1);
            this->next = new_buffer;
            new_buffer->previous = this;
            return new_buffer;
        }

        Buffer(int my_number) {
            nr.store(my_number);
            state.store(0);
            buffer = (T*) malloc(part_size * sizeof(T));
            next = nullptr;
            previous = nullptr;
        }

        Buffer(Buffer&& old): buffer(old.buffer) {
            state.store(old.state);
        }
    };

    Buffer* current_buffer;
    std::atomic<int> in_position;
    std::atomic<int> out_position;

public:

    AQueue() {
        current_buffer = new Buffer(0);
        in_position.store(-1);
        out_position.store(-1);
        current_buffer->get_next();
        std::thread([]() -> void {
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(2s);
                        if(current_buffer->next == nullptr) {
                            current_buffer->get_next();
                        }
                    });
    }

    void enqueue(T obj, int index = -1) {
        if(index == -1)
            index = in_position.fetch_add(1);
        auto buffer_nr = std::floor(index/buffer_size);
        auto insertion_buffer = current_buffer.load();
        if(buffer_nr == insertion_buffer->nr && insertion_buffer != nullptr) {
            insertion_buffer[index % buffer_size] = obj;
            enqueue(obj);
        }
        /* needed to make it truely lock "free" ?
        if(current_buffer->next == nullptr) {
            auto allocation_thread = std::thread([]() -> void {
                current_buffer->get_next();
            });
            allocation_thread.detach();
        }
        */
        //auto buffer_nr_should_be = std::floor(in_position/buffer_size);
        //insertion_buffer.nr.compare_exchange_strong(insertion_buffer_nr, buffer_nr_should_be);
    }

    void dequeue(T*& item) {
        auto get = out_position;
        if(out_position >= in_position)
            item = nullptr;

    }
};
