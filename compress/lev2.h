#pragma once
#include <vector>

#include "compress/codec.h"

// compress_lev2: lev1 + 델타 인코딩 (무손실)
//
// 형식:
//   [바이오마커 개수]
//   바이오마커마다: [이름] [ts] [value] [b_ts] [b_mean] [b_min] [b_max] [b_n]
//   각 열은 write_delta_column (개수 + zigzag varint 차이들)
namespace lev2 {

inline Bytes encode(const std::vector<Columns>& all) {
    Bytes out;
    write_varint(out, all.size());
    for (const Columns& c : all) {
        write_string(out, c.name);
        for (const auto* col : {&c.ts, &c.value, &c.b_ts, &c.b_mean, &c.b_min, &c.b_max, &c.b_n})
            write_delta_column(out, *col);
    }
    return out;
}

inline std::vector<Columns> decode(const Bytes& in) {
    std::size_t pos = 0;
    std::vector<Columns> all(read_varint(in, pos));
    for (Columns& c : all) {
        c.name = read_string(in, pos);
        for (auto* col : {&c.ts, &c.value, &c.b_ts, &c.b_mean, &c.b_min, &c.b_max, &c.b_n})
            *col = read_delta_column(in, pos);
    }
    return all;
}

}  // namespace lev2
