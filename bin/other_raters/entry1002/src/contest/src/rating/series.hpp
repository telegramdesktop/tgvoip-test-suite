#pragma once

#include <vector>
#include <memory>
#include <algorithm>



template <class T>
class Series {
private:
    static void deleter(T* ptr) {
        delete[] ptr;
    }


    std::shared_ptr<T> data;
    size_t length;

public:
    Series()
        : data(nullptr), length(0) {}

    template <class Iterator>
    explicit Series(Iterator begin, Iterator end)
        : Series(begin, end, end - begin) {}

    template <class Iterator>
    Series(Iterator begin, Iterator end, size_t size)
        : data(new T[size], deleter), length(size) {
        std::copy(begin, end, data.get());
        size_t payload_size = end - begin;
        if (size > payload_size)
            std::fill_n(data.get() + payload_size, size - payload_size, 0.0f);
    }

    explicit Series(size_t size)
        : data(new T[size], deleter), length(size) {
        std::fill_n(data.get(), size, 0.0f);
    }

    size_t size() const {
        return length;
    }

    T* begin() {
        return data.get();
    }

    const T* begin() const {
        return data.get();
    }

    T& operator[](const size_t pos) {
        return data.get()[pos];
    }

    T operator[](const size_t pos) const {
        return data.get()[pos];
    }

    Series copy() const {
        return Series(data.get(), data.get() + length);
    }

    Series copy(size_t new_len) const {
        return Series(data.get(), data.get() + length, new_len);
    }
};


using Signal = Series<float>;
