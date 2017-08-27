/*
*   A single producer single consumer lock free queue of fixed size (defined at compile time)
*/
#pragma once
#include <atomic>
#include <vector>

namespace ouraglyfi
{
    //Must be used in order to figure out the status after calling various operations on the queue
    enum struct ReturnCode {
        Done    = 0,
        Full    = 1,
        Empty   = 2,
        Busy    = 4
    };

    template <class T, bool multi_reader = false, bool multi_writer = false, bool variable_size = false>
    class FixedQueue {
    public:
        FixedQueue(size_t size) {
            internal_capacity.store(size + 1);
            buffer = std::vector<T>(internal_capacity.load(relaxed));
        }

        ReturnCode dequeue(T& read) noexcept {
            if constexpr (multi_reader || variable_size) {
                auto free_to_go = false;
                if(!reading.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel))
                    return ReturnCode::Busy;
            }

            if(read_position.load(relaxed) == write_position.load(relaxed)) {
                if constexpr (multi_reader || variable_size)
                    reading.store(false, relaxed);
                return ReturnCode::Empty;
            }
            read = std::move(buffer[read_position.load(relaxed)]);
            read_position.store(next_position_in_buffer(read_position.load(relaxed)), relaxed);
            if constexpr (multi_reader || variable_size)
                reading.store(false, relaxed);
            return ReturnCode::Done;
        }

        ReturnCode peek(T& read) noexcept {
            if constexpr (multi_reader || variable_size) {
                auto free_to_go = false;
                if(!reading.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel))
                    return ReturnCode::Busy;
            }

            if(read_position.load(relaxed) == write_position.load(relaxed)) {
                if constexpr (multi_reader || variable_size)
                    reading.store(false, relaxed);
                return ReturnCode::Empty;
            }
            read = buffer[read_position.load(relaxed)];
            if constexpr (multi_reader || variable_size)
                reading.store(false, relaxed);
            return ReturnCode::Done;
        }

        ReturnCode enqueue(const T& value) noexcept {
            if constexpr (multi_writer) {
                auto free_to_go = false;
                if(!writing.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel))
                    return ReturnCode::Busy;
            }
            auto write_next = next_position_in_buffer(write_position.load(relaxed));
            //Check if the queue is full
            if constexpr (variable_size) {
                while(read_position.load(relaxed) == write_next) {
                    //Spinlock-here
                    auto free_to_go = false;
                    while(!reading.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel)) { free_to_go = false; }
                    auto new_cap = internal_capacity.load(relaxed) * 2;
                    auto old_buffer = std::move(buffer);
                    buffer = std::vector<T>(new_cap);

                    size_t n = 0;
                    size_t pos = read_position.load(relaxed);
                    for(size_t i = 0; i < old_buffer.size(); i++) {
                        if(pos == write_position.load(relaxed))
                            break;
                        buffer.at(n) = std::move(old_buffer[pos]);
                        pos = next_position_in_buffer(pos);
                        n++;
                    }

                    internal_capacity.store(new_cap, relaxed);
                    read_position.store(0, relaxed);
                    write_position.store(n, relaxed);
                    write_next = next_position_in_buffer(write_position.load(relaxed));
                    reading.store(false, relaxed);
                }
            }
            if constexpr (!variable_size) {
                if(read_position.load(relaxed) == write_next) {
                    if constexpr (multi_writer)
                        writing.store(false, relaxed);
                    return ReturnCode::Full;
                }
            }
            buffer[write_position.load(relaxed)] = std::move(value);
            write_position.store(write_next, relaxed);
            if constexpr (multi_writer)
                writing.store(false, relaxed);
            return ReturnCode::Done;
        }

        size_t size() const noexcept {
            if(read_position.load(relaxed) >  write_position.load(relaxed))
                return internal_capacity.load(relaxed) - read_position.load(relaxed) + write_position.load(relaxed);
            return write_position.load(relaxed) - read_position.load(relaxed);
        }

        size_t capacity() const noexcept {
            return internal_capacity.load(relaxed);
        }

        size_t max_size() const noexcept {
            if constexpr (!variable_size)
                return buffer.max_size();
            return capacity();
        }

        bool empty() const {
            if(size() == 0)
                return true;
            return false;
        }

    private:
        size_t next_position_in_buffer(size_t pos) const noexcept {
            return pos == (internal_capacity.load(relaxed) - 1) ? 0 : (pos + 1);
        }

        std::atomic<size_t> internal_capacity = 0;
        std::vector<T> buffer;

        //Used internally for synchronization, user can't influence these at construction
        //The reading writting flag aren't always needed but a few extra bytes for the sake of kiss is ok with me
        std::atomic<bool> reading          = false;
        std::atomic<bool> writing          = false;
        std::atomic<size_t> read_position  = 0;
        std::atomic<size_t> write_position = 0;

        //Some internal defintions, not using since the memory ordering is defined as an enum
        static constexpr auto relaxed = std::memory_order_relaxed;
        static constexpr auto seq_cst = std::memory_order_seq_cst;
        static constexpr auto seq_acq = std::memory_order_acquire;
        static constexpr auto seq_rel = std::memory_order_release;
    };
}
