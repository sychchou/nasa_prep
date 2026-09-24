#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "compress/lev1.h"

// lev2~lev4 가 공통으로 쓰는 바이트 인코딩 도구.
//
// lev1 의 내용을 "정수 열(column)" 로 바꾼 뒤 열 단위로 압축한다.
// 같은 종류의 값끼리 모여 있어야 차이(delta)가 작고 규칙적이라 압축이 잘 된다.

using Bytes = std::vector<std::uint8_t>;

// 값은 소수점 3자리까지라서 1000 배 한 정수(milli 단위)로 바꿔도 손실이 없다.
constexpr double MILLI = 1000.0;
inline std::int64_t to_milli(double v) { return std::llround(v * MILLI); }

// 한 바이오마커의 lev1 내용을 정수 열로 펼친 것
struct Columns {
    std::string name;
    // recent: 링버퍼 원본
    std::vector<std::int64_t> ts, value;  // value 는 milli 단위
    // archive: 1시간 버킷
    std::vector<std::int64_t> b_ts, b_mean, b_min, b_max, b_n;

    bool operator==(const Columns& o) const {
        return name == o.name && ts == o.ts && value == o.value && b_ts == o.b_ts &&
               b_mean == o.b_mean && b_min == o.b_min && b_max == o.b_max && b_n == o.b_n;
    }
};

inline std::vector<Columns> to_columns(const Lev1Store& store) {
    std::vector<Columns> out;
    for (const std::string& name : store.names()) {
        Columns c;
        c.name = name;
        for (const Point& p : store.recent_points(name)) {
            c.ts.push_back(p.ts);
            c.value.push_back(to_milli(p.value));
        }
        for (const Bucket& b : store.archive_rows(name)) {
            c.b_ts.push_back(b.ts);
            c.b_mean.push_back(to_milli(b.mean()));  // 평균만 반올림 (lev1 dump 와 같은 정밀도)
            c.b_min.push_back(to_milli(b.min));
            c.b_max.push_back(to_milli(b.max));
            c.b_n.push_back(b.n);
        }
        out.push_back(std::move(c));
    }
    return out;
}

// ---- zigzag: 부호 있는 정수 -> 부호 없는 정수 ----
// 0,-1,1,-2,2 ... -> 0,1,2,3,4 ...
// 작은 음수도 작은 양수가 되어야 varint 가 짧아진다. (-1 을 그대로 쓰면 0xFFFF...FF 로 10바이트)
inline std::uint64_t zigzag_encode(std::int64_t x) {
    return (std::uint64_t(x) << 1) ^ std::uint64_t(x >> 63);  // x>>63: 음수면 모든 비트 1, 양수면 0
}
inline std::int64_t zigzag_decode(std::uint64_t z) {
    return std::int64_t(z >> 1) ^ -std::int64_t(z & 1);
}

// ---- varint: 작은 수는 적은 바이트로 ----
// 7비트씩 끊어 쓰고, 각 바이트의 최상위 비트(0x80)는 "뒤에 더 있음" 표시.
//   1   -> 01
//   300 -> AC 02   (300 = 0b10_0101100 -> 0101100|0x80, 10)
inline void write_varint(Bytes& out, std::uint64_t x) {
    while (x >= 0x80) {
        out.push_back(std::uint8_t(x) | 0x80);
        x >>= 7;
    }
    out.push_back(std::uint8_t(x));
}
inline std::uint64_t read_varint(const Bytes& in, std::size_t& pos) {
    std::uint64_t x = 0;
    for (int shift = 0;; shift += 7) {
        if (pos >= in.size()) throw std::runtime_error("varint: unexpected end");
        std::uint8_t b = in[pos++];
        x |= std::uint64_t(b & 0x7F) << shift;
        if (!(b & 0x80)) return x;
    }
}

inline void write_signed(Bytes& out, std::int64_t x) { write_varint(out, zigzag_encode(x)); }
inline std::int64_t read_signed(const Bytes& in, std::size_t& pos) {
    return zigzag_decode(read_varint(in, pos));
}

// ---- delta 열: 개수, 그다음 "앞 값과의 차이" 들 ----
// 첫 값은 0 과의 차이로 쓰면 특별 처리가 필요 없다.
// 1분 간격 ts: 0,1,2,3... -> 0,1,1,1...  -> 전부 1바이트
inline void write_delta_column(Bytes& out, const std::vector<std::int64_t>& col) {
    write_varint(out, col.size());
    std::int64_t prev = 0;
    for (std::int64_t x : col) {
        write_signed(out, x - prev);
        prev = x;
    }
}
inline std::vector<std::int64_t> read_delta_column(const Bytes& in, std::size_t& pos) {
    std::vector<std::int64_t> col(read_varint(in, pos));
    std::int64_t prev = 0;
    for (std::int64_t& x : col) {
        x = prev + read_signed(in, pos);
        prev = x;
    }
    return col;
}

inline void write_string(Bytes& out, const std::string& s) {
    write_varint(out, s.size());
    out.insert(out.end(), s.begin(), s.end());
}
inline std::string read_string(const Bytes& in, std::size_t& pos) {
    std::size_t n = read_varint(in, pos);
    if (pos + n > in.size()) throw std::runtime_error("string: unexpected end");
    std::string s(in.begin() + pos, in.begin() + pos + n);
    pos += n;
    return s;
}
