#pragma once
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "compress/codec.h"

// compress_lev3: lev2 + 양자화 (이상치는 원본 정밀도 유지, 손실 압축)
//
// 값을 step 단위로 반올림하면 (예: 심박 73.733 -> 73.5) 차이값이 작아지고 같은 값이 반복되어
// 바이트가 줄어든다. 대신 이상치는 정확한 값이 중요하니 따로 원본을 보관한다.
//
// 값 열 하나의 형식:
//   [step] [양자화 인덱스 열 (delta)] [이상치 개수] [(이상치 위치 delta, 원본 값) ...]
// 복원: value = 인덱스 * step, 그다음 이상치 위치만 원본 값으로 덮어쓴다.
namespace lev3 {

// ---- 임시 기준 (나중에 정해야 함) ----

// 양자화 간격 (milli 단위). 최대 오차 = step / 2
inline std::int64_t quant_step(const std::string& biomarker) {
    static const std::map<std::string, std::int64_t> table = {
        {"heart_rate", 500},             // 0.5 bpm
        {"spo2", 100},                   // 0.1 %
        {"body_temp", 20},               // 0.02 °C
        {"cortisol", 100},               // 0.1
        {"bone_resorption_marker", 5},   // 0.005
    };
    return table.at(biomarker);
}

// 이상치: 최근 30일 원본의 평균에서 3σ 넘게 벗어난 값
constexpr double OUTLIER_SIGMA = 3.0;

struct Stats {
    double mean = 0, sd = 0;
};

inline Stats stats_of(const std::vector<std::int64_t>& v) {
    Stats s;
    if (v.empty()) return s;
    for (std::int64_t x : v) s.mean += x;
    s.mean /= v.size();
    for (std::int64_t x : v) s.sd += (x - s.mean) * (x - s.mean);
    s.sd = std::sqrt(s.sd / v.size());
    return s;
}

inline bool is_outlier(std::int64_t v, const Stats& s) {
    return std::fabs(v - s.mean) > OUTLIER_SIGMA * s.sd;
}

// ---- 값 열 하나 인코딩/디코딩 ----

inline void write_quantized_column(Bytes& out, const std::vector<std::int64_t>& col,
                                   std::int64_t step, const Stats& s) {
    std::vector<std::int64_t> q(col.size());
    std::vector<std::size_t> outliers;
    for (std::size_t i = 0; i < col.size(); ++i) {
        q[i] = std::llround(double(col[i]) / step);
        if (is_outlier(col[i], s)) outliers.push_back(i);
    }

    write_varint(out, step);
    write_delta_column(out, q);  // 이상치 자리도 양자화 값을 넣어둔다 (delta 가 튀지 않게)

    write_varint(out, outliers.size());
    std::size_t prev = 0;
    for (std::size_t i : outliers) {
        write_varint(out, i - prev);  // 위치도 delta 로
        write_signed(out, col[i]);
        prev = i;
    }
}

inline std::vector<std::int64_t> read_quantized_column(const Bytes& in, std::size_t& pos) {
    std::int64_t step = read_varint(in, pos);
    std::vector<std::int64_t> col = read_delta_column(in, pos);
    for (std::int64_t& x : col) x *= step;

    std::size_t n_outliers = read_varint(in, pos), i = 0;
    for (std::size_t k = 0; k < n_outliers; ++k) {
        i += read_varint(in, pos);
        col.at(i) = read_signed(in, pos);
    }
    return col;
}

// ---- 전체 ----
// 형식은 lev2 와 같고, 값 열(value, b_mean, b_min, b_max)만 양자화 열로 쓴다.
// ts, b_ts, b_n 은 정수라 그대로 delta.

inline Bytes encode(const std::vector<Columns>& all) {
    Bytes out;
    write_varint(out, all.size());
    for (const Columns& c : all) {
        std::int64_t step = quant_step(c.name);
        Stats s = stats_of(c.value);  // 이상치 기준은 최근 원본으로 잡는다

        write_string(out, c.name);
        write_delta_column(out, c.ts);
        write_quantized_column(out, c.value, step, s);
        write_delta_column(out, c.b_ts);
        for (const auto* col : {&c.b_mean, &c.b_min, &c.b_max})
            write_quantized_column(out, *col, step, s);
        write_delta_column(out, c.b_n);
    }
    return out;
}

inline std::vector<Columns> decode(const Bytes& in) {
    std::size_t pos = 0;
    std::vector<Columns> all(read_varint(in, pos));
    for (Columns& c : all) {
        c.name = read_string(in, pos);
        c.ts = read_delta_column(in, pos);
        c.value = read_quantized_column(in, pos);
        c.b_ts = read_delta_column(in, pos);
        for (auto* col : {&c.b_mean, &c.b_min, &c.b_max})
            *col = read_quantized_column(in, pos);
        c.b_n = read_delta_column(in, pos);
    }
    return all;
}

}  // namespace lev3
