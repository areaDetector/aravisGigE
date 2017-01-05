// Test (re)connection and feature <-> param synchronization
#include <string>
#include <vector>
#include <sstream>

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
#include "testhelper.h"

// paramErrors.h isn't installed :(
#define asynParamAlreadyExists (asynStatus)(asynDisabled + 1)
#define asynParamNotFound      (asynStatus)(asynDisabled + 2)
#define asynParamWrongType     (asynStatus)(asynDisabled + 3)
#define asynParamBadIndex      (asynStatus)(asynDisabled + 4)
#define asynParamUndefined     (asynStatus)(asynDisabled + 5)

extern "C" {
int asynSetTraceMask(const char *portName,int addr,int mask);

extern REGISTRAR EPICS_EXPORT_PFUNC(aravisCameraRegister);

// must match definition in aravisCamera.cpp
int aravisCameraConfig(const char *portName, const char *cameraName,
                                 int maxBuffers, size_t maxMemory, int priority, int stackSize);
} // extern "C"

namespace {

GWrapper<ArvCamera> cam;
ArvDevice *dev;

void testFeature(const char *name, const char *expect)
{
    ArvGcNode *node = arv_gc_get_node(arv_device_get_genicam(dev), name);
    if(!node) {
        testFail("Get Feature %s which isn't a feature", name);
        return;
    }
    GErrorHelper err;
    std::ostringstream actual;
    if(ARV_IS_GC_INTEGER(node)) {
        guint64 val = arv_gc_integer_get_value(ARV_GC_INTEGER(node), err.get());
        actual << val;
    } else if(ARV_IS_GC_FLOAT(node)) {
        double val = arv_gc_float_get_value(ARV_GC_FLOAT(node), err.get());
        actual << val;
    } else if(ARV_IS_GC_STRING(node)) {
        const char* val = arv_gc_string_get_value(ARV_GC_STRING(node), err.get());
        actual << val;
    } else {
        testFail("Get Feature %s has unsupported type\n", name);
        return;
    }
    std::string act(actual.str());

    if(err)
        testFail("Get Feature %s -> Error: %s", name, err->message);
    else
        testOk(strcmp(act.c_str(), expect)==0, "Get Feature %s -> \"%s\" (actual \"%s\")",
               name, expect, act.c_str());
}

void setFeature(const char *name, const char *value)
{
    ArvGcNode *node = arv_gc_get_node(arv_device_get_genicam(dev), name);
    GErrorHelper err;
    arv_gc_feature_node_set_value_from_string(ARV_GC_FEATURE_NODE(node), value, err.get());
    if(err)
        testFail("Set Feature %s <- %s Error: %s", name, value, err->message);
    else
        testPass("Set Feature %s <- %s", name, value);
}

void prepareCam()
{
    testDiag("Prepare camera");

    setFeature("TestRegister", "40");
    setFeature("GainRaw", "10");
    setFeature("ExposureTimeAbs", "30");

    testFeature("TestRegister", "40");
    testFeature("GainRaw", "10");
    testFeature("ExposureTimeAbs", "30");
}

void setupParams()
{
    testDiag("lazy create of feature parameters");

    setFlt(ADAcquireTimeString, 10e-6);

    setFlt(ADGainString, 2.0);

    testInt("ARVI_TestRegister", 0, false); // a read-back (we will not set)
    testString(ADManufacturerString, "", false);
}

void postConnect()
{
    testDiag("Check post connect, after initial sync");

    testString(ADManufacturerString, "Aravis");

    // our settings should be pushed to camera
    testFlt(ADGainString, 2.0);
    testFeature("GainRaw", "2");

    testFlt(ADAcquireTimeString, 10e-6);
    testFeature("ExposureTimeAbs", "10");

    // readback params should be updated from camera
    testInt("ARVI_TestRegister", 0x28);
    testFeature("TestRegister", "40");

}

} // namespace

MAIN(testcam)
{
    testPlan(36);
    try {

        EPICS_EXPORT_PFUNC(aravisCameraRegister)();

        cam.reset(arv_camera_new("Aravis-GV01"));
        if(!cam)
            testAbort("Can't get direct connection to camera");
        dev = arv_camera_get_device(cam);

        prepareCam();

        dev = NULL;
        cam.reset();

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

        testDiag("Second connection directly to camera");
        cam.reset(arv_camera_new("Aravis-GV01"));
        if(!cam)
            testAbort("Can't get direct connection to camera");
        dev = arv_camera_get_device(cam);

        postConnect();

        testDiag("Disconnnect all");
        dev = NULL;
        cam.reset();

        setString("ARAVIS_CAMNAME", "invalid");
        testDiag("Wait for disconnect");
        for(unsigned i=0; i<10; i++) {
            if(i==9)
                testAbort("No disconnect");
            else if(readInt("ARAVIS_CONNECTION")==0)
                break;
            epicsThreadSleep(1.0);
        }
        testDiag("Disconnected");

        setupParams();

        cam.reset(arv_camera_new("Aravis-GV01"));
        if(!cam)
            testAbort("Can't get direct connection to camera");
        dev = arv_camera_get_device(cam);

        prepareCam();

        dev = NULL;
        cam.reset();

        testDiag("Start re-connect");
        setString("ARAVIS_CAMNAME", "Aravis-GV01");

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

        testDiag("Second connection directly to camera");
        cam.reset(arv_camera_new("Aravis-GV01"));
        if(!cam)
            testAbort("Can't get direct connection to camera");
        dev = arv_camera_get_device(cam);

        postConnect();

    } catch(std::exception& e) {
        testAbort("Unexpected exception: %s", e.what());
    }
    cam.reset();

    testDiag("Exit");
    epicsExit(testDone());
    return 2; // not reached
}
