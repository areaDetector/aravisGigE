#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include <stdio.h>
#include <string.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <ADDriver.h> /* for AD*String */

#include <epicsExit.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <initHooks.h>
#include <asynDriver.h>
#include <asynPortClient.h>
#include <shareLib.h>

#include "ghelper.h"
#include "adhelper.h"
#include "testhelper.h"

extern "C" {
int asynSetTraceMask(const char *portName,int addr,int mask);

extern REGISTRAR EPICS_EXPORT_PFUNC(aravisCameraRegister);

// must match definition in aravisCamera.cpp
int aravisCameraConfig(const char *portName, const char *cameraName,
                                 int maxBuffers, size_t maxMemory, int priority, int stackSize);
} // extern "C"

namespace {

struct NDImageListener : public asynPortClient
{
    NDImageListener(const char *portName, int addr, const char *drvInfo, double timeout=DEFAULT_TIMEOUT)
    : asynPortClient(portName, addr, asynGenericPointerType, drvInfo, timeout)
    {
        pInterface_ = (asynGenericPointer *)pasynInterface_->pinterface;
        if (pasynGenericPointerSyncIO->connect(portName, addr, &pasynUserSyncIO_, drvInfo))
            throw std::runtime_error("pasynGenericPointerSyncIO->connect failed");
        if (pInterface_->registerInterruptUser(pasynInterface_->drvPvt, pasynUser_,
                                                          &cb, this, &interruptPvt_))
            throw std::runtime_error("asynGenericPointer->registerInterruptUser failed");
    }
    ~NDImageListener()
    {
        pInterface_->cancelInterruptUser(pasynInterface_->drvPvt, pasynUser_, interruptPvt_);
        pasynGenericPointerSyncIO->disconnect(pasynUserSyncIO_);
    }

    virtual void newImage(NDArray *borrowed)
    {
        bool wake;
        {
            epicsGuard<epicsMutex> G(lock);
            wake = queue.empty();
            if(borrowed->reserve()) {
                asynPrint(pasynUser_, ASYN_TRACE_ERROR, "%s oh no!\n", __FUNCTION__);
            } else {
                queue.push_back(NDArrayPtr());
                queue.back().reset(borrowed);
            }
        }
        if(wake) {
            evt.signal();
        }
    }

    void tryWait(NDArrayPtr& ret, double timeout=-1) {
        ret.reset();
        epicsGuard<epicsMutex> G(lock);
        while(queue.empty()) {
            epicsGuardRelease<epicsMutex> U(G);
            if(timeout>=0 && !evt.wait(timeout)) {
                return; // timeout
            } else if(timeout<0) {
                evt.wait();
            }
        }
        queue.front().swap(ret);
        queue.pop_front();
    }

    static void cb(void *userPvt, asynUser *pasynUser,
                   void *pointer)
    {
        NDImageListener *self = (NDImageListener*)userPvt;
        try {
            self->newImage((NDArray*)pointer);
        } catch(std::exception& e) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR, "Unhandled exception in %s: %s\n",
                       __FUNCTION__, e.what());
        }
    }

    epicsMutex lock;
    epicsEvent evt;
    std::deque<NDArrayPtr> queue;

    asynGenericPointer *pInterface_;
};

void setupParams()
{
    setInt(NDArrayCallbacksString, 1);
    setInt(ADImageModeString, ADImageSingle);
    setFlt(ADAcquirePeriodString, 0.1); // 10 Hz

    setInt("ARAVIS_HWIMAGEMODE", 0);
}

void captureSingle()
{
    testDiag("Setup and capture a single frame");
    NDImageListener images("DUT", -1, NDArrayDataString);

    setInt(ADImageModeString, ADImageSingle);
    testDiag("Start acquire");
    setInt(ADAcquireString, 1);

    NDArrayPtr img;
    images.tryWait(img, 0.5);
    if(!img.get())
        testAbort("No frame");
    testPass("Have frame");

    {
        NDArrayPtr img2;
        images.tryWait(img2, 0.5);
        testOk(!img2.get(), "Single acquire should not give a second image (%p)", img2.get());
    }
}

void captureMultiple()
{
    const unsigned N = 5;
    testDiag("Setup and capture %u frames", N);

    NDImageListener images("DUT", -1, NDArrayDataString);

    setInt(ADImageModeString, ADImageMultiple);
    setInt(ADNumImagesString, N);

    testDiag("Start acquire");
    setInt(ADAcquireString, 1);

    // collect arrays to ensure they aren't reused
    std::vector<NDArrayPtr> img(N+1);

    unsigned i;
    for(i=0; i<img.size(); i++) {
        images.tryWait(img[i], 0.5);
        if(!img[i].get())
            break;
    }

    testOk(i==N, "Received %u images", i);

    bool pass = true;
    for(i=0; i<img.size(); i++) {
        for(unsigned j=i+1; j<img.size(); j++) {
            pass &= img[i].get()!=img[j].get();
            if(!pass)
                testDiag("buffer reused %u %u %p", i, j, img[i].get());
        }
    }
    testOk(pass, "No reuse of buffers still referenced");
}

void captureContinuous()
{
    // should be >=NRAW in aravisCamera.cpp
    // to make sure consumed buffers are being replaced
    const unsigned N = 25;

    testDiag("Setup and capture %u frames", N);

    NDImageListener images("DUT", -1, NDArrayDataString);

    setInt(ADImageModeString, ADImageContinuous);

    testDiag("Start acquire");
    setInt(ADAcquireString, 1);

    // collect arrays to ensure they aren't reused
    std::vector<NDArrayPtr> img(N+1);

    unsigned i;
    for(i=0; i<img.size(); i++) {
        images.tryWait(img[i], 0.5);
        if(!img[i].get())
            break;
    }

    testOk(i>=N, "Received %u images", i);

    bool pass = true;
    for(i=0; i<img.size(); i++) {
        for(unsigned j=i+1; j<img.size(); j++) {
            pass &= img[i].get()!=img[j].get();
            if(!pass)
                testDiag("buffer reused %u %u %p", i, j, img[i].get());
        }
    }
    testOk(pass, "No reuse of buffers still referenced");
}

} // namespace

MAIN(testimage)
{
    testPlan(0);

    EPICS_EXPORT_PFUNC(aravisCameraRegister)();

    testDiag("Create port");
    if(aravisCameraConfig("DUT", "Aravis-GV01", 0, 0, 0, 0)!=asynSuccess)
        testAbort("Unable to create port");

    asynSetTraceMask("DUT", -1, 0x3f);

    setupParams();

    initHookAnnounce(initHookAfterIocRunning);
    testDiag("Announced IocRunning");

    testDiag("Wait for connect");
    for(unsigned i=0; i<10; i++) {
        if(i==9)
            testAbort("No connect");
        else if(readInt("ARAVIS_CONNECTION")==1)
            break;
        epicsThreadSleep(1.0);
    }
    testDiag("Connected");
    waitSyncd();

    captureSingle();
    captureMultiple();
    captureContinuous();

    arv_shutdown();

    epicsExit(testDone());
    return 2; // not reached
}
