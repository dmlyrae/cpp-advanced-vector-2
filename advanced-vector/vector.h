#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        capacity_ = other.capacity_;
        other.capacity_ = 0;
        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::move(rhs.buffer_);
            capacity_ = std::move(rhs.capacity_);
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }
    
    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    /**
     * Конструкторы
     */
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    /**
     * Итераторы
     */

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    /**
     * Операторы
     */

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ < size_) {
                    std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                } else {
                    std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }
        Swap(rhs);
        return *this;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    /**
     * Методы
     */

    void Swap(Vector& other) noexcept {
        std::swap(size_, other.size_);
        data_.Swap(other.data_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void PopBack() /* noexcept */ {
        if (size_ < 1) {
            return;
        }
        std::destroy_at(data_.GetAddress() + size_--);
    };

    template<typename ...Args>
    iterator Emplace(const_iterator pos, Args && ...args) {
        assert(pos >= begin() && pos <= end());

        size_t index = pos - begin();

        if (data_.Capacity() > size_) {
            try {
                if (pos != end()) {
                    T new_s(std::forward<Args>(args)...);
                    new (end()) T(std::forward<T>(data_[size_ - 1]));

                    std::move_backward(begin() + index, end() - 1, end());
                    *(begin() + index) = std::forward<T>(new_s);
                } else {
                    new (end()) T(std::forward<Args>(args)...);
                }
            } catch (...) {
                operator delete (end());
                throw;
            }
        } else {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            new (new_data.GetAddress() + index) T(std::forward<Args>(args)...);

            if constexpr (!std::is_nothrow_move_constructible_v<T> && std::is_copy_constructible_v<T>) {
                std::uninitialized_copy_n(data_.GetAddress(), index, new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + index, size_ - index, new_data.GetAddress() + index + 1);
            } else {
                std::uninitialized_move_n(data_.GetAddress(), index, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + index, size_ - index, new_data.GetAddress() + index + 1);
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        size_++;
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    };

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    };

    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());
        size_t shift = pos - begin();
        std::move(begin() + shift + 1, end(), begin() + shift);
        PopBack();
        return begin() + shift;
    }

    template<typename B>
    void PushBack(B&& value) {
        EmplaceBack(std::forward<B>(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ != Capacity()) {
            T* r = new (data_ + size_) T(std::forward<Args>(args)...);
            size_++;
            return *r;
        }
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        T* result = new (new_data + size_) T(std::forward<Args>(args)...);
        if constexpr (!std::is_nothrow_move_constructible_v<T> && std::is_copy_constructible_v<T>) {
            try {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress() + size_, 1);
                throw;
            }
        } else {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
        return *result;
    };

    void Resize(size_t new_size) {
        if (new_size == size_) {
            return;
        }
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        std::swap(size_, new_size);
    };

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);

        // Конструируем элементы в new_data, копируя их из data_
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);

        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};