**TODO**
- [x] compress_lev0: 120일 원본
- [x] compress_lev1: 30일 링버퍼 (지난 기간은 다운 샘플링)
- [ ] compress_lev2: lev1 + 델타 인코딩
- [ ] compress_lev3: lev2 + 양자화 (이상치 구간 제외) -> 이상치 기준 정해야함
- [ ] compress_lev4: lev3 + gzip (zlib 링크)
- [ ] tests/benchmark.cpp
