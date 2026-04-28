//=============================================================================
//                                   Pa.c
//=============================================================================
// AUTHOR      : hthngoc@tma.com.vn
// DATE        : 25/08/2022
// DESCRIPTION : Lib interface
//=============================================================================

//=============================================================================
//                               I N C L U D E
//============================================================================="
#include "Pa.h"

using namespace PA;

const std::string _compiled = "Compilation : ";
const std::string _compiledDate = __DATE__;
const std::string _compiledTime = __TIME__;

#ident "\\n** Version : Pa.o v1.0.1 (31/10/2022) **\\n"
//=============================================================================
//                              C O N S T A N T
//=============================================================================

//=============================================================================
//                      S T A T I C   V A R I A B L E
//=============================================================================

//=============================================================================
//                    C L A S S   A N D   S T R U C T U R E
//=============================================================================
PA::PA_TYPE& operator++(PA::PA_TYPE& orig) {
    orig = static_cast<PA::PA_TYPE>(orig + 1);
    return orig;
}

PA::PA_TYPE& operator++(PA::PA_TYPE& orig, int) {
    return ++orig;
}

/********************************* PA *****************************************/
/**
 * Get DFM type's name
 * 
 * E.g: input: 0
 *      output: "PAG"
 */
const xc8* PA::getDfmKey(xui32 t) {
    PA::DFM_TYPE type = PA::DFM_TYPE_MAX;
    if (t < PA::DFM_TYPE_MAX) {
        type = (PA::DFM_TYPE) t;
    }
    return PA::DFM_TYPE_STRS[type];
}

/**
 * Get DFM type's value range as string
 * 
 * E.g: input: 0
 *      output: "00000000001-09999999999"
 */
const xc8* PA::getDfmRange(xui32 t) {
    PA::DFM_TYPE type = PA::DFM_TYPE_MAX;
    static xc8 ret[24];
    
    memset(ret, 0, sizeof(ret));
    if (t < PA::DFM_TYPE_MAX) {
        type = (PA::DFM_TYPE) t;
    }
    PA::DFMInfo* item = PA::DFM::get(type);
    if (item) {
        sprintf(ret, PA_FMT_DIGIT_3 " " PA_FMT_DIGIT_3, item->getMin(), item->getMax());
    }
    return ret;
}

/**
 * Get DFM type's value range
 * 
 * E.g: input: 0, &min, &max
 *      output: uTrue
 */
Boolean PA::getDfmRange(xui32 t, long* min, long *max) {
    PA::DFM_TYPE type = (PA::DFM_TYPE) t;
    if (t > PA::DFM_TYPE_MAX) {
        return uFalse;
    }
    PA::DFMInfo* item = PA::DFM::get(type);
    
    if (item) {
        *min = item->getMin();
        *max = item->getMax();
        return uTrue;
    }
    return uFalse;
}

/**
 * validate segmentID string against desired type
 * 
 * E.g: input: 00000000001, PA_TYPE_3
 *      output: uTrue
 */
Boolean PA::matchType(const xc8* segmentID, PA_TYPE type) {
    uLogApp(LOG_DEB3, "%s() segmentID (%s) type (%d)", __func__, segmentID, type);
    // len
    if (!segmentID || strlen(segmentID) != PA_TYPE_DIGITS[type]) {
        goto _err;
    }
    
    // number only
    for (xi32 i = 0; i < strlen(segmentID); i++) {
        if (segmentID[i] < '0' || segmentID[i] > '9') {
            goto _err;
        }
    }
    return uTrue;
_err:
    uLogApp(LOG_DEB3, "%s() KO", __func__); 
    return uFalse;
}


/**
 * backward compatible only? when type 1 & type 2 has less than 4 digits
 * 
 * E.g: input: 23, PA_TYPE_1
 *      output: "0023"
 */
const xc8* PA::standardize(const xc8* segmentID, PA_TYPE type) {
    if (!segmentID) return "";
    
    // Hmm.., auto support legacy test cases? E.g: ann=23, ever?
    // TODO: contain number only
    if ((type == PA_TYPE_1 || type == PA_TYPE_2)
        && strlen(segmentID) < PA_TYPE_DIGIT_2) {
        static xc8 tmp[8];
        memset(tmp, 0, sizeof(tmp));
        xui32 val = atoi(segmentID);
        sprintf(tmp, "%04d", val);
        uLogApp(LOG_DEB3, "%s() segmentID (%s) --> (%s)", __func__, segmentID, tmp);
        return (const xc8*) tmp;
    }
    
    return segmentID;
}

/**
 * decode segmentID to get file path
 * 
 * E.g: input: 0001, &type
 *      output: "system/0001"
 *
 *      input:  00000000001
 *      output: DFM/PAG/000/000/000/00000000001
 */
const xc8* PA::decode(const xc8* segmentID, xc8* lang) {
    static xc8 ret[PA_MAX_LEN];
    PA_TYPE type = PA_TYPE_MAX;
    const char* prefix = "";
    
    memset(&ret, 0, sizeof(ret));
    if (!segmentID) {
        goto _done;
    }
    // type1 or type2
    if (strlen(segmentID) <= PA_TYPE_DIGIT_2) {
        xi32 segmentVal;
        segmentVal = atoi(segmentID);
        if (segmentVal < PA_TYPE_1_MAX) {
            prefix = "system";
            type = PA_TYPE_1;
        }
        else { // must less than 9999 already
            prefix = "basic";
            type = PA_TYPE_2;
        }
    }
    // type3
    else if (strlen(prefix = PA::DFM::decode(segmentID)) > 0) {
        type = PA_TYPE_3;
    }
    else {
        uLogApp(LOG_DEB3, "%s() undefined type segmentID (%s)", __func__, segmentID);
    }
    
    // combine
    if (strlen(prefix) > 0) sprintf(ret, "%s/", prefix);
    if (lang) {
        sprintf(ret + strlen(ret), "%s-%s", lang, PA::standardize(segmentID, type));
    }
    else {
       sprintf(ret + strlen(ret), "%s", PA::standardize(segmentID, type));
    }
    
_done:
    return ret;
}



/**
 * Get absolute path segmentID type [1, 2, 3] (wo extension)
 */
const xc8* PA::decode(const xc8* segmentID, xc8* lang, xc8* depth2, xc8* depth1) {
    if (!segmentID || !depth2 || !depth1) {
        goto _done;
    }
    static xc8 ret[PA_MAX_LEN];
    memset(ret, 0, sizeof(ret));
    sprintf(ret, "%s/%s/%s", depth1, depth2, PA::decode(segmentID, lang));
_done:
    uLogApp(LOG_DEB0, "%s() done (%s)", __func__, ret);
    return ret;
}

/**
 * Get absolute path segmentID type [1, 2, 3] (w extension)
 */
const xc8* PA::decode(const xc8* segmentID, xc8* lang, xc8* fmt, xc8* depth2, xc8* depth1) {
    if (!segmentID || !fmt || !depth2 || !depth1) {
        goto _done;
    }
    static xc8 ret[PA_MAX_LEN];
    memset(ret, 0, sizeof(ret));
    sprintf(ret, "%s.%s", PA::decode(segmentID, lang, depth2, depth1), fmt);
_done:
    uLogApp(LOG_DEB0, "%s() done (%s)", __func__, ret);
    return ret;
}
/**
 * Sync dfm data file,(local nas<->remote nas)
 */
Boolean PA::syncDataFile(DFM_TYPE type, const xc8* local, const xc8* remote, xui32 mode) {
	    return PA::DFM::syncFn(type, local, remote, mode);
}

/**
 * Set custom dfm data file, instead of using the default DFM_CACHE_FILE
 */
Boolean PA::setDataFile(const xc8* val, xui32 mode) {
    return PA::DFM::setFn(val, mode);
}

/**
 * Generate/allocate ID
 */
xc8* PA::generate(xui32 t, DFM_ID_MODE mode) {
    PA::DFM_TYPE type = PA::DFM_TYPE_MAX;

    if (t < PA::DFM_TYPE_MAX) {
        type = (PA::DFM_TYPE) t;
    }
    return PA::DFM::generate(type, mode);
}

/**
 * Custom seed
 */
Boolean PA::seed(xui32 t, long val) {
    PA::DFM_TYPE type = PA::DFM_TYPE_MAX;

    if (t < PA::DFM_TYPE_MAX) {
        type = (PA::DFM_TYPE) t;
    }
    return PA::DFM::seed(type, val);
}

/******************************************************************************/
/**
 * query system file stats
 * 
 */
struct stat* fileStat(xc8* path) {
    static struct stat ret;
    char* type;
    
    uLogApp(LOG_DEB3, "%s() path (%s)", __func__, path);
    if (!path || strlen(path) == 0) {
        goto _err;
    };
    
    if (stat(path, &ret) == -1) {
        uLogApp(LOG_DEB0, "Failed to get stat, errno (%d)", errno);
        goto _err;
    }
    switch (ret.st_mode & S_IFMT) {
        case S_IFBLK:  type = "block device";       break;
        case S_IFCHR:  type = "character device";   break;
        case S_IFDIR:  type = "directory";          break;
        case S_IFIFO:  type = "FIFO/pipe";          break;
        case S_IFLNK:  type = "symlink";            break;
        case S_IFREG:  type = "regular file";       break;
        case S_IFSOCK: type = "socket";             break;
        default:       type = "unknown";            break;
    }
    
    // E.g: DEB0 {Type: regular file, Size: 3793 (bytes), Mode: 100700 (octal)
    //    , Ownership: {UID=1005, GID=1005}, Last status change: Tue Sep  6 16:39:03 2022
    //    , Last file access: Tue Sep  6 16:39:03 2022, Last file modification: Tue Sep  6 16:39:03 2022}
    uLogApp(LOG_DEB0, "{Type: %s, Size: %lld (bytes), Mode: %lo (octal), Ownership: {UID=%ld, GID=%ld}\
, Last status change: %s, Last file access: %s, Last file modification: %s}"
        , type, (long long) ret.st_size, (unsigned long) ret.st_mode, (long) ret.st_uid, (long) ret.st_gid
        , ctime(&ret.st_ctime), ctime(&ret.st_atime), ctime(&ret.st_mtime));
    return &ret;
_err:
    return NULL;
}
