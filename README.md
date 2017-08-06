#Ouraglyfi

This is a micro-library for implementing various lock free producer - consumer patterns in a concurrent environment.
Ouraglyfi has a [sister implementation](#) written in rust, so if you prefer that language consider checking it out

##FixedQueue
This is a fixed size, lock free, wait free, thread safe queue that allows for lock free dequeuing and enqueuing on a FIFO basis.

When construction the queue you can decide on the size (determined at runtime) and whether you want multiple producer and/or multiple consumers

###Examples of fixed queue types:
```
//A queue that can hold up to 100 elements and supports a single reader and a single writer
auto queue = ouraglyfi::FixedQueue<std::string, false, false>{100};

//A queue that can hold up to 3 elements and supports a single reader and multiple writers
auto queue = ouraglyfi::FixedQueue<std::string, false, true>{3};

//A queue that can hold up to 800 elements and supports multiple readers and multiple writers
auto queue = ouraglyfi::FixedQueue<std::string, true, true>{800};
```

###Usage:

```
    auto status = queue.enqueue(std::string{"Adding a string"});
    switch(status) {
        case ouraglyfi::ReturnCode::Done:
            //Successfully enqueued object, do whatever you wish to do next
        case ouraglyfi::ReturnCode::Full:
            //Could not enqueue object, the queue is full...
            //If you don't wish to run into this scenario often use a bigger queue size
        case ouraglyfi::ReturnCode::Busy
            //The queue is busy, someone else is writting to it
            //If you don't have a way of handling this simply write in a while loop or model your problem to use a single writer queue
    }
```

```
    auto std::string value_to_fill{};
    auto status = queue.dequeue(value_to_fill);
    switch(status) {
        case ouraglyfi::ReturnCode::Done:
            //'value_to_fill' now holds the dequeued object, use it as you wish, you have full ownership of it
        case ouraglyfi::ReturnCode::Empty:
            //Could not dequeued object, the queue is empty
            //There's really no way of avoiding this, it just means you have no more events to process for now
        case ouraglyfi::ReturnCode::Busy
            //The queue is busy, someone else is reading from it
            //If you don't have a way of handling this simply read in a loop or consider modeling your problem using a single reader
    }
```

###Notes about the implementation and possible changes:

The enqueue and dequeue methods can fail in a multi reader and/or writer scenario since the alternative seems to be mostly looping internally and I prefer
to leave that choice to the user, even if it adds overhead. I am considering adding some separate methods that can't fail with the "Busy" code and instead
loop internally (which can be implemented a tad bit more efficiently).

##Design philosophy

The purpose of this library was initially purely academical.

However I cam to realize there don't actually seem to be many easy to use, elegantly designed and easy to understand lock free data structures for C++.

Most popular implementations for lock free concurrent code seem to be either very long and arcane, old, inefficient or even straight out wrong.

As such I have set the following deisgn goals:

####Clean Clear Code
That means having a well documented code base that is small in size, has few dependencies and is easy to read.

####Modular
That means not having a separate class for every feature. If I can add a few lines of code that are compile-time generated to an existing piece of code
to extend it's functionality at the user's behest I will strive for that.

####Truly Lock Free
That includes calls to kernel mutexes. As long as the user provides reasonable type to work with (checks for that coming soon TM) there will be
no need to allocate memory internally.

By "reasonable types" I mean types with a move ctor and a move operator that don't allocate memory internally. As far as I know all std::containers
and most well designed data-holder object fulfill this requirement (and, of course, any 2/4/8 byte value does so by default as well).

###Practical and Easy to use
In the end I've designed this library to use on some of my own projects so I want it to be easy to use and reasonably efficient.

Currently there is a lot that can be done on the efficiency front but there is a line to be drawn between efficient and using byte masking to
treat a four atomics as a single value because of a few precious extra bit and one extra relaxed operation...

##Credit where credit is due
I've been helped in designing this by looking over code, books and blog posts from various people, the two that foremost come to mind are:
[Jeff Preshing](https://github.com/preshing), [Cameron](https://github.com/cameron314) and [Anthony Williams](https://github.com/anthonywilliams)

































##
