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
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

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
    virtual void setShutter(int open);
    void report(FILE *fp, int details);
    void aravisTask(); /**< Should be private, but gets called from C, so must be public */
    
protected:
    int AravisGainX;
    #define FIRST_ARAVIS_CAMERA_PARAM AravisGainX
    int AravisGainY;
    int AravisGainRed;
    int AravisGainGreen;
    int AravisGainBlue;
    int AravisResetImage;
    #define LAST_ARAVIS_CAMERA_PARAM AravisResetImage

private:                                        
    /* These are the methods that are new to this class */
    template <typename epicsType> int computeArray(int sizeX, int sizeY);
    int computeImage();

    /* Our data */
    epicsEventId startEventId;
    epicsEventId stopEventId;
    NDArray *pRaw;
	ArvCamera *camera;
	ArvStream *stream;
	ArvBuffer *buffer;    
};

#define AravisGainXString          "SIM_GAIN_X"
#define AravisGainYString          "SIM_GAIN_Y" 
#define AravisGainRedString        "SIM_GAIN_RED" 
#define AravisGainGreenString      "SIM_GAIN_GREEN"  
#define AravisGainBlueString       "SIM_GAIN_BLUE"  
#define AravisResetImageString     "RESET_IMAGE"


#define NUM_ARAVIS_CAMERA_PARAMS (&LAST_ARAVIS_CAMERA_PARAM - &FIRST_ARAVIS_CAMERA_PARAM + 1)

/** Template function to compute the GigE detector data for any data type */
template <typename epicsType> int aravisCamera::computeArray(int sizeX, int sizeY)
{
    epicsType *pMono=NULL, *pRed=NULL, *pGreen=NULL, *pBlue=NULL;
    int columnStep=0, rowStep=0, colorMode;
    epicsType incMono, incRed, incGreen, incBlue;
    int status = asynSuccess;
    double exposureTime, gain, gainX, gainY, gainRed, gainGreen, gainBlue;
    int resetImage;
    int i, j;

    status = getDoubleParam (ADGain,        &gain);
    status = getDoubleParam (AravisGainX,      &gainX);
    status = getDoubleParam (AravisGainY,      &gainY);
    status = getDoubleParam (AravisGainRed,    &gainRed);
    status = getDoubleParam (AravisGainGreen,  &gainGreen);
    status = getDoubleParam (AravisGainBlue,   &gainBlue);
    status = getIntegerParam(AravisResetImage, &resetImage);
    status = getIntegerParam(NDColorMode,   &colorMode);
    status = getDoubleParam (ADAcquireTime, &exposureTime);

    /* The intensity at each pixel[i,j] is:
     * (i * gainX + j* gainY) + imageCounter * gain * exposureTime * 1000. */
    incMono  = (epicsType) (gain      * exposureTime * 1000.);
    incRed   = (epicsType) gainRed   * incMono;
    incGreen = (epicsType) gainGreen * incMono;
    incBlue  = (epicsType) gainBlue  * incMono;
    
    switch (colorMode) {
        case NDColorModeMono:
            pMono = (epicsType *)this->pRaw->pData;
            break;
        case NDColorModeRGB1:
            columnStep = 3;
            rowStep = 0;
            pRed   = (epicsType *)this->pRaw->pData;
            pGreen = (epicsType *)this->pRaw->pData+1;
            pBlue  = (epicsType *)this->pRaw->pData+2;
            break;
        case NDColorModeRGB2:
            columnStep = 1;
            rowStep = 2 * sizeX;
            pRed   = (epicsType *)this->pRaw->pData;
            pGreen = (epicsType *)this->pRaw->pData + sizeX;
            pBlue  = (epicsType *)this->pRaw->pData + 2*sizeX;
            break;
        case NDColorModeRGB3:
            columnStep = 1;
            rowStep = 0;
            pRed   = (epicsType *)this->pRaw->pData;
            pGreen = (epicsType *)this->pRaw->pData + sizeX*sizeY;
            pBlue  = (epicsType *)this->pRaw->pData + 2*sizeX*sizeY;
            break;
    }
    this->pRaw->pAttributeList->add("ColorMode", "Color mode", NDAttrInt32, &colorMode);

    if (resetImage) {
        for (i=0; i<sizeY; i++) {
            switch (colorMode) {
                case NDColorModeMono:
                    for (j=0; j<sizeX; j++) {
                        (*pMono++) = (epicsType) (incMono * (gainX*j + gainY*i));
                    }
                    break;
                case NDColorModeRGB1:
                case NDColorModeRGB2:
                case NDColorModeRGB3:
                    for (j=0; j<sizeX; j++) {
                        *pRed   = (epicsType) (incRed   * (gainX*j + gainY*i));
                        *pGreen = (epicsType) (incGreen * (gainX*j + gainY*i));
                        *pBlue  = (epicsType) (incBlue  * (gainX*j + gainY*i));
                        pRed   += columnStep;
                        pGreen += columnStep;
                        pBlue  += columnStep;
                    }
                    pRed   += rowStep;
                    pGreen += rowStep;
                    pBlue  += rowStep;
                    break;
            }
        }
    } else {
        for (i=0; i<sizeY; i++) {
            switch (colorMode) {
                case NDColorModeMono:
                    for (j=0; j<sizeX; j++) {
                            *pMono++ += incMono;
                    }
                    break;
                case NDColorModeRGB1:
                case NDColorModeRGB2:
                case NDColorModeRGB3:
                    for (j=0; j<sizeX; j++) {
                        *pRed   += incRed;
                        *pGreen += incGreen;
                        *pBlue  += incBlue;
                        pRed   += columnStep;
                        pGreen += columnStep;
                        pBlue  += columnStep;
                    }
                    pRed   += rowStep;
                    pGreen += rowStep;
                    pBlue  += rowStep;
                    break;
            }
        }
    }
    return(status);
}

/** Controls the shutter */
void aravisCamera::setShutter(int open)
{
    int shutterMode;
    
    getIntegerParam(ADShutterMode, &shutterMode);
    if (shutterMode == ADShutterModeDetector) {
        /* Aravisulate a shutter by just changing the status readback */
        setIntegerParam(ADShutterStatus, open);
    } else {
        /* For no shutter or EPICS shutter call the base class method */
        ADDriver::setShutter(open);
    }
}

/** Computes the new image data */
int aravisCamera::computeImage()
{
    int status = asynSuccess;
    NDDataType_t dataType;
    int binX, binY, minX, minY, sizeX, sizeY, reverseX, reverseY;
    int xDim=0, yDim=1, colorDim=-1;
    int resetImage;
    int maxSizeX, maxSizeY;
    int colorMode;
    int ndims=0;
    NDDimension_t dimsOut[3];
    int dims[3];
    NDArrayInfo_t arrayInfo;
    NDArray *pImage;
    const char* functionName = "computeImage";

    /* NOTE: The caller of this function must have taken the mutex */
    
    status |= getIntegerParam(ADBinX,         &binX);
    status |= getIntegerParam(ADBinY,         &binY);
    status |= getIntegerParam(ADMinX,         &minX);
    status |= getIntegerParam(ADMinY,         &minY);
    status |= getIntegerParam(ADSizeX,        &sizeX);
    status |= getIntegerParam(ADSizeY,        &sizeY);
    status |= getIntegerParam(ADReverseX,     &reverseX);
    status |= getIntegerParam(ADReverseY,     &reverseY);
    status |= getIntegerParam(ADMaxSizeX,     &maxSizeX);
    status |= getIntegerParam(ADMaxSizeY,     &maxSizeY);
    status |= getIntegerParam(NDColorMode,    &colorMode);
    status |= getIntegerParam(NDDataType,     (int *)&dataType);
    status |= getIntegerParam(AravisResetImage,  &resetImage);
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error getting parameters\n",
                    driverName, functionName);

    /* Make sure parameters are consistent, fix them if they are not */
    if (binX < 1) {
        binX = 1; 
        status |= setIntegerParam(ADBinX, binX);
    }
    if (binY < 1) {
        binY = 1;
        status |= setIntegerParam(ADBinY, binY);
    }
    if (minX < 0) {
        minX = 0; 
        status |= setIntegerParam(ADMinX, minX);
    }
    if (minY < 0) {
        minY = 0; 
        status |= setIntegerParam(ADMinY, minY);
    }
    if (minX > maxSizeX-1) {
        minX = maxSizeX-1; 
        status |= setIntegerParam(ADMinX, minX);
    }
    if (minY > maxSizeY-1) {
        minY = maxSizeY-1; 
        status |= setIntegerParam(ADMinY, minY);
    }
    if (minX+sizeX > maxSizeX) {
        sizeX = maxSizeX-minX; 
        status |= setIntegerParam(ADSizeX, sizeX);
    }
    if (minY+sizeY > maxSizeY) {
        sizeY = maxSizeY-minY; 
        status |= setIntegerParam(ADSizeY, sizeY);
    }

    switch (colorMode) {
        case NDColorModeMono:
            ndims = 2;
            xDim = 0;
            yDim = 1;
            break;
        case NDColorModeRGB1:
            ndims = 3;
            colorDim = 0;
            xDim     = 1;
            yDim     = 2;
            break;
        case NDColorModeRGB2:
            ndims = 3;
            colorDim = 1;
            xDim     = 0;
            yDim     = 2;
            break;
        case NDColorModeRGB3:
            ndims = 3;
            colorDim = 2;
            xDim     = 0;
            yDim     = 1;
            break;
    }

    if (resetImage) {
    /* Free the previous raw buffer */
        if (this->pRaw) this->pRaw->release();
        /* Allocate the raw buffer we use to compute images. */
        dims[xDim] = maxSizeX;
        dims[yDim] = maxSizeY;
        if (ndims > 2) dims[colorDim] = 3;
        this->pRaw = this->pNDArrayPool->alloc(ndims, dims, dataType, 0, NULL);

        if (!this->pRaw) {
            asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s:%s: error allocating raw buffer\n",
                      driverName, functionName);
            return(status);
        }
    }
    
    switch (dataType) {
        case NDInt8: 
            status |= computeArray<epicsInt8>(maxSizeX, maxSizeY);
            break;
        case NDUInt8: 
            status |= computeArray<epicsUInt8>(maxSizeX, maxSizeY);
            break;
        case NDInt16: 
            status |= computeArray<epicsInt16>(maxSizeX, maxSizeY);
            break;
        case NDUInt16: 
            status |= computeArray<epicsUInt16>(maxSizeX, maxSizeY);
            break;
        case NDInt32: 
            status |= computeArray<epicsInt32>(maxSizeX, maxSizeY);
            break;
        case NDUInt32: 
            status |= computeArray<epicsUInt32>(maxSizeX, maxSizeY);
            break;
        case NDFloat32: 
            status |= computeArray<epicsFloat32>(maxSizeX, maxSizeY);
            break;
        case NDFloat64: 
            status |= computeArray<epicsFloat64>(maxSizeX, maxSizeY);
            break;
    }
    
    /* Extract the region of interest with binning.  
     * If the entire image is being used (no ROI or binning) that's OK because
     * convertImage detects that case and is very efficient */
    this->pRaw->initDimension(&dimsOut[xDim], sizeX);
    this->pRaw->initDimension(&dimsOut[yDim], sizeY);
    if (ndims > 2) this->pRaw->initDimension(&dimsOut[colorDim], 3);
    dimsOut[xDim].binning = binX;
    dimsOut[xDim].offset  = minX;
    dimsOut[xDim].reverse = reverseX;
    dimsOut[yDim].binning = binY;
    dimsOut[yDim].offset  = minY;
    dimsOut[yDim].reverse = reverseY;
    /* We save the most recent image buffer so it can be used in the read() function.
     * Now release it before getting a new version. */
    if (this->pArrays[0]) this->pArrays[0]->release();
    status = this->pNDArrayPool->convert(this->pRaw,
                                         &this->pArrays[0],
                                         dataType,
                                         dimsOut);
    if (status) {
        asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error allocating buffer in convert()\n",
                    driverName, functionName);
        return(status);
    }
    pImage = this->pArrays[0];
    pImage->getInfo(&arrayInfo);
    status = asynSuccess;
    status |= setIntegerParam(NDArraySize,  arrayInfo.totalBytes);
    status |= setIntegerParam(NDArraySizeX, pImage->dims[xDim].size);
    status |= setIntegerParam(NDArraySizeY, pImage->dims[yDim].size);
    status |= setIntegerParam(AravisResetImage, 0);
    if (status) asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s:%s: error setting parameters\n",
                    driverName, functionName);
    return(status);
}

static void aravisTaskC(void *drvPvt)
{
    aravisCamera *pPvt = (aravisCamera *)drvPvt;    
    pPvt->aravisTask();
}

/** This thread calls computeImage to compute new image data and does the callbacks to send it to higher layers.
  * It implements the logic for single, multiple or continuous acquisition. */
void aravisCamera::aravisTask()
{
    int status = asynSuccess;
    int imageCounter;
    int numImages, numImagesCounter;
    int imageMode;
    int arrayCallbacks;
    int acquire;
    double acquireTime, acquirePeriod, delay;
    epicsTimeStamp startTime, endTime;
    double elapsedTime;
    const char *functionName = "aravisTask";
    int xDim=0, yDim=1, colorDim=-1;
    int ndims=2;
    int dims[3];

    this->lock();
    /* Loop forever */
    while (1) {
        /* Is acquisition active? */
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are not acquiring then wait for a semaphore that is given when acquisition is started */
        if (!acquire) {
            setIntegerParam(ADStatus, ADStatusIdle);
            callParamCallbacks();
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: waiting for acquire to start\n", driverName, functionName);
            this->unlock();
            status = epicsEventWait(this->startEventId);
            this->lock();
            setIntegerParam(ADNumImagesCounter, 0);
			arv_camera_start_acquisition (this->camera);            
        }
        
        /* We are acquiring. */
        /* Get the current time */
        epicsTimeGetCurrent(&startTime);
        
        /* Get the exposure parameters */
        getDoubleParam(ADAcquireTime, &acquireTime);
        getDoubleParam(ADAcquirePeriod, &acquirePeriod);
        
        setIntegerParam(ADStatus, ADStatusAcquire);
        
        /* Open the shutter */
        setShutter(ADShutterOpen);

        /* Call the callbacks to update any changes */
        callParamCallbacks();

		/* Get the latest image from the camera */
		buffer = arv_stream_pop_buffer (stream);
		if (buffer != NULL) {
			if (buffer->status == ARV_BUFFER_STATUS_SUCCESS) {
            	/* Free the previous raw buffer */
                if (this->pRaw) this->pRaw->release();
                /* Allocate the raw buffer we use to compute images. */
                dims[xDim] = buffer->width;
                dims[yDim] = buffer->height;
                if (ndims > 2) dims[colorDim] = 3;
                this->pRaw = this->pNDArrayPool->alloc(ndims, dims, NDInt8, 0, NULL);
                if (!this->pRaw) {
                    asynPrint(this->pasynUserSelf, ASYN_TRACE_ERROR,
                              "%s:%s: error allocating raw buffer\n",
                              driverName, functionName);
                    continue;
                }
                memcpy(this->pRaw->pData, buffer->data, dims[xDim] * dims[yDim]);
			}
			arv_stream_push_buffer (stream, buffer);
		} else {
			/* wait for next frame ********HACK************/
            this->unlock();
            status = epicsEventWaitWithTimeout(this->stopEventId, acquireTime);
            this->lock();
            continue;
        }
        
        /* Close the shutter */
        setShutter(ADShutterClosed);
        setIntegerParam(ADStatus, ADStatusReadout);
        /* Call the callbacks to update any changes */
        callParamCallbacks();
        
        /* Get the current parameters */
        getIntegerParam(NDArrayCounter, &imageCounter);
        getIntegerParam(ADNumImages, &numImages);
        getIntegerParam(ADNumImagesCounter, &numImagesCounter);
        getIntegerParam(ADImageMode, &imageMode);
        getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
        imageCounter++;
        numImagesCounter++;
        setIntegerParam(NDArrayCounter, imageCounter);
        setIntegerParam(ADNumImagesCounter, numImagesCounter);
        
        /* Put the frame number and time stamp into the buffer */
        this->pRaw->uniqueId = imageCounter;
        this->pRaw->timeStamp = startTime.secPastEpoch + startTime.nsec / 1.e9;

        /* Get any attributes that have been defined for this driver */        
        this->getAttributes(this->pRaw->pAttributeList);

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
        }
        
        /* Call the callbacks to update any changes */
        callParamCallbacks();
        getIntegerParam(ADAcquire, &acquire);
        
        /* If we are acquiring then sleep for the acquire period minus elapsed time. */
        if (acquire) {
            epicsTimeGetCurrent(&endTime);
            elapsedTime = epicsTimeDiffInSeconds(&endTime, &startTime);
            delay = acquirePeriod - elapsedTime;
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
                     "%s:%s: delay=%f\n",
                      driverName, functionName, delay);            
            if (delay >= 0.0) {
                /* We set the status to readOut to indicate we are in the period delay */
                setIntegerParam(ADStatus, ADStatusWaiting);
                callParamCallbacks();
                this->unlock();
                status = epicsEventWaitWithTimeout(this->stopEventId, delay);
                this->lock();
            }
        }
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
    int adstatus;
    asynStatus status = asynSuccess;

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setIntegerParam(function, value);

    /* For a real detector this is where the parameter is sent to the hardware */
    if (function == ADAcquire) {
        getIntegerParam(ADStatus, &adstatus);
        if (value && (adstatus == ADStatusIdle)) {
            /* Send an event to wake up the aravisulation task.  
             * It won't actually start generating new images until we release the lock below */
            epicsEventSignal(this->startEventId);
        } 
        if (!value && (adstatus != ADStatusIdle)) {
            /* This was a command to stop acquisition */
            /* Send the stop event */
			arv_camera_stop_acquisition (this->camera);                        
            epicsEventSignal(this->stopEventId);
        }
    } else if ((function == NDDataType) || (function == NDColorMode)) {
        status = setIntegerParam(AravisResetImage, 1);
    } else if (function == ADShutterControl) {
        setShutter(value);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_ARAVIS_CAMERA_PARAM) status = ADDriver::writeInt32(pasynUser, value);
    }
    
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    
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
    asynStatus status = asynSuccess;

    /* Set the parameter and readback in the parameter library.  This may be overwritten when we read back the
     * status at the end, but that's OK */
    status = setDoubleParam(function, value);

    /* Changing any of the following parameters requires recomputing the base image */
    if ((function == ADAcquireTime) ||
        (function == ADGain) ||
        (function == AravisGainX) ||
        (function == AravisGainY) ||
        (function == AravisGainRed) ||
        (function == AravisGainGreen) ||
        (function == AravisGainBlue)) {
            status = setIntegerParam(AravisResetImage, 1);
    } else {
        /* If this parameter belongs to a base class call its method */
        if (function < FIRST_ARAVIS_CAMERA_PARAM) status = ADDriver::writeFloat64(pasynUser, value);
    }

    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    if (status) 
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
              "%s:writeFloat64 error, status=%d function=%d, value=%f\n", 
              driverName, status, function, value);
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

    /* Create the epicsEvents for signaling to the aravisulate task when acquisition starts and stops */
    this->startEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->startEventId) {
        printf("%s:%s epicsEventCreate failure for start event\n", 
            driverName, functionName);
        return;
    }
    this->stopEventId = epicsEventCreate(epicsEventEmpty);
    if (!this->stopEventId) {
        printf("%s:%s epicsEventCreate failure for stop event\n", 
            driverName, functionName);
        return;
    }
    
    createParam(AravisGainXString,      asynParamFloat64, &AravisGainX);
    createParam(AravisGainYString,      asynParamFloat64, &AravisGainY);
    createParam(AravisGainRedString,    asynParamFloat64, &AravisGainRed);
    createParam(AravisGainGreenString,  asynParamFloat64, &AravisGainGreen);
    createParam(AravisGainBlueString,   asynParamFloat64, &AravisGainBlue);
    createParam(AravisResetImageString, asynParamInt32,   &AravisResetImage);

    /* Set some default values for parameters */
    status =  setStringParam (ADManufacturer, "Aravisulated detector");
    status |= setStringParam (ADModel, "Basic aravisulator");
    status |= setIntegerParam(ADMaxSizeX, 0);
    status |= setIntegerParam(ADMaxSizeY, 0);
    status |= setIntegerParam(ADSizeX, 0);
    status |= setIntegerParam(ADSizeX, 0);
    status |= setIntegerParam(ADSizeY, 0);
    status |= setIntegerParam(NDArraySizeX, 0);
    status |= setIntegerParam(NDArraySizeY, 0);
    status |= setIntegerParam(NDArraySize, 0);
    status |= setIntegerParam(ADImageMode, ADImageContinuous);
    status |= setDoubleParam (ADAcquireTime, .001);
    status |= setDoubleParam (ADAcquirePeriod, .005);
    status |= setIntegerParam(ADNumImages, 100);
    status |= setIntegerParam(AravisResetImage, 1);
    status |= setDoubleParam (AravisGainX, 1);
    status |= setDoubleParam (AravisGainY, 1);
    status |= setDoubleParam (AravisGainRed, 1);
    status |= setDoubleParam (AravisGainGreen, 1);
    status |= setDoubleParam (AravisGainBlue, 1);
    if (status) {
        printf("%s: unable to set camera parameters\n", functionName);
        return;
    }
    
    /* Create the thread that updates the images */
    status = (epicsThreadCreate("AravisCameraTask",
                                epicsThreadPriorityMedium,
                                epicsThreadGetStackSize(epicsThreadStackMedium),
                                (EPICSTHREADFUNC)aravisTaskC,
                                this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for image task\n", 
            driverName, functionName);
        return;
    }

	GError *error = NULL;
	int i;

	g_thread_init (NULL);
	g_type_init ();

/*	arv_debug_enable (1);*/

	g_print ("Looking for camera '%s'\n", cameraName);


	this->camera = arv_camera_new (cameraName);
	if (this->camera != NULL) {
		gint payload;
		gint x, y, width, height;
		gint dx, dy;
		double exposure;
		guint64 n_completed_buffers;
		guint64 n_failures;
		guint64 n_underruns;
		int gain;

        arv_camera_set_pixel_format(this->camera, ARV_PIXEL_FORMAT_MONO_8);
        arv_camera_set_region (this->camera, 0, 0, 0, 0);
		arv_camera_set_binning (this->camera, 0, 0);
		arv_camera_set_exposure_time (this->camera, 0);
		arv_camera_set_gain (this->camera, 0);

		arv_camera_get_region (this->camera, &x, &y, &width, &height);
		arv_camera_get_binning (this->camera, &dx, &dy);
		exposure = arv_camera_get_exposure_time (this->camera);
		payload = arv_camera_get_payload (this->camera);
		gain = arv_camera_get_gain (this->camera);

		printf ("vendor name         = %s\n", arv_camera_get_vendor_name (this->camera));
		printf ("model name          = %s\n", arv_camera_get_model_name (this->camera));
		printf ("device id           = %s\n", arv_camera_get_device_id (this->camera));
		printf ("image width         = %d\n", width);
		printf ("image height        = %d\n", height);
		printf ("horizontal binning  = %d\n", dx);
		printf ("vertical binning    = %d\n", dy);
		printf ("exposure            = %g µs\n", exposure);
		printf ("gain                = %d dB\n", gain);

		this->stream = arv_camera_create_stream (this->camera, NULL, NULL);
/*		if (arv_option_auto_buffer)
			arv_gv_stream_set_option (ARV_GV_STREAM (this->stream),
						  ARV_GV_STREAM_OPTION_SOCKET_BUFFER_AUTO,
						  0);*/

		for (i = 0; i < 200; i++)
			arv_stream_push_buffer (this->stream, arv_buffer_new (payload, NULL));

		arv_camera_set_acquisition_mode (this->camera, ARV_ACQUISITION_MODE_CONTINUOUS);

/*		if (arv_option_frequency > 0.0)
			arv_camera_set_frame_rate (this->camera, arv_option_frequency);

		if (arv_option_trigger != NULL)
			arv_camera_set_trigger (this->camera, arv_option_trigger);*/


/*		arv_stream_get_statistics (this->stream, &n_completed_buffers, &n_failures, &n_underruns);

		printf ("Completed buffers = %Lu\n", (unsigned long long) n_completed_buffers);
		printf ("Failures          = %Lu\n", (unsigned long long) n_failures);
		printf ("Underruns         = %Lu\n", (unsigned long long) n_underruns);*/


	} else {
		printf ("No camera found\n");
	}
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
