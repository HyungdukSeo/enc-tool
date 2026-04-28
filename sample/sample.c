#include "PaIncl.h"
#include <iostream>
#include <assert.h>

#define PA_FMT_DIGIT_1                     "%04d"
#define PA_FMT_DIGIT_3                     "%011ld"
using namespace PA;
    DFMInfo range[DFM_TYPE_MAX] = {        
          DFMInfo(DFM_TYPE_PAG)
        , DFMInfo(DFM_TYPE_REC)
        , DFMInfo(DFM_TYPE_TTS)
        , DFMInfo(DFM_TYPE_RBT)
    };
    
void testNoInit() {
    Boolean ret = uFalse;
    
    uLogApp(LOG_DEB0, "----------BEGIN LOCAL MODE --------------");
    ret = PA::DFM::set(PA::DFM_TYPE_PAG, 00, 99);
    assert(ret && "ASSERT: SET NEW PAG RANGE OK");
    
    ret = PA::DFM::set(PA::DFM_TYPE_REC, 100, 200);
    assert(ret && "ASSERT: SET NEW REC RANGE OK");
    
    ret = PA::DFM::set(PA::DFM_TYPE_TTS, 0300, 400);
    assert(!ret && "ASSERT: SET NEW TTS RANGE OK");
    
    ret = PA::DFM::set(PA::DFM_TYPE_RBT, 500, 600);
    assert(ret && "ASSERT: SET NEW RBT RANGE OK");
    
    PA::DFM::dump();
    
    // get
    std::cout << "\n\t RET_PA decode2 =" << PA::decode("00000000065", "kor", "alaw", "vCOMP", "/SI/Ann");
    std::cout << "\n\t RET_PA decode2 =" << PA::decode("00000000165", "kor", "alaw", "vCOMP", "/SI/Ann");
    
}


void testIDRange() {
    Boolean ret = uFalse;
    Boolean cases[20];
    
    uLogApp(LOG_DEB0, "----------BEGIN test INIT--------------");
    ret = PA::DFM::init((void*)&ret, sizeof(ret), uTrue);
    assert(!ret && "ASSERT: INVALID ADDR");
    
    ret = PA::DFM::init((void*)&range, sizeof(range), uFalse);
    PA::DFM::dump();
    assert(ret && "ASSERT: OK ADDR");
    memset(cases, 0, sizeof(cases));
    
    ret = PA::DFM::init((void*)&range, sizeof(range), uTrue);
    PA::DFM::dump();
    assert(ret && "ASSERT: OK ADDR");
    
    uLogApp(LOG_DEB0, "----------BEGIN test RANGE ID--------------");
    #define IF(_x) \
        if (!cases[_x]) { \
        uLogApp(LOG_DEB0, "      BEGIN CASE %d", _x);
    
    #define END(_x) \
        uLogApp(LOG_DEB0, "      END CASE %d", _x); \
        cases[_x] = uTrue; goto _reset;}

    ret = PA::DFM::set(PA::DFM_TYPE_REC, 00, 99);
    assert(ret && "ASSERT: SET NEW FIRST RANGE OK");
    
    ret = PA::DFM::set(PA::DFM_TYPE_PAG, 03000, 2000);
    assert(ret && "ASSERT: SET NEW SECOND RANGE OK");
_reset:
    PA::DFM::dump();
    PA::DFM::init((void*)range, sizeof(range), uTrue);
    ret = PA::DFM::set(PA::DFM_TYPE_PAG, 1000, 2000);
    assert(ret && "ASSERT: SET NEW RANGE OK");
    ret = PA::DFM::set(PA::DFM_TYPE_REC, 10, 50);
    assert(ret && "ASSERT: SET NEW RANGE OK");
    
    PA::DFM::dump();
    
    // overlap suite
    IF(0)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 10, 50);
        assert(!ret && "ASSERT: SET NEW RANGE 1a swap");
    END(0)
    IF(1)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 10, 20);
        assert(!ret && "ASSERT: SET NEW RANGE 1b");
    END(1)
    IF(2)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 10, 100);
        assert(!ret && "ASSERT: SET NEW RANGE 1c");
    END(2)
            
    IF(3)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 0, 20);
        assert(!ret && "ASSERT: SET NEW RANGE 2a");
    END(3)
    IF(4)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 0, 50);
        assert(!ret && "ASSERT: SET NEW RANGE 2b");
    END(4)
    IF(5)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 0, 100);
        assert(!ret && "ASSERT: SET NEW RANGE 2c");
    END(5)
    IF(6)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 0, 1500);
        assert(!ret && "ASSERT: SET NEW RANGE 2?");
    END(6)
            
    IF(7)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 20, 50);
        assert(!ret && "ASSERT: SET NEW RANGE 3a");
    END(7)
    IF(8)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 20, 100);
        assert(!ret && "ASSERT: SET NEW RANGE 3b");
    END(8)
    IF(9)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 20, 200);
        assert(!ret && "ASSERT: SET NEW RANGE 3c");
    END(9)  
    IF(10)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 200, 400);
        assert(ret && "ASSERT: SET NEW RANGE 3d");
    END(10)
    IF(11)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 200, 200);
        assert(ret && "ASSERT: SET NEW RANGE x");
    END(11)
    IF(12)
        ret = PA::DFM::set(PA::DFM_TYPE_TTS, 0, 50);
        assert(!ret && "ASSERT: SET NEW RANGE no force");
    END(12)
            
    uLogApp(LOG_DEB0, "----------END test RANGE ID--------------");
    
}


void getInit(){
    static xc8 ret[24];
    long curseed;
    uLogApp(LOG_DEB0, "----------BEGIN test get ID--------------");
    PA::DFM::init((void*)range, sizeof(range), uTrue);
//    for (DFM_TYPE i = DFM_TYPE_MIN; i < DFM_TYPE_MAX; i++) {
//        DFM::types[i].init(i, uFalse);
//    }
    //PA::setDataFile("/SI/notexist/dataDFM.txt");
    //PA::setDataFile("./.dfm.dat");
    //PA::DFM::set(PA::DFM_TYPE_PAG, 1, 9999999999);

    PA::setDataFile("/vmsdata/DJ_vHVMS/.dfm.dat",1);
    PA::DFM::set(PA::DFM_TYPE_PAG, 00000000000, 9999999999);
    PA::DFM::set(PA::DFM_TYPE_REC, 10000000001, 19999999999);
    PA::DFM::set(PA::DFM_TYPE_TTS, 20000000001, 29999999999);
    PA::DFM::set(PA::DFM_TYPE_RBT, 30000000001, 39999999999);


    PA::DFM::dump();
    PA::DFMInfo* item = PA::DFM::get(PA::DFM_TYPE_REC);
    if (item) {
        sprintf(ret, PA_FMT_DIGIT_3 " " PA_FMT_DIGIT_3, item->getMin(), item->getMax());
    }
    curseed = item->getCurSeed();
    std::cout << "\n\t get REC range : " << ret << "\n\t curseed :" << curseed << "\n\n";

    //std::cout << "\n\t ./DfmChanger.exe 4 reset  : " << ret << "\n curseed :" << curseed << "\n\n";

    //std::cout << "\n\t Generate REC (seq) " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_SEQ);
}


void getDFM(){
    static xc8 ret[24];
    long curseed;
    uLogApp(LOG_DEB0, "----------BEGIN test get ID--------------");
    PA::DFM::init((void*)range, sizeof(range), uTrue);
//    for (DFM_TYPE i = DFM_TYPE_MIN; i < DFM_TYPE_MAX; i++) {
//        DFM::types[i].init(i, uFalse);
//    }
    //PA::setDataFile("/SI/notexist/dataDFM.txt");
    //PA::setDataFile("./.dfm.dat");
    //PA::DFM::set(PA::DFM_TYPE_PAG, 1, 9999999999);

    PA::setDataFile("/vmsdata/DJ_vHVMS/.dfm.dat",0);
    PA::DFM::set(PA::DFM_TYPE_PAG, 00000000000, 9999999999);
    PA::DFM::set(PA::DFM_TYPE_REC, 10000000001, 19999999999);
    PA::DFM::set(PA::DFM_TYPE_TTS, 20000000001, 29999999999);
    PA::DFM::set(PA::DFM_TYPE_RBT, 30000000001, 39999999999);


    PA::DFM::dump();
    PA::DFMInfo* item = PA::DFM::get(PA::DFM_TYPE_REC);
    if (item) {
        sprintf(ret, PA_FMT_DIGIT_3 " " PA_FMT_DIGIT_3, item->getMin(), item->getMax());
    }
    curseed = item->getCurSeed();
    std::cout << "\n\t get REC range : " << ret << "\n\t current seed :" << curseed << "\n\n";

    std::cout << "\n\t  1. (SA-odd) generate ID  or ./DfmChanger.exe 1";
    std::cout << "\n\t  2. (DJ-even) generate ID or ./DfmChanger.exe 2";
    std::cout << "\n\t  3.  manual input ID      or ./DfmChanger.exe 3 10000031756";
    std::cout << "\n\t  4.  get PATH by ID       or ./DfmChanger.exe 4 10000031756";
    std::cout << "\n\t  0.  EXIT "<< "\n\n";
    //std::cout << "\n\t ./DfmChanger.exe 4 reset  : " << ret << "\n curseed :" << curseed << "\n\n";

    //std::cout << "\n\t Generate REC (seq) " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_SEQ);
}

void testGen() {
    uLogApp(LOG_DEB0, "----------BEGIN test GENERATE ID--------------");
    PA::DFM::init((void*)range, sizeof(range), uTrue);
//    for (DFM_TYPE i = DFM_TYPE_MIN; i < DFM_TYPE_MAX; i++) {
//        DFM::types[i].init(i, uFalse);
//    }
    //PA::setDataFile("/SI/notexist/dataDFM.txt");
    //PA::setDataFile("./.dfm.dat");
    //PA::DFM::set(PA::DFM_TYPE_PAG, 1, 9999999999);

    PA::setDataFile("/vmsdata/DJ_vHVMS/.dfm.dat",1);
    PA::DFM::set(PA::DFM_TYPE_PAG, 00000000000, 9999999999);
    PA::DFM::set(PA::DFM_TYPE_REC, 10000000001, 19999999999);
    PA::DFM::set(PA::DFM_TYPE_TTS, 20000000001, 29999999999);
    PA::DFM::set(PA::DFM_TYPE_RBT, 30000000001, 39999999999);


    PA::DFM::dump();

    // std::cout << "\n\t Generate 0 (odd) " << PA::generate(0, PA::DFM_ID_MODE_ODD);
    // PA::seed(DFM_TYPE_PAG, 0);
    // std::cout << "\n\t Generate 0 (odd) " << PA::generate(0, PA::DFM_ID_MODE_ODD);
    // std::cout << "\n\t Generate PAG (odd)" << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);
    // std::cout << "\n\t Generate PAG (odd)" << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);
    // std::cout << "\n\t Generate PAG (odd)" << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);
    // std::cout << "\n\t Generate PAG (odd)" << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);
    // std::cout << "\n\t Generate PAG (odd)" << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);
    // std::cout << "\n\t Generate PAG (odd)" << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);
    // //PA::seed(DFM_TYPE_PAG, 29);
    // //std::cout << "\n\t Generate PAG (odd, after seed 29)" << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);

    // PA::DFM::set(PA::DFM_TYPE_PAG, 1, 9999999998);
    // std::cout << "\n\t Generate PAG (odd)- after SET " << PA::generate(DFM_TYPE_PAG, PA::DFM_ID_MODE_ODD);

    std::cout << "\n\t Generate REC (seq) " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_SEQ);
    std::cout << "\n\t Generate REC (seq) " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_SEQ);
    std::cout << "\n\t Generate REC (seq) " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_SEQ);
    PA::seed(DFM_TYPE_REC, 10000000009);
    PA::seed(DFM_TYPE_REC, 33000000009);
    //std::cout << "\n\t Generate REC (seq)-after SEED " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_SEQ);

    // std::cout << "\n\t Generate TTS (even) " << PA::generate(DFM_TYPE_TTS, PA::DFM_ID_MODE_EVEN);
    // std::cout << "\n\t Generate TTS (odd)" << PA::generate(DFM_TYPE_TTS, PA::DFM_ID_MODE_ODD);
    // std::cout << "\n\t Generate TTS (even)" << PA::generate(DFM_TYPE_TTS, PA::DFM_ID_MODE_EVEN);
    // std::cout << "\n\t Generate TTS (seq)" << PA::generate(DFM_TYPE_TTS, PA::DFM_ID_MODE_SEQ);

    std::cout << "\n\t Generate RBT (even)" << PA::generate(DFM_TYPE_RBT, PA::DFM_ID_MODE_EVEN);
    std::cout << "\n\t Generate RBT (even)" << PA::generate(DFM_TYPE_RBT, PA::DFM_ID_MODE_EVEN);
    PA::DFM::dump();
    uLogApp(LOG_DEB0, "----------END test GENERATE ID--------------");
}

int main(int argc, char** argv) {
    xc8* annId = "10000000000";
    //Boolean ret = uFalse;
    //xc8* dfm_mode = 0;
    if (!uLogOpen("LIBPA", 0, "./XN_LOG_LEVEL.ini")) {
        printf("mpLogOpen fail\n");
        exit(1);
    }
    
    // local mode
    //testNoInit();

    // dumping registered ID range
    //testIDRange();

    // for (DFM_TYPE i = DFM_TYPE_MIN; i < DFM_TYPE_MAX; i++) {
    //    DFM::types[i].init(i, uFalse);
    // }
    //PA::DFM::dump();
    // decoding ID to mid-path
    // while (argc > 1) {
    //    annId = argv[argc -1];
    //    std::cout << "\n\t RESULT decode1 =" << PA::DFM::decode(annId);
    //    std::cout << "\n\t RESULT decode2 =" << PA::DFM::decode(annId, "kor", "alaw");
    //    std::cout << "\n\t RESULT decode2 =" << PA::DFM::decode(annId, "kor", "alaw", "vCOMP", "/SI/Ann");
    //    std::cout << "\n\t RET_PA decode1 =" << PA::decode(annId, "kor", "vCOMP", "/SI/Ann");
    //    std::cout << "\n\t RET_PA decode2 =" << PA::decode(annId, "kor", "alaw", "vCOMP", "/SI/Ann");
    //    std::cout << "\n\n";
    //    argc--;
    // }

    // get file attr
    //fileStat("./sample.c");

    // test annID generation
    //testGen();
    while(1){
        
        if(argc > 1) 
        {
            //std::cout << "argc : " << argc << " argv[1] : " << argv[1];
            int dfm_mode  = atoi(argv[1]);
            std::cout << "dfm_mode : " << dfm_mode <<"\n\n";
            getInit();
            if(dfm_mode == 2)
            {
                std::cout << "\n\t Generate REC (even) : " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_EVEN) << "\n\n\n";
                PA::DFM::dump();
            }
            else if(dfm_mode == 1)
            {
                std::cout << "\n\t Generate REC (odd) : " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_ODD) << "\n\n\n";
                PA::DFM::dump();
            }
            else if(dfm_mode == 3)
            {
                annId = argv[2];
                PA::seed(DFM_TYPE_REC, atol(annId));
                PA::DFMInfo* item = PA::DFM::get(PA::DFM_TYPE_REC);
                long curseed = item->getCurSeed();
                std::cout << "\n\t Generate REC (manual) : " << curseed << "\n\n";
                PA::DFM::dump();
            }
            else if(dfm_mode == 4)
            {
                annId = argv[2];
                std::cout << "\n\t Generate REC_PATH (manual) : " << PA::decode(annId, "kor", "wav", "DJ_vHVMS", "/vmsdata") << "\n\n";
                PA::DFM::dump();
            }
            exit(1);
        }
        else{
            getDFM();
            //PA::DFM::dump();
            int dfm_mode;
            std::cout << "enter mode : ";
            std::cin >> dfm_mode;
            if(dfm_mode == 2)
            {
                //std::cout << "\n\t Generate REC (even) : " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_EVEN) << "\n\n\n";
                PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_EVEN);
                PA::DFM::dump();
            }
            else if(dfm_mode == 1)
            {
                //std::cout << "\n\t generate seed(odd) : " << PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_ODD) << "\n\n\n";
                PA::generate(DFM_TYPE_REC, PA::DFM_ID_MODE_ODD);
                PA::DFM::dump();
            }
            else if(dfm_mode == 3)
            {
                long dfmID;
                std::cout << "enter Dfm ID : ";
                std::cin >> dfmID;
                PA::seed(DFM_TYPE_REC, dfmID);
                //getInit();
                PA::DFM::dump();
            }
            else if(dfm_mode == 4)
            {
                long dfmID;
                std::cout << "enter Dfm ID : ";
                std::cin >> dfmID;
                xc8* annid = "10000000000";
                sprintf(annid,"%ld",dfmID);
                std::cout << "\n\t Generate REC_PATH (manual) : " << PA::decode(annid, "kor", "wav", "DJ_vHVMS", "/vmsdata") << "\n\n";
                PA::DFM::dump();
            }
            else if(dfm_mode == 0)
            {
                exit(1);
            }
        }
    }
    
    std::cout << "\n";
    //while(1); // prevent auto cleanup resource
    return 0;
}
