/*

    Just messing around atm please ignore

*/
#pragma once

namespace ouraglyfi
{

    template<class T>
    class Element {
    public:
        T value;
        T* next;
    }

    template<class T>
    class List {
    public:
        List() {}
        ~List() {}

        void push_back(const T& element) {

        }

        T pop_front() {

        }

        //Implement itterator

    private:
        T* head = nullptr;
    }

}
