/*
*   Implementation for the hazard pointer used for various other structures
*/
#pragma once

#include <atomic>
#include <list>

namespace ouraglyfi
{
    template<class T> class HazardPtrList

    template<class T>
    class HazardPtr {
    public:
        HazardPtr() noexcept
            : next(nullptr), is_active(true), data_ptr(nullptr)
            {}

        void remember(T * ptr) noexcept {
            this->data_ptr.store(ptr)
        }

        void release() noexcept {
            this->data_ptr.store(nullptr)
            this->is_active.store(false)
        }

    private:
        friend class HazardPtrList<T>
        //The next hazard pointer
        HazardPtr<T> *next;
        //Is at least one thread accessing this pointer
        std::atomic<bool> is_active;
        //The actual pointer to the data
        std::atomic<T*> data_ptr;
    }

    template<class T>
    class HazardPtrList {
    public:
        HazardPtr<T> &acquire() noexcept {
            bool inactive{false};
            for(auto &ptr : hazard_pointers) {
                //If the pointer is inactive
                if(!ptr->is_active.load()) {
                    //If someone else hasnt' done it first, activate it and return it
                    if(ptr->is_active.compare_exchange_strong(inactive, true, std::memory_order_seq_cst) {
                        return *ptr;
                    }
                }
            }
            //If at this point we could find no inactive(unused) hazard pointer we need to allocate a new one'
            auto ptr = new HazardPtr<T>{};
            static std::atomic<bool> busy = false;
            while(true) {
                auto not_busy = false;
                //Spinlock... should be really hard to reach, alternative is to spin on a manual list
                //comparing the head and the ptr...
                if(alone.compare_exchange_strong(not_busy, true)) {
                    hazard_pointers.push_back(ptr);
                    return hazard_pointers.front();
                }
            }
        }

        //Checks if the list contains this pointer as an active pointer
        bool contains(const T * const ptr) {
            for(auto &p : hazard_pointers) {
                if(p->data_ptr.load() == ptr && ptr->is_active.load()) {
                    return true;
                }
            }
            return false;
        }

    private:
        std::list< std::atomic<HazardPtr<T> *> > hazard_pointers;
    }
}
