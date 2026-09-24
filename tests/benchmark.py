"""lev0 vs lev1 벤치마크.  실행: python3 tests/benchmark.py"""
import sys
import time
from datetime import timedelta
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from compress.common import load_biodata
from compress.lev0 import Lev0Store
from compress.lev1 import Lev1Store, hour_bucket


def ingest(store, rows):
    t0 = time.perf_counter()
    for ts, b, v in rows:
        store.add(ts, b, v)
    return time.perf_counter() - t0


def timed_query(store, biomarker, start, end, repeat=5):
    t0 = time.perf_counter()
    for _ in range(repeat):
        result = store.query(biomarker, start, end)
    return (time.perf_counter() - t0) / repeat, len(result)


def downsample_error(lev0, lev1, biomarker):
    """오래된 구간에서 원본 값 vs 그 시간 평균의 차이 (MAE). 다운샘플링으로 잃은 정보량."""
    means = {ts: mean for ts, mean, *_ in lev1._archive_rows(biomarker)}
    errs = [abs(v - means[hour_bucket(ts)])
            for ts, b, v in lev0.rows
            if b == biomarker and hour_bucket(ts) in means]
    return sum(errs) / len(errs)


def main():
    rows = load_biodata(ROOT / "data" / "biodata.csv")
    last = rows[-1][0]
    print(f"입력: {len(rows):,} 샘플, {rows[0][0]} ~ {last}\n")

    lev0, lev1 = Lev0Store(), Lev1Store(keep_days=30)
    stores = {"lev0": lev0, "lev1": lev1}

    queries = {
        "최근 24h HR": ("heart_rate", last - timedelta(days=1), last + timedelta(minutes=1)),
        "1월 1주 HR": ("heart_rate", rows[0][0], rows[0][0] + timedelta(days=7)),
    }

    print(f"{'':6}{'ingest(s)':>10}{'size(KB)':>11}{'ratio':>8}", end="")
    for name in queries:
        print(f"{name + '(ms/rows)':>22}", end="")
    print()

    base_size = None
    for name, store in stores.items():
        t_ingest = ingest(store, rows)
        size = len(store.dump())
        base_size = base_size or size
        print(f"{name:6}{t_ingest:>10.3f}{size / 1024:>11.1f}{base_size / size:>7.1f}x", end="")
        for biomarker, start, end in queries.values():
            t, n = timed_query(store, biomarker, start, end)
            print(f"{f'{t * 1000:.1f} / {n}':>22}", end="")
        print()

    print("\nlev1 다운샘플링 오차 (원본 vs 시간평균, MAE):")
    for b in lev1.recent:
        print(f"  {b:24} {downsample_error(lev0, lev1, b):.4f}")


if __name__ == "__main__":
    main()
