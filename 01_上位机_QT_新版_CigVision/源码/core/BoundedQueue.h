#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace cigvision {

enum class QueueOverflowPolicy {
    RejectNewest = 0,
    DropOldest
};

enum class QueuePushResult {
    Pushed = 0,
    DroppedOldest,
    RejectedFull,
    RejectedClosed
};

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity,
        QueueOverflowPolicy overflowPolicy = QueueOverflowPolicy::RejectNewest)
        : capacity_(capacity), overflowPolicy_(overflowPolicy)
    {
        if (capacity_ == 0) {
            throw std::invalid_argument("BoundedQueue capacity must be greater than zero");
        }
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    QueuePushResult tryPush(const T& value)
    {
        return tryPushImpl(value);
    }

    QueuePushResult tryPush(T&& value)
    {
        return tryPushImpl(std::move(value));
    }

private:
    template <typename U>
    QueuePushResult tryPushImpl(U&& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) {
            return QueuePushResult::RejectedClosed;
        }

        QueuePushResult result = QueuePushResult::Pushed;
        if (items_.size() == capacity_) {
            if (overflowPolicy_ == QueueOverflowPolicy::RejectNewest) {
                return QueuePushResult::RejectedFull;
            }
            items_.pop_front();
            ++droppedCount_;
            result = QueuePushResult::DroppedOldest;
        }

        items_.emplace_back(std::forward<U>(value));
        available_.notify_one();
        return result;
    }

public:

    bool tryPop(T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (items_.empty()) {
            return false;
        }
        value = std::move(items_.front());
        items_.pop_front();
        return true;
    }

    bool waitPop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        available_.wait(lock, [this] { return closed_ || !items_.empty(); });
        if (items_.empty()) {
            return false;
        }
        value = std::move(items_.front());
        items_.pop_front();
        return true;
    }

    void close()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        available_.notify_all();
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

    std::size_t capacity() const noexcept
    {
        return capacity_;
    }

    std::size_t droppedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return droppedCount_;
    }

    bool isClosed() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

private:
    const std::size_t capacity_;
    const QueueOverflowPolicy overflowPolicy_;
    mutable std::mutex mutex_;
    std::condition_variable available_;
    std::deque<T> items_;
    std::size_t droppedCount_ = 0;
    bool closed_ = false;
};

} // namespace cigvision
