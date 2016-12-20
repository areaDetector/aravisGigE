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
#include <math.h>
#include <stdint.h>
#include <string.h>

/* EPICS includes */
#include <iocsh.h>
#include <epicsExport.h>
#include <epicsExit.h>
#include <epicsEndian.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <initHooks.h>

/* areaDetector includes */
#include <ADDriver.h>

/* aravis includes */
extern "C" {
    #include <arv.h>
}

/* number of raw buffers in our queue */
#define NRAW 20

/* maximum number of custom features that we support */
#define NFEATURES 1000

/* driver name for asyn trace prints */
static const char *driverName = "aravisCamera";

/* flag to say IOC is running */
static int iocRunning = 0;

/* lookup for binning mode strings */
struct bin_lookup {
    const char * mode;
    int binx, biny;
};
static const struct bin_lookup bin_lookup[] = {
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

/* lookup for pixel format types */
struct pix_lookup {
    ArvPixelFormat fmt;
    int colorMode, dataType, bayerFormat;
};

static const struct pix_lookup pix_lookup[] = {
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

/** Aravis GigE detector driver */
class aravisCamera : public ADDriver, epicsThreadRunable {
public:
    /* Constructor */
    aravisCamera(const char *portName, const char *cameraName,
                int maxBuffers, size_t maxMemory,
                int priority, int stackSize);

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
    void report(FILE *fp, int details);

    /* This is the method we override from epicsThreadRunable */
    void run();

    /* This should be private, but is used in the aravis callback so must be public */
    epicsMessageQueueId msgQId;

    /** Used by epicsAtExit */
    ArvCamera *camera;

    /** Used by connection lost callback */
    int connectionValid;

protected:
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
    int AravisGetFeatures;
    int AravisHWImageMode;
    int AravisReset;
    #define LAST_ARAVIS_CAMERA_PARAM AravisReset
    int features[NFEATURES];
    #define NUM_ARAVIS_CAMERA_PARAMS (&LAST_ARAVIS_CAMERA_PARAM - &FIRST_ARAVIS_CAMERA_PARAM + 1 + NFEATURES)

private:
    asynStatus allocBuffer();
    asynStatus processBuffer(ArvBuffer *buffer);
    asynStatus start();
    asynStatus stop();    
    asynStatus getBinning(int *binx, int *biny);
    asynStatus setBinning(int binx, int biny);
    asynStatus getGeometry();
    asynStatus setGeometry();
    asynStatus lookupColorMode(ArvPixelFormat fmt, int *colorMode, int *dataType, int *bayerFormat);
    asynStatus lookupPixelFormat(int colorMode, int dataType, int bayerFormat, ArvPixelFormat *fmt);
    asynStatus setIntegerValue(const char *feature, epicsInt32 value, epicsInt32 *rbv);
    asynStatus setFloatValue(const char *feature, epicsFloat64 value, epicsFloat64 *rbv);
    asynStatus connectToCamera();
    asynStatus makeCameraObject();
    asynStatus makeStreamObject();
    asynStatus getAllFeatures();
    asynStatus getNextFeature();
    int hasEnumString(const char* feature, const char *value);
    gboolean hasFeature(const char *feature);
    asynStatus tryAddFeature(int *ADIdx, const char *featureString);

    ArvStream *stream;
    ArvDevice *device;
    ArvGc *genicam;
    char *cameraName;
    GHashTable* featureLookup;
    GList *featureKeys;
    unsigned int featureIndex;
    int payload;
    epicsThread pollingLoop;
};

/** Called by epicsAtExit to shutdown camera */
static void aravisShutdown(void* arg) {
    aravisCamera *pPvt = (aravisCamera *) arg;
    ArvCamera *cam = pPvt->camera;
    printf("aravisCamera: Stopping %s... ", pPvt->portName);
    arv_camera_stop_acquisition(cam);
    pPvt->connectionValid = 0;
    epicsThreadSleep(0.1);
    pPvt->camera = NULL;
    g_object_unref(cam);
    printf("aravisCamera: OK\n");
}

/** Called by aravis when destroying a buffer with an NDArray wrapper */
static void destroyBuffer(gpointer data){
    NDArray *pRaw;
    if (data != NULL) {
        pRaw = (NDArray *) data;
        pRaw->release();
    }
}

/** Called by aravis when a new buffer is produced */
static void newBufferCallback (ArvStream *stream, aravisCamera *pPvt) {
    ArvBuffer *buffer;
    int status;
    static int  nConsecutiveBadFrames   = 0;
    buffer = arv_stream_try_pop_buffer(stream);
    if (buffer == NULL)    return;
    ArvBufferStatus buffer_status = arv_buffer_get_status(buffer);
    if (buffer_status == ARV_BUFFER_STATUS_SUCCESS /*|| buffer->status == ARV_BUFFER_STATUS_MISSING_PACKETS*/) {
        nConsecutiveBadFrames = 0;
        status = epicsMessageQueueTrySend(pPvt->msgQId,
                &buffer,
                sizeof(&buffer));
        if (status) {
            // printf as pPvt->pasynUserSelf for asynPrint is protected
            printf("Message queue full, dropped buffer\n");
            arv_stream_push_buffer (stream, buffer);
        }
    } else {
        // printf as pPvt->pasynUserSelf for asynPrint is protected
        arv_stream_push_buffer (stream, buffer);

        nConsecutiveBadFrames++;
        if ( nConsecutiveBadFrames < 10 )
            printf("Bad frame status: %s\n", ArvBufferStatusToString(buffer_status) );
        else if ( ((nConsecutiveBadFrames-10) % 1000) == 0 ) {
            static int  nBadFramesPrior = 0;
            printf("Bad frame status: %s, %d msgs suppressed.\n", ArvBufferStatusToString(buffer_status),
                    nConsecutiveBadFrames - nBadFramesPrior );
            nBadFramesPrior = nConsecutiveBadFrames;
        }
    }
}

/** Called by aravis when control signal is lost */
static void controlLostCallback(ArvDevice *device, aravisCamera *pPvt) {
    pPvt->connectionValid = 0;
}

/** Init hook that sets iocRunning flag */
static void setIocRunningFlag(initHookState state) {
    switch(state) {
        case initHookAfterIocRunning:
            iocRunning = 1;
            break;
        default:
            break;
    }
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
               priority, stackSize),
       camera(NULL),
       connectionValid(0),
       stream(NULL),
       device(NULL),
       genicam(NULL),
       featureKeys(NULL),
       payload(0),
       pollingLoop(*this, "aravisPoll", stackSize, epicsThreadPriorityHigh)
{
    const char *functionName = "aravisCamera";

    /* glib initialisation */
    g_type_init ();

    /* Duplicate camera name so we can use it if we reconnect */
    this->cameraName = epicsStrDup(cameraName);

    /* Create a lookup table from AD id to feature name string */
    this->featureLookup = g_hash_table_new(g_int_hash, g_int_equal);

    /* Create a message queue to hold completed frames */
    this->msgQId = epicsMessageQueueCreate(NRAW, sizeof(ArvBuffer*));
    if (!this->msgQId) {
        printf("%s:%s: epicsMessageQueueCreate failure\n", driverName, functionName);
        return;
    }

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
    createParam("ARAVIS_GETFEATURES",    asynParamInt32,   &AravisGetFeatures);
    createParam("ARAVIS_HWIMAGEMODE",    asynParamInt32,   &AravisHWImageMode);
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
    setIntegerParam(AravisReset, 0);
    
    /* Enable the fake camera for simulations */
    arv_enable_interface ("Fake");

    /* Connect to the camera */
    this->featureIndex = 0;
    this->connectToCamera();

    /* Register the shutdown function for epicsAtExit */
    epicsAtExit(aravisShutdown, (void*)this);

    /* Register the pollingLoop to start after iocInit */
    initHookRegister(setIocRunningFlag);
    this->pollingLoop.start();
}

asynStatus aravisCamera::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize) {
    static const char *functionName = "drvUserCreate";


    int index;

    // If parameter is of format ARVx_... where x is I for int, D for double, or S for string
    // then it is a camera parameter, so create it here
    if (findParam(drvInfo, &index) && strlen(drvInfo) > 5 && strncmp(drvInfo, "ARV", 3) == 0 && drvInfo[4] == '_') {
        /* Check we have allocated enough space */
        if (featureIndex > NFEATURES) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: Not enough space allocated to store all camera features, increase NFEATURES\n",
                        driverName, functionName);
            return asynError;
        }
        /* Check we have a feature */
        if (!this->hasFeature(drvInfo+5)) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: Parameter '%s' doesn't exist on camera\n",
                        driverName, functionName, drvInfo + 5);
            return asynError;
        }
        /* Make parameter of the correct type and get initial value if camera is connected */
        char *feature = epicsStrDup(drvInfo + 5);
        switch(drvInfo[3]) {
        case 'I':
            createParam(drvInfo, asynParamInt32, &(this->features[featureIndex]));
            if (this->connectionValid == 1) {
                ArvGcNode *featureNode = arv_device_get_feature(this->device, drvInfo + 5);
                int         curValue    = 0;
                if (!ARV_IS_GC_COMMAND(featureNode)) {
                    curValue = arv_device_get_integer_feature_value(this->device, feature);
                }
                setIntegerParam(this->features[featureIndex], curValue);
            }
            break;
        case 'D':
            createParam(drvInfo, asynParamFloat64, &(this->features[featureIndex]));
            if (this->connectionValid == 1)
                setDoubleParam(this->features[featureIndex], arv_device_get_float_feature_value(this->device, feature));
            break;
        case 'S':
            createParam(drvInfo, asynParamOctet, &(this->features[featureIndex]));
            if (this->connectionValid == 1) {
                const char *stringValue;
                stringValue = arv_device_get_string_feature_value(this->device, feature);
                if( stringValue == NULL )
                    stringValue = "(null)";
                printf("aravisCamera: Adding feature %s with value: %s\n", feature, stringValue);
                setStringParam(this->features[featureIndex], stringValue );
            }
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: Expected ARVx_... where x is one of I, D or S. Got '%c'\n",
                        driverName, functionName, drvInfo[4]);
            return asynError;
        }
        g_hash_table_insert(this->featureLookup, (gpointer) &(this->features[featureIndex]), (gpointer) feature);
        featureIndex++;
    }

    // Now return baseclass result
    return ADDriver::drvUserCreate(pasynUser, drvInfo, pptypeName, psize);
}

asynStatus aravisCamera::makeCameraObject() {
    const char *functionName = "makeCameraObject";
    /* remove old camera if it exists */
    if (this->camera != NULL) {
        g_object_unref(this->camera);
        this->camera = NULL;
    }
    /* remove ref to device and genicam */
    this->device = NULL;
    this->genicam = NULL;

    /* connect to camera */
    printf ("aravisCamera: Looking for camera '%s'... \n", this->cameraName);
    this->camera = arv_camera_new (this->cameraName);
    if (this->camera == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: No camera found\n",
                    driverName, functionName);
        return asynError;
    }
    /* Store device */
    this->device = arv_camera_get_device(this->camera);
    if (this->device == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: No device associated with camera\n",
                    driverName, functionName);
        return asynError;
    }
    // Make standard size packets
    arv_gv_device_set_packet_size(ARV_GV_DEVICE(this->device), ARV_GV_DEVICE_GVSP_PACKET_SIZE_DEFAULT);
    // Uncomment this line to set jumbo packets
//    arv_gv_device_set_packet_size(ARV_GV_DEVICE(this->device), 9000);
    /* Store genicam */
    this->genicam = arv_device_get_genicam (this->device);
    if (this->genicam == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: No genicam element associated with camera\n",
                    driverName, functionName);
        return asynError;
    }
    return asynSuccess;
}

asynStatus aravisCamera::makeStreamObject() {
    const char *functionName = "makeStreamObject";    
    asynStatus status = asynSuccess;
    
    /* remove old stream if it exists */
    if (this->stream != NULL) {
        g_object_unref(this->stream);
        this->stream = NULL;
    }
    this->stream = arv_camera_create_stream (this->camera, NULL, NULL);
    if (this->stream == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Making stream failed, retrying in 5s...\n",
                    driverName, functionName);
        epicsThreadSleep(5);
        /* make the camera object */
        status = this->makeCameraObject();
        if (status != asynSuccess) return (asynStatus) status;
        /* Make the stream */
        this->stream = arv_camera_create_stream (this->camera, NULL, NULL);
    }
    if (this->stream == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Making stream failed\n",
                    driverName, functionName);
        return asynError;
    }
    /* configure the stream */
    // Available stream options:
    //  socket-buffer:      ARV_GV_STREAM_SOCKET_BUFFER_FIXED, ARV_GV_STREAM_SOCKET_BUFFER_AUTO, defaults to auto which follows arvgvbuffer size
    //  socket-buffer-size: 64 bit int, Defaults to -1
    //  packet-resend:      ARV_GV_STREAM_PACKET_RESEND_NEVER, ARV_GV_STREAM_PACKET_RESEND_ALWAYS, defaults to always
    //  packet-timeout:     64 bit int, units us, ARV_GV_STREAM default 40000
    //  frame-retention:    64 bit int, units us, ARV_GV_STREAM default 200000

    epicsInt32      FrameRetention, PktResend, PktTimeout;
    getIntegerParam(AravisFrameRetention,  &FrameRetention);
    getIntegerParam(AravisPktResend,       &PktResend);
    getIntegerParam(AravisPktTimeout,      &PktTimeout);
    g_object_set (ARV_GV_STREAM (this->stream),
              "packet-resend",      (guint64) PktResend,
              "packet-timeout",     (guint64) PktTimeout,
              "frame-retention",    (guint64) FrameRetention,
              NULL);

    // Enable callback on new buffers
    arv_stream_set_emit_signals (this->stream, TRUE);
    g_signal_connect (this->stream, "new-buffer", G_CALLBACK (newBufferCallback), this);
    return asynSuccess;
}


asynStatus aravisCamera::connectToCamera() {
    const char *functionName = "connectToCamera";
    int status = asynSuccess;
    int w, h;
    const char *vendor, *model;

    /* stop old camera if it exists */
    this->connectionValid = 0;
    if (this->camera != NULL) {
        arv_camera_stop_acquisition(this->camera);
    }

    /* Tell areaDetector it is no longer acquiring */
    setIntegerParam(ADAcquire, 0);

    /* make the camera object */
    status = this->makeCameraObject();
    if (status) return (asynStatus) status;

    /* Make sure it's stopped */
    arv_camera_stop_acquisition(this->camera);
    status |= setIntegerParam(ADStatus, ADStatusIdle);
    
    /* Check the tick frequency */
    guint64 freq = arv_gv_device_get_timestamp_tick_frequency(ARV_GV_DEVICE(this->device));
    printf("aravisCamera: Your tick frequency is %" G_GUINT64_FORMAT "\n", freq);
    if (freq > 0) {
        printf("So your timestamp resolution is %f ns\n", 1.e9/freq);
    } else {
        printf("So your camera doesn't provide timestamps. Using system clock instead\n");
    }
    
    /* Make the stream */
    status = this->makeStreamObject();
    if (status) return (asynStatus) status;    
    
    /* connect connection lost signal to camera */
    g_signal_connect (this->device, "control-lost", G_CALLBACK (controlLostCallback), this);

    /* Set vendor and model number */
    vendor = arv_camera_get_vendor_name(this->camera);
    if (vendor) status |= setStringParam (ADManufacturer, vendor);
    model = arv_camera_get_model_name(this->camera);
    if (model) status |= setStringParam (ADModel, model);

    /* Get sensor size */
    arv_camera_get_sensor_size(this->camera, &w, &h);
    status |= setIntegerParam(ADMaxSizeX, w);
    status |= setIntegerParam(ADMaxSizeY, h);

    /* Get geometry */
    if(this->getGeometry()) {
        /* If getting geometry failed, set some safe defaults */
        setIntegerParam(ADBinX, 1);
        setIntegerParam(ADBinY, 1);
        setIntegerParam(NDColorMode, NDColorModeMono);
        setIntegerParam(NDDataType, NDUInt8);
        this->setGeometry();
    }

    /* Report if anything has failed */
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Unable to get all camera parameters\n",
                    driverName, functionName);
    }

    /* For Baumer cameras, AcquisitionFrameRate will not do anything until we set this
     * NOTE: *no* d in AcquisitionFrameRateEnable */
    if (this->hasFeature("AcquisitionFrameRateEnable")) {
        arv_device_set_integer_feature_value (this->device, "AcquisitionFrameRateEnable", 1);
    }

    /* For Point Grey cameras, AcquisitionFrameRate will not do anything until we set this
     * NOTE: there is a d in AcquisitionFrameRateEnabled */
    if (this->hasFeature("AcquisitionFrameRateEnabled")) {
        arv_device_set_integer_feature_value (this->device, "AcquisitionFrameRateEnabled", 1);
    }
    /* Mark connection valid again */
    this->connectionValid = 1;
    printf("aravisCamera: Done.\n");

    printf("aravisCamera: Getting feature list...\n");
    /* Add gain lookup */
    if (tryAddFeature(&ADGain, "Gain"))
        if (tryAddFeature(&ADGain, "GainRaw"))
            tryAddFeature(&ADGain, "GainRawChannelA");

    /* Add exposure lookup */
    if (tryAddFeature(&ADAcquireTime, "ExposureTime"))
        tryAddFeature(&ADAcquireTime, "ExposureTimeAbs");

    /* Add framerate lookup */
    if (tryAddFeature(&ADAcquirePeriod, "AcquisitionFrameRate"))
        tryAddFeature(&ADAcquirePeriod, "AcquisitionFrameRateAbs");

    /* Get all values in the hash table, note that this won't do anything that comes from db */
    if (this->getAllFeatures()) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Unable to get all camera features\n",
                    driverName, functionName);
        status = asynError;
    }

    printf("aravisCamera: Done.\n");
    return (asynStatus) status;
}

/** Called when asyn clients call pasynInt32->write().
  * This function performs actions for some parameters, including ADAcquire, ADColorMode, etc.
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus aravisCamera::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    epicsInt32 rbv;
    char *featureName;
    ArvGcNode *feature;
    const char  *   reasonName = "unknownReason";
    getParamName( 0, function, &reasonName );

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    getIntegerParam(function, &rbv);
    status = setIntegerParam(function, value);

    /* If we have no camera, then just fail */
    if (function == AravisReset) {
        status = this->connectToCamera();
    } else if (this->camera == NULL || this->connectionValid != 1) {
        if (rbv != value)
            setIntegerParam(ADStatus, ADStatusDisconnected);
        status = asynError;
    } else if (function == AravisConnection) {
        if (this->connectionValid != 1) status = asynError;
    } else if (function == AravisLeftShift) {
        if (value < 0 || value > 1) {
            setIntegerParam(function, rbv);
            status = asynError;
        }
    } else if (function == ADAcquire) {
        if (value) {
            /* This was a command to start acquisition */
            status = this->start();
        } else {
            /* This was a command to stop acquisition */
            status = this->stop();
        }
    } else if (function == ADBinX || function == ADBinY ||
            function == ADMinX || function == ADMinY || function == ADSizeX || function == ADSizeY ||
            function == NDDataType || function == NDColorMode) {
        status = this->setGeometry();
    } else if (function == ADReverseX || function == ADReverseY || function == ADFrameType) {
        /* not supported yet */
        if (value) status = asynError;
    } else if (function == ADNumExposures) {
        /* only one at the moment */
        if (value!=1) {
            setIntegerParam(ADNumExposures, 1);
            status = asynError;
        }
    } else if (function == AravisGetFeatures || function == AravisFrameRetention
            || function == AravisPktResend   || function == AravisPktTimeout 
            || function == AravisHWImageMode) {
        /* just write the value for these as they get fetched via getIntegerParam when needed */
    } else if (function < FIRST_ARAVIS_CAMERA_PARAM) {
        /* If this parameter belongs to a base class call its method */
        status = ADDriver::writeInt32(pasynUser, value);
    /* generic feature lookup */
    } else if (g_hash_table_lookup_extended(this->featureLookup, &function, NULL, NULL)) {
        featureName = (char *) g_hash_table_lookup(this->featureLookup, &function);
        feature = arv_device_get_feature(this->device, featureName);
        if (ARV_IS_GC_COMMAND(feature)) {
            arv_gc_command_execute(ARV_GC_COMMAND(feature), NULL);
        } else {
            status = this->setIntegerValue(featureName, value, &rbv);
            if (status) setIntegerParam(function, rbv);
        }
    } else {
           status = asynError;
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

    if (function != AravisConnection)
    {
        /* Report any errors */
        if (status)
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s:writeInt32 error, status=%d function=%d %s, value=%d\n",
                  driverName, status, function, reasonName, value);
        else
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s:writeInt32: function=%d %s, value=%d\n",
                  driverName, function, reasonName, value);
    }
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
    asynStatus status = asynSuccess;
    char *featureName;
    ArvGcNode *feature;
    const char  *   reasonName = "unknownReason";
    getParamName( 0, function, &reasonName );

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    getDoubleParam(function, &rbv);
    status = setDoubleParam(function, value);

    /* If we have no camera, then just fail */
    if (this->camera == NULL || this->connectionValid != 1) {
        status = asynError;
    /* Gain */
    } else if (function == ADGain) {
        featureName = (char *) g_hash_table_lookup(this->featureLookup, &function);
        if (featureName == NULL) {
            status = asynError;
        } else {
            feature = arv_device_get_feature(this->device, featureName);
            if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_DOUBLE) {
                status = this->setFloatValue(featureName, value, &rbv);
            } else {
                epicsInt32 i_rbv, i_value = (epicsInt32) value;
                status = this->setIntegerValue(featureName, i_value, &i_rbv);
                if (strcmp("GainRawChannelA", featureName) == 0) this->setIntegerValue("GainRawChannelB", i_value, NULL);
                rbv = i_rbv;
            }
            if (status) setDoubleParam(function, rbv);
        }
    /* Acquire time / exposure */
    } else if (function == ADAcquireTime) {
        featureName = (char *) g_hash_table_lookup(this->featureLookup, &function);
        if (featureName == NULL) {
            status = asynError;
        } else {
          feature = arv_device_get_feature(this->device, featureName);
          if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_DOUBLE) {
            status = this->setFloatValue(featureName, value * 1000000, &rbv);
          } else {
            epicsInt32 i_rbv, i_value = (epicsInt32) (value * 1000000);
            status = this->setIntegerValue(featureName, i_value, &i_rbv);
            rbv = i_rbv;
          }
          if (status) setDoubleParam(function, rbv / 1000000);
        }
    /* Acquire period / framerate */
    } else if (function == ADAcquirePeriod) {
        featureName = (char *) g_hash_table_lookup(this->featureLookup, &function);
        if (value <= 0.0) value = 0.1;
        feature = arv_device_get_feature(this->device, featureName);
        if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_DOUBLE) {
          status = this->setFloatValue(featureName, 1/value, &rbv);
        } else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_INT64) {
          epicsInt32 i_rbv, i_value = (epicsInt32) (1/value);
          status = this->setIntegerValue(featureName, i_value, &i_rbv);
          rbv = (epicsFloat64)i_rbv;
          if (rbv <= 0.0)
            rbv = 0.1;
        } else {
          status = asynError;
        }
        if (status) setDoubleParam(function, 1/rbv);
    /* generic feature lookup */
    } else if (g_hash_table_lookup_extended(this->featureLookup, &function, NULL, NULL)) {
        featureName = (char *) g_hash_table_lookup(this->featureLookup, &function);
        status = this->setFloatValue(featureName, value, &rbv);
        if (status) setDoubleParam(function, rbv);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_ARAVIS_CAMERA_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
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

/** Report status of the driver.
  * Prints details about the driver if details>0.
  * It then calls the ADDriver::report() method.
  * \param[in] fp File pointed passed by caller where the output is written to.
  * \param[in] details If >0 then driver details are printed.
  */
void aravisCamera::report(FILE *fp, int details)
{

    fprintf(fp, "Aravis GigE detector %s\n", this->portName);
    if (details > 0) {
        int nx, ny, dataType;
        getIntegerParam(ADSizeX, &nx);
        getIntegerParam(ADSizeY, &ny);
        getIntegerParam(NDDataType, &dataType);
        fprintf(fp, "  NX, NY:            %d  %d\n", nx, ny);
        fprintf(fp, "  Data type:         %d\n", dataType);
    }
    /* Invoke the base class method */
    ADDriver::report(fp, details);
}


/** Allocate an NDArray and prepare a buffer that is passed to the stream
    this->camera exists, lock taken */
asynStatus aravisCamera::allocBuffer() {
    const char *functionName = "allocBuffer";
    ArvBuffer *buffer;
    NDArray *pRaw;
    size_t bufferDims[2] = {1,1};

    /* check stream exists */
    if (this->stream == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Cannot allocate buffer on a NULL stream\n",
                    driverName, functionName);
        return asynError;
    }

    pRaw = this->pNDArrayPool->alloc(2, bufferDims, NDInt8, this->payload, NULL);
    if (pRaw==NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating raw buffer\n",
                    driverName, functionName);
        return asynError;
    }

    buffer = arv_buffer_new_full(this->payload, pRaw->pData, (void *)pRaw, destroyBuffer);
    arv_stream_push_buffer (this->stream, buffer);
    return asynSuccess;
}

/** Check what event we have, and deal with new frames.
    this->camera exists, lock not taken */
void aravisCamera::run() {
    epicsTimeStamp lastFeatureGet, now;
    int getFeatures, numImagesCounter, imageMode, numImages, acquire;
    const char *functionName = "run";
    ArvBuffer *buffer;

    /* Wait for database to be up */
    while (!iocRunning) {
        epicsThreadSleep(0.1);
    }

    /* Loop forever */
    epicsTimeGetCurrent(&lastFeatureGet);
    while (1) {
        /* Wait 5ms for an array to arrive from the queue */
        if (epicsMessageQueueReceiveWithTimeout(this->msgQId, &buffer, sizeof(&buffer), 0.005) == -1) {
            /* No array, so if there is a camera, get the next feature*/
            if (this->camera != NULL && this->connectionValid == 1) {
                /* We only want to get a feature once every 25ms (max 40 features/s)
                 * so compare timestamp against last feature get
                 */
                epicsTimeGetCurrent(&now);
                if (epicsTimeDiffInSeconds(&now, &lastFeatureGet) > 0.025) {
                    this->lock();
                    getIntegerParam(AravisGetFeatures, &getFeatures);
                    if (getFeatures) {
                        this->getNextFeature();
                        callParamCallbacks();
                    }
                    this->unlock();
                }
            }
        } else {
            /* Got a buffer, so lock up and process it */
            this->lock();
            getIntegerParam(ADAcquire, &acquire);
            if (acquire) {
                this->processBuffer(buffer);
                /* free memory */
                g_object_unref(buffer);
                /* See if acquisition is done */
                getIntegerParam(ADNumImages, &numImages);
                getIntegerParam(ADNumImagesCounter, &numImagesCounter);
                getIntegerParam(ADImageMode, &imageMode);
                if ((imageMode == ADImageSingle) ||
                    ((imageMode == ADImageMultiple) &&
                     (numImagesCounter >= numImages))) {
                    this->stop();
                    // Want to make sure we're idle before we callback on ADAcquire
                    callParamCallbacks();
                    setIntegerParam(ADAcquire, 0);
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                          "%s:%s: acquisition completed\n", driverName, functionName);
                } else {
                    /* Allocate the new raw buffer we use to compute images. */
                    this->allocBuffer();
                }
            } else {
                // We recieved a buffer that we didn't request
                g_object_unref(buffer);
            }
            this->unlock();
        }
    }
}

asynStatus aravisCamera::processBuffer(ArvBuffer *buffer) {
    int arrayCallbacks, imageCounter, numImages, numImagesCounter, imageMode;
    int colorMode, dataType, bayerFormat;
    size_t expected_size;
    int xDim=0, yDim=1, binX, binY, left_shift;
    double acquirePeriod;
    const char *functionName = "processBuffer";
    guint64 n_completed_buffers, n_failures, n_underruns;
    NDArray *pRaw;

    /* Get the current parameters */
    getIntegerParam(NDArrayCounter, &imageCounter);
    getIntegerParam(ADNumImages, &numImages);
    getIntegerParam(ADNumImagesCounter, &numImagesCounter);
    getIntegerParam(ADImageMode, &imageMode);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    getDoubleParam(ADAcquirePeriod, &acquirePeriod);
    getIntegerParam(AravisLeftShift, &left_shift); 
    /* The buffer structure does not contain the binning, get that from param lib,
     * but it could be wrong for this frame if recently changed */
    getIntegerParam(ADBinX, &binX);
    getIntegerParam(ADBinY, &binY);
    /* Report a new frame with the counters */
    imageCounter++;
    numImagesCounter++;
    setIntegerParam(NDArrayCounter, imageCounter);
    setIntegerParam(ADNumImagesCounter, numImagesCounter);
    if (imageMode == ADImageMultiple) {
        setDoubleParam(ADTimeRemaining, (numImages - numImagesCounter) * acquirePeriod);
    }
    /* find the buffer */

    pRaw = (NDArray *) arv_buffer_get_user_data(buffer);
    if (pRaw == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: where did this buffer come from?\n",
                driverName, functionName);
        return asynError;
    }
//            printf("callb buffer: %p, pRaw[%d]: %p, pData %p\n", buffer, i, pRaw, pRaw->pData);
    /* Put the frame number and time stamp into the buffer */
    pRaw->uniqueId = imageCounter;
    pRaw->timeStamp = arv_buffer_get_timestamp(buffer) / 1.e9;

    /* Update the areaDetector timeStamp */
    updateTimeStamp(&pRaw->epicsTS);

    /* Get any attributes that have been defined for this driver */
    this->getAttributes(pRaw->pAttributeList);

    /* Annotate it with its dimensions */
    int pixel_format = arv_buffer_get_image_pixel_format(buffer);
    if (this->lookupColorMode(pixel_format, &colorMode, &dataType, &bayerFormat) != asynSuccess) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: unknown pixel format %d\n",
                    driverName, functionName, pixel_format);
        return asynError;
    }
    pRaw->pAttributeList->add("BayerPattern", "Bayer Pattern", NDAttrInt32, &bayerFormat);
    pRaw->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);
    pRaw->dataType = (NDDataType_t) dataType;
    int width = arv_buffer_get_image_width(buffer);
    int height = arv_buffer_get_image_height(buffer);
    int x_offset = arv_buffer_get_image_x(buffer);
    int y_offset = arv_buffer_get_image_y(buffer);
    size_t size = 0;
    arv_buffer_get_data(buffer, &size);
    switch (colorMode) {
        case NDColorModeMono:
        case NDColorModeBayer:
            xDim = 0;
            yDim = 1;
            pRaw->ndims = 2;
            expected_size = width * height;
            break;
        case NDColorModeRGB1:
            xDim = 1;
            yDim = 2;
            pRaw->ndims = 3;
            pRaw->dims[0].size    = 3;
            pRaw->dims[0].offset  = 0;
            pRaw->dims[0].binning = 1;
            expected_size = width * height * 3;
            break;
        default:
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: unknown colorMode %d\n",
                        driverName, functionName, colorMode);
            return asynError;
    }
    pRaw->dims[xDim].size    = width;
    pRaw->dims[xDim].offset  = x_offset;
    pRaw->dims[xDim].binning = binX;
    pRaw->dims[yDim].size    = height;
    pRaw->dims[yDim].offset  = y_offset;
    pRaw->dims[yDim].binning = binY;

    /* If we are 16 bit, shift by the correct amount */
    if (pRaw->dataType == NDUInt16) {
        expected_size *= 2;
        if (left_shift) {
            int shift = 0;
            switch (pixel_format) {
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
                uint16_t *array = (uint16_t *) pRaw->pData;
                for (unsigned int ib = 0; ib < size / 2; ib++) {
                    array[ib] = array[ib] << shift;
                }
            }
        }
    }

    if (expected_size != size) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: w: %d, h: %d, size: %zu, expected_size: %zu\n",
                    driverName, functionName, width, height, size, expected_size);
        return asynError;
    }
/*
    for (int ib = 0; ib<10; ib++) {
        unsigned char *ix = ((unsigned char *)pRaw->pData) + ib;
        printf("%d,", (int) (*ix));
    }
    printf("\n");
*/
    /* this is a good image, so callback on it */
    if (arrayCallbacks) {
        /* Call the NDArray callback */
        /* Must release the lock here, or we can get into a deadlock, because we can
         * block on the plugin lock, and the plugin can be calling us */
        this->unlock();
        asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
             "%s:%s: calling imageData callback\n", driverName, functionName);
        doCallbacksGenericPointer(pRaw, NDArrayData, 0);
        this->lock();
    }

    /* Report statistics */
    if (this->stream != NULL) {
        arv_stream_get_statistics(this->stream, &n_completed_buffers, &n_failures, &n_underruns);
        setDoubleParam(AravisCompleted, (double) n_completed_buffers);
        setDoubleParam(AravisFailures, (double) n_failures);
        setDoubleParam(AravisUnderruns, (double) n_underruns);

        guint64 n_resent_pkts, n_missing_pkts;
        arv_gv_stream_get_statistics(ARV_GV_STREAM(this->stream), &n_resent_pkts, &n_missing_pkts);
        setIntegerParam(AravisResentPkts,  (epicsInt32) n_resent_pkts);
        setIntegerParam(AravisMissingPkts, (epicsInt32) n_missing_pkts);
    }

    /* Call the callbacks to update any changes */
    callParamCallbacks();
    return asynSuccess;
}

asynStatus aravisCamera::stop() {
    /* Stop the camera */
    arv_camera_stop_acquisition(this->camera);
    setIntegerParam(ADStatus, ADStatusIdle);
    /* Tear down the old stream and make a new one */
    return this->makeStreamObject();
}

asynStatus aravisCamera::start() {
    int imageMode, numImages, hwImageMode;
    const char *functionName = "start";
    
    getIntegerParam(AravisHWImageMode, &hwImageMode);
    getIntegerParam(ADImageMode, &imageMode);

    if (hwImageMode and imageMode == ADImageSingle) {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_SINGLE_FRAME);
    } else if (hwImageMode and imageMode == ADImageMultiple and hasFeature("AcquisitionFrameCount")) {
        getIntegerParam(ADNumImages, &numImages);
        arv_device_set_string_feature_value(this->device, "AcquisitionMode", "MultiFrame");
        arv_device_set_integer_feature_value(this->device, "AcquisitionFrameCount", numImages);
    } else {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_CONTINUOUS);
    }
    setIntegerParam(ADNumImagesCounter, 0);
    setIntegerParam(ADStatus, ADStatusAcquire);

    /* fill the queue */
    this->payload = arv_camera_get_payload(this->camera);
    for (int i=0; i<NRAW; i++) {
        if (this->allocBuffer() != asynSuccess) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: allocBuffer returned error\n",
                        driverName, functionName);
            return asynError;
        }
    }

    // Start the camera acquiring
    arv_camera_start_acquisition (this->camera);
    return asynSuccess;
}

asynStatus aravisCamera::getBinning(int *binx, int *biny) {
    const char *functionName = "getBinning";
    if (this->hasFeature("BinningMode")) {
        /* lookup the enum */
        const int N = sizeof(bin_lookup) / sizeof(struct bin_lookup);
        const char *mode = arv_device_get_string_feature_value (this->device, "BinningMode");
        for (int i = 0; i < N; i ++)
            if (strcmp(bin_lookup[i].mode, mode) == 0) {
                *binx   = bin_lookup[i].binx;
                *biny   = bin_lookup[i].biny;
                return asynSuccess;
            }
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Could not find a match for binning mode '%s''\n",
                    driverName, functionName, mode);
        return asynError;
    } else {
        *binx = arv_device_get_integer_feature_value (this->device, "BinningHorizontal");
        *biny = arv_device_get_integer_feature_value (this->device, "BinningVertical");
        if (*binx < 1) *binx = 1;
        if (*biny < 1) *biny = 1;
        return asynSuccess;
    }
}

asynStatus aravisCamera::setBinning(int binx, int biny) {
    const char *functionName = "getBinning";
    if (this->hasFeature("BinningMode")) {
        /* lookup the enum */
        const int N = sizeof(bin_lookup) / sizeof(struct bin_lookup);
        for (int i = 0; i < N; i ++)
            if (binx == bin_lookup[i].binx &&
                biny == bin_lookup[i].biny) {
                arv_device_set_string_feature_value (this->device, "BinningMode", bin_lookup[i].mode);
                return asynSuccess;
            }
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Could not find a match for binning mode binx: %d, biny: %d\n",
                    driverName, functionName, binx, biny);
        return asynError;
    } else {
        arv_device_set_integer_feature_value (this->device, "BinningHorizontal", binx);
        arv_device_set_integer_feature_value (this->device, "BinningVertical", biny);
        return asynSuccess;
    }
}

/** Read camera geometry
    this->camera exists, lock taken */
asynStatus aravisCamera::getGeometry() {
    asynStatus status = asynSuccess;
    int binx, biny, x, y, w, h, colorMode, dataType, bayerFormat, bps=1;
    ArvPixelFormat fmt;

    /* check binning */
    if (this->getBinning(&binx, &biny)) {
        status = asynError;
    } else {
        setIntegerParam(ADBinX, binx);
        setIntegerParam(ADBinY, biny);
    }

    /* check pixel format */
    fmt = arv_camera_get_pixel_format(this->camera);
    if (this->lookupColorMode(fmt, &colorMode, &dataType, &bayerFormat)) {
        status = asynError;
    } else {
        setIntegerParam(NDColorMode, colorMode);
        setIntegerParam(NDDataType, dataType);
    }

    /* check roi */
    arv_camera_get_region(this->camera, &x, &y, &w, &h);
    setIntegerParam(ADMinX, x);
    setIntegerParam(ADMinY, y);
    setIntegerParam(ADSizeX, w);
    setIntegerParam(ADSizeY, h);

    /* Set sizes */
    if (colorMode == NDUInt16) bps = 2;
    if (dataType == NDColorModeRGB1) bps *= 3;
    setIntegerParam(NDArraySize, w*h*bps);
    setIntegerParam(NDArraySizeX, w);
    setIntegerParam(NDArraySizeY, h);

    /* return */
    return status;
}



/** Change camera geometry
    this->camera exists, lock taken */
asynStatus aravisCamera::setGeometry() {
    asynStatus status = asynSuccess;
    int acquiring, bayerFormat=0;
    int binx, biny, x, y, w, h, colorMode, dataType;
    int binx_rbv, biny_rbv, x_rbv, y_rbv, w_rbv, h_rbv, colorMode_rbv, dataType_rbv;
    ArvPixelFormat fmt;

    /* Get the demands */
    getIntegerParam(ADBinX, &binx);
    getIntegerParam(ADBinY, &biny);
    getIntegerParam(ADMinX, &x);
    getIntegerParam(ADMinY, &y);
    getIntegerParam(ADSizeX, &w);
    getIntegerParam(ADSizeY, &h);
    getIntegerParam(NDColorMode, &colorMode);
    getIntegerParam(NDDataType, &dataType);

    /* stop acquiring if we are acquiring */
    getIntegerParam(ADAcquire, &acquiring);
    if (acquiring) this->stop();

    /* Lookup the pix format, fail if not supported */
    if (this->lookupPixelFormat(colorMode, dataType, bayerFormat, &fmt)) {
        status = asynError;
    } else {
        //printf("Set pixel format %x\n", fmt);
        arv_camera_set_pixel_format(this->camera, fmt);
        //printf("Get pixel format %x\n", arv_camera_get_pixel_format(this->camera));
        status = asynSuccess;
    }

    /* Write binning and region information */
    if (status == 0) {
        // Avoid devide-by-zero fault
        if (binx <= 0) {
            binx = 1; setIntegerParam(ADBinX, 1);
        }
        if (biny <= 0) {
            biny = 1; setIntegerParam(ADBinY, 1);
        }
        this->setBinning(binx, biny);
        arv_camera_set_region(this->camera, x, y, w, h);
    }

    /* Read back values */
    if (this->getGeometry()) {
        status = asynError;
    }

    /* Check values match */
    getIntegerParam(ADBinX, &binx_rbv);
    getIntegerParam(ADBinY, &biny_rbv);
    getIntegerParam(ADMinX, &x_rbv);
    getIntegerParam(ADMinY, &y_rbv);
    getIntegerParam(ADSizeX, &w_rbv);
    getIntegerParam(ADSizeY, &h_rbv);
    getIntegerParam(NDColorMode, &colorMode_rbv);
    getIntegerParam(NDDataType, &dataType_rbv);
    if (binx != binx_rbv || biny != biny_rbv ||
            x != x_rbv || y != y_rbv || w != w_rbv || h != h_rbv ||
            colorMode != colorMode_rbv || dataType != dataType_rbv) {
        status = asynError;
    }

    /* Start camera again */
    if (acquiring) this->start();
    return status;
}


/** Lookup a colorMode, dataType and bayerFormat from an ArvPixelFormat */
asynStatus aravisCamera::lookupColorMode(ArvPixelFormat fmt, int *colorMode, int *dataType, int *bayerFormat) {
    const char *functionName = "lookupColorMode";
    const int N = sizeof(pix_lookup) / sizeof(struct pix_lookup);
    for (int i = 0; i < N; i ++)
        if (pix_lookup[i].fmt == fmt) {
            *colorMode   = pix_lookup[i].colorMode;
            *dataType    = pix_lookup[i].dataType;
            *bayerFormat = pix_lookup[i].bayerFormat;
            return asynSuccess;
        }
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: Could not find a match for pixel format: %d\n",
                driverName, functionName, fmt);
    return asynError;
}

/** Lookup an ArvPixelFormat from a colorMode, dataType and bayerFormat */
asynStatus aravisCamera::lookupPixelFormat(int colorMode, int dataType, int bayerFormat, ArvPixelFormat *fmt) {
    const char *functionName = "lookupPixelFormat";
    const int N = sizeof(pix_lookup) / sizeof(struct pix_lookup);
    ArvGcNode *node = arv_gc_get_node(genicam, "PixelFormat");
    for (int i = 0; i < N; i ++)
        if (colorMode   == pix_lookup[i].colorMode &&
            dataType    == pix_lookup[i].dataType &&
            bayerFormat == pix_lookup[i].bayerFormat) {
            if (ARV_IS_GC_ENUMERATION (node)) {
                /* Check if the pixel format is supported by the camera */
                ArvGcEnumeration *enumeration = (ARV_GC_ENUMERATION (node));
                const GSList *iter;
                for (iter = arv_gc_enumeration_get_entries (enumeration); iter != NULL; iter = iter->next) {
                    if (arv_gc_feature_node_is_available(ARV_GC_FEATURE_NODE(iter->data), NULL) &&
                            arv_gc_enum_entry_get_value(ARV_GC_ENUM_ENTRY(iter->data), NULL) == pix_lookup[i].fmt) {
                        *fmt = pix_lookup[i].fmt;
                        return asynSuccess;
                    }
                }
            } else {
                /* No PixelFormat node to check against */
                *fmt = pix_lookup[i].fmt;
                return asynSuccess;
            }
        }
    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: Could not find a match for colorMode: %d, dataType: %d, bayerFormat: %d\n",
                driverName, functionName, colorMode, dataType, bayerFormat);
    return asynError;
}

int aravisCamera::hasEnumString(const char* feature, const char *value) {
    ArvGcNode *node = arv_gc_get_node(this->genicam, feature);
    if (ARV_IS_GC_ENUMERATION (node)) {
        ArvGcEnumeration *enumeration = (ARV_GC_ENUMERATION (node));
        const GSList *iter;
        for (iter = arv_gc_enumeration_get_entries (enumeration); iter != NULL; iter = iter->next) {
            if (strcmp(arv_gc_feature_node_get_name (ARV_GC_FEATURE_NODE(iter->data)), value) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

gboolean aravisCamera::hasFeature(const char *feature) {
    return arv_gc_get_node(this->genicam, feature) != NULL;
}

asynStatus aravisCamera::setIntegerValue(const char *feature, epicsInt32 value, epicsInt32 *rbv) {
    const char *functionName = "setIntegerValue";
    if (feature == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: Cannot set integer value of a NULL feature\n",
                    driverName, functionName);
        return asynError;
    }
    asynPrint(  this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s:%s: arv_device_set_integer_feature_value %s value %d\n",
                driverName, functionName, feature, value );
    arv_device_set_integer_feature_value (this->device, feature, value);
    if (rbv != NULL) {
        *rbv = arv_device_get_integer_feature_value (this->device, feature);
        if (value != *rbv) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: value %d != rbv %d\n",
                        driverName, functionName, value, *rbv);
            return asynError;
        }
    }
    return asynSuccess;
}

asynStatus aravisCamera::setFloatValue(const char *feature, epicsFloat64 value, epicsFloat64 *rbv) {
    const char *functionName = "setFloatValue";
    if (feature == NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: Cannot set float value of a NULL feature\n",
            driverName, functionName);
        return asynError;
    }

    asynPrint(  this->pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s:%s: arv_device_set_float_feature_value %s value %f\n",
            driverName, functionName, feature, value );
    arv_device_set_float_feature_value (this->device, feature, value);
    if (rbv != NULL) {
        *rbv = arv_device_get_float_feature_value (this->device, feature);
        if (fabs(value - *rbv) > 0.001) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: feature %s value %f != rbv %f\n",
                    driverName, functionName, feature, value, *rbv);
            return asynError;
        }
    }
    return asynSuccess;
}

/* Get all features, called with lock taken */
asynStatus aravisCamera::getAllFeatures() {
    int status = asynSuccess;
    /* ensure we go back to the beginning */
    if (this->featureKeys != NULL) {
        g_list_free(this->featureKeys);
        this->featureKeys = NULL;
    }
    /* Get the first features */
    status |= this->getNextFeature();
    /* Get the rest of the features */
    while (this->featureKeys != NULL) status |= this->getNextFeature();
    return (asynStatus) status;
}

asynStatus aravisCamera::getNextFeature() {
    const char *functionName = "getNextFeature";
    int status = asynSuccess;
    const char *featureName;
    ArvGcNode *node;
    int *index;
    epicsFloat64 floatValue;
    epicsInt32 integerValue;
    const char *stringValue;
    guint64 n_completed_buffers, n_failures, n_underruns;

    /* Get geometry on first run */
    if (this->featureKeys == NULL) {
        this->featureKeys = g_hash_table_get_keys(this->featureLookup);
        this->featureIndex = 0;
        return (asynStatus) this->getGeometry();
    }

    /* Then iterate through keys */
    if (this->featureIndex < g_list_length(this->featureKeys)) {
        index = (int *) g_list_nth_data(this->featureKeys, this->featureIndex);
        featureName = (const char *) g_hash_table_lookup(this->featureLookup, index);
        node = arv_device_get_feature(this->device, featureName);
        //printf("Get %p %s %d\n", node, featureName, *index);
        if (node == NULL) {
            status = asynError;
        } else if (ARV_IS_GC_ENUMERATION(node)) {
            integerValue = arv_device_get_integer_feature_value (this->device, featureName);
            status |= setIntegerParam(*index, integerValue);
        } else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(node)) == G_TYPE_DOUBLE) {
            floatValue = arv_device_get_float_feature_value (this->device, featureName);
            /* special cases for exposure and frame rate */
            if (*index == ADAcquireTime) floatValue /= 1000000;
            if (*index == ADAcquirePeriod && floatValue > 0) floatValue = 1/floatValue;
            status |= setDoubleParam(*index, floatValue);
        } else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(node)) == G_TYPE_STRING) {
            stringValue = arv_device_get_string_feature_value(this->device, featureName);
            if (stringValue == NULL) {
                printf("aravisCamera: Feature %s has NULL value\n", featureName);
                status = asynError;
            } else {
                status |= setStringParam(*index, stringValue);
            }
        //} else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(node)) == G_TYPE_INT64) {
        } else if (!ARV_IS_GC_COMMAND(node)) {
            integerValue = arv_device_get_integer_feature_value (this->device, featureName);
            if (*index == ADGain) {
                /* Gain is sometimes an integer */
                status |= setDoubleParam(*index, integerValue);
            } else if (*index == ADAcquireTime) {
                /* Exposure time is an integer for JAI CM series */
                status |= setDoubleParam(*index, integerValue / 1000000.0);
            } else if (*index == ADAcquirePeriod) {
                /* For JAI CM this is an enum. This should prevent an error 
                   message in that case, and also work correctly if this 
                   camera uses an integer FPS rate.*/
                floatValue = (epicsFloat64) integerValue; 
                if (floatValue > 0) 
                    floatValue = 1/floatValue;
                
                status |= setDoubleParam(*index, floatValue);
            } else {
                status |= setIntegerParam(*index, integerValue);
            }
        }
        this->featureIndex++;
        return (asynStatus) status;
    }

    /* On last tick report statistics */
    if (this->stream != NULL) {
        arv_stream_get_statistics(this->stream, &n_completed_buffers, &n_failures, &n_underruns);
        status |= setDoubleParam(AravisCompleted, (double) n_completed_buffers);
        status |= setDoubleParam(AravisFailures, (double) n_failures);
        status |= setDoubleParam(AravisUnderruns, (double) n_underruns);
        guint64 n_resent_pkts, n_missing_pkts;
        arv_gv_stream_get_statistics(ARV_GV_STREAM(this->stream), &n_resent_pkts, &n_missing_pkts);
        setIntegerParam(AravisResentPkts,  (epicsInt32) n_resent_pkts);
        setIntegerParam(AravisMissingPkts, (epicsInt32) n_missing_pkts);
    }

    /* ensure we go back to the beginning */
    g_list_free(this->featureKeys);
    this->featureKeys = NULL;
    return (asynStatus) status;
}

/* Define to add feature if available */
asynStatus aravisCamera::tryAddFeature(int *ADIdx, const char *featureString) {
    ArvGcNode *feature = arv_device_get_feature(this->device, featureString);
    if (feature != NULL && !ARV_IS_GC_CATEGORY(feature)) {
        g_hash_table_insert(this->featureLookup, (gpointer)(ADIdx), (gpointer)featureString);
        return asynSuccess;
    }
    return asynError;
}


/** Configuration command, called directly or from iocsh */
extern "C" int aravisCameraConfig(const char *portName, const char *cameraName,
                                 int maxBuffers, size_t maxMemory, int priority, int stackSize)
{
    if (stackSize <= 0)
        stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
    new aravisCamera(portName, cameraName, maxBuffers, maxMemory,
                     priority, stackSize);
    return(asynSuccess);
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


static void aravisCameraRegister(void)
{

    iocshRegister(&configAravisCamera, configAravisCameraCallFunc);
}

extern "C" {
    epicsExportRegistrar(aravisCameraRegister);
}
