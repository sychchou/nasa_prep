// lev0 ~ lev4 벤치마크.  실행: make run  (또는 ./build/benchmark data/biodata.csv)
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "compress/common.h"
#include "compress/lev0.h"
#include "compress/lev1.h"
#include "compress/lev2.h"
#include "compress/lev3.h"
#include "compress/lev4.h"

using Clock = std::chrono::steady_clock;

static double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

template <typename Store>
static double ingest(Store& store, const std::vector<Sample>& rows) {
    auto t0 = Clock::now();
    for (const Sample& r : rows) store.add(r.ts, r.biomarker, r.value);
    return ms_since(t0);
}

// 여러 번 돌려서 평균 (한 번은 너무 빨라서 측정 오차가 큼)
template <typename Store>
static double timed_query(const Store& store, const std::string& biomarker,
                          Minutes start, Minutes end, std::size_t& n_rows) {
    const int repeat = 50;
    auto t0 = Clock::now();
    for (int i = 0; i < repeat; ++i) n_rows = store.query(biomarker, start, end).size();
    return ms_since(t0) / repeat;
}

// 오래된 구간에서 원본 값 vs 그 시간 평균의 차이 (MAE). 다운샘플링으로 잃은 정보량.
static double downsample_error(const Lev0Store& lev0, const Lev1Store& lev1,
                               const std::string& biomarker) {
    std::unordered_map<Minutes, double> means;
    for (const Bucket& b : lev1.archive_rows(biomarker)) means[b.ts] = b.mean();

    double err = 0;
    std::size_t n = 0;
    for (const Point& p : lev0.series().at(biomarker)) {
        auto it = means.find(hour_bucket(p.ts));
        if (it == means.end()) continue;
        err += std::fabs(p.value - it->second);
        ++n;
    }
    return n ? err / n : 0;
}

struct Query {
    const char* name;
    std::string biomarker;
    Minutes start, end;
};

// lev2~4: lev1 내용을 바이트로 인코딩 -> 디코딩해서 크기/시간/복원 여부 확인
using Encoder = std::function<Bytes(const std::vector<Columns>&)>;
using Decoder = std::function<std::vector<Columns>(const Bytes&)>;

static std::vector<Columns> report_codec(const char* name, const Encoder& enc, const Decoder& dec,
                                         const std::vector<Columns>& cols, double base_size) {
    const int repeat = 5;
    Bytes bytes;
    auto t0 = Clock::now();
    for (int i = 0; i < repeat; ++i) bytes = enc(cols);
    double t_enc = ms_since(t0) / repeat;

    std::vector<Columns> back;
    t0 = Clock::now();
    for (int i = 0; i < repeat; ++i) back = dec(bytes);
    double t_dec = ms_since(t0) / repeat;

    std::printf("%-18s%11.1f%8.1fx%12.2f%12.2f   %s\n", name, bytes.size() / 1024.0,
                base_size / bytes.size(), t_enc, t_dec, back == cols ? "무손실" : "손실");
    return back;
}

template <typename Store>
static void report(const char* name, Store& store, const std::vector<Sample>& rows,
                   const std::vector<Query>& queries, double base_size) {
    double t_ingest = ingest(store, rows);
    double size = store.dump().size();
    std::printf("%-6s%11.1f%11.1f%8.1fx%11.1f", name, t_ingest, size / 1024,
                base_size > 0 ? base_size / size : 1.0, store.memory_bytes() / 1024.0);
    for (const Query& q : queries) {
        std::size_t n = 0;
        double t = timed_query(store, q.biomarker, q.start, q.end, n);
        std::printf("%14.3f / %-6zu", t, n);
    }
    std::printf("\n");
}

int main(int argc, char** argv) {
    std::string path = argc > 1 ? argv[1] : "data/biodata.csv";
    std::vector<Sample> rows = load_biodata(path);
    Minutes first = rows.front().ts, last = rows.back().ts;
    std::printf("입력: %zu 샘플, %s ~ %s\n\n", rows.size(),
                format_ts(first).c_str(), format_ts(last).c_str());

    std::vector<Query> queries = {
        {"최근 24h HR", "heart_rate", last - 24 * 60, last + 1},
        {"1월 1주 HR", "heart_rate", first, first + 7 * 24 * 60},
    };

    std::printf("%-6s%11s%11s%9s%11s", "", "ingest(ms)", "dump(KB)", "ratio", "mem(KB)");
    for (const Query& q : queries) std::printf("   %s(ms / rows)", q.name);
    std::printf("\n");

    Lev0Store lev0;
    Lev1Store lev1(30);
    report("lev0", lev0, rows, queries, 0);
    report("lev1", lev1, rows, queries, lev0.dump().size());

    std::printf("\nlev1 다운샘플링 오차 (원본 vs 시간평균, MAE):\n");
    for (const auto& [b, _] : lev0.series())
        std::printf("  %-24s %.4f\n", b.c_str(), downsample_error(lev0, lev1, b));

    // ---- lev2 ~ lev4: 저장 형식 ----
    std::vector<Columns> cols = to_columns(lev1);
    double base = lev0.dump().size();

    std::printf("\n%-18s%11s%9s%12s%12s   %s\n", "", "size(KB)", "ratio", "encode(ms)", "decode(ms)", "복원");
    std::printf("%-18s%11.1f%8.1fx%12s%12s   %s\n", "lev0 CSV", base / 1024, 1.0, "-", "-", "-");
    std::printf("%-18s%11.1f%8.1fx%12s%12s   %s\n", "lev1 CSV", lev1.dump().size() / 1024.0,
                base / lev1.dump().size(), "-", "-", "-");
    report_codec("lev2 delta", lev2::encode, lev2::decode, cols, base);
    std::vector<Columns> lossy = report_codec("lev3 +quant", lev3::encode, lev3::decode, cols, base);
    report_codec("lev4 +zlib", lev4::encode, lev4::decode, cols, base);

    // 참고: "그냥 zlib 만 쓰면?" / "양자화 없이 zlib 까지 하면?"
    std::string csv0 = lev0.dump();
    Bytes raw0(csv0.begin(), csv0.end());
    std::printf("%-18s%11.1f%8.1fx\n", "ref: lev0+zlib", lev4::zlib_compress(raw0).size() / 1024.0,
                base / lev4::zlib_compress(raw0).size());
    report_codec("ref: lev2+zlib",
                 [](const std::vector<Columns>& c) { return lev4::zlib_compress(lev2::encode(c)); },
                 [](const Bytes& b) { return lev2::decode(lev4::zlib_decompress(b)); }, cols, base);

    // ---- lev3 양자화 오차 (최근 30일 원본 기준) ----
    std::printf("\nlev3 양자화 오차 (최근 원본 기준, 이상치는 원본 유지):\n");
    std::printf("  %-24s%8s%10s%10s%10s\n", "", "step", "MAE", "max", "이상치");
    for (std::size_t k = 0; k < cols.size(); ++k) {
        const auto& orig = cols[k].value;
        const auto& got = lossy[k].value;
        double sum = 0, mx = 0;
        for (std::size_t i = 0; i < orig.size(); ++i) {
            double e = std::fabs(double(orig[i] - got[i])) / MILLI;
            sum += e;
            mx = std::max(mx, e);
        }
        lev3::Stats s = lev3::stats_of(orig);
        std::size_t n_out = std::count_if(orig.begin(), orig.end(),
                                          [&](std::int64_t v) { return lev3::is_outlier(v, s); });
        std::printf("  %-24s%8.3f%10.4f%10.4f%10zu\n", cols[k].name.c_str(),
                    lev3::quant_step(cols[k].name) / MILLI, orig.empty() ? 0 : sum / orig.size(), mx, n_out);
    }
}
