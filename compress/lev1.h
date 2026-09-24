#pragma once
#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "compress/common.h"
#include "compress/ring_buffer.h"

// 바이오마커별 하루 샘플 수 (biodata.csv 측정 간격 기준)
inline int samples_per_day(const std::string& biomarker) {
    static const std::map<std::string, int> table = {
        {"heart_rate", 1440},            // 1분
        {"spo2", 288},                   // 5분
        {"body_temp", 96},               // 15분
        {"cortisol", 4},                 // 6시간
        {"bone_resorption_marker", 1},   // 1일
    };
    return table.at(biomarker);
}

// 12:37 -> 12:00. 같은 시간대 샘플을 한 묶음으로 만든다.
inline Minutes hour_bucket(Minutes ts) { return ts - ts % 60; }

// 1시간 동안 밀려난 샘플들의 요약
struct Bucket {
    Minutes ts;  // 버킷 시작 시각 (정시)
    double sum, min, max;
    int n;
    double mean() const { return sum / n; }
};

// compress_lev1: 최근 30일은 원본(링버퍼), 그보다 오래된 건 1시간 단위로 다운샘플링.
//
//   새 샘플 ──push──> [ 링버퍼 30일치 원본 ] ──밀려난 샘플──> 1시간 버킷(mean/min/max) ──> archive
class Lev1Store {
public:
    explicit Lev1Store(int keep_days = 30) : keep_days_(keep_days) {}

    void add(Minutes ts, const std::string& biomarker, double value) {
        auto it = series_.find(biomarker);
        if (it == series_.end()) {
            std::size_t capacity = std::size_t(keep_days_) * samples_per_day(biomarker);
            it = series_.emplace(biomarker, Series(capacity)).first;
        }
        Series& s = it->second;

        if (auto evicted = s.recent.push({ts, value}))
            downsample(s, *evicted);
    }

    // [start, end) 구간. 오래된 구간은 시간별 평균, 최근 구간은 원본을 돌려준다.
    std::vector<Point> query(const std::string& biomarker, Minutes start, Minutes end) const {
        std::vector<Point> out;
        auto it = series_.find(biomarker);
        if (it == series_.end()) return out;
        const Series& s = it->second;

        for (const Bucket& b : archive_rows(s))
            if (start <= b.ts && b.ts < end) out.push_back({b.ts, b.mean()});
        for (std::size_t i = 0; i < s.recent.size(); ++i)
            if (start <= s.recent[i].ts && s.recent[i].ts < end) out.push_back(s.recent[i]);
        return out;
    }

    // 저장한다면 디스크에 쓰일 내용 (크기 비교용)
    std::string dump() const {
        std::string out = "# recent\ntimestamp,biomarker,value\n";
        for (const auto& [name, s] : series_)
            for (std::size_t i = 0; i < s.recent.size(); ++i)
                out += format_ts(s.recent[i].ts) + "," + name + "," +
                       std::to_string(s.recent[i].value) + "\n";

        out += "# archive\ntimestamp,biomarker,mean,min,max,n\n";
        char buf[128];
        for (const auto& [name, s] : series_)
            for (const Bucket& b : archive_rows(s)) {
                std::snprintf(buf, sizeof(buf), ",%.3f,%f,%f,%d\n", b.mean(), b.min, b.max, b.n);
                out += format_ts(b.ts) + "," + name + buf;
            }
        return out;
    }

    // 데이터가 차지하는 메모리 (컨테이너 오버헤드 제외)
    // 링버퍼는 비어 있어도 capacity 만큼 이미 잡혀 있다는 점에 주의.
    std::size_t memory_bytes() const {
        std::size_t n = 0;
        for (const auto& [name, s] : series_)
            n += s.recent.capacity() * sizeof(Point) + archive_rows(s).size() * sizeof(Bucket);
        return n;
    }

    // 닫힌 버킷 + 아직 채우는 중인 버킷까지
    std::vector<Bucket> archive_rows(const std::string& biomarker) const {
        auto it = series_.find(biomarker);
        return it == series_.end() ? std::vector<Bucket>{} : archive_rows(it->second);
    }

private:
    struct Series {
        explicit Series(std::size_t capacity) : recent(capacity) {}
        RingBuffer<Point> recent;          // 최근 30일 원본
        std::vector<Bucket> archive;       // 닫힌 버킷
        std::optional<Bucket> open_bucket; // 채우는 중인 버킷
    };

    // 밀려난 샘플을 1시간 버킷에 누적. 시간대가 바뀌면 이전 버킷을 archive 로 닫는다.
    static void downsample(Series& s, const Point& p) {
        Minutes key = hour_bucket(p.ts);

        if (s.open_bucket && s.open_bucket->ts != key) {
            s.archive.push_back(*s.open_bucket);
            s.open_bucket.reset();
        }

        if (!s.open_bucket) {
            s.open_bucket = Bucket{key, p.value, p.value, p.value, 1};
        } else {
            Bucket& b = *s.open_bucket;
            b.sum += p.value;
            b.min = std::min(b.min, p.value);
            b.max = std::max(b.max, p.value);
            b.n += 1;
        }
    }

    static std::vector<Bucket> archive_rows(const Series& s) {
        std::vector<Bucket> rows = s.archive;
        if (s.open_bucket) rows.push_back(*s.open_bucket);
        return rows;
    }

    int keep_days_;
    std::map<std::string, Series> series_;  // biomarker -> 링버퍼 + archive
};
