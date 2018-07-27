#pragma once


#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <native/task.h>
#include <native/timer.h>
#include <native/mutex.h>
#include <native/cond.h>


#include <mutex>
#include <condition_variable>

#include <rtdk.h>


#include <array>
#include <tuple>

#include <memory>






template<typename ThreadsafeInput, typename ThreadsafeOutput> class InputOutputObject
{
public:
    InputOutputObject() { }
    virtual ~InputOutputObject() { }

    virtual std::shared_ptr<ThreadsafeInput> get_input() = 0;

    virtual std::shared_ptr<const ThreadsafeInput> get_output() = 0;


};



namespace xenomai
{
class mutex
{
public:
    RT_MUTEX rt_mutex_;

    mutex()
    {
        rt_mutex_create(&rt_mutex_, NULL);
    }

    void lock()
    {
        rt_mutex_acquire(&rt_mutex_, TM_INFINITE);

    }

    void unlock()
    {
        rt_mutex_release(&rt_mutex_);
    }

};

//template<typename Mutex> class unique_lock
//{
//    Mutex& mutex_;
//public:
//    unique_lock(Mutex& mutex)
//    {
//        mutex_ = mutex;
//    }
//    ~unique_lock()
//    {
//        std::unique_lock<mutex>(mutex_);
//        mutex_.unlock();
//    }
//};



class condition_variable
{
public:
    RT_COND rt_condition_variable_;


    condition_variable()
    {
        rt_cond_create(&rt_condition_variable_, NULL);
    }

    void wait(std::unique_lock<mutex>& lock )
    {
        lock.release();
        rt_cond_wait(&rt_condition_variable_, &lock.mutex()->rt_mutex_, TM_INFINITE);
    }

    void notify_all()
    {
        rt_cond_broadcast(&rt_condition_variable_);
    }
};

}



template<typename ...Types> class ThreadsafeObject
{
public:
    template<int INDEX> using Type
    = typename std::tuple_element<INDEX, std::tuple<Types...>>::type;

    static const std::size_t SIZE = sizeof...(Types);

private:

    std::shared_ptr<std::tuple<Types ...> > data_;

    mutable std::shared_ptr<RT_COND> condition_;
    mutable std::shared_ptr<RT_MUTEX> condition_mutex_;
    std::shared_ptr<std::array<long unsigned, SIZE>> modification_counts_;
    std::shared_ptr<long unsigned> total_modification_count_;

    std::shared_ptr<std::array<xenomai::mutex, SIZE>> data_mutexes_;


public:
    ThreadsafeObject()
    {
        // initialize shared pointers ------------------------------------------
        data_ = std::make_shared<std::tuple<Types ...> >();
        condition_ = std::make_shared<RT_COND>();
        condition_mutex_ = std::make_shared<RT_MUTEX>();
        modification_counts_ = std::make_shared<std::array<long unsigned, SIZE>>();
        total_modification_count_ = std::make_shared<long unsigned>();
        data_mutexes_ = std::make_shared<std::array<xenomai::mutex, SIZE>>();




        // mutex and cond variable stuff ---------------------------------------
        rt_cond_create(condition_.get(), NULL);
        rt_mutex_create(condition_mutex_.get(), NULL);

        for(size_t i = 0; i < SIZE; i++)
        {
            (*modification_counts_)[i] = 0;
        }
        *total_modification_count_ = 0;





    }

    template<int INDEX> Type<INDEX> get()
    {
//        std::unique_lock<xenomai::mutex> lock((*data_mutexes_)[INDEX]);
        (*data_mutexes_)[INDEX].lock();
//        rt_mutex_acquire(&(*data_mutexes_)[INDEX], TM_INFINITE);
        Type<INDEX> datum = std::get<INDEX>(*data_);
//        rt_mutex_release(&(*data_mutexes_)[INDEX]);
        (*data_mutexes_)[INDEX].unlock();


        return datum;
    }

    template<int INDEX> void set(Type<INDEX> datum)
    {
        (*data_mutexes_)[INDEX].lock();

//        rt_mutex_acquire(&(*data_mutexes_)[INDEX], TM_INFINITE);
        std::get<INDEX>(*data_) = datum;
//        rt_mutex_release(&(*data_mutexes_)[INDEX]);
        (*data_mutexes_)[INDEX].unlock();


        // this is a bit suboptimal since we always broadcast on the same condition
        rt_mutex_acquire(condition_mutex_.get(), TM_INFINITE);
        (*modification_counts_)[INDEX] += 1;
        *total_modification_count_ += 1;
        rt_cond_broadcast(condition_.get());
        rt_mutex_release(condition_mutex_.get());
    }

    void wait_for_datum(unsigned index)
    {
        rt_mutex_acquire(condition_mutex_.get(), TM_INFINITE);
        long unsigned initial_modification_count = (*modification_counts_)[index];

        while(initial_modification_count == (*modification_counts_)[index])
        {
            rt_cond_wait(condition_.get(), condition_mutex_.get(), TM_INFINITE);
        }

        if(initial_modification_count + 1 != (*modification_counts_)[index])
        {
            rt_printf("size: %d, \n other info: %s \n", SIZE, __PRETTY_FUNCTION__ );

            rt_printf("something went wrong, we missed a message.");
            rt_printf(" SIZE: %d, initial_modification_count: %d, current modification count: %d\n",
                      SIZE, initial_modification_count, (*modification_counts_)[index]);


            exit(-1);
        }

        rt_mutex_release(condition_mutex_.get());
    }

    long unsigned wait_for_datum()
    {
        rt_mutex_acquire(condition_mutex_.get(), TM_INFINITE);

        std::array<long unsigned, SIZE> initial_modification_counts = *modification_counts_;
        long unsigned initial_modification_count = *total_modification_count_;

        while(initial_modification_count == *total_modification_count_)
        {
            rt_cond_wait(condition_.get(), condition_mutex_.get(), TM_INFINITE);
        }

        if(initial_modification_count + 1 != *total_modification_count_)
        {
            rt_printf("size: %d, \n other info: %s \n", SIZE, __PRETTY_FUNCTION__ );

            rt_printf("something went wrong, we missed a message.");
            rt_printf("initial_modification_count: %d, current modification count: %d\n",
                      initial_modification_count, *total_modification_count_);
            exit(-1);
        }

        int modified_index = -1;
        for(size_t i = 0; i < SIZE; i++)
        {
            if(initial_modification_counts[i] + 1 == (*modification_counts_)[i])
            {
                if(modified_index != -1)
                {
                    rt_printf("something in the threadsafe object went horribly wrong\n");
                    exit(-1);
                }

                modified_index = i;
            }
            else if(initial_modification_counts[i] != (*modification_counts_)[i])
            {
                rt_printf("something in the threadsafe object went horribly wrong\n");
                exit(-1);
            }
        }

        rt_mutex_release(condition_mutex_.get());
        return modified_index;
    }

};
