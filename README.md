Simple lib to extract path from announcement ID (segment ID)

===========================================================
31/10/2022 hthngoc@tma.com.vn
    libPA.a v1.0.2
        - generate ODD|EVEN|SEQ annID by type
        - fix bug in PA::decode* functions: memset ret buffer

===========================================================
18/10/2022 hthngoc@tma.com.vn
    libPA.a v1.0.1
        - support registering DFM range address

===========================================================
13/09/2022 hthngoc@tma.com.vn, init lib
    libPA.a v1.0.0
        - get, set, dump ID range (PAG, REC, ...)
        - decode ID to get hidden path by IPG rule (system, basic, DFM/PAG/xxx/xxx/xxx, ...)
        - get fileStat

===========================================================
Build:
    cd src; make clean all install

===========================================================
Sample usage:
    cd sample; make clean all
    ./SamplePA.exe 000000000092 0001 29999999999