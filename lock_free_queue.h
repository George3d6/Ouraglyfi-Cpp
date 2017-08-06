/*
*   A single producer single consumer lock free queue of fixed size (defined at compile time)
*/
#pragma once

#include <atomic>
#include <vector>

//Because I'm sure ouroboro is taken and it never actully catches the tail... I went to great pains to make sure of that
//I'm also thinking of replacing it with seralibre or just libre... but I'm not in a linguistic mood today, suggestions are apreciated
namespace ouraglyfi
{
    //Must be used in order to figure out the status after calling various operations on the queue
    enum struct ReturnCode {
        Done    = 0,
        Full    = 1,
        Empty   = 2,
        Busy    = 4
    };

    template <class T, bool multi_reader = false, bool multi_writer = false>
    class FixedQueue {
    public:
        FixedQueue(size_t size): internal_capacity(size + 1) {
            buffer = std::vector<T>(internal_capacity);
            buffer.reserve(internal_capacity);
        }

        //Can be safely queried to know the internal capacity of the queue
        const size_t internal_capacity;

        ReturnCode dequeue(T& read) noexcept {
            if constexpr (multi_reader) {
                auto free_to_go = false;
                if(!reading.compare_exchange_strong(free_to_go, true, sequential, sequential))
                    return ReturnCode::Busy;
            }

            if(read_position.load(relaxed) == write_position.load(relaxed)) {
                if constexpr (multi_reader)
                    reading.store(false, relaxed);
                return ReturnCode::Empty;
            }
            read =std::move(buffer.at(read_position.load(relaxed)));
            read_position.store(next_position_in_buffer(read_position.load(relaxed)), relaxed);
            if constexpr (multi_reader)
                reading.store(false, relaxed);
            return ReturnCode::Done;
        }

        ReturnCode enqueue(const T& write) noexcept {
            if constexpr (multi_writer) {
                if(auto free_to_go = false; !writing.compare_exchange_strong(free_to_go, true, sequential, sequential))
                    return ReturnCode::Busy;
            }
            auto write_next = next_position_in_buffer(write_position.load(relaxed));
            //Check if the queue is full
            if(read_position.load(relaxed) == write_next) {
                if constexpr (multi_writer)
                    writing.store(false, relaxed);
                return ReturnCode::Full;
            }
            buffer.at(write_position.load(relaxed)) = std::move(write);
            write_position.store(write_next, relaxed);
            if constexpr (multi_writer)
                writing.store(false, relaxed);
            return ReturnCode::Done;
        }

    private:
        inline size_t next_position_in_buffer(size_t pos) const noexcept {
            return pos == (internal_capacity - 1) ? 0 : (pos + 1);
        }

        inline size_t previous_position_in_buffer(size_t pos) const noexcept {
            return pos == 0 ? (internal_capacity - 1) : (pos - 1);
        }

        //
        std::vector<T> buffer;

        //Used internally for synchronization, user can't influence these at construction
        //The reading writting flag aren't always needed but a few extra bytes for the sake of kiss is ok with me
        std::atomic<bool> reading          = false;
        std::atomic<bool> writing          = false;
        std::atomic<size_t> read_position  = 0;
        std::atomic<size_t> write_position = 0;

        //Some internal defintions, not using since the memory ordering is defined as an enum
        static constexpr auto relaxed    = std::memory_order_relaxed;
        static constexpr auto sequential = std::memory_order_seq_cst;
    };
}
