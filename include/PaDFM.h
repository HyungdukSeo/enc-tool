//=============================================================================
//                                   PaDFM.h
//=============================================================================
// AUTHOR      : hthngoc@tma.com.vn
// DATE        : 25/08/2022
// DESCRIPTION : Get/Set DFFM range & decode hidden path in announcementID 
//=============================================================================
#ifndef __libPA_DFM__
#define __libPA_DFM__

//=============================================================================
//                               I N C L U D E
//=============================================================================
#include <LIB/Gen/GenIncl.h>
#include <LIB/Ce/CeIncl.h>
#include <LIB/Sk/SkIncl.h>
#include <vector>

//=============================================================================
//                              C O N S T A N T
//=============================================================================
namespace PA {

    enum DFM_TYPE {
        DFM_TYPE_PAG,
        DFM_TYPE_REC,
        DFM_TYPE_TTS,
        DFM_TYPE_RBT,
        DFM_TYPE_MAX,
    };
    static const xc8* DFM_TYPE_STRS[] = {"PAG", "REC", "TTS", "RBT", "UNKNOWN"};
    #define DFM_TYPE_MIN                DFM_TYPE_PAG
    #define DFM_NAME                    "DFM"
    #define DFM_INVALID_VAL             -1

    enum DFM_ID_MODE {
        DFM_ID_MODE_ODD,
        DFM_ID_MODE_EVEN,
        DFM_ID_MODE_SEQ,
        DFM_ID_MODE_MAX
    };
    #define DFM_CACHE_NAME              ".dfm.dat"
    #define DFM_CACHE_FILE_DEF          "./" DFM_CACHE_NAME
    #define DFM_CACHE_FILE(dfm_id)              "./" DFM_CACHE_NAME "_" #dfm_id
};

//=============================================================================
//                    C L A S S   A N D   S T R U C T U R E
//=============================================================================
namespace PA {
    /**
     * DFM entry info
     */
    class DFMInfo {
        DFM_TYPE                        id;
        long                            min;
        long                            max;
        long                            seed;
        long                            curSeed;
        Boolean                         archive;
    public:
        DFMInfo();
        DFMInfo(DFM_TYPE idx);
        DFMInfo* operator=(DFMInfo& term);
        
        void                            init(DFM_TYPE idx, Boolean withInvalid = uTrue);

        Boolean                         setSeed(long val, Boolean setCurSeed = uTrue);
        long                            getSeed();
        long                            getCurSeed();

        Boolean                         setArchive();
        Boolean                         getArchive();

        Boolean                         set(long min, long max);
        DFM_TYPE                        getId();
        long                            getMin();
        long                            getMax();
        Boolean                         isOverlap(long min, long max);
        Boolean                         isOverlap(DFMInfo* target);
        void                            dump();

        // generate/allocate new ID in range
        long                            generateL(DFM_ID_MODE mode);
        xc8*                            generate(DFM_ID_MODE mode);
    };
    
    /**
     * DFM container
     */
    class DFM {
        static xc8*                     fn;
        static FILE*                    fp;
        static FILE*                    fpRead;

        static Boolean                  openFp(Boolean readOnly = uFalse);
        //static Boolean                  closeFp();   // [LGU_HVMS_2022] should be able to close fp in "HA-standby" state. move to "public"
        static Boolean                  archive(DFM_TYPE type);
        static Boolean                  restore(DFM_TYPE type);
    public:
        static xc8*                     name;
        static DFMInfo*                 types; // point to DFMInfo[DFM_TYPE_MAX]
        
        // manage value range
        static Boolean                  init(void* addr, xui32 sz, Boolean reset);
        static Boolean                  closeFp();  // [LGU_HVMS_2022] 2022.11, should be able to close fp in "HA-standby" state.
        static std::vector<DFM_TYPE>    find(long min, long max);
        static DFM_TYPE                 find(const xc8* segmentID);
        static Boolean                  set(DFM_TYPE type, long min, long max);
        static DFMInfo*                 get(DFM_TYPE type);
        static void                     dump();
        
        // decode & build full path from announceID & params
        static Boolean                  isType3(const xc8* segmentID);
        static const xc8*               decode(const xc8* segmentID);
        static const xc8*               decode(const xc8* segmentID, xc8* lang, xc8* fmt);
        static const xc8*               decode(const xc8* segmentID, xc8* lang, xc8* fmt, xc8* comp, xc8* base);

        // generate/allocate annID by type
        static Boolean                  setFn(const xc8* val, xui32 mode);
        static Boolean                  syncFn(DFM_TYPE type, const xc8* local, const xc8* remote, xui32 mode);
        static long                     generateL(DFM_TYPE type, DFM_ID_MODE mode);
        static xc8*                     generate(DFM_TYPE type, DFM_ID_MODE mode);
        static Boolean                  seed(DFM_TYPE type, long val);
    }; 
}

PA::DFM_TYPE& operator++(PA::DFM_TYPE& orig);
PA::DFM_TYPE& operator++(PA::DFM_TYPE& orig, xi32);
#endif
