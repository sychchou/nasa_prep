#pragma once
#include <map>
#include <string>
#include <vector>

#include "compress/common.hpp"

// compress_lev0: 120일치 원본을 전부 그대로 보관 (비교 기준선).
class Lev0Store {
public:
    void add(Minutes ts, const std::string& biomarker, double value) {
        series_[biomarker].push_back({ts, value});
    }

    // [start, end) 구간
    std::vector<Point> query(const std::string& biomarker, Minutes start, Minutes end) const {
        std::vector<Point> out;
        auto it = series_.find(biomarker);
        if (it == series_.end()) return out;
        for (const Point& p : it->second)
            if (start <= p.ts && p.ts < end) out.push_back(p);
        return out;
    }

    // 저장한다면 디스크에 쓰일 내용 (크기 비교용)
    std::string dump() const {
        std::string s = "timestamp,biomarker,value\n";
        for (const auto& [b, points] : series_)
            for (const Point& p : points)
                s += format_ts(p.ts) + "," + b + "," + std::to_string(p.value) + "\n";
        return s;
    }

    // 데이터가 차지하는 메모리 (컨테이너 오버헤드 제외)
    std::size_t memory_bytes() const {
        std::size_t n = 0;
        for (const auto& [b, points] : series_) n += points.size() * sizeof(Point);
        return n;
    }

    const std::map<std::string, std::vector<Point>>& series() const { return series_; }

private:
    std::map<std::string, std::vector<Point>> series_;  // biomarker -> 시간순 원본
};
