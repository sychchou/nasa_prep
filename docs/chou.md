**TODO**
- [ ] compress_lev1: 120일 원본
- [ ] compress_lev2: 30일 링버퍼 (지난 기간은 다운 샘플링)
- [ ] compress_lev3: lev2 + 델타 인코딩
- [ ] compress_lev4: lev3 + 양자화 (이상치 구간 제외) -> 이상치 기준 정해야함
- [ ] compress_lev5: lev4 + gzip (zlib 링크)
- [ ] tests/bechmark.py
