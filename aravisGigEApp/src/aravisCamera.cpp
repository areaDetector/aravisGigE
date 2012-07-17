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

/* areaDetector includes */
#include <ADDriver.h>

/* aravis includes */
extern "C" {
	#include <arv.h>
}

/* number of raw buffers in our queue */
#define NRAW 10

/* maximum number of custom features that we support */
#define NFEATURES 1000

/* driver name for asyn trace prints */
static const char *driverName = "aravisCamera";

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

/** Aravis GigE detector driver */
class aravisCamera : public ADDriver {
public:
	/* Constructor */
    aravisCamera(const char *portName, const char *cameraName,
                int maxBuffers, size_t maxMemory,
                int priority, int stackSize);

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    void report(FILE *fp, int details);

    /* These should be private, but are for the aravis callback so must be public */
    void callback();
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
    int AravisLeftShift;
    int AravisConnection;
    int AravisGetFeatures;
    int AravisReset;
    #define LAST_ARAVIS_CAMERA_PARAM AravisReset
    int features[NFEATURES];
	#define NUM_ARAVIS_CAMERA_PARAMS (&LAST_ARAVIS_CAMERA_PARAM - &FIRST_ARAVIS_CAMERA_PARAM + 1 + NFEATURES)

private:
    asynStatus allocBuffer();
    void freeBufferAndUnlock(ArvBuffer *buffer);
    asynStatus start();
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
    int initialConnectDone;
	GList *featureKeys;
	unsigned int featureIndex;
};

/** Called by epicsAtExit to shutdown camera */
static void aravisShutdown(void* arg) {
    aravisCamera *pPvt = (aravisCamera *) arg;
    ArvCamera *cam = pPvt->camera;
    g_print("Stopping %s... ", pPvt->portName);
    arv_camera_stop_acquisition(cam);
    pPvt->connectionValid = 0;
    epicsThreadSleep(0.1);
    pPvt->camera = NULL;
    g_object_unref(cam);
    g_print("OK\n");
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
	buffer = arv_stream_timeout_pop_buffer(stream, 100000);
	if (buffer == NULL)	return;
	if (buffer->status == ARV_BUFFER_STATUS_SUCCESS) {
        status = epicsMessageQueueTrySend(pPvt->msgQId,
        		&buffer,
        		sizeof(&buffer));
        if (status) {
        	printf("Message queue full, dropped buffer\n");
    		arv_stream_push_buffer (stream, buffer);
        }
	} else {
        printf("Bad frame status: %d size: %d\n", buffer->status, buffer->size);
		arv_stream_push_buffer (stream, buffer);
	}
}
static void controlLostCallback(ArvDevice *device, aravisCamera *pPvt) {
	pPvt->connectionValid = 0;
}

/** Called by callback thread that sits listening to the epicsMessageQueue */
void callbackC(void *drvPvt) {
	aravisCamera *pPvt = (aravisCamera *)drvPvt;
    pPvt->callback();
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
      camera(NULL), connectionValid(0), stream(NULL), device(NULL), genicam(NULL), initialConnectDone(0), featureKeys(NULL)
{
    const char *functionName = "aravisCamera";

    /* glib initialisation */
    g_thread_init (NULL);
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
    createParam("ARAVIS_LEFTSHIFT",      asynParamInt32,   &AravisLeftShift);
    createParam("ARAVIS_CONNECTION",     asynParamInt32,   &AravisConnection);
    createParam("ARAVIS_GETFEATURES",    asynParamInt32,   &AravisGetFeatures);
    createParam("ARAVIS_RESET",          asynParamInt32,   &AravisReset);

    /* Set some initial values for other parameters */
    setIntegerParam(ADReverseX, 0);
    setIntegerParam(ADReverseY, 0);
    setIntegerParam(ADImageMode, ADImageContinuous);
    setIntegerParam(ADNumImages, 100);
    setDoubleParam(AravisCompleted, 0);
    setDoubleParam(AravisFailures, 0);
    setDoubleParam(AravisUnderruns, 0);
    setIntegerParam(AravisLeftShift, 1);
    setIntegerParam(AravisReset, 0);

    /* Connect to the camera */
    this->connectToCamera();

	/* Start the image grabbing thread */
    /* Create the thread that handles the NDArray callbacks */
    if (epicsThreadCreate("aravisGrab",
                          epicsThreadPriorityHigh,
                          stackSize,
                          callbackC,
                          this) == NULL) {
        printf("%s:%s: epicsThreadCreate failure\n", driverName, functionName);
        return;
    }

    /* Register the shutdown function for epicsAtExit */
    epicsAtExit(aravisShutdown, (void*)this);

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
	g_print ("Looking for camera '%s'... \n", this->cameraName);
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
	arv_gv_device_set_packet_size(ARV_GV_DEVICE(this->device), ARV_GV_DEVICE_GVSP_PACKET_SIZE_DEFAULT);
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

asynStatus aravisCamera::connectToCamera() {
	const char *functionName = "connectToCamera";
	int status = asynSuccess;
    int w, h;
    GList *keys, *values;
    const GSList *features, *iter;
    const char *vendor, *model;
    ArvGcNode *feature;
    char *featureName;

    /* stop old camera if it exists */
    this->connectionValid = 0;
    if (this->camera != NULL) {
    	arv_camera_stop_acquisition(this->camera);
    }

    /* Tell areaDetector it is no longer acquiring */
    setIntegerParam(ADAcquire, 0);

    /* remove old stream if it exists */
    if (this->stream != NULL) {
    	g_object_unref(this->stream);
    	this->stream = stream;
    }

    /* make the camera object */
    status = this->makeCameraObject();
    if (status) return (asynStatus) status;

    /* Make the stream */
	this->stream = arv_camera_create_stream (this->camera, NULL, NULL);
	if (this->stream == NULL) {
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
					"%s:%s: Making stream failed, retrying in 10s...\n",
					driverName, functionName);
		epicsThreadSleep(10);
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
    /* Make sure it's stopped */
    arv_camera_stop_acquisition(this->camera);
    status |= setIntegerParam(ADStatus, ADStatusIdle);    
	/* configure the stream */
	g_object_set (ARV_GV_STREAM (this->stream),
			  "packet-timeout", 50000, //50ms
			  "frame-retention", 200000, //200ms
			  NULL);
	arv_stream_set_emit_signals (this->stream, TRUE);
	g_signal_connect (this->stream, "new-buffer", G_CALLBACK (newBufferCallback), this);

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

    /* For Baumer cameras, AcquisitionFrameRate will not do anything until we set this */
	if (this->hasFeature("AcquisitionFrameRateEnable")) {
		arv_device_set_integer_feature_value (this->device, "AcquisitionFrameRateEnable", 1);
	}

    /* For Point Grey cameras, AcquisitionFrameRate will not do anything until we set this */
	if (this->hasFeature("AcquisitionFrameRateEnabled")) {
		arv_device_set_integer_feature_value (this->device, "AcquisitionFrameRateEnabled", 1);
	}
	/* Mark connection valid again */
	this->connectionValid = 1;
    g_print("Done.\n");

    /* If we have done initial connect finish here */
    if (this->initialConnectDone) {
    	return (asynStatus) status;
    }

    g_print("Getting feature list...\n");

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

	/* Add params for all nodes */
	keys = g_hash_table_get_keys(this->genicam->nodes);
	unsigned int index = 0;
	for (unsigned int j = 0; j<g_list_length(keys); j++) {
		/* Get the feature node */
		featureName = (char *) g_list_nth_data(keys, j);
		feature = arv_device_get_feature(this->device, featureName);
		/* If it is a category */
		if (ARV_IS_GC_CATEGORY(feature)) {
			features = arv_gc_category_get_features(ARV_GC_CATEGORY(feature));
			/* For each feature in the category */
			for (iter = features; iter != NULL; iter = iter->next) {
				featureName = (char *) iter->data;
				feature = arv_device_get_feature(this->device, featureName);
				/* If it isn't another category */
				if (!ARV_IS_GC_CATEGORY(feature)) {
					/* Check it doesn't exist in the list of features */
					values = g_hash_table_get_values(this->featureLookup);
					if (g_list_find(values, featureName) == NULL) {
						/* Check we have allocated enough space */
						if (index > NFEATURES) {
							asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
										"%s:%s: Not enough space allocated to store all camera features, increase NFEATURES\n",
										driverName, functionName);
							status = asynError;
							return (asynStatus) status;
						}
						/* Add it to our lookup table */
						//g_print("Adding %s\n", featureName);
						if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_DOUBLE) {
							createParam(featureName,      asynParamFloat64, &(this->features[index]));
						} else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_STRING) {
							createParam(featureName,      asynParamOctet, &(this->features[index]));
						} else {
							createParam(featureName,      asynParamInt32, &(this->features[index]));
						}
						g_hash_table_insert(this->featureLookup, (gpointer) &(this->features[index]), (gpointer) epicsStrDup(featureName));
						index++;
					}
					g_list_free(values);
				}
			}
		}
	}

    /* Get all values in the hash table */
	if (this->getAllFeatures()) {
		asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
					"%s:%s: Unable to get all camera features\n",
					driverName, functionName);
		status = asynError;
	}

    g_print("Done.\n");
    this->initialConnectDone = 1;
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

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    getIntegerParam(function, &rbv);
    status = setIntegerParam(function, value);

    /* If we have no camera, then just fail */
	if (function == AravisReset) {
		status = this->connectToCamera();
	} else if (this->camera == NULL || this->connectionValid != 1) {
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
            arv_camera_stop_acquisition(this->camera);
            setIntegerParam(ADStatus, ADStatusIdle);
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
	/* just write the value */
    } else if (function == AravisGetFeatures) {
    } else if (function < FIRST_ARAVIS_CAMERA_PARAM) {
        /* If this parameter belongs to a base class call its method */
        status = ADDriver::writeInt32(pasynUser, value);
    } else {
   		status = asynError;
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();

    /* Report any errors */
    if (status)
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:writeInt32 error, status=%d function=%d, value=%d\n",
              driverName, status, function, value);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeInt32: function=%d, value=%d\n",
              driverName, function, value);
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
		status = this->setFloatValue(featureName, value * 1000000, &rbv);
		if (status) setDoubleParam(function, rbv / 1000000);
    /* Acquire period / framerate */
    } else if (function == ADAcquirePeriod) {
    	featureName = (char *) g_hash_table_lookup(this->featureLookup, &function);
        if (value <= 0.0) value = 0.1;
		status = this->setFloatValue(featureName, 1/value, &rbv);
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
              "%s:writeFloat64 error, status=%d function=%d, value=%f, rbv=%f\n",
              driverName, status, function, value, rbv);
    else
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s:writeFloat64: function=%d, value=%f\n",
              driverName, function, value);
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
    int payload = arv_camera_get_payload(this->camera);
    NDArray *pRaw;
    int bufferDims[2] = {1,1};

    /* check stream exists */
    if (this->stream == NULL) {
    	asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
    				"%s:%s: Cannot allocate buffer on a NULL stream\n",
    				driverName, functionName);
    	return asynError;
    }

    pRaw = this->pNDArrayPool->alloc(2, bufferDims, NDInt8, payload, NULL);
    if (pRaw==NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating raw buffer\n",
                    driverName, functionName);
        return asynError;
    }

    buffer = arv_buffer_new_full(payload, pRaw->pData, (void *)pRaw, destroyBuffer);
    arv_stream_push_buffer (this->stream, buffer);
    return asynSuccess;
}

/** Check what event we have, and deal with new frames.
    this->camera exists, lock not taken */
void aravisCamera::callback() {
    int arrayCallbacks, imageCounter, numImages, numImagesCounter, imageMode;
    int colorMode, dataType, bayerFormat;
    size_t expected_size;
    int xDim=0, yDim=1, binX, binY, left_shift, getFeatures;
    double acquirePeriod;
    const char *functionName = "callback";
    epicsTimeStamp lastFeatureGet, now;
    NDArray *pRaw;
    ArvBuffer *buffer;
    guint64 n_completed_buffers, n_failures, n_underruns;
    epicsTimeGetCurrent(&lastFeatureGet);

    /* Loop forever */
    while (1) {
        /* Wait 5ms for an array to arrive from the queue */
        if (epicsMessageQueueReceiveWithTimeout(this->msgQId, &buffer, sizeof(&buffer), 0.005) == -1) {
        	/* If no camera, wait for one to appear */
        	if (this->camera == NULL || this->connectionValid != 1) {
        		continue;
        	}
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
        	continue;
        }

		this->lock();

		/* Get the current parameters */
		getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
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
		pRaw = (NDArray *) (buffer->user_data);
		if (pRaw == NULL) {
			asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
					"%s:%s: where did this buffer come from?\n",
					driverName, functionName);
			this->freeBufferAndUnlock(buffer);
			continue;
		}
	//            printf("callb buffer: %p, pRaw[%d]: %p, pData %p\n", buffer, i, pRaw, pRaw->pData);
		/* Put the frame number and time stamp into the buffer */
		pRaw->uniqueId = imageCounter;
		pRaw->timeStamp = buffer->timestamp_ns / 1.e9;
		/* Get any attributes that have been defined for this driver */
		this->getAttributes(pRaw->pAttributeList);

		/* Annotate it with its dimensions */
		if (this->lookupColorMode(buffer->pixel_format, &colorMode, &dataType, &bayerFormat) != asynSuccess) {
			asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
						"%s:%s: unknown pixel format %d\n",
						driverName, functionName, buffer->pixel_format);
			this->freeBufferAndUnlock(buffer);
			continue;
		}
		pRaw->pAttributeList->add("BayerPattern", "Bayer Pattern", NDAttrInt32, &bayerFormat);
		pRaw->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);
		pRaw->dataType = (NDDataType_t) dataType;
		switch (colorMode) {
			case NDColorModeMono:
			case NDColorModeBayer:
				xDim = 0;
				yDim = 1;
				pRaw->ndims = 2;
				expected_size = buffer->width * buffer->height;
				break;
			case NDColorModeRGB1:
				xDim = 1;
				yDim = 2;
				pRaw->ndims = 3;
				pRaw->dims[0].size    = 3;
				pRaw->dims[0].offset  = 0;
				pRaw->dims[0].binning = 1;
				expected_size = buffer->width * buffer->height * 3;
				break;
			default:
				asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
							"%s:%s: unknown colorMode %d\n",
							driverName, functionName, colorMode);
				this->freeBufferAndUnlock(buffer);
				continue;
		}
		pRaw->dims[xDim].size    = buffer->width;
		pRaw->dims[xDim].offset  = buffer->x_offset;
		pRaw->dims[xDim].binning = binX;
		pRaw->dims[yDim].size    = buffer->height;
		pRaw->dims[yDim].offset  = buffer->y_offset;
		pRaw->dims[yDim].binning = binY;

		/* If we are 16 bit, shift by the correct amount */
		if (pRaw->dataType == NDUInt16) {
			expected_size *= 2;
			if (left_shift) {
				int shift = 0;
				switch (buffer->pixel_format) {
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
					for (unsigned int ib = 0; ib < buffer->size / 2; ib++) {
						array[ib] = array[ib] << shift;
					}
				}
			}
		}

		if (expected_size != buffer->size) {
			asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
						"%s:%s: w: %d, h: %d, size: %d, expected_size: %d\n",
						driverName, functionName, buffer->width, buffer->height, buffer->size, expected_size);
			this->freeBufferAndUnlock(buffer);
			continue;
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
		}

		/* See if acquisition is done */
		if ((imageMode == ADImageSingle) ||
			((imageMode == ADImageMultiple) &&
			 (numImagesCounter >= numImages))) {
			arv_camera_stop_acquisition(this->camera);
			setIntegerParam(ADStatus, ADStatusIdle);
			// Want to make sure we're idle before we callback on ADAcquire
			callParamCallbacks();
			setIntegerParam(ADAcquire, 0);
			asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
				  "%s:%s: acquisition completed\n", driverName, functionName);
		}

		/* Call the callbacks to update any changes */
		callParamCallbacks();
		this->freeBufferAndUnlock(buffer);
    }
}

void aravisCamera::freeBufferAndUnlock(ArvBuffer *buffer) {
	/* free memory */
	g_object_unref(buffer);
	if (this->stream != NULL) {
		/* Allocate the raw buffer we use to compute images. */
		this->allocBuffer();
	}
	this->unlock();
}

asynStatus aravisCamera::start() {
    int imageMode;
    const char *functionName = "start";
	gint in_buffers, out_buffers;
	ArvBuffer *buffer;
    /* This will pop buffers from the stream */
	if (this->stream != NULL) {
		arv_stream_get_n_buffers(this->stream, &in_buffers, &out_buffers);
		for (int i=0; i<in_buffers; i++) {
			buffer = arv_stream_pop_input_buffer(this->stream);
			if (buffer == NULL) {
				asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
							"%s:%s: Null buffer popped from input queue\n",
							driverName, functionName);
			} else {
				g_object_unref(buffer);
			}
		}
		for (int i=0; i<out_buffers; i++) {
			buffer = arv_stream_pop_buffer(this->stream);
			if (buffer == NULL) {
				asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
							"%s:%s: Null buffer popped from output queue\n",
							driverName, functionName);
			} else {
				g_object_unref(buffer);
			}
		}
	}

    getIntegerParam(ADImageMode, &imageMode);
    if (imageMode == ADImageSingle) {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_SINGLE_FRAME);
    } else {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_CONTINUOUS);
    }

    setIntegerParam(ADNumImagesCounter, 0);
    setIntegerParam(ADStatus, ADStatusAcquire);

    /* fill the queue */
    for (int i=0; i<NRAW; i++) {
    	if (this->allocBuffer() != asynSuccess) {
			asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
						"%s:%s: allocBuffer returned error\n",
						driverName, functionName);
    		return asynError;
    	}
    }

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
	setIntegerParam(ADSizeX, w*binx);
	setIntegerParam(ADSizeY, h*biny);

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
    if (acquiring) {
    	arv_camera_stop_acquisition(this->camera);
    	this->unlock();
    	epicsThreadSleep(0.5);
    	this->lock();
    }

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
    	this->setBinning(binx, biny);
    	arv_camera_set_region(this->camera, x, y, w/binx, h/biny);
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
    if (acquiring) {
    	this->unlock();
    	epicsThreadSleep(0.5);
    	this->lock();
    	this->start();
    }
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
	return g_hash_table_lookup_extended(this->genicam->nodes, feature, NULL, NULL);
}

asynStatus aravisCamera::setIntegerValue(const char *feature, epicsInt32 value, epicsInt32 *rbv) {
    const char *functionName = "setIntegerValue";
    if (feature == NULL) {
    	asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
    				"%s:%s: Cannot set integer value of a NULL feature\n",
    				driverName, functionName);
    	return asynError;
    }
	arv_device_set_integer_feature_value (this->device, feature, value);
	if (rbv != NULL) {
		*rbv = arv_device_get_integer_feature_value (this->device, feature);
		if (value != *rbv) {
	    	asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	    				"%s:%s: value %d != rbv %d\n",
	    				driverName, functionName, value, rbv);
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
    arv_device_set_float_feature_value (this->device, feature, value);
	if (rbv != NULL) {
		*rbv = arv_device_get_float_feature_value (this->device, feature);
		if (fabs(value - *rbv) > 0.001) {
	    	asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
	    				"%s:%s: value %f != rbv %f\n",
	    				driverName, functionName, value, rbv);
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
	int status = asynSuccess;
	const char *featureName;
	ArvGcNode *feature;
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
		feature = arv_device_get_feature(this->device, featureName);
		if (feature == NULL) {
			status = asynError;
		} else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_DOUBLE) {
			floatValue = arv_device_get_float_feature_value (this->device, featureName);
			/* special cases for exposure and frame rate */
			if (*index == ADAcquireTime) floatValue /= 1000000;
			if (*index == ADAcquirePeriod && floatValue > 0) floatValue = 1/floatValue;
			status |= setDoubleParam(*index, floatValue);
		} else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_STRING) {
			stringValue = arv_device_get_string_feature_value(this->device, featureName);
			status |= setStringParam(*index, stringValue);
		} else if (arv_gc_feature_node_get_value_type(ARV_GC_FEATURE_NODE(feature)) == G_TYPE_INT64) {
			integerValue = arv_device_get_integer_feature_value (this->device, featureName);
			if (*index == ADGain) {
				/* Gain is sometimes an integer */
				status |= setDoubleParam(*index, integerValue);
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
    aravisCamera *pCamera
        = new aravisCamera(portName, cameraName,
                          maxBuffers, maxMemory, priority, stackSize);
    pCamera = NULL;
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
