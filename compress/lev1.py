from compress.common import TS_FMT
from compress.ring_buffer import RingBuffer

# 바이오마커별 하루 샘플 수 (biodata.csv 측정 간격 기준)
SAMPLES_PER_DAY = {
    "heart_rate": 1440,            # 1분
    "spo2": 288,                   # 5분
    "body_temp": 96,               # 15분
    "cortisol": 4,                 # 6시간
    "bone_resorption_marker": 1,   # 1일
}


def hour_bucket(ts):
    """12:37 -> 12:00. 같은 시간대 샘플을 한 묶음으로 만든다."""
    return ts.replace(minute=0, second=0, microsecond=0)


class Lev1Store:
    """compress_lev1: 최근 30일은 원본(링버퍼), 그보다 오래된 건 1시간 단위로 다운샘플링.

        새 샘플 ──push──> [ 링버퍼 30일치 원본 ] ──밀려난 샘플──> 1시간 버킷(mean/min/max) ──> archive
    """

    def __init__(self, keep_days=30):
        self.keep_days = keep_days
        self.recent = {}      # biomarker -> RingBuffer[(ts, value)]
        self.archive = {}     # biomarker -> [(bucket_ts, mean, min, max, n), ...]  닫힌 버킷
        self.open_bucket = {} # biomarker -> [bucket_ts, sum, min, max, n]           채우는 중인 버킷

    def add(self, ts, biomarker, value):
        if biomarker not in self.recent:
            capacity = self.keep_days * SAMPLES_PER_DAY[biomarker]
            self.recent[biomarker] = RingBuffer(capacity)
            self.archive[biomarker] = []

        evicted = self.recent[biomarker].push((ts, value))
        if evicted is not None:
            self._downsample(biomarker, *evicted)

    def _downsample(self, biomarker, ts, value):
        """밀려난 샘플을 1시간 버킷에 누적. 시간대가 바뀌면 이전 버킷을 archive 로 닫는다."""
        key = hour_bucket(ts)
        cur = self.open_bucket.get(biomarker)

        if cur is not None and cur[0] != key:
            self._close_bucket(biomarker)
            cur = None

        if cur is None:
            self.open_bucket[biomarker] = [key, value, value, value, 1]
        else:
            cur[1] += value
            cur[2] = min(cur[2], value)
            cur[3] = max(cur[3], value)
            cur[4] += 1

    def _close_bucket(self, biomarker):
        key, total, lo, hi, n = self.open_bucket.pop(biomarker)
        self.archive[biomarker].append((key, total / n, lo, hi, n))

    def _archive_rows(self, biomarker):
        """닫힌 버킷 + 아직 채우는 중인 버킷까지."""
        yield from self.archive.get(biomarker, [])
        cur = self.open_bucket.get(biomarker)
        if cur is not None:
            key, total, lo, hi, n = cur
            yield (key, total / n, lo, hi, n)

    def query(self, biomarker, start, end):
        """[start, end) 구간. 오래된 구간은 시간별 평균, 최근 구간은 원본을 돌려준다."""
        out = [(ts, mean) for ts, mean, _, _, _ in self._archive_rows(biomarker)
               if start <= ts < end]
        out += [(ts, v) for ts, v in self.recent.get(biomarker, [])
                if start <= ts < end]
        return out

    def dump(self):
        """저장한다면 디스크에 쓰일 내용 (크기 비교용)."""
        lines = ["# recent", "timestamp,biomarker,value"]
        for b, ring in self.recent.items():
            for ts, v in ring:
                lines.append(f"{ts.strftime(TS_FMT)},{b},{v}")

        lines += ["# archive", "timestamp,biomarker,mean,min,max,n"]
        for b in self.recent:
            for ts, mean, lo, hi, n in self._archive_rows(b):
                lines.append(f"{ts.strftime(TS_FMT)},{b},{mean:.3f},{lo},{hi},{n}")
        return "\n".join(lines).encode()
