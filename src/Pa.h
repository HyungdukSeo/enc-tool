//=============================================================================
//                                   Pa.h
//=============================================================================
// AUTHOR      : hthngoc@tma.com.vn
// DATE        : 25/08/2022
// DESCRIPTION : Lib interface 
//=============================================================================
#ifndef __libPA1__
#define __libPA1__

//=============================================================================
//                               I N C L U D E
//=============================================================================
#include <LIB/Gen/GenIncl.h>
#include <LIB/Ce/CeIncl.h>
#include <LIB/Sk/SkIncl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <PaDFM.h>
//=============================================================================
//                              C O N S T A N T
//=============================================================================

#ifndef INT32_MAX
#define INT32_MAX   2147483647
#endif

#ifndef INT_MAX
#define INT_MAX  INT32_MAX      /*  for 32 bit  */
#endif
namespace PA {
    #define PA_MAX_LEN                          4096

    /* Segment ID types:
     *      type1: 4-digits,    0000 - 0999
     *      type2: 4-digits,    1000 - 9999
     *      type3: 11-digits,   00000000000 - 999999999999
     */
    enum PA_TYPE {
        PA_TYPE_1,
        PA_TYPE_2,
        PA_TYPE_3,
        PA_TYPE_MAX,
    };
    #define PA_TYPE_MIN                         PA_TYPE_1

    #define PA_TYPE_DIGIT_1                     4
    #define PA_TYPE_DIGIT_2                     4
    #define PA_TYPE_DIGIT_3                     11
    static xi32 PA_TYPE_DIGITS[] = {PA_TYPE_DIGIT_1, PA_TYPE_DIGIT_2, PA_TYPE_DIGIT_3, INT_MAX};
    #define PA_FMT_DIGIT_1                     "%04d"
    #define PA_FMT_DIGIT_3                     "%011ld"
    
    #define PA_TYPE_1_MIN                       0
    #define PA_TYPE_1_MAX                       1000

    #define PA_TYPE_2_MIN                       1001
    #define PA_TYPE_2_MAX                       9999

    #define PA_TYPE_3_MIN                       0
    #define PA_TYPE_3_MAX                       99999999999  

};

//=============================================================================
//                    C L A S S   A N D   S T R U C T U R E
//=============================================================================
    struct stat*                fileStat(xc8* path);
    
namespace PA {

    const xc8*                  getDfmKey(xui32 type);
    const xc8*                  getDfmRange(xui32 type);
    Boolean                     getDfmRange(xui32 t, long* min,long *max);
    /* To support legacy test case type 1, 2: %04d */
    const xc8*                  standardize(const xc8* segmentID, PA_TYPE type);
    
    /* Validate ID against expected type */
    Boolean                     matchType(const xc8* segmentID, PA_TYPE type);
    
    /* Get file path from segmentID (w/wo extension) */
    const xc8*                  decode(const xc8* segmentID, xc8* lang);
    const xc8*                  decode(const xc8* segmentID, xc8* lang, xc8* depth2, xc8* depth1);
    const xc8*                  decode(const xc8* segmentID, xc8* lang, xc8* fmt, xc8* depth2, xc8* depth1);

    /* Generate/Allocate annID */
    Boolean                     setDataFile(const xc8* val, xui32 mode);
	Boolean                     syncDataFile(DFM_TYPE type, const xc8* local, const xc8* remote, xui32 mode);
    xc8*                        generate(xui32 type, DFM_ID_MODE mode);
    Boolean                     seed(xui32 type, long val);

}

PA::PA_TYPE& operator++(PA::PA_TYPE& orig);
PA::PA_TYPE& operator++(PA::PA_TYPE& orig, int);
#endif
