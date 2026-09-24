// lev0 vs lev1 벤치마크.  실행: make run  (또는 ./build/benchmark data/biodata.csv)
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "compress/common.hpp"
#include "compress/lev0.hpp"
#include "compress/lev1.hpp"

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
}
