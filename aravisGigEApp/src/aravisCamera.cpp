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

extern "C" {
#include <arv.h>
}
#include <assert.h>

static const char *driverName = "aravisCamera";

/** Aravisulation detector driver; demonstrates most of the features that areaDetector drivers can support. */
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
    #define LAST_ARAVIS_CAMERA_PARAM AravisUnderruns

private:
    /* Our data */
    NDArray *pRaw;
    ArvBuffer *buffer;
    ArvStream *stream;
    ArvCamera *camera;
    asynStatus allocBuffer();
    asynStatus start();    
	asynStatus setBinning();
	asynStatus setRegion();	
	asynStatus lookupColorMode(ArvPixelFormat fmt, int *colorMode, int *dataType, int *bayerFormat);
	asynStatus lookupPixelFormat(int colorMode, int dataType, int bayerFormat, ArvPixelFormat *fmt);
	asynStatus setPixelFormat();

};

#define AravisCompletedString  "ARAVIS_COMPLETED"
#define AravisFailuresString   "ARAVIS_FAILURES"
#define AravisUnderrunsString  "ARAVIS_UNDERRUNS"

#define NUM_ARAVIS_CAMERA_PARAMS (&LAST_ARAVIS_CAMERA_PARAM - &FIRST_ARAVIS_CAMERA_PARAM + 1)

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
asynStatus aravisCamera::allocBuffer() {
    int dims[3];
    int xDim = 0, yDim = 1, colorDim = -1;
    int ndims = 2;
    int x, y;
    const char *functionName = "allocBuffer";

    /* Free the previous NDArray and ArvBuffer*/
    if (this->pRaw) this->pRaw->release();
    if (this->buffer) g_object_unref(this->buffer);

    /* Allocate the raw buffer we use to compute images. */
    arv_camera_get_region(this->camera, &x, &y, &dims[xDim], &dims[yDim]);
    if (ndims > 2) dims[colorDim] = 3;
    this->pRaw = this->pNDArrayPool->alloc(ndims, dims, NDInt8, 0, NULL);
    printf("Alloc %d %d %d %d\n", x, y, dims[xDim], dims[yDim]);
    if (this->pRaw==NULL) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating raw buffer\n",
                    driverName, functionName);
        return asynError;
    } 
    this->buffer = arv_buffer_new(dims[xDim]*dims[yDim], this->pRaw->pData);
    arv_stream_push_buffer (this->stream, this->buffer);
    return asynSuccess;
}

/** Check what event we have, and deal with new frames.
    this->camera exists, lock not taken */
void aravisCamera::callback(ArvStreamCallbackType type, ArvBuffer *buffer) {
    int arrayCallbacks, imageCounter, numImages, numImagesCounter, imageMode;
    double acquirePeriod;
    const char *functionName = "aravisCallback";
    guint64 n_completed_buffers, n_failures, n_underruns;

    /* Always need the lock */
    this->lock();

      /* Buffer is being filled */
    if (type == ARV_STREAM_CALLBACK_TYPE_START_BUFFER) {
           /* Report being busy */
	       setIntegerParam(ADStatus, ADStatusAcquire);

      /* Buffer is complete */
    } else if (type == ARV_STREAM_CALLBACK_TYPE_BUFFER_DONE) {

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

            /* Report a new frame with the counters */
            imageCounter++;
            numImagesCounter++;
            setIntegerParam(NDArrayCounter, imageCounter);
            setIntegerParam(ADNumImagesCounter, numImagesCounter);
            if (imageMode == ADImageMultiple) {
            	setDoubleParam(ADTimeRemaining, (numImagesCounter - numImages) * acquirePeriod);
            }            

            /* Put the frame number and time stamp into the buffer */
            this->pRaw->uniqueId = imageCounter;
            this->pRaw->timeStamp = buffer->timestamp_ns / 1.e9;

            /* Get any attributes that have been defined for this driver */
            this->getAttributes(this->pRaw->pAttributeList);

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
                doCallbacksGenericPointer(this->pRaw, NDArrayData, 0);
                this->lock();
            }

            /* See if acquisition is done */
            if ((imageMode == ADImageSingle) ||
                ((imageMode == ADImageMultiple) &&
                 (numImagesCounter >= numImages))) {
                arv_camera_stop_acquisition (this->camera);
                setIntegerParam(ADAcquire, 0);
                asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s:%s: acquisition completed\n", driverName, functionName);
            } else {
                /* Allocate the raw buffer we use to compute images. */
                this->allocBuffer();
            }

            /* Call the callbacks to update any changes */
            setIntegerParam(ADStatus, ADStatusIdle);

           /* bad frame, just push the buffer back */
        } else {
        	printf("Bad\n");
//        	this->allocBuffer();
//            arv_stream_push_buffer (this->stream, this->buffer);
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
/*        arv_gv_stream_set_option (ARV_GV_STREAM (this->stream),
                      ARV_GV_STREAM_OPTION_SOCKET_BUFFER_AUTO,
                      0);*/
    setIntegerParam(ADNumImagesCounter, 0);
    setIntegerParam(ADStatus, ADStatusAcquire);
	if (this->allocBuffer()) return asynError;    
    arv_camera_start_acquisition (this->camera);
    return asynSuccess;
}

/** Change camera binning
    this->camera exists, lock taken */
asynStatus aravisCamera::setBinning() {
    gint binx_rbv, biny_rbv;
    int binx, biny;
    /* Get the demands */
    getIntegerParam(ADBinX, &binx);
    getIntegerParam(ADBinY, &biny);
    /* Send them to the camera */
    arv_camera_set_binning(this->camera, binx, biny);
    /* Check they match */
    arv_camera_get_binning(this->camera, &binx_rbv, &biny_rbv);
    if (binx != binx_rbv || biny != biny_rbv) {
        setIntegerParam(ADBinX, binx_rbv);
        setIntegerParam(ADBinY, biny_rbv);
        return asynError;
    } else {
        return asynSuccess;
    }
}

/** Change camera region
    this->camera exists, lock taken */
asynStatus aravisCamera::setRegion() {
    gint x_rbv, y_rbv, w_rbv, h_rbv;
    int x, y, w, h;
    /* Get the demands */
    getIntegerParam(ADMinX, &x);
    getIntegerParam(ADMinY, &y);
    getIntegerParam(ADSizeX, &w);
    getIntegerParam(ADSizeY, &h);
    /* Send them to the camera */
    arv_camera_set_region(this->camera, x, y, w, h);
    /* Check they match */
    arv_camera_get_region(this->camera, &x_rbv, &y_rbv, &w_rbv, &h_rbv);
    if (x != x_rbv || y != y_rbv || w != w_rbv || h != h_rbv) {
        setIntegerParam(ADMinX, x_rbv);
        setIntegerParam(ADMinY, y_rbv);
        setIntegerParam(ADSizeX, w_rbv);
        setIntegerParam(ADSizeY, h_rbv);
        return asynError;
    } else {
        return asynSuccess;
    }
}

struct pix_lookup {
	ArvPixelFormat fmt;
	int colorMode, dataType, bayerFormat;
};

static const struct pix_lookup pix_lookup[] = {
	{ ARV_PIXEL_FORMAT_MONO_8,       NDColorModeMono,  NDUInt8,  0           },
    { ARV_PIXEL_FORMAT_RGB_8_PACKED, NDColorModeRGB1,  NDUInt8,  0           },
    { ARV_PIXEL_FORMAT_BAYER_GR_8,   NDColorModeBayer, NDUInt8,  NDBayerGRBG },
    { ARV_PIXEL_FORMAT_BAYER_RG_8,   NDColorModeBayer, NDUInt8,  NDBayerRGGB },
    { ARV_PIXEL_FORMAT_BAYER_GB_8,   NDColorModeBayer, NDUInt8,  NDBayerGBRG },
    { ARV_PIXEL_FORMAT_BAYER_BG_8,   NDColorModeBayer, NDUInt8,  NDBayerBGGR },	
	{ ARV_PIXEL_FORMAT_MONO_16,      NDColorModeMono,  NDUInt16, 0           },
    { ARV_PIXEL_FORMAT_BAYER_GR_16,  NDColorModeBayer, NDUInt16, NDBayerGRBG },
    { ARV_PIXEL_FORMAT_BAYER_RG_16,  NDColorModeBayer, NDUInt8,  NDBayerRGGB },
    { ARV_PIXEL_FORMAT_BAYER_GB_16,  NDColorModeBayer, NDUInt8,  NDBayerGBRG },
    { ARV_PIXEL_FORMAT_BAYER_BG_16,  NDColorModeBayer, NDUInt8,  NDBayerBGGR }	
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
	for (int i = 0; i < N; i ++)
		if (colorMode   == pix_lookup[i].colorMode &&
			dataType    == pix_lookup[i].dataType &&
			bayerFormat == pix_lookup[i].bayerFormat) {			
			*fmt = pix_lookup[i].fmt;
			return asynSuccess;
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
        return asynError;
    } else {
        return asynSuccess;
    }
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

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setIntegerParam(function, value);

    /* If we have no camera, then just fail */
    if (this->camera == NULL) {
        status = asynError;
    } else if (function == ADAcquire) {
        if (value) {
            /* This was a command to start acquisition */
            status = this->start();
        } else {
            /* This was a command to stop acquisition */
            arv_camera_stop_acquisition (this->camera);
        }
    } else if (function == ADBinX || function == ADBinY) {
        status = this->setBinning();
    } else if (function == ADMinX || function == ADMinY || function == ADSizeX || function == ADSizeY) {
        status = this->setRegion();
    } else if (function == ADReverseX || function == ADReverseY || function == ADFrameType) {
        /* not supported yet */
        status = asynError;
    } else if (function == ADTriggerMode) {
        /* need a lookup on value, but how to get possible strings? */
        status = asynError;
    } else if (function == ADNumExposures) {
        /* only one at the moment */
        if (value!=1) {
            setIntegerParam(ADNumExposures, 1);
            status = asynError;
        }
    } else if (function == NDDataType || function == NDColorMode) {
        status = this->setPixelFormat();
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_ARAVIS_CAMERA_PARAM) {
            status = ADDriver::writeInt32(pasynUser, value);
        }
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

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setDoubleParam(function, value);

    /* If we have no camera, then just fail */
    if (this->camera == NULL) {
        status = asynError;
    } else if (function == ADGain) {
        arv_camera_set_gain(this->camera, (gint64)value);
        rbv = arv_camera_get_gain(this->camera);
        if (fabs(rbv-value)>0.001) {
            setDoubleParam(function, rbv);
            status = asynError;
        }
    } else if (function == ADAcquireTime) {
        arv_camera_set_exposure_time(this->camera, value * 1000000);
        rbv = arv_camera_get_exposure_time(this->camera) / 1000000;
        if (fabs(rbv-value)>0.001) {
            setDoubleParam(function, rbv);
            status = asynError;
        }
    } else if (function == ADAcquirePeriod) {
        if (value > 0) {
            arv_camera_set_frame_rate(this->camera, 1 / value);
        }
        rbv = 1 / arv_camera_get_frame_rate(this->camera);
        if (fabs(rbv-value)>0.001) {
            setDoubleParam(function, rbv);
            status = asynError;
        }
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

    fprintf(fp, "Aravisulation detector %s\n", this->portName);
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
  * \param[in] maxSizeX The maximum X dimension of the images that this driver can create.
  * \param[in] maxSizeY The maximum Y dimension of the images that this driver can create.
  * \param[in] dataType The initial data type (NDDataType_t) of the images that this driver will create.
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
      pRaw(NULL)

{
    int status = asynSuccess;
    const char *functionName = "aravisCamera";
    int x,y,w,h,binx,biny;
    ArvPixelFormat fmt;
    int colorMode, dataType, bayerFormat=0;    
    
    /* Create some custom parameters */
    createParam(AravisCompletedString, asynParamFloat64, &AravisCompleted);
    createParam(AravisFailuresString,  asynParamFloat64, &AravisFailures);
    createParam(AravisUnderrunsString, asynParamFloat64, &AravisUnderruns);

	/* Connect to the camera */
    g_thread_init (NULL);
    g_type_init ();
    g_print ("Looking for camera '%s'\n", cameraName);
    this->camera = arv_camera_new (cameraName);
    if (this->camera == NULL) {
        printf ("No camera found\n");
        return;
    } else {
        this->stream = arv_camera_create_stream (this->camera, aravisCallback, (void *) this);
    }
	
    /* Set some default values for parameters */
    status =  setStringParam (ADManufacturer, arv_camera_get_vendor_name(this->camera));
    status |= setStringParam (ADModel, arv_camera_get_model_name(this->camera));
    status |= setDoubleParam(ADGain, arv_camera_get_gain(this->camera));    
    arv_camera_get_sensor_size(this->camera, &w, &h);
    status |= setIntegerParam(ADMaxSizeX, w);
    status |= setIntegerParam(ADMaxSizeY, h);
    arv_camera_get_region(this->camera, &x, &y, &w, &h);    
    status |= setIntegerParam(ADMinX, x);
    status |= setIntegerParam(ADMinY, y);
    status |= setIntegerParam(NDArraySizeX, w);
    status |= setIntegerParam(NDArraySizeY, h);    
    arv_camera_get_binning(this->camera, &binx, &biny);    
    status |= setIntegerParam(ADBinX, x);
    status |= setIntegerParam(ADBinY, y);    
    status |= setIntegerParam(ADSizeX, binx*w);
    status |= setIntegerParam(ADSizeY, biny*h);
    status |= setIntegerParam(ADReverseX, 0);
    status |= setIntegerParam(ADReverseY, 0);    
    status |= setIntegerParam(NDArraySize, x*y);
    status |= setIntegerParam(ADImageMode, ADImageContinuous);
    status |= setDoubleParam (ADAcquireTime, arv_camera_get_exposure_time(this->camera) / 1000000);
    status |= setDoubleParam (ADAcquirePeriod, 1/arv_camera_get_frame_rate(this->camera));
    status |= setIntegerParam(ADNumImages, 100);
    status |= setDoubleParam(AravisCompleted, 0);
    status |= setDoubleParam(AravisFailures, 0);
    status |= setDoubleParam(AravisUnderruns, 0);
    fmt = arv_camera_get_pixel_format(this->camera);
   	status |= this->lookupColorMode(fmt, &colorMode, &dataType, &bayerFormat);
    status |= setIntegerParam(NDColorMode, colorMode);
    status |= setIntegerParam(NDDataType, dataType);

    if (status) {
        printf("%s: unable to set camera parameters\n", functionName);
        return;
    }

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
