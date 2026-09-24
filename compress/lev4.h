#pragma once
#include <stdexcept>
#include <vector>

#include <zlib.h>

#include "compress/codec.h"
#include "compress/lev3.h"

// compress_lev4: lev3 + zlib (deflate, gzip 과 같은 알고리즘. 헤더 형식만 다름)
//
// delta/양자화로 값이 작아지고 같은 패턴이 반복되니, 범용 압축기가 그 반복을 한 번 더 줄인다.
// 형식: [원본(lev3) 크기 varint] [zlib 압축 바이트]
namespace lev4 {

inline Bytes zlib_compress(const Bytes& raw, int level = Z_BEST_COMPRESSION) {
    Bytes out;
    write_varint(out, raw.size());  // 풀 때 버퍼 크기를 알아야 해서 먼저 적어둔다
    std::size_t header = out.size();

    uLongf n = compressBound(raw.size());
    out.resize(header + n);
    if (compress2(out.data() + header, &n, raw.data(), raw.size(), level) != Z_OK)
        throw std::runtime_error("zlib compress failed");
    out.resize(header + n);
    return out;
}

inline Bytes zlib_decompress(const Bytes& in) {
    std::size_t pos = 0;
    uLongf n = read_varint(in, pos);
    Bytes raw(n);
    if (uncompress(raw.data(), &n, in.data() + pos, in.size() - pos) != Z_OK || n != raw.size())
        throw std::runtime_error("zlib uncompress failed");
    return raw;
}

inline Bytes encode(const std::vector<Columns>& all) { return zlib_compress(lev3::encode(all)); }
inline std::vector<Columns> decode(const Bytes& in) { return lev3::decode(zlib_decompress(in)); }

}  // namespace lev4
