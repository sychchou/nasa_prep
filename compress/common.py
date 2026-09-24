import csv
from datetime import datetime

TS_FMT = "%Y-%m-%d %H:%M"


def load_biodata(path):
    """biodata.csv -> [(datetime, biomarker, value), ...] 시간순 정렬.

    센서에서 실시간으로 들어오는 상황을 흉내내려고 시간순으로 정렬한다.
    member 는 지금 A 한 명뿐이라 생략.
    """
    rows = []
    with open(path, encoding="utf-8-sig") as f:  # utf-8-sig: 파일 맨 앞 BOM 제거
        for r in csv.DictReader(f):
            ts = datetime.strptime(r["timestamp"], TS_FMT)
            rows.append((ts, r["biomarker"], float(r["value"])))
    rows.sort(key=lambda r: r[0])
    return rows
