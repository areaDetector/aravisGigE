/* aravisCamera.cpp
 *
 * This is a driver for a GigE area detector.
 *
 * Author: Tom Cobb
 *         Diamond Light Source
 *
 * Created:  4th October 2010
 *
 */

/* System includes */
#include <map>
#include <vector>
#include <string>
#include <deque>
#include <limits>

#include <stdarg.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* EPICS includes */
#include <iocsh.h>
#include <epicsGuard.h>
#include <epicsExport.h>
#include <epicsExit.h>
#include <epicsEndian.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <initHooks.h>

#include "ghelper.h"
#include "adhelper.h"

typedef epicsGuard<asynPortDriver> Guard;
typedef epicsGuardRelease<asynPortDriver> UnGuard;

// catch in asynPortDriver methods, should be followed with appropriate error return
#define CATCH(asynUser) catch(std::exception& e) { \
    asynPrint(asynUser, ASYN_TRACE_ERROR, \
                "%s:%s: %s Error: %s\n", \
                driverName, __FUNCTION__, portName, e.what()); \
    }


/* number of raw buffers in our queue */
#define NRAW 20

/* maximum number of custom features that we support */
#define NFEATURES 1000

/* driver name for asyn trace prints */
static const char *driverName = "aravisCamera";

/* lookup for BinningMode strings */
struct bin_lookup {
    const char * mode;
    int binx, biny;
};
// the first entry in this array is used as a default
static const bin_lookup bin_lookup_arr[] = {
    { "Binning1x1", 1, 1 },
    { "Binning1x2", 1, 2 },
    { "Binning1x4", 1, 4 },
    { "Binning2x1", 2, 1 },
    { "Binning2x2", 2, 2 },
    { "Binning2x4", 2, 4 },
    { "Binning4x1", 4, 1 },
    { "Binning4x2", 4, 2 },
    { "Binning4x4", 4, 4 }
};

const bin_lookup* binmode_by_string(const char *name)
{
    const unsigned N = sizeof(bin_lookup_arr) / sizeof(bin_lookup_arr[0]);
    for(unsigned i=0; i<N; i++) {
        if(strcmp(name, bin_lookup_arr[i].mode)==0)
            return &bin_lookup_arr[i];
    }
    return NULL;
}

const bin_lookup* binmode_by_xy(int x, int y)
{
    const unsigned N = sizeof(bin_lookup_arr) / sizeof(bin_lookup_arr[0]);
    for(unsigned i=0; i<N; i++) {
        if(bin_lookup_arr[i].binx==x && bin_lookup_arr[i].biny==y)
            return &bin_lookup_arr[i];
    }
    return NULL;
}

/* lookup for pixel format types */
struct pix_lookup {
    ArvPixelFormat fmt;
    int colorMode, dataType, bayerFormat;
};

/* The GENICAM pixel format code convention was unfortunately not
 * done in an constructable/parsable way.
 * Only bits per pixel can be extracted programatically (0x00FF0000).
 * So we have a lookup table...
 *
 * The first entry in this array is used as a default for all cameras
 */
static const pix_lookup pix_lookup_arr[] = {
    { ARV_PIXEL_FORMAT_MONO_8,        NDColorModeMono,  NDUInt8,  0           },
    { ARV_PIXEL_FORMAT_RGB_8_PACKED,  NDColorModeRGB1,  NDUInt8,  0           },
    { ARV_PIXEL_FORMAT_BAYER_GR_8,    NDColorModeBayer, NDUInt8,  NDBayerGRBG },
    { ARV_PIXEL_FORMAT_BAYER_RG_8,    NDColorModeBayer, NDUInt8,  NDBayerRGGB },
    { ARV_PIXEL_FORMAT_BAYER_GB_8,    NDColorModeBayer, NDUInt8,  NDBayerGBRG },
    { ARV_PIXEL_FORMAT_BAYER_BG_8,    NDColorModeBayer, NDUInt8,  NDBayerBGGR },
// For Int16, use Mono16 if available, otherwise Mono12
    { ARV_PIXEL_FORMAT_MONO_16,       NDColorModeMono,  NDUInt16, 0           },
// this doesn't work on Manta camers    { ARV_PIXEL_FORMAT_MONO_14,       NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_MONO_12,       NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_MONO_10,       NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_RGB_12_PACKED, NDColorModeRGB1,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_RGB_10_PACKED, NDColorModeRGB1,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_BAYER_GR_12,   NDColorModeBayer, NDUInt16, NDBayerGRBG },
    { ARV_PIXEL_FORMAT_BAYER_RG_12,   NDColorModeBayer, NDUInt16, NDBayerRGGB },
    { ARV_PIXEL_FORMAT_BAYER_GB_12,   NDColorModeBayer, NDUInt16, NDBayerGBRG },
    { ARV_PIXEL_FORMAT_BAYER_BG_12,   NDColorModeBayer, NDUInt16, NDBayerBGGR }
};

const pix_lookup* pixel_by_format(ArvPixelFormat fmt)
{
    const unsigned N = sizeof(pix_lookup_arr) / sizeof(pix_lookup_arr[0]);
    for(unsigned i=0; i<N; i++) {
        if(pix_lookup_arr[i].fmt==fmt)
            return &pix_lookup_arr[i];
    }
    return NULL;
}

const pix_lookup* pixel_by_info(int mode, int dtype, int bayer)
{
    const unsigned N = sizeof(pix_lookup_arr) / sizeof(pix_lookup_arr[0]);
    for(unsigned i=0; i<N; i++) {
        if(pix_lookup_arr[i].colorMode==mode &&
                pix_lookup_arr[i].dataType==dtype &&
                pix_lookup_arr[i].bayerFormat==bayer)
            return &pix_lookup_arr[i];
    }
    return NULL;
}

/* Convert ArvBufferStatus enum to string */
const char * ArvBufferStatusToString( ArvBufferStatus buffer_status )
{
    const char  *   pString;
    switch( buffer_status )
    {
        default:
        case ARV_BUFFER_STATUS_UNKNOWN:         pString = "Unknown";        break;
        case ARV_BUFFER_STATUS_SUCCESS:         pString = "Success";        break;
        case ARV_BUFFER_STATUS_CLEARED:         pString = "Buffer Cleared"; break;
        case ARV_BUFFER_STATUS_TIMEOUT:         pString = "Timeout";        break;
        case ARV_BUFFER_STATUS_MISSING_PACKETS: pString = "Missing Pkts";   break;
        case ARV_BUFFER_STATUS_WRONG_PACKET_ID: pString = "Wrong Pkt ID";   break;
        case ARV_BUFFER_STATUS_SIZE_MISMATCH:   pString = "Image>bufSize";  break;
        case ARV_BUFFER_STATUS_FILLING:         pString = "Filling";        break;
        case ARV_BUFFER_STATUS_ABORTED:         pString = "Aborted";        break;
    }
    return pString;
}

struct Feature {
    // one time setup
    asynParamType type;
    int param;
    typedef std::vector<std::string> names_t;
    names_t names; // all "aliases" of this feature
    Feature& addName(const char *name) {
        names.push_back(name);
        return *this;
    }

    // linear scaling (with optional invert) to convert between asyn and aravis (feature) units
    // applied to integer and float
    bool invert;
    double slope, offset;

    double arv2asyn(double arv) const {
        double T = slope*arv + offset;
        return invert ? 1.0/T : T;
    }
    double asyn2arv(double asyn) const {
        double T = invert ? 1.0/asyn : asyn;
        return (T - offset)/slope;
    }

    // must hold lock to access

    // set if some asynUser has set this parameter (via. a write*() method)
    // an indication this this feature value is a setting
    // which should be pushed on re-connect
    bool userSetting;
    Feature& setting(bool v) {
        userSetting = v;
        return *this;
    }

    // number of consecutive times the feature scanner
    // has pushed a change.
    // used to break update loops due to eg. rounding with read-back
    unsigned nchanged;

    // set on connect by worker thread w/o lock while Connecting
    // R/O in other states
    std::string activeName;
    enum FType { Invalid, Integer, Float, String, Command } activeType;
    ArvGcNode *activeNode;

    Feature()
        :type(asynParamNotDefined), param(-1)
        ,invert(false) ,slope(1), offset(0)
        ,userSetting(false), nchanged(0)
        ,activeType(Invalid), activeNode(NULL)
    {}
    Feature(const std::string& name, asynParamType type, int param)
        :type(type), param(param)
        ,invert(false) ,slope(1), offset(0)
        ,userSetting(false), nchanged(0)
        ,activeType(Invalid), activeNode(NULL) {
        names.push_back(name);
    }
    Feature& scale(double slo, double off, bool inv = false) {
        slope = slo;
        offset = off;
        invert = inv;
        return *this;
    }
};

/** Aravis GigE detector driver */
class aravisCamera : public ADDriver, epicsThreadRunable {
public:
    typedef std::map<std::string, aravisCamera*> cameras_t;
    static cameras_t cameras;

    struct FeatureScanner : epicsThreadRunable {
        aravisCamera * const owner;
        virtual void run() {
            owner->runScanner();
        }
        FeatureScanner(aravisCamera *o) :owner(o) {}
    } scanner;

    /* Constructor */
    aravisCamera(const char *portName, const char *cameraName,
                int maxBuffers, size_t maxMemory,
                int priority, int stackSize);
    virtual ~aravisCamera();

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeOctet(asynUser *pasynUser, const char *value, size_t maxChars, size_t *nActual);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
    void report(FILE *fp, int details);

    /* This is the method we override from epicsThreadRunable */
    void run();
    void runScanner();

    std::string cameraName;

    /* all features which have an associated aPD parameter, indexed by parameter.
     * populated during IOC initialization as record INP/OUT are scanned.
     * Entries must never be removed as storage is shared with activeFeatures
     */
    typedef std::map<int, Feature> interestingFeatures_t;
    interestingFeatures_t interestingFeatures;

    // interestingFeatures indexed by feature name
    typedef std::map<std::string, Feature*> interestingFeatureNames_t;
    interestingFeatureNames_t interestingFeatureNames;

    void insertInteresting(const Feature& f);

    /* Map from aPD param to feature which are interesting, and present in
     * the connected camera.
     * Contains pointers to storage of interestingFeatures.
     * This mapping may include some magic X* params
     */
    typedef std::vector<Feature*> activeFeatures_t;
    activeFeatures_t activeFeatures;

    //! Image data size in bytes
    size_t payloadSize;
    //! flag requests a re-check of payload size due to possible changes
    bool payloadSizeCheck;

    GWrapper<ArvStream> stream;
    ArvDevice* device;
    ArvGc* genicam;
    GWrapper<ArvCamera> camera;

    enum state_t {
        Init,         // before our worker starts
        RetryWait,
        Connecting,
        Connected,
        Shutdown,
    } target_state,  // updated by various
      current_state; // updated only by worker thread

    int AravisCompleted;
    #define FIRST_ARAVIS_CAMERA_PARAM AravisCompleted
    int AravisFailures;
    int AravisUnderruns;
    int AravisFrameRetention;
    int AravisMissingPkts;
    int AravisPktResend;
    int AravisPktTimeout;
    int AravisResentPkts;
    int AravisLeftShift;
    int AravisConnection;
    int AravisHWImageMode;
    int AravisSyncd;
    int AravisCamName;
    int AravisReset;
    #define LAST_ARAVIS_CAMERA_PARAM AravisReset
    #define NUM_ARAVIS_CAMERA_PARAMS (&LAST_ARAVIS_CAMERA_PARAM - &FIRST_ARAVIS_CAMERA_PARAM + 1 + NFEATURES)

    // special magic parameters for features which don't map to a single aPD param
    // these are *not* valid aPD params
    int XPixelFormat; // PixelFormat <-> NDColorMode, NDDataType, NDBayerPattern
    int XBinningMode; // BinningMode <-> ADBinX, ADBinY

    // the do*() methods must only be called from the poller worker thread
    void doCleanup(Guard &G);
    void doConnect(Guard &G);
    void doAcquireStart(Guard &G);
    void doAcquireStop(Guard &G);
    void doHandleBuffer(Guard &G, GWrapper<ArvBuffer>&);

    void dropConnection();

    void pushNDArray();

    typedef std::deque<GWrapper<ArvBuffer> > bufqueue_t;
    bufqueue_t bufqueue;

    // Guards access
    epicsMutex arvLock;

    epicsEvent  pollingEvent, scannerEvent;
    epicsThread pollingLoop, scannerLoop;

    void error(const char *fmt, ...) EPICS_PRINTF_STYLE(2,3);
    void trace(const char *fmt, ...) EPICS_PRINTF_STYLE(2,3);

    static void aravisShutdown(void* arg);
    static void newBufferCallback (ArvStream *stream, aravisCamera *pPvt);
    static void controlLostCallback(ArvDevice *device, aravisCamera *pPvt);
};

aravisCamera::cameras_t aravisCamera::cameras;

void aravisCamera::error(const char *fmt, ...) {
    if((*pasynTrace->getTraceMask)(pasynUserSelf)&ASYN_TRACE_ERROR) {
        va_list args;
        va_start(args, fmt);
        (*pasynTrace->vprint)(pasynUserSelf, ASYN_TRACE_ERROR, fmt, args);
        va_end(args);
    }
}
void aravisCamera::trace(const char *fmt, ...) {
    if(pasynTrace->getTraceMask(pasynUserSelf)&ASYN_TRACE_FLOW) {
        va_list args;
        va_start(args, fmt);
        (*pasynTrace->vprint)(pasynUserSelf, ASYN_TRACE_FLOW, fmt, args);
        va_end(args);
    }
}

/** Called by epicsAtExit to shutdown camera */
void aravisCamera::aravisShutdown(void* arg) {
    aravisCamera *pPvt = (aravisCamera *) arg;
    try {
        Guard G(*pPvt);
        pPvt->trace("%s: begin shutdown\n", pPvt->portName);
        pPvt->target_state = Shutdown;
        {
            UnGuard U(G);
            pPvt->pollingEvent.signal();
            pPvt->scannerEvent.signal();
            pPvt->pollingLoop.exitWait();
            pPvt->scannerLoop.exitWait();
        }
        pPvt->doCleanup(G);
        pPvt->trace("%s: complete shutdown\n", pPvt->portName);
    }catch(std::exception& e){
        pPvt->error("%s: Error during camera shutdown: %s\n",
                    pPvt->portName, e.what());
    }
}

/** Called by aravis when destroying a buffer with an NDArray wrapper */
static void destroyBuffer(gpointer data){
    if (data != NULL) {
        NDArray *pRaw = (NDArray *) data;
        pRaw->release();
    }
}

/** Called by aravis when a new buffer is produced */
void aravisCamera::newBufferCallback (ArvStream *stream, aravisCamera *pPvt) {
    static int  nConsecutiveBadFrames;

    const char * const portName = pPvt->portName;
    try {
        GWrapper<ArvBuffer> buffer(arv_stream_try_pop_buffer(stream));

        ArvBufferStatus buffer_status = arv_buffer_get_status(buffer);

        if (buffer_status == ARV_BUFFER_STATUS_SUCCESS /*|| buffer->status == ARV_BUFFER_STATUS_MISSING_PACKETS*/) {

            bool wakeup = false;
            {
                Guard G(*pPvt);
                wakeup = pPvt->bufqueue.empty();

                if(pPvt->bufqueue.size()<NRAW) {
                    pPvt->bufqueue.push_back(buffer);
                    pPvt->trace("%s:%s have new frame%c\n",
                                pPvt->portName, __FUNCTION__,
                                wakeup ? '!' : '.');

                } else {
                    pPvt->error("%s:%s Message queue full, dropped buffer\n", pPvt->portName, __FUNCTION__);
                    arv_stream_push_buffer (stream, buffer.release());
                }
            }

            if(wakeup) pPvt->pollingEvent.signal();

        } else {
            // bad buffer
            //TODO: as of 0.5.6 aravis doesn't set ARV_BUFFER_STATUS_SIZE_MISMATCH
            //      when buffer size is too small.
            //      so check buffer size before re-queue

            arv_stream_push_buffer (stream, buffer.release());

            nConsecutiveBadFrames++;
            if ( nConsecutiveBadFrames < 10 )
                pPvt->error("Bad frame status: %s\n", ArvBufferStatusToString(buffer_status) );
            else if ( ((nConsecutiveBadFrames-10) % 1000) == 0 ) {
                static int  nBadFramesPrior = 0;
                pPvt->error("Bad frame status: %s, %d msgs suppressed.\n", ArvBufferStatusToString(buffer_status),
                        nConsecutiveBadFrames - nBadFramesPrior );
                nBadFramesPrior = nConsecutiveBadFrames;
            }
        }

    }CATCH(pPvt->pasynUserSelf)
}

/** Constructor for aravisCamera; most parameters are simply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to compute the GigE detector data,
  * and sets reasonable default values for parameters defined in this class, asynNDArrayDriver and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] cameraName The name of the camera, \<vendor\>-\<serial#\>, as returned by arv-show-devices
  * \param[in] maxBuffers The maximum number of NDArray buffers that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited number of buffers.
  * \param[in] maxMemory The maximum amount of memory that the NDArrayPool for this driver is
  *            allowed to allocate. Set this to -1 to allow an unlimited amount of memory.
  * \param[in] priority The thread priority for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  * \param[in] stackSize The stack size for the asyn port driver thread if ASYN_CANBLOCK is set in asynFlags.
  */
aravisCamera::aravisCamera(const char *portName, const char *cameraName,
                         int maxBuffers, size_t maxMemory, int priority, int stackSize)

    : ADDriver(portName, 1, NUM_ARAVIS_CAMERA_PARAMS, maxBuffers, maxMemory,
               0, 0, /* No interfaces beyond those set in ADDriver.cpp */
               0, 1, /* ASYN_CANBLOCK=0, ASYN_MULTIDEVICE=0, autoConnect=1 */
               priority, stackSize)
    ,scanner(this)
    ,cameraName(cameraName)
    ,payloadSize(0)
    ,payloadSizeCheck(false)
    ,target_state(Connecting)
    ,current_state(Init)
    ,pollingLoop(*this, "aravisPoll", stackSize, epicsThreadPriorityHigh)
    ,scannerLoop(scanner, "aravisScan", stackSize, epicsThreadPriorityMedium)
{

    trace("%s Create\n", portName);

    /* Create some custom parameters */
    createParam("ARAVIS_COMPLETED",      asynParamFloat64, &AravisCompleted);
    createParam("ARAVIS_FAILURES",       asynParamFloat64, &AravisFailures);
    createParam("ARAVIS_UNDERRUNS",      asynParamFloat64, &AravisUnderruns);
    createParam("ARAVIS_FRAME_RETENTION",asynParamInt32,   &AravisFrameRetention);
    createParam("ARAVIS_MISSING_PKTS",   asynParamInt32,   &AravisMissingPkts);
    createParam("ARAVIS_RESENT_PKTS",    asynParamInt32,   &AravisResentPkts);
    createParam("ARAVIS_PKT_RESEND",     asynParamInt32,   &AravisPktResend);
    createParam("ARAVIS_PKT_TIMEOUT",    asynParamInt32,   &AravisPktTimeout);
    createParam("ARAVIS_LEFTSHIFT",      asynParamInt32,   &AravisLeftShift);
    createParam("ARAVIS_CONNECTION",     asynParamInt32,   &AravisConnection);
    createParam("ARAVIS_HWIMAGEMODE",    asynParamInt32,   &AravisHWImageMode);
    createParam("ARAVIS_SYNCD",          asynParamInt32,   &AravisSyncd);
    createParam("ARAVIS_CAMNAME",        asynParamOctet,   &AravisCamName);
    createParam("ARAVIS_RESET",          asynParamInt32,   &AravisReset);

    /* Set some initial values for other parameters */
    setIntegerParam(ADReverseX, 0);
    setIntegerParam(ADReverseY, 0);
    setIntegerParam(ADImageMode, ADImageContinuous);
    setIntegerParam(ADNumImages, 100);
    setDoubleParam(AravisCompleted, 0);
    setDoubleParam(AravisFailures, 0);
    setDoubleParam(AravisUnderruns, 0);
    setIntegerParam(AravisFrameRetention, 100000);  // aravisGigE default 100ms
    setIntegerParam(AravisMissingPkts, 0);
    setIntegerParam(AravisPktResend, 1);
    setIntegerParam(AravisPktTimeout, 20000);       // aravisGigE default 20ms
    setIntegerParam(AravisResentPkts, 0);
    setIntegerParam(AravisLeftShift, 1);
    setIntegerParam(AravisHWImageMode, 0);
    setIntegerParam(AravisSyncd, 0);
    setStringParam(AravisCamName, cameraName);
    setIntegerParam(AravisReset, 0);
    setIntegerParam(AravisConnection, 0);

    // pre-populate with standard AD parameter mappings
    // Note: mappings w/ .setting(true) must have default values

    // some "standard" features aren't so standard, consider several possibilities
    insertInteresting(Feature("Gain", asynParamFloat64, ADGain)
                      .addName("GainRaw")
                      .addName("GainRawChannelA")
                      .setting(true));
    setDoubleParam(ADGain, 1.0); // arbitrary default

    /* For JAI CM this is an enum.  We treat this as an Integer
       in that case, this should work correctly if this
       camera uses an integer FPS rate.*/
    // feature is freq. in Hz, asyn is period in sec.
    insertInteresting(Feature("AcquisitionFrameRate", asynParamFloat64, ADAcquirePeriod)
                      .addName("AcquisitionFrameRate")
                      .addName("AcquisitionFrameRateAbs")
                      .scale(1, 0, true)
                      .setting(true));
    setDoubleParam(ADAcquirePeriod, 1.0); // arbitrary default

    // feature is micro-seconds, asyn is sec
    insertInteresting(Feature("ExposureTime", asynParamFloat64, ADAcquireTime)
                      .addName("ExposureTimeAbs")
                      .scale(1e-6, 0)
                      .setting(true));
    setDoubleParam(ADAcquireTime, 20e-6); // arbitrary default

    // arv_camera_get_vendor_name()
    insertInteresting(Feature("DeviceVendorName", asynParamOctet, ADManufacturer));
    // arv_camera_get_model_name()
    insertInteresting(Feature("DeviceModelName", asynParamOctet, ADModel));
    // arv_camera_get_sensor_size()
    insertInteresting(Feature("SensorWidth", asynParamInt32, ADMaxSizeX));
    insertInteresting(Feature("SensorHeight", asynParamInt32, ADMaxSizeY));
    // arv_camera_get_region()
    insertInteresting(Feature("OffsetX", asynParamInt32, ADMinX));
    insertInteresting(Feature("OffsetY", asynParamInt32, ADMinY));
    // TODO: also set NDArraySizeX and NDArraySizeY from Width and Height
    insertInteresting(Feature("Width", asynParamInt32, ADSizeX));
    insertInteresting(Feature("Height", asynParamInt32, ADSizeY));
    insertInteresting(Feature("PayloadSize", asynParamInt32, NDArraySize));

    // arv_camera_set_trigger();
    insertInteresting(Feature("TriggerMode", asynParamInt32, ADTriggerMode));

    insertInteresting(Feature("ReverseX", asynParamInt32, ADReverseX).setting(true));
    insertInteresting(Feature("ReverseY", asynParamInt32, ADReverseY).setting(true));
    setIntegerParam(ADReverseX, 0);
    setIntegerParam(ADReverseY, 0);

    // note: special handling below for cameras with BinningMode
    //       instead of BinningHorizontal+BinningVertical
    insertInteresting(Feature("BinningHorizontal", asynParamInt32, ADBinX)
                      .setting(true));
    insertInteresting(Feature("BinningVertical", asynParamInt32, ADBinY)
                      .setting(true));

    // special handling needed later for some parameters
    // we assume regular aPD parameters ID are >=0
    XPixelFormat = -42;
    insertInteresting(Feature("PixelFormat", asynParamNotDefined, XPixelFormat)
                      .setting(true));

    // ensure that the inputs for PixelFormat start with valid values
    setIntegerParam(NDColorMode, pix_lookup_arr[0].colorMode);
    setIntegerParam(NDDataType, pix_lookup_arr[0].dataType);
    setIntegerParam(NDBayerPattern, pix_lookup_arr[0].bayerFormat);

    XBinningMode = -43;
    insertInteresting(Feature("BinningMode", asynParamNotDefined, XBinningMode)
                      .setting(true));

    setIntegerParam(ADBinX, bin_lookup_arr[0].binx);
    setIntegerParam(ADBinY, bin_lookup_arr[0].biny);

    /* Register the shutdown function for epicsAtExit */
    epicsAtExit(aravisShutdown, (void*)this);
}

aravisCamera::~aravisCamera() {
    cantProceed("can't ~aravisCamera().  asyn ports are forever\n");
}

void aravisCamera::insertInteresting(const Feature &f)
{
    assert(current_state==Init);

    Feature& store = interestingFeatures[f.param] = f; // copy
    for(Feature::names_t::const_iterator it = store.names.begin(),
                                        end = store.names.end();
        it!=end; ++it)
    {
        interestingFeatureNames[*it] = &store;
    }
}

asynStatus aravisCamera::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize)
{
    try {
        // attempt lazy parameter creation unless Initialization is complete
        if(strncmp("ARVI_", drvInfo, 5)==0 ||
           strncmp("ARVD_", drvInfo, 5)==0 ||
           strncmp("ARVS_", drvInfo, 5)==0)
        {

            asynParamType newtype;
            switch(drvInfo[3]) {
            case 'I': newtype=asynParamInt32; break;
            case 'D': newtype=asynParamFloat64; break;
            case 'S': newtype=asynParamOctet; break;
            default:
                asynPrint(pasynUser, ASYN_TRACE_ERROR,
                            "%s:%s: Expected ARVx_... where x is one of I, D or S. Got '%c'\n",
                            driverName, __FUNCTION__, drvInfo[3]);
                return asynError;
            }

            std::string pname(&drvInfo[5]);

            interestingFeatureNames_t::const_iterator it = interestingFeatureNames.find(pname);

            if(it!=interestingFeatureNames.end()) {
                const Feature &feat = *it->second;
                if(feat.type!=newtype) {
                    asynPrint(pasynUser, ASYN_TRACE_ERROR,
                                "%s:%s: parameter %s already present with different type\n",
                                driverName, __FUNCTION__, drvInfo);
                    return asynError;
                }

                /* use existing param name as some existing features are mapped
                 * to standard AD params
                 */
                getParamName( feat.param, &drvInfo );

            } else if(current_state!=Init) {
                error("After Init, Won't lazy create param for %s\n", drvInfo);

            } else {
                // lazy param creation
                int param = -1;
                if(createParam(drvInfo, newtype, &param)!=asynSuccess) {
                    asynPrint(pasynUser, ASYN_TRACE_ERROR,
                                "%s:%s: parameter creation fails for %s\n",
                                driverName, __FUNCTION__, drvInfo);
                    return asynError;
                }

                setParamStatus(param, asynError);

                // parameter name has ARV*_ prefix
                // stored feature name does not
                insertInteresting(Feature(pname, newtype, param));
                trace("Lazy create parameter for feature %s\n", pname.c_str());
            }
        }

        // Now return baseclass result
        return ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
    }CATCH(pasynUser);
    return asynError;
}

// image acquisition worker
void aravisCamera::run()
{
    Guard G(*this);

    bool acquiring = false;
    trace("%s: poller starting\n", portName);

    while(current_state!=Shutdown) {
        try {
            switch(current_state) {
            case Init:
                // notify for lazily created parameters
                for(interestingFeatures_t::const_iterator it = interestingFeatures.begin(),
                                                         end = interestingFeatures.end();
                    it != end; ++it)
                {
                    if(it->first>=0)
                        setParamStatus(it->first, asynError);
                }
                callParamCallbacks();
                target_state = Connecting;
                break;

            case RetryWait:
                doCleanup(G);
                bool ok;
            {
                UnGuard U(G);
                ok = !pollingEvent.wait(5.0);
            }
                if(ok && target_state==RetryWait)
                    target_state = Connecting;
                break;

            case Connecting:
                target_state = Connected;
                acquiring = false;
                doCleanup(G);
                doConnect(G);
                break;

            case Connected:
            {
                int acq = 0;
                getIntegerParam(ADAcquire, &acq);

                if(payloadSizeCheck && acquiring && acq) {
                    // we don't need a temp reference to camera while unlocked
                    // as only this thread could change it it.
                    guint psize;
                    {
                        UnGuard U(G);
                        psize = arv_camera_get_payload(camera);
                    }
                    if(psize!=payloadSize) {
                        acquiring = false;
                        doAcquireStop(G);
                        acquiring = true;
                        doAcquireStart(G);
                    }
                    payloadSizeCheck = false;
                }

                if(acquiring && !acq) {
                    // stop acquiring
                    acquiring = false;
                    doAcquireStop(G);
                } else if(!acquiring && acq) {
                    // start acquiring
                    doAcquireStart(G);
                    acquiring = true;
                }

                if(acquiring) {
                    while(target_state==Connected && bufqueue.empty()) {
                        UnGuard U(G);
                        pollingEvent.wait();
                    }
                    if(!bufqueue.empty()) {
                        GWrapper<ArvBuffer> buf;
                        bufqueue.front().swap(buf);
                        bufqueue.pop_front();

                        // decide if we should stop acquiring
                        int imageMode = ADImageSingle;
                        getIntegerParam(ADImageMode, &imageMode);

                        bool stopit = false;
                        switch(imageMode) {
                        case ADImageSingle:
                            stopit = true;
                            break;
                        case ADImageMultiple:
                        {
                            int needed = 0, sofar = 0;
                            getIntegerParam(ADNumImagesCounter, &sofar); // does not include this frame
                            getIntegerParam(ADNumImages, &needed);
                            stopit = sofar >= needed-1;
                        }
                            break;
                        case ADImageContinuous:
                            break;
                        }

                        if(stopit) {
                            trace("%s:%s stop acquire from manual control\n", portName, __FUNCTION__);
                            acquiring = false;
                            setIntegerParam(ADAcquire, 0);
                            doAcquireStop(G);
                        }

                        if(stream)
                            pushNDArray(); // pre-emptively add a replacement for the used frame

                        trace("%s:%s handle new buffer %p\n", portName, __FUNCTION__, buf.get());
                        doHandleBuffer(G, buf);
                    }

                } else {
                    UnGuard U(G);
                    pollingEvent.wait();
                }

            }
                break;
            case Shutdown:
                target_state = Shutdown;
                break;
            }

        }catch(std::exception& e) {
            error("%s:%s: Error in worker: %s\n",
                  portName, __FUNCTION__, e.what());
            target_state = RetryWait;
        }

        if(current_state != target_state) {
            trace("%s: poller %d -> %d\n", portName, (int)current_state, (int)target_state);
            scannerEvent.signal();

            current_state = target_state;
        }
    }

    trace("%s: poller stopping\n", portName);
}

// feature scanning worker
void aravisCamera::runScanner()
{
    Guard G(*this);
    trace("%s:%s scanner start\n", portName, __FUNCTION__);

    std::vector<char> old_str(100);

    // we will hold a reference while unlocked
    GWrapper<ArvCamera> cam;
    int sync_cnt = 0;

    while(target_state!=Shutdown) {
        if(current_state!=Connected) {
            trace("%s:%s scanner idle\n", portName, __FUNCTION__);

            sync_cnt = 0;
            setIntegerParam(AravisSyncd, sync_cnt);
            callParamCallbacks();

            UnGuard U(G);
            scannerEvent.wait();

        } else {
            trace("%s:%s scanner begin\n", portName, __FUNCTION__);

            // grab reference for use while locked
            cam = camera;

            for(activeFeatures_t::const_iterator it = activeFeatures.begin(),
                                                end = activeFeatures.end();
                it!=end; ++it)
            {
                Feature * const feat = *it;

                // Commands can't be sync'd
                if(feat->activeType==Feature::Command) continue;

                // some parts of feat storage not stable when unlocked,
                // so copy out important pieces
                ArvGcNode *node = feat->activeNode;
                Feature::FType ftype = feat->activeType;
                bool userSetting = feat->userSetting;
                unsigned nchanges = feat->nchanged;

                // old/new value are in feature units
                double old_val, new_val = 0.0;
                std::string new_str;

                // fetch current param value while locked
                unsigned rbsts;
                switch(feat->type) {
                case asynParamInt32:
                {
                    int old;
                    rbsts = getIntegerParam(feat->param, &old);
                    old_val = feat->asyn2arv(old);
                }
                    break;
                case asynParamFloat64:
                    rbsts = getDoubleParam(feat->param, &old_val);
                    old_val = feat->asyn2arv(old_val);
                    break;
                case asynParamOctet:
                    rbsts = getStringParam(feat->param, old_str.size(), &old_str[0]);
                    old_str.back() = '\0';
                    break;
                case asynParamNotDefined:
                    // "magic" parameters (< 0) have no type
                    if(feat->param==XPixelFormat) {
                        int mode = NDColorModeMono, type = NDUInt8, bayer = NDBayerRGGB;
                        rbsts = getIntegerParam(NDColorMode, &mode);
                        rbsts|= getIntegerParam(NDDataType, &type);
                        rbsts|= getIntegerParam(NDBayerPattern, &bayer);

                        const pix_lookup *px = pixel_by_info(mode, type, bayer);
                        if(!px) {
                            // hum, we shouldn't have allowed this in writeInt32()
                            // soft fail to a default
                            px = &pix_lookup_arr[0];
                        }
                        old_val = feat->asyn2arv(px->fmt);

                    } else if(feat->param==XBinningMode) {
                        int x, y;
                        rbsts = getIntegerParam(ADBinX, &x);
                        rbsts|= getIntegerParam(ADBinY, &y);

                        const bin_lookup *bm = binmode_by_xy(x, y);
                        if(!bm) {
                            bm = &bin_lookup_arr[0];
                        }
                        strncpy(&old_str[0], bm->mode, old_str.size());
                        old_str.back() = '\0';

                    } else {
                        continue; // should not be hit
                    }
                    break;
                default:
                    continue; // should not be hit
                }

                // read current value and update if userSetting and differs
                GErrorHelper gerr;
                bool changed = rbsts!=asynSuccess;

                bool forcestore = changed && userSetting;
                if(forcestore) {
                    error("%s:%s param %s marked as setting w/o a setting value.  Forcing to current value.\n",
                          portName, __FUNCTION__, feat->activeName.c_str());
                }

                {
                    UnGuard U(G);
                    epicsGuard<epicsMutex> IO(arvLock);

                    switch(ftype) {
                    case Feature::Integer:
                        new_val = arv_gc_integer_get_value(ARV_GC_INTEGER(node), gerr.get());
                        changed|= !gerr && fabs(old_val-new_val) > 1e-10;
                        if(userSetting && changed && nchanges<3) {
                            // camera setting out of sync, push our value
                            arv_gc_integer_set_value(ARV_GC_INTEGER(node), old_val, gerr.get());
                        }
                        break;

                    case Feature::Float:
                        new_val = arv_gc_float_get_value(ARV_GC_FLOAT(node), gerr.get());
                        changed|= !gerr && fabs(old_val-new_val) > 1e-10;
                        if(userSetting && changed && nchanges<3) {
                            // camera setting out of sync, push our value
                            arv_gc_float_set_value(ARV_GC_FLOAT(node), old_val, gerr.get());
                        }
                        break;

                    case Feature::String:
                        new_str = arv_gc_string_get_value(ARV_GC_STRING(node), gerr.get());
                        changed|= !gerr && strcmp(&old_str[0], new_str.c_str())!=0;
                        if(userSetting && changed && nchanges<3) {
                            // camera setting out of sync, push our value
                            arv_gc_string_set_value(ARV_GC_STRING(node), &old_str[0], gerr.get());
                        }
                        break;

                    case Feature::Command:
                    case Feature::Invalid:
                        break; // never reached
                    }
                }
                // locked again

                // connection state may have changed while we were unlocked
                if(current_state!=Connected || cam!=camera) {
                    // lost connection, or re-connected
                    UnGuard U(G);
                    cam.reset();
                    break; // from activeFeatures loop
                }

                if(gerr) {
                    error("Error %s syncing feature %s\n", gerr->message, feat->activeName.c_str());
                    if(gerr->code==ARV_DEVICE_STATUS_TIMEOUT)
                        dropConnection();
                }

                if(userSetting && !forcestore) {
                    if(changed) {
                        if(feat->nchanged<3) {
                            feat->nchanged++;
                            trace("%s:%s pushed %s (%u)\n",
                                  portName, __FUNCTION__, feat->activeName.c_str(), feat->nchanged);
                        } else if(feat->nchanged==3) {
                            feat->nchanged++;
                            error("%s:%s would push %s, but gave up\n", portName, __FUNCTION__, feat->activeName.c_str());
                        }
                    } else {
                        feat->nchanged = 0;
                    }

                } else if(!gerr && changed) {
                    setParamStatus(feat->param, asynSuccess);

                    switch(feat->type) {
                    case asynParamInt32:
                        trace("%s:%s pulled %s int %g -> %g\n",
                              portName, __FUNCTION__, feat->activeName.c_str(),
                              old_val, new_val);
                        setIntegerParam(feat->param, feat->arv2asyn(new_val));
                        break;
                    case asynParamFloat64:
                        trace("%s:%s pulled %s flt %g -> %g\n",
                              portName, __FUNCTION__, feat->activeName.c_str(),
                              old_val, new_val);
                        setDoubleParam(feat->param, feat->arv2asyn(new_val));
                        break;
                    case asynParamOctet:
                        trace("%s:%s pulled %s str \"%s\" -> \"%s\"\n",
                              portName, __FUNCTION__, feat->activeName.c_str(),
                              &old_str[0], new_str.c_str());
                        setStringParam(feat->param, new_str.c_str());
                        break;
                    case asynParamNotDefined:
                        // "magic" parameters (< 0) have no type
                        error("magic parameter not userSetting?\n");
                        break;
                    default:
                        break; // never reached
                    }

                    callParamCallbacks();

                    {
                        UnGuard U(G);
                        scannerEvent.wait(0.01);
                    }
                    if(current_state!=Connected || cam!=camera) {
                        // lost connection, or re-connected
                        UnGuard U(G);
                        cam.reset();
                        break; // from activeFeatures loop
                    }
                }
            } // end activeFeatures loop

            setIntegerParam(AravisSyncd, ++sync_cnt);
            callParamCallbacks();
            trace("%s:%s scanner complete %u\n", portName, __FUNCTION__, sync_cnt);

            UnGuard U(G);
            scannerEvent.wait(0.1);
        }
    }

    trace("%s:%s scanner stop\n", portName, __FUNCTION__);
}

void aravisCamera::doCleanup(Guard &G)
{
    trace("%s:%s\n", portName, __FUNCTION__);
    /* Tell areaDetector it is no longer acquiring */
    setIntegerParam(AravisConnection, 0);
    setIntegerParam(ADStatus, ADStatusIdle);
    setIntegerParam(ADAcquire, 0); //TODO wait for initial sync and then begin acquire automatically

    GWrapper<ArvCamera> cam;
    GWrapper<ArvStream> strm;

    cam.swap(camera);
    device = NULL;
    genicam = NULL;
    strm.swap(stream);

    payloadSize = 0;

    // mark all "readback" paramters as INVALID
    for(activeFeatures_t::const_iterator it = activeFeatures.begin(), end = activeFeatures.end();
        it!=end; ++it)
    {
        Feature * const feat = *it;
        if(feat->param<0 || feat->userSetting) continue;

        setParamStatus(feat->param, asynError); //TODO asynParamUndefined?
    }

    callParamCallbacks();

    activeFeatures.clear();

    {
        UnGuard U(G);
        epicsGuard<epicsMutex> IO(arvLock);
        if(cam)
            arv_camera_stop_acquisition(cam); // cmd: AcquisitionStop

        // release reference w/o port lock in case this does I/O
        //  eg. to clear Control Priv. register
        strm.reset();
        cam.reset();
    }
}

/** Called by aravis when control signal is lost */
void aravisCamera::controlLostCallback(ArvDevice *device, aravisCamera *pPvt) {
    printf("Control lost!\n");
    pPvt->lock();
    pPvt->dropConnection();
    pPvt->unlock();
}

void aravisCamera::doConnect(Guard& G)
{
    trace("%s:%s\n", portName, __FUNCTION__);
    // if camera name not specified, then bail early
    if(cameraName.empty()) {
        trace("%s:%s no cameraName\n", portName, __FUNCTION__);
        target_state = RetryWait;
        return;
    }

    GWrapper<ArvCamera> cam;
    ArvDevice* dev;
    ArvGc* gcam;
    activeFeatures_t active;

    // unlock while we perform setup of new camera (lots of I/O)
    // we modify Feature::active*
    // don't modify any other members
    {
        UnGuard U(G);
        epicsGuard<epicsMutex> IO(arvLock);

        cam.reset(arv_camera_new (cameraName.c_str()));
        if(!cam)
            throw std::runtime_error(cameraName+": No such camera");
        trace("%s:%s have camera\n", portName, __FUNCTION__);

        arv_camera_stop_acquisition(cam); // paranoia?  cmd: AcquisitionStop

        dev = arv_camera_get_device(cam);
        gcam = arv_device_get_genicam(dev);
        if(!dev || !gcam)
            throw std::runtime_error("No ArvDevice or ArvGC?!?");

        // Make standard size packets
    //    arv_gv_device_set_packet_size(ARV_GV_DEVICE(dev), ARV_GV_DEVICE_GVSP_PACKET_SIZE_DEFAULT);
        // Uncomment this line to set jumbo packets
    //    arv_gv_device_set_packet_size(ARV_GV_DEVICE(dev), 9000);

        /* Check the tick frequency */
        guint64 freq = arv_gv_device_get_timestamp_tick_frequency(ARV_GV_DEVICE(dev));
        printf("aravisCamera: Your tick frequency is %" G_GUINT64_FORMAT "\n", freq);
        if (freq > 0) {
            printf("So your timestamp resolution is %f ns\n", 1.e9/freq);
        } else {
            printf("So your camera doesn't provide timestamps. Using system clock instead\n");
        }

        // explicitly check that we are the controlling client.
        // stream creation will fail anyway if we are not.
        guint32 regval = 0;
        if (arv_device_read_register(dev, ARV_GVBS_CONTROL_CHANNEL_PRIVILEGE_OFFSET, &regval, NULL)) {
            if (regval&ARV_GVBS_CONTROL_CHANNEL_PRIVILEGE_CONTROL) {
                // all ok
            } else {
                //TODO: read/print GevSCDA and GevSCPHostPort to give some clue who has control
                throw std::runtime_error("Another client has control of this camera.");
            }
        } else {
            error("Unable to read camera CONTROL_CHANNEL register.  Device not accessible?!?!");
        }

        /* For Baumer cameras, AcquisitionFrameRate will not do anything until we set this
         * NOTE: *no* d in AcquisitionFrameRateEnable */
        if (arv_device_get_feature(dev, "AcquisitionFrameRateEnable")) {
            arv_device_set_integer_feature_value (dev, "AcquisitionFrameRateEnable", 1);
        }

        /* For Point Grey cameras, AcquisitionFrameRate will not do anything until we set this
         * NOTE: there is a d in AcquisitionFrameRateEnabled */
        if (arv_device_get_feature(dev, "AcquisitionFrameRateEnabled")) {
            arv_device_set_integer_feature_value (dev, "AcquisitionFrameRateEnabled", 1);
        }
    deviceID = arv_camera_get_device_id(this->camera);
    if (deviceID) status |= setStringParam (ADSerialNumber, deviceID);
    firmwareVersion = arv_device_get_string_feature_value(this->device, "DeviceFirmwareVersion");
    if (firmwareVersion) status |= setStringParam (ADFirmwareVersion, firmwareVersion);

        // map camera features to AD parameters
        trace("%s:%s mapping features\n", portName, __FUNCTION__);

        for(interestingFeatures_t::iterator it = interestingFeatures.begin(),
                                                 end = interestingFeatures.end();
            it!=end; ++it)
        {
            Feature &feat = it->second;
            // spoil
            feat.activeName.clear();
            feat.activeType = Feature::Invalid;
            feat.activeNode = NULL;
            feat.nchanged = 0;

            // check all possible names for this feature
            for(Feature::names_t::const_iterator it2 = feat.names.begin(),
                                                 end2 = feat.names.end();
                it2!=end2; ++it2)
            {
                const std::string& name = *it2;
                ArvGcNode *node = arv_device_get_feature(dev, name.c_str());

                if(!node) continue;

                //TODO handle magic parameters with asynParamNotDefined
                /* type mapping asyn -> aravis (feature)
                 * int32   -> Integer or Command
                 * float64 -> Integer or Float
                 * octet   -> String
                 * undefined handled based on X* param ID
                 */

                if((feat.type==asynParamFloat64 || feat.type==asynParamInt32 || feat.param==XPixelFormat)
                        && ARV_IS_GC_INTEGER(node)) {
                    feat.activeType = Feature::Integer;

                } else if((feat.type==asynParamFloat64 || feat.type==asynParamInt32)
                          && ARV_IS_GC_FLOAT(node)) {
                    feat.activeType = Feature::Float;

                } else if((feat.type==asynParamOctet || feat.param==XBinningMode)
                          && ARV_IS_GC_STRING(node)) {
                    feat.activeType = Feature::String;

                } else if(feat.type==asynParamInt32 && ARV_IS_GC_COMMAND(node)) {
                    feat.activeType = Feature::Command;

                } else {
                    error("%s:%s interesting Feature %s has unsupported type asyn=%d arv=%s\n",
                          portName, __FUNCTION__, name.c_str(), (int)feat.type,
                          g_type_name(arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(node))));

                }

                if(feat.activeType!=Feature::Invalid) {
                    feat.activeName = name;
                    feat.activeNode = node;
                    active.push_back(&feat);
                    trace("%s:%s active %s\n", portName, __FUNCTION__, name.c_str());
                }

                break; // found one alt. so don't check others
            }

            if(feat.activeType==Feature::Invalid) {
                trace("%s:%s not found %s\n", portName, __FUNCTION__, feat.names.front().c_str());
            }
        }
    }
    // port locked again now
    // copy info from locals to class members

    trace("%s:%s install camera\n", portName, __FUNCTION__);

    camera.swap(cam);
    device = dev;
    genicam = gcam;
    activeFeatures.swap(active);

    setIntegerParam(AravisConnection, 1);

    callParamCallbacks();

    epicsGuard<epicsMutex> IO(arvLock);
    g_signal_connect (device, "control-lost", G_CALLBACK (controlLostCallback), this);
}

void aravisCamera::doAcquireStart(Guard &G)
{
    trace("%s:%s\n", portName, __FUNCTION__);
    // we don't need a temp reference as we
    // are only called from the poller worker thread,
    // so current_state can't change on us

    GWrapper<ArvStream> strm;
    guint psize;

    int imageMode = ADImageSingle,
        numImages = 1,
        hwImageMode = 0;

    getIntegerParam(AravisHWImageMode, &hwImageMode);
    getIntegerParam(ADImageMode, &imageMode);
    getIntegerParam(ADNumImages, &numImages);

    epicsInt32      FrameRetention = 200000,
                    PktResend      = ARV_GV_STREAM_PACKET_RESEND_ALWAYS,
                    PktTimeout     = 40000;
    getIntegerParam(AravisFrameRetention,  &FrameRetention);
    getIntegerParam(AravisPktResend,       &PktResend);
    getIntegerParam(AravisPktTimeout,      &PktTimeout);

    payloadSizeCheck = false;
    {
        UnGuard U(G);
        epicsGuard<epicsMutex> IO(arvLock);

        strm.reset(arv_camera_create_stream (camera, NULL, NULL));
        if(!strm) {
            throw std::runtime_error("Unable to create image stream.");
        }

        /* configure the stream */
        // Available stream options:
        //  socket-buffer:      ARV_GV_STREAM_SOCKET_BUFFER_FIXED, ARV_GV_STREAM_SOCKET_BUFFER_AUTO, defaults to auto which follows arvgvbuffer size
        //  socket-buffer-size: 64 bit int, Defaults to -1
        //  packet-resend:      ARV_GV_STREAM_PACKET_RESEND_NEVER, ARV_GV_STREAM_PACKET_RESEND_ALWAYS, defaults to always
        //  packet-timeout:     64 bit int, units us, ARV_GV_STREAM default 40000
        //  frame-retention:    64 bit int, units us, ARV_GV_STREAM default 200000
        g_object_set (strm,
                  "packet-resend",      (guint64) PktResend,
                  "packet-timeout",     (guint64) PktTimeout,
                  "frame-retention",    (guint64) FrameRetention,
                  NULL);

        psize = arv_camera_get_payload(camera);

        if (hwImageMode && imageMode == ADImageSingle) {
            arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_SINGLE_FRAME);
        } else if (hwImageMode && imageMode == ADImageMultiple && arv_device_get_feature(device, "AcquisitionFrameCount")) {
            arv_device_set_string_feature_value(this->device, "AcquisitionMode", "MultiFrame");
            arv_device_set_integer_feature_value(this->device, "AcquisitionFrameCount", numImages);
        } else {
            arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_CONTINUOUS);
        }

        // starting before filling stream's input queue may lead to some underflow
        // but this is considered acceptable as these frames would be lost in any
        // case.
        arv_camera_start_acquisition(camera);
    }

    if(arv_device_get_status(device)!=ARV_DEVICE_STATUS_SUCCESS) {
        error("%s:%s Failed to start acquire\n", portName, __FUNCTION__);
        dropConnection();
        return;
    }

    if(payloadSizeCheck) {
        // not sure if this can happen, but make sure it doesn't
        throw std::logic_error("payload size change race?!?!");
    }

    payloadSize = psize;

    stream.swap(strm);

    // load up stream input frame queue with empty NDArray
    for (int i=0; i<NRAW; i++)
        pushNDArray();

    {
        epicsGuard<epicsMutex> IO(arvLock);
        g_signal_connect (stream, "new-buffer", G_CALLBACK (newBufferCallback), this);

        arv_stream_set_emit_signals (stream, TRUE);
    }

    setIntegerParam(ADStatus, ADStatusAcquire);
    setIntegerParam(ADNumImagesCounter, 0);

    callParamCallbacks();
    trace("%s:%s acquiring\n", portName, __FUNCTION__);
}

void aravisCamera::doAcquireStop(Guard &G)
{
    trace("%s:%s\n", portName, __FUNCTION__);
    GWrapper<ArvStream> strm;

    stream.swap(strm);

    setIntegerParam(ADStatus, ADStatusIdle);

    callParamCallbacks();

    {
        UnGuard U(G);
        epicsGuard<epicsMutex> IO(arvLock);

        arv_camera_stop_acquisition(camera);

        strm.reset();
    }

    // see if AcquisitionStop command failed
    if(arv_device_get_status(device)!=ARV_DEVICE_STATUS_SUCCESS)
        dropConnection();
}

void aravisCamera::dropConnection()
{
    error("%s:%s\n", portName, __FUNCTION__);
    if(target_state!=Shutdown)
        target_state = RetryWait;
    pollingEvent.signal();
}

/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADColorMode, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus aravisCamera::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status;
    epicsInt32 rbv;
    const char  *   reasonName = "unknownReason";
    getParamName( 0, function, &reasonName );

    interestingFeatures_t::iterator featit;

    if (function == AravisReset) {
        dropConnection();
        status = asynSuccess;

    } else if (function == AravisLeftShift) {
        if (value < 0 || value > 1) {
            status = asynError;
        } else {
            status = setIntegerParam(function, value);
        }

    } else if ((featit = interestingFeatures.find(function)) != interestingFeatures.end()) {
        // change to mapped feature value
        Feature* feat = &featit->second;

        if(!feat->userSetting) {
            if(current_state==Init) {
                trace("%s:%s mark as setting %s w/ %d\n",
                      portName, __FUNCTION__, reasonName, (int)value);
                feat->userSetting = true;
            } else {
                error("%s:%s can't mark as setting %s after Init\n", portName, __FUNCTION__, feat->activeName.c_str());
            }
        }

        // Cheating here as we can't/shouldn't unlock
        // The following calls may block until reply timeout

        GErrorHelper err;
        if(current_state==Connected) {
            epicsGuard<epicsMutex> IO(arvLock);

            switch(feat->activeType) {
            case Feature::Integer:
                value = feat->asyn2arv(value);
                arv_gc_integer_set_value(ARV_GC_INTEGER(feat->activeNode), value, err.get());
                rbv = arv_gc_integer_get_value(ARV_GC_INTEGER(feat->activeNode), err.get());
                if(!err) value = rbv;
                value = feat->arv2asyn(value);
                break;
            case Feature::Command:
                arv_gc_command_execute(ARV_GC_COMMAND(feat->activeNode), err.get());
                break;
            default:
                // should not happen unless activeFeatures type check was wrong
                break;
            }
        }

        status = setIntegerParam(function, value);

        if(err) {
            error("Error setting %s : %s\n", feat->activeName.c_str(), err->message);
            status = asynError;
        } else {
            setParamStatus(function, asynSuccess);
            payloadSizeCheck = true;
        }

    } else if (function == ADAcquire) {
        setIntegerParam(function, value);
        // wake up the poller thread to actually execute the start/stop
        pollingEvent.signal();
        status = asynSuccess;

    } else if (function == ADBinX || function == ADBinY) {
        // special handling for BinningMode
        // only reached if BinningHorizontal+BinningVertical not in activeFeatures
        rbv = value;
        getIntegerParam(function, &rbv);
        setIntegerParam(function, value);

        int x = 1, y = 1;
        getIntegerParam(ADBinX, &x);
        getIntegerParam(ADBinY, &y);

        const bin_lookup *bm = binmode_by_xy(x, y);
        if(!bm) {
            status = asynError;
            setIntegerParam(function, rbv);

        } else if(current_state==Connected) {
            epicsGuard<epicsMutex> IO(arvLock);

            arv_device_set_string_feature_value(device, "BinningMode", bm->mode);
            switch(arv_device_get_status(device)) {
            case ARV_DEVICE_STATUS_SUCCESS:
                status = asynSuccess;
                break;
            case ARV_DEVICE_STATUS_TIMEOUT:
                dropConnection();
                // fall through
            default:
                status = asynError;
            }
            payloadSizeCheck = true;

        } else {
            status = asynSuccess;
        }

    } else if (function == NDDataType || function == NDColorMode || NDBayerPattern) {
        // special handling for PixelFormat
        rbv = value;
        getIntegerParam(function, &rbv);
        setIntegerParam(function, value);

        int mode = pix_lookup_arr[0].colorMode,
            type = pix_lookup_arr[0].dataType,
            bayer= pix_lookup_arr[0].bayerFormat;
        getIntegerParam(NDColorMode, &mode);
        getIntegerParam(NDDataType, &type);
        getIntegerParam(NDBayerPattern, &bayer);

        const pix_lookup *px = pixel_by_info(mode, type, bayer);
        if(!px) {
            status = asynError;
            setIntegerParam(function, rbv);

        } else if(current_state==Connected) {
            epicsGuard<epicsMutex> IO(arvLock);
            //TODO: validate against PixelFormat enumerations

            arv_camera_set_pixel_format(camera, px->fmt);
            switch(arv_device_get_status(device)) {
            case ARV_DEVICE_STATUS_SUCCESS:
                status = asynSuccess;
                break;
            case ARV_DEVICE_STATUS_TIMEOUT:
                dropConnection();
                // fall through
            default:
                // attempting to set an unsupported format will land here
                status = asynError;
                error("%s:%s failed to set PixelFormat\n", portName, __FUNCTION__);
            }
            payloadSizeCheck = true;
            setParamStatus(function, asynSuccess);
        } else {
            status = asynSuccess;
        }

    } else if (function == ADReverseX || function == ADReverseY || function == ADFrameType) {
        /* if not supported, then succeed if not reversed */
        status = value ? asynError : asynSuccess;

    } else if (function == ADNumExposures) {
        /* only one at the moment */
        if (value!=1) {
            status = asynError;
        } else {
            status = setIntegerParam(function, value);
        }

    } else if (function == AravisFrameRetention
            || function == AravisPktResend   || function == AravisPktTimeout 
            || function == AravisHWImageMode) {
        /* just write the value for these as they get fetched via getIntegerParam when needed */
        status = setIntegerParam(function, value);

    } else if (function < FIRST_ARAVIS_CAMERA_PARAM) {
        /* If this parameter belongs to a base class call its method */
        status = ADDriver::writeInt32(pasynUser, value);

    } else {
        error("%s:%s unhandled param %s\n", portName, __FUNCTION__, reasonName);
        status = asynError;
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

    /* Report any errors */
    if (status && current_state==Connected)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:writeInt32 error, status=%d function=%d %s, value=%d\n",
              driverName, status, function, reasonName, value);
    else if(!status)
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeInt32: function=%d %s, value=%d\n",
              driverName, function, reasonName, value);

    return status;
}

/** Called when asyn clients call pasynFloat64->write().
  * This function performs actions for some parameters, including ADAcquireTime, ADGain, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus aravisCamera::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    double rbv = 0;
    asynStatus status;
    const char  *   reasonName = "unknownReason";
    getParamName( 0, function, &reasonName );

    interestingFeatures_t::iterator featit;

    if ((featit = interestingFeatures.find(function)) != interestingFeatures.end()) {
        // change to mapped feature value
        Feature* feat = &featit->second;

        if(!feat->userSetting) {
            if(current_state==Init) {
                trace("%s:%s mark as setting %s w/ %f\n",
                      portName, __FUNCTION__, feat->activeName.c_str(), value);
                feat->userSetting = true;
            } else {
                error("%s:%s can't mark as setting %s after Init\n", portName, __FUNCTION__, feat->activeName.c_str());
            }
        }

        // Cheating here as we can't/don't unlock
        // The following calls may block until reply timeout

        GErrorHelper err;
        if(current_state==Connected) {
            epicsGuard<epicsMutex> IO(arvLock);

            value = feat->asyn2arv(value);

            switch(feat->activeType) {
            case Feature::Integer:
                arv_gc_integer_set_value(ARV_GC_INTEGER(feat->activeNode), value, err.get());
                rbv = arv_gc_integer_get_value(ARV_GC_INTEGER(feat->activeNode), err.get());
                if(!err) value = rbv;
                break;
            case Feature::Float:
                arv_gc_float_set_value(ARV_GC_FLOAT(feat->activeNode), value, err.get());
                rbv = arv_gc_float_get_value(ARV_GC_FLOAT(feat->activeNode), err.get());
                if(!err) value = rbv;
                break;
            default:
                // should not happen unless activeFeatures type check was wrong
                break;
            }

            value = feat->arv2asyn(value);
        }

        status = setDoubleParam(function, value);

        if(err) {
            error("Error setting %s : %s\n", feat->activeName.c_str(), err->message);
            status = asynError;
        }
        // assume no float params effect payloadSize

    } else if (function < FIRST_ARAVIS_CAMERA_PARAM) {
        status = ADDriver::writeFloat64(pasynUser, value);

    } else {
        error("%s:%s unhandled param %s\n", portName, __FUNCTION__, reasonName);
        status = asynError;
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:writeFloat64 error, status=%d function=%d %s, value=%f, rbv=%f\n",
              driverName, status, function, reasonName, value, rbv);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeFloat64: function=%d %s, value=%f\n",
              driverName, function, reasonName, value);
    return status;
}

asynStatus aravisCamera::writeOctet(asynUser *pasynUser, const char *value,
                                    size_t maxChars, size_t *nActual)
{
    int function = pasynUser->reason;
    asynStatus status;
    const char  *   reasonName = "unknownReason";
    getParamName( 0, function, &reasonName );

    if(function == AravisCamName) {
        std::string name(value);

        setStringParam(function, value);
        cameraName.swap(name);
        if(target_state==Connected)
            target_state = Connecting;
        else if(target_state!=Shutdown)
            target_state = RetryWait;
        dropConnection();

        *nActual = maxChars;
        status = asynSuccess;

        trace("%s:%s change camera name \"%s\" -> \"%s\"",
              portName, __FUNCTION__, name.c_str(), cameraName.c_str());

    } else if(function < FIRST_ARAVIS_CAMERA_PARAM) {
        status = ADDriver::writeOctet(pasynUser, value, maxChars, nActual);

    } else {
        error("%s:%s setting string features not implemented\n", portName, __FUNCTION__);
        status = asynError;
    }

    callParamCallbacks();
    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:writeOctet error, status=%d function=%d %s, value=%s\n",
              driverName, status, function, reasonName, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeOctet: function=%d %s, value=%s\n",
              driverName, function, reasonName, value);
    return status;
}


/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void aravisCamera::report(FILE *fp, int details)
{

    fprintf(fp, "Aravis GigE detector %s\n", this->portName);
    try {
        Guard G(*this);
        fprintf(fp, " State");
        switch(current_state) {
#define SHOW(ST) case ST: fprintf(fp, " " #ST "\n"); break
        SHOW(Init);
        SHOW(RetryWait);
        SHOW(Connecting);
        SHOW(Connected);
        SHOW(Shutdown);
#undef SHOW
        }

    } CATCH(pasynUserSelf)

    ADDriver::report(fp, details);
}

void aravisCamera::pushNDArray()
{
    if(current_state!=Connected)
        throw std::logic_error("Wrong state to pushNDArray");

    size_t bufferDims[2] = {1,1};
    NDArrayPtr nbuf(this->pNDArrayPool->alloc(2, bufferDims, NDInt8, payloadSize, NULL));
    GWrapper<ArvBuffer> gbuf(arv_buffer_new_full(payloadSize, nbuf->pData, (void *)nbuf.get(), destroyBuffer));
    nbuf.release();
    arv_stream_push_buffer(stream, gbuf); // apparently this can't fail?
    gbuf.release();
}

void aravisCamera::doHandleBuffer(Guard& G, GWrapper<ArvBuffer>& buffer)
{
    ArvPixelFormat pixel_fmt = arv_buffer_get_image_pixel_format(buffer);
    unsigned bitsperpx = ARV_PIXEL_FORMAT_BIT_PER_PIXEL(pixel_fmt),
             bytesperpx= bitsperpx/8u;
    unsigned width = arv_buffer_get_image_width(buffer);
    unsigned height = arv_buffer_get_image_height(buffer);

    size_t bufsize = 0;
    arv_buffer_get_data(buffer, &bufsize);

    // basic consistency test
    if(bufsize==0 || width*height*bytesperpx!=bufsize) {
        error("%s:%s Buffer size mis-match %u*%u*%u!=%u\n",
              portName, __FUNCTION__, width, height, bytesperpx, (unsigned)bufsize);
        return;
    }

    //TODO: early return if no plugin is listening? (arrayCallbacks==0)
    //getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

    // unwrap the underlying NDArray.  Treat this as a borrowed ref.
    NDArray *nbuf = (NDArray*)arv_buffer_get_user_data(buffer);

    getIntegerParam(NDArrayCounter, &nbuf->uniqueId);
    nbuf->timeStamp = arv_buffer_get_timestamp(buffer) / 1.e9;

    updateTimeStamp(&nbuf->epicsTS);

    /* Get any attributes that have been defined for this driver */
    this->getAttributes(nbuf->pAttributeList);

    const pix_lookup *fmt = pixel_by_format(pixel_fmt);
    if(!fmt) {
        error("%s:%s Unsupported pixel format %08x\n",
              portName, __FUNCTION__, (unsigned)pixel_fmt);
        return;
    }

    // our NDArray was allocated with dummy size/shape
    // so we must fill this in now

    // add() doesn't modify the last argument, just some missing const...
    nbuf->pAttributeList->add("BayerPattern", "Bayer Pattern", NDAttrInt32, (int*)&fmt->bayerFormat);
    nbuf->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, (int*)&fmt->colorMode);

    nbuf->dataType = (NDDataType_t) fmt->dataType;

    // the following should mirror NDArray::getInfo()
    // and handle all entries found in pix_lookup_arr[]

    unsigned xDim, yDim;
    switch(fmt->colorMode) {
    case NDColorModeMono:
    case NDColorModeBayer:
        xDim = 0;
        yDim = 1;
        nbuf->ndims = 2;
        break;
    case NDColorModeRGB1:
        xDim = 1;
        yDim = 2;
        nbuf->ndims = 3;
        nbuf->dims[0].size    = 3;
        nbuf->dims[0].offset  = 0;
        nbuf->dims[0].binning = 1;
        break;
    default:
        throw std::logic_error("Unhandled color mode from pix_lookup_arr[]");
    }
    nbuf->dims[xDim].size    = width;
    nbuf->dims[xDim].offset  = arv_buffer_get_image_x(buffer);
    nbuf->dims[xDim].binning = 1;
    getIntegerParam(ADBinX, &nbuf->dims[xDim].binning);

    nbuf->dims[yDim].size    = height;
    nbuf->dims[yDim].offset  = arv_buffer_get_image_y(buffer);
    nbuf->dims[yDim].binning = 1;
    getIntegerParam(ADBinY, &nbuf->dims[yDim].binning);

    int left_shift = 0;
    getIntegerParam(AravisLeftShift, &left_shift);
    if (left_shift) {
        unsigned shift = 0;
        switch (pixel_fmt) {
            case ARV_PIXEL_FORMAT_MONO_14:
                shift = 2;
                break;
            case ARV_PIXEL_FORMAT_MONO_12:
                shift = 4;
                break;
            case ARV_PIXEL_FORMAT_MONO_10:
                shift = 6;
                break;
            default:
                break;
        }
        if (shift != 0) {
            //printf("Shift by %d\n", shift);
            uint16_t *array = (uint16_t *) nbuf->pData;
            for (unsigned int ib = 0; ib < bufsize / 2; ib++) {
                array[ib] <<= shift;
            }
        }
    }

    {
        int imageCounter, numImages, numImagesCounter, imageMode;
        double acquirePeriod;

        getIntegerParam(NDArrayCounter, &imageCounter);
        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumImagesCounter, &numImagesCounter);
        getIntegerParam(ADImageMode, &imageMode);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);

        /* Report a new frame with the counters */
        setIntegerParam(NDArrayCounter, ++imageCounter);
        setIntegerParam(ADNumImagesCounter, ++numImagesCounter);
        if (imageMode == ADImageMultiple) {
            setDoubleParam(ADTimeRemaining, (numImages - numImagesCounter) * acquirePeriod);
        }
    }

    /* Report statistics unless stream already stoped (eg. single acquire) */
    if(false){ //stream.get()) {
        //TODO: report this from scanner thread?
        guint64 n_completed_buffers, n_failures, n_underruns;
        arv_stream_get_statistics(stream, &n_completed_buffers, &n_failures, &n_underruns);
        setDoubleParam(AravisCompleted, (double) n_completed_buffers);
        setDoubleParam(AravisFailures, (double) n_failures);
        setDoubleParam(AravisUnderruns, (double) n_underruns);

        guint64 n_resent_pkts, n_missing_pkts;

        arv_gv_stream_get_statistics(ARV_GV_STREAM(stream.get()), &n_resent_pkts, &n_missing_pkts);
        setIntegerParam(AravisResentPkts,  (epicsInt32) n_resent_pkts);
        setIntegerParam(AravisMissingPkts, (epicsInt32) n_missing_pkts);
    }

    /* this is a good image, so callback on it */
    {
        /* Call the NDArray callback */
        /* Must release the lock here, or we can get into a deadlock, because we can
         * block on the plugin lock, and the plugin can be calling us */
        UnGuard U(G);
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
             "%s:%s: calling imageData callback\n", driverName, __FUNCTION__);
        doCallbacksGenericPointer(nbuf, NDArrayData, 0);
    }

    /* Call the callbacks to update any changes */
    callParamCallbacks();
}

/** Configuration command, called directly or from iocsh */
extern "C" int aravisCameraConfig(const char *portName, const char *cameraName,
                                 int maxBuffers, size_t maxMemory, int priority, int stackSize)
{
    if (stackSize <= 0)
        stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
    try {
        aravisCamera *cam =
        new aravisCamera(portName, cameraName, maxBuffers, maxMemory,
                         priority, stackSize);
        aravisCamera::cameras[portName] = cam;

        return(asynSuccess);
    } catch(std::exception& e) {
        fprintf(stderr, "Unhandled C++ exception: %s\n", e.what());
        return(asynError);
    }
}

/** Code for iocsh registration */
static const iocshArg aravisCameraConfigArg0 = {"Port name", iocshArgString};
static const iocshArg aravisCameraConfigArg1 = {"Camera name", iocshArgString};
static const iocshArg aravisCameraConfigArg2 = {"maxBuffers", iocshArgInt};
static const iocshArg aravisCameraConfigArg3 = {"maxMemory", iocshArgInt};
static const iocshArg aravisCameraConfigArg4 = {"priority", iocshArgInt};
static const iocshArg aravisCameraConfigArg5 = {"stackSize", iocshArgInt};
static const iocshArg * const aravisCameraConfigArgs[] =  {&aravisCameraConfigArg0,
                                                          &aravisCameraConfigArg1,
                                                          &aravisCameraConfigArg2,
                                                          &aravisCameraConfigArg3,
                                                          &aravisCameraConfigArg4,
                                                          &aravisCameraConfigArg5};
static const iocshFuncDef configAravisCamera = {"aravisCameraConfig", 6, aravisCameraConfigArgs};
static void configAravisCameraCallFunc(const iocshArgBuf *args)
{
    aravisCameraConfig(args[0].sval, args[1].sval,
                      args[2].ival, args[3].ival, args[4].ival, args[5].ival);
}

/** Init hook that sets iocRunning flag */
static void startArvCameras(initHookState state) {
    if(state!=initHookAfterIocRunning) return;

    for(aravisCamera::cameras_t::const_iterator it=aravisCamera::cameras.begin(),
                                               end=aravisCamera::cameras.end();
        it!=end; ++it)
    {
        aravisCamera *cam = it->second;
        fprintf(stderr, "Starting aravisCamera %s\n", cam->portName);
        try {
            cam->scannerLoop.start();
            cam->pollingLoop.start();
        } catch(std::exception& e) {
            fprintf(stderr, "Failed to start camera %s: %s\n", cam->cameraName.c_str(), e.what());
        }
    }
}



static void aravisCameraRegister(void)
{
#if !GLIB_CHECK_VERSION(2,31,99)
    /* glib initialisation, deprecated in >=2.23.0 */
    g_thread_init (NULL);
    g_type_init ();
#endif
    /* Enable the fake camera for simulations */
    arv_enable_interface ("Fake");

    /* Register the pollingLoop to start after iocInit */
    initHookRegister(startArvCameras);

    iocshRegister(&configAravisCamera, configAravisCameraCallFunc);
}

extern "C" {
    epicsExportRegistrar(aravisCameraRegister);
}
