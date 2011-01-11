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
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>

#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsStdio.h>
#include <epicsMutex.h>
#include <cantProceed.h>
#include <iocsh.h>
#include <epicsExport.h>

#include <ADDriver.h>
#include <epicsExit.h>
#include <epicsEndian.h>

extern "C" {
#include <arv.h>
}
#include <assert.h>

/* number of raw buffers in our queue */
#define NRAW 5

static const char *driverName = "aravisCamera";

/** Aravis GigE detector driver; demonstrates most of the features that areaDetector drivers can support. */
class aravisCamera : public ADDriver {
public:
    aravisCamera(const char *portName, const char *cameraName,
                int maxBuffers, size_t maxMemory,
                int priority, int stackSize);

    /* These are the methods that we override from ADDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    void report(FILE *fp, int details);
    void callback(ArvStreamCallbackType type, ArvBuffer *buffer); /**< Should be private, but gets called from C, so must be public */
    void shutdown(); /** Called by epicsAtExit */

protected:
    int AravisCompleted;
    #define FIRST_ARAVIS_CAMERA_PARAM AravisCompleted
    int AravisFailures;
    int AravisUnderruns;
    int AravisUpdate;
    int AravisWBAuto;
    int AravisWBRed;
    int AravisWBBlue;
    int AravisExpAuto;
    int AravisExpAutoTarget;
    int AravisExpAutoAlg;
    int AravisIrisAuto;
    int AravisIrisAutoTarget;
    int AravisIrisVideoLevel;
    int AravisIrisVideoLevelMin;
    int AravisIrisVideoLevelMax;
    int AravisGainAuto;
    int AravisGainAutoTarget;
    #define LAST_ARAVIS_CAMERA_PARAM AravisGainAutoTarget
	#define NUM_ARAVIS_CAMERA_PARAMS (&LAST_ARAVIS_CAMERA_PARAM - &FIRST_ARAVIS_CAMERA_PARAM + 1)


private:
    /* Our data */
    NDArray *pRaw[NRAW];
    ArvStream *stream;
    ArvCamera *camera;
    ArvDevice *device;
    ArvGc *genicam;
    int bufferSize;
    int bufferDims[2];
    GHashTable* featureInteger;
    GHashTable* featureFloat;
    asynStatus allocBuffer(int i);
    asynStatus start();
    asynStatus getBinning(int *binx, int *biny);
    asynStatus setBinning(int binx, int biny);
    asynStatus setGeometry();
    asynStatus lookupColorMode(ArvPixelFormat fmt, int *colorMode, int *dataType, int *bayerFormat);
    asynStatus lookupPixelFormat(int colorMode, int dataType, int bayerFormat, ArvPixelFormat *fmt);
    asynStatus setPixelFormat();
    asynStatus setIntegerValue(const char *feature, epicsInt32 value, epicsInt32 *rbv);
    asynStatus setFloatValue(const char *feature, epicsFloat64 value, epicsFloat64 *rbv);
    asynStatus setStringValue(const char *feature, const char *value);
    asynStatus getAllFeatures();
    gboolean hasFeature(const char *feature);
};

#define AravisCompletedString         "ARAVIS_COMPLETED"
#define AravisFailuresString          "ARAVIS_FAILURES"
#define AravisUnderrunsString         "ARAVIS_UNDERRUNS"
#define AravisUpdateString            "ARAVIS_UPDATE"
#define AravisWBAutoString            "ARAVIS_WBAUTO"
#define AravisWBRedString             "ARAVIS_WBRED"
#define AravisWBBlueString            "ARAVIS_WBBLUE"
#define AravisExpAutoString           "ARAVIS_EXPAUTO"
#define AravisExpAutoTargetString     "ARAVIS_EXPAUTOTARGET"
#define AravisExpAutoAlgString        "ARAVIS_EXPAUTOALG"
#define AravisIrisAutoString          "ARAVIS_IRISAUTO"
#define AravisIrisAutoTargetString    "ARAVIS_IRISAUTOTARGET"
#define AravisIrisVideoLevelString    "ARAVIS_IRISVIDEOLEVEL"
#define AravisIrisVideoLevelMinString "ARAVIS_IRISVIDEOLEVELMIN"
#define AravisIrisVideoLevelMaxString "ARAVIS_IRISVIDEOLEVELMAX"
#define AravisGainAutoString          "ARAVIS_GAINAUTO"
#define AravisGainAutoTargetString    "ARAVIS_GAINAUTOTARGET"

#define addFeatureConditionalInt(name, param) \
		if (this->hasFeature(name)) { \
    		createParam(param##String, asynParamInt32, &param); \
			g_hash_table_insert(this->featureInteger, &param, (gpointer) name); \
}


/** Called by epicsAtExit to shutdown camera */
static void aravisShutdown(void* arg) {
    aravisCamera *pPvt = (aravisCamera *) arg;
    pPvt->shutdown();
}

/** Called by ArvStream callback at stream start, end, and frame start, end */
void aravisCallback(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer) {
    aravisCamera *pPvt = (aravisCamera *) user_data;
    pPvt->callback(type, buffer);
}

/** Stop the camera and clean up */
void aravisCamera::shutdown() {
    if (this->camera != NULL) {
        arv_camera_stop_acquisition(this->camera);
        g_object_unref(this->camera);
        this->camera = NULL;
    }
}

/** Allocate an NDArray and prepare a buffer that is passed to the stream
    this->camera exists, lock taken */
asynStatus aravisCamera::allocBuffer(int i) {
    const char *functionName = "allocBuffer";
    ArvBuffer *buffer;

    this->pRaw[i] = this->pNDArrayPool->alloc(2, this->bufferDims, NDInt8, this->bufferSize, NULL);
    if (this->pRaw[i]==NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating raw buffer\n",
                    driverName, functionName);
        return asynError;
    }
    buffer = arv_buffer_new(this->bufferSize, this->pRaw[i]->pData);
//    printf("alloc buffer: %p, pRaw[%d]: %p, pData %p\n", buffer, i, this->pRaw[i], this->pRaw[i]->pData);
    arv_stream_push_buffer (this->stream, buffer);
    return asynSuccess;
}

/** Check what event we have, and deal with new frames.
    this->camera exists, lock not taken */
void aravisCamera::callback(ArvStreamCallbackType type, ArvBuffer *buffer) {
    int arrayCallbacks, imageCounter, numImages, numImagesCounter, imageMode;
    int colorMode, dataType, bayerFormat;
    int xDim=0, yDim=1, binX, binY, i;
    double acquirePeriod;
    const char *functionName = "callback";
    guint64 n_completed_buffers, n_failures, n_underruns;

    /* Always need the lock */
    this->lock();

    /* Buffer is complete */
    if (type == ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE) {

           /* Buffer contains a complete image */
        if (buffer->status == ARV_BUFFER_STATUS_SUCCESS) {

            /* Get the current parameters */
            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
            getIntegerParam(NDArrayCounter, &imageCounter);
            getIntegerParam(ADNumImages, &numImages);
            getIntegerParam(ADNumImagesCounter, &numImagesCounter);
            getIntegerParam(ADImageMode, &imageMode);
            getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
            getDoubleParam(ADAcquirePeriod, &acquirePeriod);

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
                setDoubleParam(ADTimeRemaining, (numImagesCounter - numImages) * acquirePeriod);
            }

            /* find the buffer */
            for (i = 0; i<NRAW; i++) {
                if (this->pRaw[i] != NULL && this->pRaw[i]->pData == buffer->data) break;
            }
            if (i >= NRAW) {
                asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s:%s: where did this buffer come from?\n",
                        driverName, functionName);
                return;
            }
//            printf("callb buffer: %p, pRaw[%d]: %p, pData %p\n", buffer, i, this->pRaw[i], this->pRaw[i]->pData);

            /* Put the frame number and time stamp into the buffer */
            this->pRaw[i]->uniqueId = imageCounter;
            this->pRaw[i]->timeStamp = buffer->timestamp_ns / 1.e9;

            /* Annotate it with its dimensions */
            this->lookupColorMode(buffer->pixel_format, &colorMode, &dataType, &bayerFormat);
            this->pRaw[i]->pAttributeList->add("BayerPattern", "Bayer Pattern", NDAttrInt32, &bayerFormat);
            this->pRaw[i]->pAttributeList->add("ColorMode", "Color Mode", NDAttrInt32, &colorMode);
            this->pRaw[i]->dataType = (NDDataType_t) dataType;
            switch (colorMode) {
                case NDColorModeMono:
                case NDColorModeBayer:
                    xDim = 0;
                    yDim = 1;
                    this->pRaw[i]->ndims = 2;
                    break;
                case NDColorModeRGB1:
                    xDim = 1;
                    yDim = 2;
                    this->pRaw[i]->ndims = 3;
                    this->pRaw[i]->dims[0].size    = 3;
                    this->pRaw[i]->dims[0].offset  = 0;
                    this->pRaw[i]->dims[0].binning = 1;
                    break;
                default:
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                                "%s:%s: unknown colorMode\n",
                                driverName, functionName);
            }
            this->pRaw[i]->dims[xDim].size    = buffer->width;
            this->pRaw[i]->dims[xDim].offset  = buffer->x_offset;
            this->pRaw[i]->dims[xDim].binning = binX;
            this->pRaw[i]->dims[yDim].size    = buffer->height;
            this->pRaw[i]->dims[yDim].offset  = buffer->y_offset;
            this->pRaw[i]->dims[yDim].binning = binY;

            /* If we are 16 bit, shift by the correct amount */
            if (dataType == NDUInt16) {
            	int shift = 0;
            	switch (buffer->pixel_format) {
            		case ARV_PIXEL_FORMAT_MONO_12:
            			shift = 4;
            			break;
            		default:
            			break;
            	}
            	if (shift != 0) {
            		//printf("Shift by %d\n", shift);
                	uint64_t *array = (uint64_t *) this->pRaw[i]->pData;
            		for (unsigned int ib = 0; ib < buffer->width * buffer->height * this->pRaw[i]->ndims / 4; ib++) {
            			array[ib] = array[ib] << shift;
            		}
            	}
            }
/*
            for (int ib = 0; ib<10; ib++) {
            	unsigned char *ix = ((unsigned char *)this->pRaw[i]->pData) + ib;
            	printf("%d,", (int) (*ix));
            }
            printf("\n");
*/
            /* Get any attributes that have been defined for this driver */
            this->getAttributes(this->pRaw[i]->pAttributeList);

            /* Call the callbacks to update any changes */
            callParamCallbacks();

            /* this is a good image, so callback on it */
            if (arrayCallbacks) {
                /* Call the NDArray callback */
                /* Must release the lock here, or we can get into a deadlock, because we can
                 * block on the plugin lock, and the plugin can be calling us */
                this->unlock();
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                     "%s:%s: calling imageData callback\n", driverName, functionName);
                doCallbacksGenericPointer(this->pRaw[i], NDArrayData, 0);
                this->lock();
            }

            /* See if acquisition is done */
            if ((imageMode == ADImageSingle) ||
                ((imageMode == ADImageMultiple) &&
                 (numImagesCounter >= numImages))) {
                arv_camera_stop_acquisition (this->camera);
                setIntegerParam(ADAcquire, 0);
                setIntegerParam(ADStatus, ADStatusIdle);
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s:%s: acquisition completed\n", driverName, functionName);
            } else {
                /* free memory */
                this->pRaw[i]->release();
                this->pRaw[i] = NULL;
                g_object_unref(buffer);
                /* Allocate the raw buffer we use to compute images. */
                this->allocBuffer(i);
            }

        /* bad frame, just push the buffer back */
        } else {
            arv_stream_push_buffer (this->stream, buffer);
        }
        /* Report statistics */
        arv_stream_get_statistics(this->stream, &n_completed_buffers, &n_failures, &n_underruns);
        setDoubleParam(AravisCompleted, (double) n_completed_buffers);
        setDoubleParam(AravisFailures, (double) n_failures);
        setDoubleParam(AravisUnderruns, (double) n_underruns);
        callParamCallbacks();
    }
    this->unlock();
}

/** Start acquisition
    this->camera exists, lock taken */
asynStatus aravisCamera::start() {
    int imageMode;
    getIntegerParam(ADImageMode, &imageMode);
    if (imageMode == ADImageSingle) {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_SINGLE_FRAME);
    } else {
        arv_camera_set_acquisition_mode(this->camera, ARV_ACQUISITION_MODE_CONTINUOUS);
    }
    for (int i=0; i<NRAW; i++) {
        if (this->pRaw[i]==NULL) this->allocBuffer(i);
    }
    setIntegerParam(ADNumImagesCounter, 0);
    setIntegerParam(ADStatus, ADStatusAcquire);
    arv_camera_start_acquisition (this->camera);
    //arv_gv_device_leave_control(ARV_GV_DEVICE(this->device));
    return asynSuccess;
}

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

asynStatus aravisCamera::getBinning(int *binx, int *biny) {
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
	    return asynError;
	} else {
		*binx = arv_device_get_integer_feature_value (this->device, "BinningHorizontal");
		*biny = arv_device_get_integer_feature_value (this->device, "BinningVertical");
		return asynSuccess;
	}
}

asynStatus aravisCamera::setBinning(int binx, int biny) {
	if (this->hasFeature("BinningMode")) {
		/* lookup the enum */
	    const int N = sizeof(bin_lookup) / sizeof(struct bin_lookup);
	    for (int i = 0; i < N; i ++)
	        if (binx == bin_lookup[i].binx &&
	        	biny == bin_lookup[i].biny) {
	            arv_device_set_string_feature_value (this->device, "BinningMode", bin_lookup[i].mode);
	            return asynSuccess;
	        }
	    return asynError;
	} else {
		arv_device_set_integer_feature_value (this->device, "BinningHorizontal", binx);
		arv_device_set_integer_feature_value (this->device, "BinningVertical", biny);
		return asynSuccess;
	}
}

/** Change camera binning
    this->camera exists, lock taken */
asynStatus aravisCamera::setGeometry() {
    asynStatus status = asynSuccess;
    gint x_rbv, y_rbv, w_rbv, h_rbv;
    int binx_rbv, biny_rbv, binx, biny, x, y, w, h, maxW, maxH, colorMode, dataType, bps=1;
    /* Get the demands */
    getIntegerParam(ADBinX, &binx);
    getIntegerParam(ADBinY, &biny);
    getIntegerParam(ADMinX, &x);
    getIntegerParam(ADMinY, &y);
    getIntegerParam(ADSizeX, &w);
    getIntegerParam(ADSizeY, &h);
    getIntegerParam(ADMaxSizeX, &maxW);
    getIntegerParam(ADMaxSizeY, &maxH);
    getIntegerParam(NDColorMode, &colorMode);
    getIntegerParam(NDDataType, &dataType);
    /* make sure they're sensible */
    if (binx<1) {
        binx = 1;
        setIntegerParam(ADBinX, binx);
        status = asynError;
    }
    if (biny<1) {
        biny = 1;
        setIntegerParam(ADBinY, biny);
        status = asynError;
    }
    if (w>maxW) {
        w = maxW;
        setIntegerParam(ADSizeX, w);
        status = asynError;
    }
    if (h>maxH) {
        h = maxH;
        setIntegerParam(ADSizeY, h);
        status = asynError;
    }
    /* Send them to the camera */
    this->setBinning(binx, biny);
    arv_camera_set_region(this->camera, x, y, w/binx, h/biny);
    /* Check they match */
    this->getBinning(&binx_rbv, &biny_rbv);
    arv_camera_get_region(this->camera, &x_rbv, &y_rbv, &w_rbv, &h_rbv);
    if (x != x_rbv || y != y_rbv || w/binx != w_rbv || h/biny != h_rbv || binx != binx_rbv || biny != biny_rbv) {
        setIntegerParam(ADMinX, x_rbv);
        setIntegerParam(ADMinY, y_rbv);
        setIntegerParam(ADSizeX, w_rbv*binx);
        setIntegerParam(ADSizeY, h_rbv*biny);
        setIntegerParam(ADBinX, binx_rbv);
        setIntegerParam(ADBinY, biny_rbv);
        status = asynError;
    }
    /* Set sizes */
    if (colorMode == NDInt16) bps = 2;
    if (dataType == NDColorModeRGB1) bps *= 3;
    setIntegerParam(NDArraySize, w_rbv*h_rbv*bps);
    setIntegerParam(NDArraySizeX, w_rbv);
    setIntegerParam(NDArraySizeY, h_rbv);
    return status;
}

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
    { ARV_PIXEL_FORMAT_MONO_12,       NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_RGB_12_PACKED, NDColorModeRGB1,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_RGB_10_PACKED, NDColorModeRGB1,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_BAYER_GR_12,   NDColorModeBayer, NDUInt16, NDBayerGRBG },
    { ARV_PIXEL_FORMAT_BAYER_RG_12,   NDColorModeBayer, NDUInt16, NDBayerRGGB },
    { ARV_PIXEL_FORMAT_BAYER_GB_12,   NDColorModeBayer, NDUInt16, NDBayerGBRG },
    { ARV_PIXEL_FORMAT_BAYER_BG_12,   NDColorModeBayer, NDUInt16, NDBayerBGGR }
};

/** Lookup a colorMode, dataType and bayerFormat from an ArvPixelFormat */
asynStatus aravisCamera::lookupColorMode(ArvPixelFormat fmt, int *colorMode, int *dataType, int *bayerFormat) {
    const int N = sizeof(pix_lookup) / sizeof(struct pix_lookup);
    for (int i = 0; i < N; i ++)
        if (pix_lookup[i].fmt == fmt) {
            *colorMode   = pix_lookup[i].colorMode;
            *dataType    = pix_lookup[i].dataType;
            *bayerFormat = pix_lookup[i].bayerFormat;
            return asynSuccess;
        }
    return asynError;
}

/** Lookup an ArvPixelFormat from a colorMode, dataType and bayerFormat */
asynStatus aravisCamera::lookupPixelFormat(int colorMode, int dataType, int bayerFormat, ArvPixelFormat *fmt) {
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
            	for (iter = arv_gc_node_get_childs(ARV_GC_NODE (enumeration)); iter != NULL; iter = iter->next) {
            		if (arv_gc_enum_entry_get_value(ARV_GC_ENUM_ENTRY(iter->data)) == pix_lookup[i].fmt) {
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
    return asynError;
}

/** Change camera pixel format
    this->camera exists, lock taken */
asynStatus aravisCamera::setPixelFormat() {
    asynStatus status = asynSuccess;
    ArvPixelFormat fmt, fmt_rbv;
    int colorMode, dataType, bayerFormat=0;
    /* Get the demands */
    getIntegerParam(NDColorMode, &colorMode);
    getIntegerParam(NDDataType, &dataType);
    /* Lookup the pix format, fail if not supported */
    status = this->lookupPixelFormat(colorMode, dataType, bayerFormat, &fmt);
    if (status) return status;
    /* Send them to the camera */
    arv_camera_set_pixel_format(this->camera, fmt);
    /* Check they match */
    fmt_rbv = arv_camera_get_pixel_format(this->camera);
    if (fmt_rbv != fmt) {
        /* Just fail if what comes back from the camera isn't in our lookup */
        status = this->lookupColorMode(fmt_rbv, &colorMode, &dataType, &bayerFormat);
        if (status) return status;
        /* Otherwise set things back */
        setIntegerParam(NDColorMode, colorMode);
        setIntegerParam(NDDataType, dataType);
        status = asynError;
    } else {
        status = asynSuccess;
    }
    this->setGeometry();
    return status;
}

gboolean aravisCamera::hasFeature(const char *feature) {
	return g_hash_table_lookup_extended(this->genicam->nodes, feature, NULL, NULL);
}

asynStatus aravisCamera::setIntegerValue(const char *feature, epicsInt32 value, epicsInt32 *rbv) {
    asynStatus status = asynSuccess;
    if (feature == NULL) return asynError;
	arv_device_set_integer_feature_value (this->device, feature, value);
	if (rbv != NULL) {
		*rbv = arv_device_get_integer_feature_value (this->device, feature);
		if (value != *rbv) {
			status = asynError;
		}
	}
    return status;
}

asynStatus aravisCamera::setFloatValue(const char *feature, epicsFloat64 value, epicsFloat64 *rbv) {
    asynStatus status = asynSuccess;
    if (feature == NULL) return asynError;
    arv_device_set_float_feature_value (this->device, feature, value);
	if (rbv != NULL) {
		*rbv = arv_device_get_float_feature_value (this->device, feature);
		if (fabs(value - *rbv) > 0.001) {
			status = asynError;
		}
	}
    return status;
}

asynStatus aravisCamera::setStringValue(const char *feature, const char *value) {
    asynStatus status = asynSuccess;
    if (feature == NULL) return asynError;
    arv_device_set_string_feature_value (this->device, feature, value);
    return status;
}

asynStatus aravisCamera::getAllFeatures() {
	int status = asynSuccess;
	GList *keys;
	const char *feature;
	unsigned int j;
	int *index;
	epicsFloat64 floatValue;
	epicsInt32 integerValue;
    guint64 n_completed_buffers, n_failures, n_underruns;

	/* Set float values */
	keys = g_hash_table_get_keys(this->featureFloat);
	for (j = 0; j<g_list_length(keys); j++) {
		index = (int *) g_list_nth_data(keys, j);
		feature = (const char *) g_hash_table_lookup(this->featureFloat, index);
		floatValue = arv_device_get_float_feature_value (this->device, feature);
		/* special cases for exposure and frame rate */
		if (*index == ADAcquireTime) floatValue /= 1000000;
		if (*index == ADAcquirePeriod && floatValue > 0) floatValue = 1/floatValue;
		status |= setDoubleParam(*index, floatValue);
	}
	if (status) printf("Error getting float values\n");

	/* Special cases for white balance red and blue */
	if (this->hasFeature("BalanceRationSelector")) {
		this->setIntegerValue("BalanceRatioSelector", 0, NULL);
		floatValue = arv_device_get_float_feature_value (this->device, "BalanceRatioAbs");
		status |= setDoubleParam(AravisWBRed, floatValue);
		this->setIntegerValue("BalanceRatioSelector", 1, NULL);
		floatValue = arv_device_get_float_feature_value (this->device, "BalanceRatioAbs");
		status |= setDoubleParam(AravisWBBlue, floatValue);
		if (status) printf("Error getting wb values\n");
	}

	/* Set integer values */
	keys = g_hash_table_get_keys(this->featureInteger);
	for (j = 0; j<g_list_length(keys); j++) {
		index = (int *) g_list_nth_data(keys, j);
		feature = (const char *) g_hash_table_lookup(this->featureInteger, index);
		integerValue = arv_device_get_integer_feature_value (this->device, feature);
		if (*index == ADGain) {
			/* Gain is sometimes an integer */
			status |= setDoubleParam(*index, integerValue);
		} else {
			status |= setIntegerParam(*index, integerValue);
		}
	}
	if (status) printf("Error getting int values\n");

    /* Report statistics */
    arv_stream_get_statistics(this->stream, &n_completed_buffers, &n_failures, &n_underruns);
    setDoubleParam(AravisCompleted, (double) n_completed_buffers);
    setDoubleParam(AravisFailures, (double) n_failures);
    setDoubleParam(AravisUnderruns, (double) n_underruns);
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

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    getIntegerParam(function, &rbv);
    status = setIntegerParam(function, value);

    /* If we have no camera, then just fail */
    if (this->camera == NULL) {
        status = asynError;
    } else if (function == AravisUpdate) {
    	status = this->getAllFeatures();
    } else if (function == ADAcquire) {
        if (value) {
            /* This was a command to start acquisition */
            status = this->start();
        } else {
            /* This was a command to stop acquisition */
            arv_camera_stop_acquisition (this->camera);
            setIntegerParam(ADStatus, ADStatusIdle);
        }
    } else if (function == ADBinX || function == ADBinY || function == ADMinX || function == ADMinY || function == ADSizeX || function == ADSizeY) {
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
    } else if (function == NDDataType || function == NDColorMode) {
        status = this->setPixelFormat();
    } else if (g_hash_table_lookup_extended(this->featureInteger, &function, NULL, NULL)) {
		featureName = (char *) g_hash_table_lookup(this->featureInteger, &function);
		status = this->setIntegerValue(featureName, value, &rbv);
		if (status) setIntegerParam(function, rbv);
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

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    getDoubleParam(function, &rbv);
    status = setDoubleParam(function, value);

    /* If we have no camera, then just fail */
    if (this->camera == NULL) {
        status = asynError;
    } else if (function == ADGain) {
    	if (g_hash_table_lookup_extended(this->featureInteger, &function, NULL, NULL)) {
    		epicsInt32 i_rbv, i_value = (epicsInt32) value;
    		featureName = (char *) g_hash_table_lookup(this->featureInteger, &function);
    		status = this->setIntegerValue(featureName, i_value, &i_rbv);
    		if (strcmp("GainRawChannelA", featureName) == 0) this->setIntegerValue("GainRawChannelB", i_value, NULL);
    		rbv = i_rbv;
    	} else {
    		featureName = (char *) g_hash_table_lookup(this->featureFloat, &function);
    		status = this->setFloatValue(featureName, value, &rbv);
    	}
    	if (status) setDoubleParam(function, rbv);
    } else if (function == ADAcquireTime) {
    	featureName = (char *) g_hash_table_lookup(this->featureFloat, &function);
    	status = this->setFloatValue(featureName, value * 1000000, &rbv);
    	if (status) setDoubleParam(function, rbv / 1000000);
    } else if (function == AravisWBRed) {
    	this->setIntegerValue("BalanceRatioSelector", 0, NULL);
    	status = this->setFloatValue("BalanceRatioAbs", value, &rbv);
    	if (status) setDoubleParam(function, rbv);
    } else if (function == AravisWBBlue) {
    	this->setIntegerValue("BalanceRatioSelector", 1, NULL);
    	status = this->setFloatValue("BalanceRatioAbs", value, &rbv);
    	if (status) setDoubleParam(function, rbv);
    } else if (function == ADAcquirePeriod) {
    	featureName = (char *) g_hash_table_lookup(this->featureFloat, &function);
        if (value <= 0.0) value = 0.1;
		this->setFloatValue(featureName, 1/value, &rbv);
        if (status) setDoubleParam(function, 1/rbv);
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

/** Constructor for aravisCamera; most parameters are aravisply passed to ADDriver::ADDriver.
  * After calling the base class constructor this method creates a thread to compute the GigE detector data,
  * and sets reasonable default values for parameters defined in this class, asynNDArrayDriver and ADDriver.
  * \param[in] portName The name of the asyn port driver to be created.
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
      stream(NULL), camera(NULL)

{
    int status = asynSuccess;
    const char *functionName = "aravisCamera";
    int x,y,w,h,binx,biny;
    ArvPixelFormat fmt;
    int colorMode, dataType, bayerFormat=0;
    const char *vendor, *model;
    this->featureInteger = g_hash_table_new(g_int_hash, g_int_equal);
    this->featureFloat = g_hash_table_new(g_int_hash, g_int_equal);

    /* Create some custom parameters */
    createParam(AravisCompletedString,      asynParamFloat64, &AravisCompleted);
    createParam(AravisFailuresString,       asynParamFloat64, &AravisFailures);
    createParam(AravisUnderrunsString,      asynParamFloat64, &AravisUnderruns);
    createParam(AravisUpdateString,         asynParamInt32,   &AravisUpdate);

    /* Connect to the camera */
    g_thread_init (NULL);
    g_type_init ();
    g_print ("Looking for camera '%s'... ", cameraName);
    this->camera = arv_camera_new (cameraName);
    if (this->camera == NULL) {
        printf ("No camera found\n");
        return;
    }

    /* Store device and genicam elements */
    this->device = arv_camera_get_device(this->camera);
    this->genicam = arv_device_get_genicam (this->device);

    /* Create a stream */
	printf ("Found %p\n", this->camera);
	this->stream = arv_camera_create_stream (this->camera, aravisCallback, (void *) this);
	arv_gv_stream_set_option (ARV_GV_STREAM (this->stream), ARV_GV_STREAM_OPTION_SOCKET_BUFFER_AUTO, 0);
	arv_gv_stream_set_packet_resend (ARV_GV_STREAM (this->stream), ARV_GV_STREAM_PACKET_RESEND_ALWAYS);


    /* Set vendor and model number */
    vendor = arv_camera_get_vendor_name(this->camera);
    if (vendor) status |= setStringParam (ADManufacturer, vendor);
    model = arv_camera_get_model_name(this->camera);
    if (model) status |= setStringParam (ADModel, model);

    /* Set gain */
    if (this->hasFeature("GainRawChannelA")) {
    	g_hash_table_insert(this->featureInteger, &ADGain, (gpointer)"GainRawChannelA");
    } else if (this->hasFeature("Gain")) {
		g_hash_table_insert(this->featureFloat, &ADGain, (gpointer)"Gain");
	} else if (this->hasFeature("GainRaw")){
		g_hash_table_insert(this->featureInteger, &ADGain, (gpointer)"GainRaw");
	}

	/* Set exposure */
	if (this->hasFeature("ExposureTimeAbs")) {
		g_hash_table_insert(this->featureFloat, &ADAcquireTime, (gpointer)"ExposureTimeAbs");
	}

	/* Set framerate */
	if (this->hasFeature("AcquisitionFrameRate")) {
		g_hash_table_insert(this->featureFloat, &ADAcquirePeriod, (gpointer)"AcquisitionFrameRate");
	} else if (this->hasFeature("AcquisitionFrameRateAbs")) {
		g_hash_table_insert(this->featureFloat, &ADAcquirePeriod, (gpointer)"AcquisitionFrameRateAbs");
	}

	/* These are needed for prosilica/AVT cameras to use triggering for each frame */
	if (this->hasFeature("TriggerSelector")) {
		this->setStringValue("TriggerSelector", "FrameStart");
		this->setStringValue("TriggerMode", "On");
	}

	/* Set sensor size */
    arv_camera_get_sensor_size(this->camera, &w, &h);
    status |= setIntegerParam(ADMaxSizeX, w);
    status |= setIntegerParam(ADMaxSizeY, h);
    /* max buffer size is w * h * colours * bytesPerPixel */
    this->bufferSize = w * h * 3 * 2;
    this->bufferDims[0] = w;
    this->bufferDims[1] = h;

    /* set ROI */
    arv_camera_get_region(this->camera, &x, &y, &w, &h);
    status |= setIntegerParam(ADMinX, x);
    status |= setIntegerParam(ADMinY, y);
    this->getBinning(&binx, &biny);
    status |= setIntegerParam(ADBinX, binx);
    status |= setIntegerParam(ADBinY, biny);
    status |= setIntegerParam(ADSizeX, binx*w);
    status |= setIntegerParam(ADSizeY, biny*h);

    /* Set some other possible features */
	addFeatureConditionalInt("TriggerSource",      ADTriggerMode);
    addFeatureConditionalInt("GainAuto",           AravisGainAuto);
    addFeatureConditionalInt("GainAutoTarget",     AravisGainAutoTarget);
    addFeatureConditionalInt("IrisMode",           AravisIrisAuto);
    addFeatureConditionalInt("IrisAutoTarget",     AravisIrisAutoTarget);
    addFeatureConditionalInt("IrisVideoLevel",     AravisIrisVideoLevel);
    addFeatureConditionalInt("IrisVideoLevelMin",  AravisIrisVideoLevelMin);
    addFeatureConditionalInt("IrisVideoLevelMax",  AravisIrisVideoLevelMax);
    addFeatureConditionalInt("ExposureAuto",       AravisExpAuto);
    addFeatureConditionalInt("ExposureAutoTarget", AravisExpAutoTarget);
    addFeatureConditionalInt("ExposureAutoAlg",    AravisExpAutoAlg);
    addFeatureConditionalInt("BalanceWhiteAuto",   AravisWBAuto);

    /* White balance is treated in special way */
    createParam(AravisWBRedString,          asynParamFloat64, &AravisWBRed);
    createParam(AravisWBBlueString,         asynParamFloat64, &AravisWBBlue);

    /* Find trigger modes */
    /*
    ArvGcNode *node = arv_gc_get_node(this->genicam, "TriggerSource");
    if (ARV_IS_GC_ENUMERATION (node)) {
    	ArvGcEnumeration *enumeration = (ARV_GC_ENUMERATION (node));
    	const GSList *iter;
    	for (iter = arv_gc_node_get_childs (ARV_GC_NODE (enumeration)); iter != NULL; iter = iter->next) {
    		//if (arv_gc_enum_entry_get_value (iter->data) == value) {
    		const char *string = arv_gc_node_get_name (ARV_GC_NODE(iter->data));
    		printf("Trig: %s\n", string);
    	}
    }
	*/

    /* Set some initial values for other parameters */
    status |= setIntegerParam(ADReverseX, 0);
    status |= setIntegerParam(ADReverseY, 0);
    status |= setIntegerParam(ADImageMode, ADImageContinuous);
    status |= setIntegerParam(ADNumImages, 100);
    status |= setDoubleParam(AravisCompleted, 0);
    status |= setDoubleParam(AravisFailures, 0);
    status |= setDoubleParam(AravisUnderruns, 0);
    status |= setIntegerParam(AravisUpdate, 0);

	/* Report if anything has failed */
    if (status) {
        printf("%s: unable to set camera parameters\n", functionName);
    }

    /* Get all values in the hash table */
    status = this->getAllFeatures();
	/* Report if anything has failed */
    if (status) {
        printf("%s: unable to get all camera features\n", functionName);
    }

    /* Set the geometry */
    fmt = arv_camera_get_pixel_format(this->camera);
	status |= this->lookupColorMode(fmt, &colorMode, &dataType, &bayerFormat);
	status |= setIntegerParam(NDColorMode, colorMode);
	status |= setIntegerParam(NDDataType, dataType);
	status |= this->setGeometry();

	/* Report if anything has failed */
    if (status) {
        printf("%s: unable to set camera geometry\n", functionName);
    }

    /* Clear buffers */
    for (int i=0; i<NRAW; i++) this->pRaw[i]=NULL;

    /* Register the shutdown function for epicsAtExit */
    epicsAtExit(aravisShutdown, (void*)this);
}

/** Configuration command, called directly or from iocsh */
extern "C" int aravisCameraConfig(const char *portName, const char *cameraName,
                                 int maxBuffers, size_t maxMemory, int priority, int stackSize)
{
    aravisCamera *pAravisCameraector
        = new aravisCamera(portName, cameraName,
                          maxBuffers, maxMemory, priority, stackSize);
    pAravisCameraector = NULL;
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
