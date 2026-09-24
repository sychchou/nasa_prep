#pragma once
#include <cstddef>
#include <optional>
#include <vector>

// 고정 크기 원형 버퍼.
//
// - 생성할 때 capacity 만큼 메모리를 한 번 잡고, 그 뒤로 절대 늘어나지 않는다.
// - 꽉 찬 상태에서 push 하면 가장 오래된 값을 덮어쓰고, 밀려난 값을 반환한다.
//
// capacity=4 일 때 1,2,3,4,5 를 넣으면:
//     buf_  = [5, 2, 3, 4]   <- 1 자리에 5 가 덮어써짐
//     head_ = 1              <- 다음에 쓸 자리 = 지금 가장 오래된 값(2)의 자리
//     push(5) 의 반환값 = 1
//
// std::array 대신 std::vector 를 쓰는 이유: 바이오마커마다 capacity 가 달라서
// 컴파일 타임에 크기를 못 정한다. 대신 생성자에서 한 번만 할당하고 resize 하지 않는다.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity) : buf_(capacity) {}

    std::optional<T> push(const T& item) {
        std::optional<T> evicted;
        if (size_ == buf_.size()) {
            evicted = buf_[head_];  // 꽉 찼으면 head_ 자리에 있는 게 가장 오래된 값 -> 밀려난다
        } else {
            ++size_;
        }
        buf_[head_] = item;
        head_ = (head_ + 1) % buf_.size();  // 끝에 닿으면 0 으로 돌아감 (원형)
        return evicted;
    }

    // i=0 이 가장 오래된 값, i=size()-1 이 최신 값
    const T& operator[](std::size_t i) const {
        std::size_t start = (head_ + buf_.size() - size_) % buf_.size();
        return buf_[(start + i) % buf_.size()];
    }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return buf_.size(); }

private:
    std::vector<T> buf_;
    std::size_t head_ = 0;  // 다음에 쓸 위치
    std::size_t size_ = 0;  // 현재 들어있는 개수 (최대 capacity)
};
