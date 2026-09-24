class RingBuffer:
    """고정 크기 원형 버퍼.

    - 크기가 capacity 로 고정되어 절대 늘어나지 않는다.
    - 꽉 찬 상태에서 push 하면 가장 오래된 값을 덮어쓰고, 밀려난 값을 반환한다.

    capacity=4 일 때 1,2,3,4,5 를 넣으면:
        buf  = [5, 2, 3, 4]   <- 1 자리에 5 가 덮어써짐
        head = 1              <- 다음에 쓸 자리 = 지금 가장 오래된 값(2)의 자리
        push(5) 의 반환값 = 1
    """

    def __init__(self, capacity):
        self.capacity = capacity
        self.buf = [None] * capacity  # 자리를 미리 다 만들어 둔다
        self.head = 0                 # 다음에 쓸 위치
        self.size = 0                 # 현재 들어있는 개수 (최대 capacity)

    def push(self, item):
        evicted = None
        if self.size == self.capacity:
            # 꽉 찼으면 head 자리에 있는 게 가장 오래된 값 -> 밀려난다
            evicted = self.buf[self.head]
        else:
            self.size += 1

        self.buf[self.head] = item
        self.head = (self.head + 1) % self.capacity  # 끝에 닿으면 0 으로 돌아감 (원형)
        return evicted

    def __iter__(self):
        """오래된 것 -> 최신 순으로 꺼낸다."""
        start = (self.head - self.size) % self.capacity
        for i in range(self.size):
            yield self.buf[(start + i) % self.capacity]

    def __len__(self):
        return self.size
