#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// 시간은 1970-01-01 00:00(UTC)부터 지난 "분" 으로 저장한다.
// 문자열보다 작고, 빼기/나머지로 구간 계산이 쉽다. (예: 시간 버킷 = t - t % 60)
using Minutes = std::int64_t;

struct Point {
    Minutes ts;
    double value;
};  // 16 bytes

struct Sample {
    Minutes ts;
    std::string biomarker;
    double value;
};

// "2026-01-01 00:15" -> Minutes
inline Minutes parse_ts(const std::string& s) {
    std::tm tm{};
    if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min) != 5)
        throw std::runtime_error("bad timestamp: " + s);
    tm.tm_year -= 1900;  // tm 은 1900 년 기준
    tm.tm_mon -= 1;      // tm 은 월이 0 부터
    return timegm(&tm) / 60;
}

// Minutes -> "2026-01-01 00:15"
inline std::string format_ts(Minutes t) {
    std::time_t secs = t * 60;
    std::tm tm{};
    gmtime_r(&secs, &tm);
    char buf[17];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

// biodata.csv -> 시간순 정렬된 Sample 목록.
// 센서에서 실시간으로 들어오는 상황을 흉내내려고 시간순으로 정렬한다.
// member 는 지금 A 한 명뿐이라 생략.
inline std::vector<Sample> load_biodata(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);

    std::vector<Sample> rows;
    std::string line;
    std::getline(f, line);  // 헤더 skip (맨 앞 BOM 도 여기서 같이 버려짐)
    while (std::getline(f, line)) {
        // timestamp,member,biomarker,value,tag
        std::stringstream ss(line);
        std::string ts, member, biomarker, value;
        std::getline(ss, ts, ',');
        std::getline(ss, member, ',');
        std::getline(ss, biomarker, ',');
        std::getline(ss, value, ',');
        rows.push_back({parse_ts(ts), biomarker, std::stod(value)});
    }
    std::stable_sort(rows.begin(), rows.end(),
                     [](const Sample& a, const Sample& b) { return a.ts < b.ts; });
    return rows;
}
