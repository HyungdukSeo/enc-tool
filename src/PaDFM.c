//=============================================================================
//                                   PaDFM.c
//=============================================================================
// AUTHOR      : hthngoc@tma.com.vn
// DATE        : 25/08/2022
// DESCRIPTION : Get/Set DFFM range & decode hidden path in announcementID 
//=============================================================================

//=============================================================================
//                               I N C L U D E
//=============================================================================
#include "PaDFM.h"
#include "Pa.h"

using namespace PA;

#ident "\\n** Version : PaDFM.o v1.0.2 (31/10/2022) **\\n"
//=============================================================================
//                              C O N S T A N T
//=============================================================================
/*
 * Default 11-digit value range
 */
long __defaultDFMRange[DFM_TYPE_MAX][2] = {
    {1, 9999999999},
    {10000000001, 19999999999},
    {20000000001, 29999999999},
    {30000000001, 39999999999},
};

//=============================================================================
//                      S T A T I C   V A R I A B L E
//=============================================================================
xc8* DFM::name = DFM_NAME;
DFMInfo* DFM::types = NULL; // this can be an internal or external address
DFMInfo _localTypes[DFM_TYPE_MAX]; // internal mode

xc8*  DFM::fn = NULL;
FILE* DFM::fp = NULL; // save current seed number
FILE* DFM::fpRead = NULL; // load previous seed number

//=============================================================================
//                      S T A T I C    F U N C T I O N 
//=============================================================================
DFM_TYPE& operator++(DFM_TYPE& orig) {
    orig = static_cast<DFM_TYPE>(orig + 1);
    return orig;
};

DFM_TYPE& operator++(DFM_TYPE& orig, int) {
    return ++orig;
};

#define CLOSE_FP(_fp) \
    if (_fp) fclose(_fp); \
    _fp = NULL;

/********************************* DFMInfo ************************************/
DFMInfo::DFMInfo() : id (DFM_TYPE_MAX), min(DFM_INVALID_VAL), max(DFM_INVALID_VAL)
    , seed (-1), curSeed(-1) {
}

DFMInfo::DFMInfo(DFM_TYPE idx) : DFMInfo() {
    this->id = idx;
}

DFMInfo* DFMInfo::operator=(DFMInfo& term) {
    this->min = term.min;
    this->max = term.max;
    return this;
}

/**
 * set default range by type
 */
void DFMInfo::init(DFM_TYPE idx, Boolean withInvalid) {
    this->id = idx;
    if (withInvalid) {
        this->min = this->max = DFM_INVALID_VAL;
    }
    else {
        this->min = __defaultDFMRange[idx][0];
        this->max = __defaultDFMRange[idx][1];
        this->setSeed(this->min, uFalse);
    }
}

/**
 * dump
 */
void DFMInfo::dump() {
    uLogApp(LOG_INFO, "%d (%s) [" PA_FMT_DIGIT_3 "," PA_FMT_DIGIT_3 "], original seed (" PA_FMT_DIGIT_3 "), current seed (" PA_FMT_DIGIT_3 ")"
        , this->id, DFM_TYPE_STRS[this->id]
        , this->min, this->max
        , this->seed, this->curSeed);
}

/**
 * Modify its range
 */
Boolean DFMInfo::set(long min, long max) {
    uLogApp(LOG_DEB3, "%s()", __func__);
    
    if (min > max) {
        uLogApp(LOG_MIN, "%s() Error, min (" PA_FMT_DIGIT_3 ") < max (" PA_FMT_DIGIT_3 ")", __func__, min, max);
        return uFalse;
    }
    
    this->min = min;
    this->max = max;
    this->setSeed(this->min, uFalse);
    this->dump();
    return uTrue;
}

/**
 * Get
 */
DFM_TYPE DFMInfo::getId() {
    return this->id;
}

/**
 * Get
 */
long DFMInfo::getMin() {
    return this->min;
}

/**
 * Get
 */
long DFMInfo::getMax() {
    return this->max;
}

/**
 * Check if requesting [min, max] is overlapped with any range
 */
Boolean DFMInfo::isOverlap(long min, long max) {
    return (Boolean) (this->min <= max && this->max >= min);
}

/**
 * 
 */
Boolean DFMInfo::isOverlap(DFMInfo* target) {
    return this->isOverlap(target->min, target->max);
}

/**
 * Set original seed and/or current seed
 */
Boolean DFMInfo::setSeed(long val, Boolean setCurSeed) {
    uLogApp(LOG_DEB3, "%s() val (%lu)", __func__, val);
    if (!this->isOverlap(val, val)) {
        uLogApp(LOG_MIN, "%s() Error, seed value must belong to value range", __func__);
        return uFalse;
    }
    this->seed = val;
    this->curSeed = (setCurSeed == uTrue) ? val : -1;
    return uTrue;
}

/**
 * Set original seed
 */
long DFMInfo::getSeed() {
    return this->seed;
}

/**
 * Set current seed
 */
long DFMInfo::getCurSeed() {
    return this->curSeed;
}

/**
* Mark only
*/
Boolean DFMInfo::setArchive() {
    this->archive = uTrue;
    return uTrue;
}

Boolean DFMInfo::getArchive() {
    return this->archive;
}

/**
 * Allocate/generate new annID from curSeed
 */
long DFMInfo::generateL(DFM_ID_MODE mode) {
    long ret = this->curSeed;
    static xui32 suits[2][3] = {{0, 0, 0}, {2, 2, 1}};
    xui32* diff = suits[1];
    xui32 gap = 0;

    uLogApp(LOG_DEB3, "%s() mode (%d), current seed (" PA_FMT_DIGIT_3 ")", __func__, mode, curSeed);

    // increasing by mode
    if (this->curSeed == -1) {
        ret = this->seed;
        diff = suits[0];
    }
    switch (mode) {
        case DFM_ID_MODE_ODD:
            gap = (ret % 2 ? diff[0] : 1);
            break;
        case DFM_ID_MODE_EVEN:
            gap = (ret % 2 ? 1 : diff[1]);
            break;
        default:
            gap = diff[2];
            break;
    }
    ret += gap;

    // guard
    if (!this->isOverlap(ret, ret)) {
        uLogApp(LOG_MIN, "%s() Error, reached max ID, couldn't generate new one, current seed (" PA_FMT_DIGIT_3 ")", __func__, this->curSeed);
        return -1;
    }
    this->curSeed = ret;
    uLogApp(LOG_DEB3, "%s() ret (" PA_FMT_DIGIT_3 ")", __func__, ret);
    return ret;
}

/**
 * Allocate/generate new annID from curSeed
 */
xc8* DFMInfo::generate(DFM_ID_MODE mode) {
    static xc8 ret[PA_MAX_LEN];
    long retVal = this->generateL(mode);

    memset(&ret, 0, sizeof(ret));
    if (retVal != -1) {
        sprintf(ret, PA_FMT_DIGIT_3, retVal);
        return ret;
    }
    return NULL;
}


/********************************* DFM ****************************************/
/**
 * Bind to internal or external addr
 */
Boolean DFM::init(void* addr, xui32 sz, Boolean reset) {
    uLogApp(LOG_DEB3, "%s() Binding addr (%p), sz (%d), reset (%d)", __func__, addr, sz, reset);
    if (sz < sizeof(_localTypes)) {
        uLogApp(LOG_MIN, "%s() Error, assigned memory is too small, need at least (%d) bytes", __func__, sizeof(_localTypes));
        return uFalse;
    }
    DFM::types = (DFMInfo *) addr;
    if (reset) {
        for (DFM_TYPE i = DFM_TYPE_MIN; i < DFM_TYPE_MAX; i++) {
            DFM::types[i].init(i);
        }
    }
    return uTrue;
}

/**
 * check 11-digits
 */
Boolean DFM::isType3(const xc8* segmentID) {
    uLogApp(LOG_DEB3, "%s() segmentID (%s)", __func__, segmentID);
    if (!segmentID || strlen(segmentID) != 11) {
        goto _err;
    }
    
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
* Find type by range, return all matched types or empty
**/

std::vector<DFM_TYPE> DFM::find(long min, long max) {
    std::vector<DFM_TYPE> ret;
    
    uLogApp(LOG_DEB3, "%s() range (%ld, %ld)", __func__, min, max);
    for (DFM_TYPE i = DFM_TYPE_MIN; i < DFM_TYPE_MAX; i++) {
        if (DFM::types[i].isOverlap(min, max)) {
            uLogApp(LOG_DEB3, "%s() found (%d) (%s)", __func__, i, DFM_TYPE_STRS[i]);
            ret.push_back(i);
        }
    }
_done:
    return ret;
}

/**
 * find type by announcement ID string
 */
DFM_TYPE DFM::find(const xc8* segmentID) {
    long val;
    
    uLogApp(LOG_DEB3, "%s() segmentID (%s)", __func__, segmentID);
    if (!DFM::types || !PA::matchType(segmentID, PA_TYPE_3)) {
        return DFM_TYPE_MAX;
    }
    val = atol(segmentID);
    std::vector<DFM_TYPE> ret = DFM::find(val, val);
    return !ret.size() ? DFM_TYPE_MAX : ret[0];
}

/**
 * Set type's range, return failure if overlapped
 */
Boolean DFM::set(DFM_TYPE type, long min, long max) {
    std::vector<DFM_TYPE> tmp;
    
    uLogApp(LOG_INFO, "%s() [%d] min (%ld), max (%ld)"
            , __func__, type, min, max);
            
    if (type >= DFM_TYPE_MAX) goto _err;
    
    // 1. switch to use local address mode
    if (!DFM::types) {
        DFM::init(&_localTypes, sizeof(_localTypes), uTrue);
    }
    
    // 2. check if desired range free or will it conflict with other ranges
    tmp = DFM::find(min, max);
    for (xui32 i = 0; i < tmp.size(); i++) {
        if (tmp[i] != type) {
            // overlap detected
            uLogApp(LOG_MAJ, "%s() Error, new range %s is overlapped with the existing %s"
                , __func__, DFM_TYPE_STRS[type], DFM_TYPE_STRS[tmp[i]]);
            goto _err;
        }
    }
    // 3. free range to use
    if (!DFM::types[type].set(min, max)) {
        goto _err;
    }

    // 4. try restoring old seed
    DFM::restore(type);
_done:
    return uTrue;
_err:
    uLogApp(LOG_INFO, "%s() KO", __func__);
    return uFalse;
}

/**
 * Get range by type
 */
DFMInfo* DFM::get(DFM_TYPE type) {
    if (!DFM::types || type >= DFM_TYPE_MAX) goto _err;
    return &DFM::types[type];
_err:
    return NULL;
}

/**
 * Dump all ranges
 */
void DFM::dump() {
    uLogApp(LOG_INFO, "======== Policy Ann-ID (Type3) ======== ");
    if (!DFM::types) return;
    
    for (DFM_TYPE i = DFM_TYPE_MIN; i < DFM_TYPE_MAX; i++) {
        DFM::types[i].dump();
    }
}


/*
 * decode type3 segmentID to find hidden path
 * 
 * E.g:
 *      input:  00000000001
 *      output: DFM/PAG/000/000/000
 *              <-----prefix ----->
*/
const xc8* DFM::decode(const xc8* segmentID) {
    static xc8 ret[24];
    
    // 1
    memset(&ret, 0, sizeof(ret));
    DFM_TYPE type = DFM::find(segmentID);
    if (type == DFM_TYPE_MAX) {
        goto _done;
    }
    
    // 2
    sprintf(ret, "%s/%s", DFM::name, DFM_TYPE_STRS[type]);
    for (xi32 i = 0; i < 9; i += 3) {
        snprintf(ret + strlen(ret), 5, "/%s", &segmentID[i]);
    }
_done:
    uLogApp(LOG_DEB3, "%s() done (%s)", __func__, ret);
    return ret;
}


/*
 * decode segmentID to find hidden path
 * then combine with lang & file extension
 * 
 * E.g:
 *      input:  "00000000001", "kor", "alaw"
 *      output: "DFM/PAG/000/000/000/kor-00000000001.alaw"
 *               <-----prefix ----->|<lang>-ID.<fmt>
*/
const xc8* DFM::decode(const xc8* segmentID, xc8* lang, xc8* fmt) {
    static xc8 ret[PA_MAX_LEN];
    memset(&ret, 0, sizeof(ret));
    
    if (!DFM::types || !segmentID || !lang || !fmt) {
        goto _done;
    }
    sprintf(ret, "%s/%s-%s.%s", DFM::decode(segmentID), lang, segmentID, fmt);
_done:
    return ret;
}


/*
 * decode segmentID to find hidden path
 * then combine with lang & file + ... to yield absolute path
 * 
 * E.g:
 *      input:  "00000000001", "kor", "alaw", "vCOMP", "/SI/Ann"
 *      output: "/SI/Ann/vCOMP/DFM/PAG/000/000/000/kor-00000000001.alaw"
 *              |<base>|<comp>|<---prefix ------->|<lang>-ID.<fmt>
*/
const xc8* DFM::decode(const xc8* segmentID, xc8* lang, xc8* fmt, xc8* comp, xc8* base) {
    if (!DFM::types || !segmentID || !lang || !fmt || !comp || !base) {
        goto _done;
    }
    static xc8 ret[PA_MAX_LEN];
    sprintf(ret, "%s/%s/%s", base, comp, DFM::decode(segmentID, lang, fmt));
_done:
    return ret;
}
//======================= Generate & sync annID =============================/
/**
 * Set data file name
 */
Boolean DFM::syncFn(DFM_TYPE type, const xc8* local, const xc8* remote, xui32 mode) {
    FILE *f_local = NULL;
    FILE *f_remote = NULL;
	DFMInfo tmp_local;
	DFMInfo tmp_remote;
	DFMInfo * tmp_fwrite;
	xui32 pos = 0;


    if (!local || strlen(local) == 0) {
        goto _err;
    }
	if (!remote || strlen(remote) == 0) {
        goto _err;
    }
    if (type >= DFM_TYPE_MAX) {
        goto _err;
    }
    pos = type * sizeof(DFMInfo);

    f_local = fopen(local, "wb+");
	f_remote = fopen(remote, "wb+");
    if (!f_local) {
        uLogApp(LOG_MIN, "%s() Unable to open local cached file (%s): %s", __func__, local, strerror(errno));
        return uFalse;
    }
    if (!f_remote) {
        uLogApp(LOG_MIN, "%s() Unable to open remote cached file (%s): %s", __func__, remote, strerror(errno));
        return uFalse;
    }

    fseek(f_local, pos, SEEK_SET);
	fseek(f_remote,pos, SEEK_SET);

    if (fread(&tmp_local, sizeof(DFMInfo), 1, f_local)) {
        if (tmp_local.getArchive()) {
    		if (fread(&tmp_remote, sizeof(DFMInfo), 1, f_remote)) {
        		if (tmp_remote.getArchive()) {
					/*
					if(tmp_remote.getCurSeed() > tmp_local.getCurSeed())
					{
						tmp_local.setSeed(tmp_remote.getCurSeed(),uTrue);
						tmp_fwrite = &tmp_local;

						fwrite(tmp_fwrite,sizeof(DFMInfo),1,f_local);
						fflush(f_local);
					}
					else if(tmp_remote.getCurSeed() < tmp_local.getCurSeed())
					{	
						tmp_remote.setSeed(tmp_local.getCurSeed(),uTrue);
						tmp_fwrite = &tmp_remote;
						write(tmp_fwrite,sizeof(DFMInfo),1,f_remote);
                        fflush(f_remote);
					}
					*/
				}
			}
		}
	}
	CLOSE_FP(f_local)
    CLOSE_FP(f_remote)
    return uTrue;
_err:
    uLogApp(LOG_MIN, "%s() Error, invalid local(%s), remote(%s)", __func__, local,remote);
    return uFalse;
}

//======================= Generate & manage annID =============================/
/**
 * Set data file name
 */
Boolean DFM::setFn(const xc8* val, xui32 mode) {
    FILE *f = NULL;
    DFM::fn = NULL;
    if (mode == 0)
        fn = strdup(DFM_CACHE_FILE_DEF);
    else
        fn = strdup(DFM_CACHE_FILE(mode));
    uLogApp(LOG_DEB3, "%s() val (%s), current file (%s)", __func__, val, DFM::fn);

    if (!val || strlen(val) == 0) {
        goto _err;
    }
    if (strcasecmp(val, DFM::fn) != 0) {
        free(DFM::fn);
        DFM::fn = strdup(val);
        DFM::closeFp();
    }
    return uTrue;
_err:
    uLogApp(LOG_MIN, "%s() Error, invalid val (%s)", __func__, val);
    return uFalse;
}


/**
 * Open fp
 */
Boolean DFM::openFp(Boolean read) {
    FILE** f = &DFM::fp;
    xc8* mode = "wb+";

    // mode
    if (read) {
        f = &DFM::fpRead;
        mode = "rb";
    }

    // file is deleted by user while running
    if (*f && (access(DFM::fn, F_OK) != 0)) {
        uLogApp(LOG_MIN, "%s() Error, missing cached file", __func__);
        CLOSE_FP(*f)
    }

    // open fp
    if (!*f) {
        *f = fopen(DFM::fn, mode);
        if (!*f) {
            uLogApp(LOG_MIN, "%s() Unable to open cached file (%s): %s", __func__, DFM::fn, strerror(errno));
            return uFalse;
        }
    }
    return uTrue;
}

Boolean DFM::closeFp() {
    CLOSE_FP(DFM::fp)
    CLOSE_FP(DFM::fpRead)
    return uTrue;
}

/**
 * Archive entire item to avoid manual editing file
 *
 * Return:
 *  success: uTrue
 *  failure: uFalse
 */
Boolean DFM::archive(DFM_TYPE type) {
    xui32 pos = 0;
    xui32 ret = 0;
    uLogApp(LOG_DEB3, "%s() type (%d), fp (%p)", __func__, type, DFM::fp);
    // 1. guards & jump
    if (!DFM::types || type >= DFM_TYPE_MAX || !DFM::openFp()) {
        goto _err;
    }
    pos = type * sizeof(DFMInfo);
    fseek(DFM::fp, pos, SEEK_SET);

    // 2. save current seed
    //fprintf(DFM::fp, PA_FMT_DIGIT_3"\n", DFM::types[type].getCurSeed());
    DFM::types[type].setArchive();
    fwrite(&DFM::types[type], sizeof(DFMInfo), 1, DFM::fp);
    fflush(DFM::fp);
    uLogApp(LOG_DEB3, "%s() OK", __func__);
    return uTrue;
_err:
    uLogApp(LOG_MIN, "%s() Error, unable to save current seed, %d", __func__, ret);
    return uFalse;
}


/**
 * Restore the previous seed if possible
 *
 * Return:
 *  success: uTrue
 *  failure: uFalse
 */
Boolean DFM::restore(DFM_TYPE type) {
    xc8 buf[PA_TYPE_DIGIT_3 + 1];
    Boolean ret = uFalse;
    long len = 0;
    long pos = 0;
    DFMInfo tmp;

    uLogApp(LOG_DEB3, "%s() type (%d) (%p)", __func__, type, DFM::fpRead);

	closeFp();
    // 1. guard & jump
    if (!DFM::types || type >= DFM_TYPE_MAX || !DFM::openFp(uTrue)) {
        goto _err;
    }
    pos = type * sizeof(DFMInfo);
    fseek(DFM::fpRead, 0L, SEEK_END);
    len = ftell(DFM::fpRead);
    uLogApp(LOG_DEB3, "%s() pos (%d) / len (%d)", __func__, pos, len);
    if (pos >= len) {
        goto _err;
    }
    fseek(DFM::fpRead, pos, SEEK_SET);

    // 2. load previous seed
    if (fread(&tmp, sizeof(DFMInfo), 1, DFM::fpRead)) {
        if (tmp.getArchive()) {
            uLogApp(LOG_DEB3, "%s() OK, found", __func__);
            tmp.dump();
            ret = DFM::types[type].setSeed(tmp.getCurSeed());
        }
        else {
            goto _err;
        }
    }
    CLOSE_FP(DFM::fpRead)
    return ret;
_err:
    CLOSE_FP(DFM::fpRead);
    uLogApp(LOG_INFO, "%s() not found previous seed of type (%d)", __func__, type);
    return uFalse;
}

/**
 * Get new ID by type & cache it
 *
 * Return:
 *  success: new free ID (long)
 *  failed:  -1 if invalid type or reached max range
 */
long DFM::generateL(DFM_TYPE type, DFM_ID_MODE mode) {
    long ret = -1;
    if (!DFM::types || type >= DFM_TYPE_MAX) {
        goto _done;
    }

    ret = DFM::types[type].generateL(mode);
    if (ret != -1) {
        DFM::archive(type);
    }
_done:
    return ret;
}

/**
 * Get new ID string by type
 *
 * Return:
 *  success: new free ID
 *  failure: NULL if invalid or reached max range
 */
xc8* DFM::generate(DFM_TYPE type, DFM_ID_MODE mode) {
    xc8* ret = NULL;

    if (!DFM::types || type >= DFM_TYPE_MAX) {
        goto _done;
    }
    ret = DFM::types[type].generate(mode);
    if (ret) {
        DFM::archive(type);
    }
_done:
    return ret;
}

/**
 * Reset seed & current seed
 *
 * Return:
 *  success: uTrue
 *  failure: uFalse
 */
Boolean DFM::seed(DFM_TYPE type, long val) {
    uLogApp(LOG_DEB3, "%s() type (%d), val ("PA_FMT_DIGIT_3")", __func__, type, val);
    if (!DFM::types || type >= DFM_TYPE_MAX) {
        goto _err;
    }

    if (DFM::types[type].setSeed(val)) {
        DFM::archive(type);
        return uTrue;
    }
_err:
    uLogApp(LOG_MIN, "%s() Error, type (%d), val ("PA_FMT_DIGIT_3")", __func__, type, val);
    return uFalse;
}
