#ifndef LOCK_FREE_QUEUE_H_
#define LOCK_FREE_QUEUE_H_

#include <cstddef> //size_t
#include <stdexcept>
#include <vector>
#include <atomic>


namespace DSPJIT
{
    /*
    *
    *  Lock free ONE Consumer - ONE Provider Queue
    *
    *
    *
    *   -> [back|||||||||||||||front] ->
    *   enqueue                        dequeue
    */

    template <class T>
    class lock_free_queue
    {
        static_assert(std::atomic<std::size_t> ::is_always_lock_free);
    public:
        lock_free_queue(const size_t capacity);
        lock_free_queue(const lock_free_queue<T> &) = delete;
        lock_free_queue(const lock_free_queue<T> &&) = delete;
        ~lock_free_queue() noexcept;

        size_t capacity() const noexcept;

        bool enqueue(const T&) noexcept;
        bool enqueue(T &&) noexcept;

        bool dequeue(T&) noexcept;
    private:
        const std::size_t m_capacity;
        std::atomic<std::size_t> m_write_ptr;
        std::atomic<std::size_t> m_read_ptr;
        std::vector<T> m_data;
    };


    template <class T>
    lock_free_queue<T>::lock_free_queue(const size_t capacity)
    :   m_capacity(capacity),
        m_write_ptr(1u),
        m_read_ptr(0u),
        m_data(capacity)
    {
        if (capacity < 3)
            throw std::domain_error("Invalid Queue capacity (size < 3)");
    }

    template <class T>
    lock_free_queue<T>::~lock_free_queue() noexcept
    {
    }

    template <class T>
    size_t lock_free_queue<T>::capacity() const noexcept
    {
        return m_capacity;
    }

    template <class T>
    bool lock_free_queue<T>::enqueue(const T& x) noexcept
    {
        const size_t read_ptr = m_read_ptr.load();
        const size_t write_ptr = m_write_ptr.load();

        if (read_ptr == write_ptr)
            return false;

        m_data[write_ptr] = x;
        const size_t next_write_ptr = (write_ptr + 1) % m_capacity;
        m_write_ptr.store(next_write_ptr);

        return true;
    }

    template <class T>
    bool lock_free_queue<T>::enqueue(T && x) noexcept
    {
        const size_t read_ptr = m_read_ptr.load();
        const size_t write_ptr = m_write_ptr.load();

        if (read_ptr == write_ptr)    //    queue is full
            return false;

        m_data[write_ptr] = std::move(x);
        const size_t next_write_ptr = (write_ptr + 1) % m_capacity;
        m_write_ptr.store(next_write_ptr);

        return true;
    }

    template <class T>
    bool lock_free_queue<T>::dequeue(T & x) noexcept
    {
        const size_t read_ptr = m_read_ptr.load();
        const size_t write_ptr = m_write_ptr.load();
        const size_t next_read_ptr = (read_ptr + 1) % m_capacity;

        if (next_read_ptr == write_ptr)     //    queue is empty
            return false;


        x = std::move(m_data[next_read_ptr]);
        m_read_ptr.store(next_read_ptr);

        return true;
    }

} /* namespace DSPJIT */

#endif