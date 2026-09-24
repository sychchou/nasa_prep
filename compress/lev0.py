from compress.common import TS_FMT


class Lev0Store:
    """compress_lev0: 120일치 원본을 전부 그대로 보관 (비교 기준선)."""

    def __init__(self):
        self.rows = []  # [(ts, biomarker, value), ...]

    def add(self, ts, biomarker, value):
        self.rows.append((ts, biomarker, value))

    def query(self, biomarker, start, end):
        return [(ts, v) for ts, b, v in self.rows
                if b == biomarker and start <= ts < end]

    def dump(self):
        """저장한다면 디스크에 쓰일 내용 (크기 비교용)."""
        lines = ["timestamp,biomarker,value"]
        for ts, b, v in self.rows:
            lines.append(f"{ts.strftime(TS_FMT)},{b},{v}")
        return "\n".join(lines).encode()
