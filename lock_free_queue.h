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
            internal_capacity.store(size, relaxed);
            buffer = std::vector<T>(internal_capacity.load(relaxed));
        }

        ReturnCode dequeue(T& read) noexcept {
            if constexpr (multi_reader || variable_size) {
                auto free_to_go = false;
                if(!reading.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel))
                    return ReturnCode::Busy;
            }

            if(read_position.load(relaxed) >= write_position.load(relaxed)) {
                if constexpr (multi_reader || variable_size)
                    reading.store(false, relaxed);
                return ReturnCode::Empty;
            }
            read = std::move(buffer[read_position.load(relaxed) % internal_capacity.load(relaxed)]);
            read_position.fetch_add(1);
            if constexpr (multi_reader || variable_size)
                reading.store(false, relaxed);
            return ReturnCode::Done;
        }

        /*
        template <class Container>
        ReturnCode dequeue_bulk(Container& read, size_t to_read) noexcept {
            if constexpr (multi_reader || variable_size) {
                auto free_to_go = false;
                if(!reading.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel))
                    return ReturnCode::Busy;
            }

            if( (read_position.load(relaxed)  + to_read - 1) >= write_position.load(relaxed)) {
                if constexpr (multi_reader || variable_size)
                    reading.store(false, relaxed);
                return ReturnCode::Empty;
            }
            auto start = ((read_position - 1) % internal_capacity.load(relaxed));
            auto end = ((read_position + to_read -1) % internal_capacity.load(relaxed));
            auto start_it = buffer.begin() + start;
            auto end_it = buffer.begin() + end;
            if (std::distance(end_it, buffer.end()) < 0) {
                std::move(start_it, buffer.end(), std::back_inserter(read));
                start_it = buffer.end();
            }
            std::move(start_it, end_it, std::back_inserter(read));
            read_position.fetch_add(to_read);
            if constexpr (multi_reader || variable_size)
                reading.store(false, relaxed);
            return ReturnCode::Done;
        }
        */

        ReturnCode peek(T& read) noexcept {
            if constexpr (multi_reader || variable_size) {
                auto free_to_go = false;
                if(!reading.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel))
                    return ReturnCode::Busy;
            }

            if(read_position.load(relaxed) >= write_position.load(relaxed)) {
                if constexpr (multi_reader || variable_size)
                    reading.store(false, relaxed);
                return ReturnCode::Empty;
            }
            read = buffer[read_position.load(relaxed)  % internal_capacity.load(relaxed)];
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
            //Check if the queue is full
            if constexpr (variable_size) {
                while((read_position.load(relaxed) + internal_capacity) == write_position.load(relaxed)) {

                    auto new_cap = internal_capacity.load(relaxed) * 2;
                    auto new_buffer = std::vector<T>(new_cap);

                    //Spinlock-here to block all reading
                    auto free_to_go = false;
                    while(!reading.compare_exchange_strong(free_to_go, true, seq_acq, seq_rel)) { free_to_go = false; }

                    size_t n = 0;
                    size_t pos = read_position.load(relaxed);
                    for(size_t i = 0; i < buffer.size(); i++) {
                        if(pos == write_position.load(relaxed))
                            break;
                        new_buffer.at(n) = std::move(buffer[pos % internal_capacity.load(relaxed)]);
                        pos += 1;
                        n++;
                    }

                    buffer = std::move(new_buffer);

                    internal_capacity.store(new_cap, relaxed);
                    read_position.store(0, relaxed);
                    write_position.store(n, relaxed);
                    reading.store(false, relaxed);
                }
            }
            if constexpr (!variable_size) {
                if((read_position.load(relaxed) + internal_capacity) == write_position.load(relaxed)) {
                    if constexpr (multi_writer)
                        writing.store(false, relaxed);
                    return ReturnCode::Full;
                }
            }
            buffer[write_position.load(relaxed) % internal_capacity.load(relaxed)] = std::move(value);
            write_position.fetch_add(1);
            if constexpr (multi_writer)
                writing.store(false, relaxed);
            return ReturnCode::Done;
        }
/*
        template <class Container>
        ReturnCode enqueue_bulk(Container elements_to_enqueue) {

        }
*/
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
        std::atomic<size_t> internal_capacity;
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
