#ifndef TESTHELPER_H
#define TESTHELPER_H

#include <epicsUnitTest.h>
#include <asynPortClient.h>

void setString(const char *name, const char *value)
{
    asynOctetClient C("DUT", -1, name);
    size_t actual = 0;
    asynStatus sts = C.write(value, strlen(value), &actual);
    testOk(sts==asynSuccess && actual==strlen(value), "Set %s -> \"%s\"", name, value);
}

void setInt(const char *name, epicsInt32 val) {
    asynInt32Client C("DUT", -1, name);
    testOk(C.write(val)==asynSuccess, "Set %s = %d", name, (int)val);
}

epicsInt32 readInt(const char *name) {
    asynInt32Client C("DUT", -1, name);
    epicsInt32 ret;
    int ok = C.read(&ret)==asynSuccess;
    if(!ok) testAbort("Failed to read %s", name);
    return ret;
}

void setFlt(const char *name, double val) {
    asynFloat64Client C("DUT", -1, name);
    testOk(C.write(val)==asynSuccess, "Set %s = %f", name, val);
}

void waitSyncd()
{
    epicsInt32 A = 0, B = 0;
    testDiag("wait for scanner thread to make one cycle");
    for(unsigned i=0; i<10; i++) {
        if(i==9)
            throw std::runtime_error("Timeout waiting for sync");

        B = readInt("ARAVIS_SYNCD");
        if(A!=0 && B!=0 && A!=B)
            break;
        A=B;
        epicsThreadSleep(1.0);
    }
    testDiag("scanner cycle complete");
}

void testInt(const char *name, epicsInt32 expect, bool defined=true)
{
    asynInt32Client C("DUT", -1, name);
    epicsInt32 ret;
    asynStatus sts = C.read(&ret);
    if(sts!=asynSuccess)
        testOk(!defined, "Get %s -> Error %d\n", name, (int)sts);
    else
        testOk(defined && ret==expect,
                "Get %s -> %d (actual %d)", name, (int)expect, (int)ret);
}

void testFlt(const char *name, double expect, bool defined=true)
{
    asynFloat64Client C("DUT", -1, name);
    double ret;
    asynStatus sts = C.read(&ret);
    if(sts!=asynSuccess)
        testOk(!defined, "Get %s -> Error %d\n", name, (int)sts);
    else
        testOk(defined && ret==expect,
                "Get %s -> %f (actual %f)", name, expect, ret);
}

void testString(const char *name, const char *expect, bool defined=true) {
    asynOctetClient C("DUT", -1, name);
    std::vector<char> buf(strlen(expect)+10);
    size_t act;
    asynStatus sts = C.read(&buf[0], buf.size(), &act, NULL);
    buf.back()='\0';
    if(sts!=asynSuccess)
        testOk(!defined, "Get %s -> Error %d\n", name, (int)sts);
    else
        testOk(defined && strcmp(expect, &buf[0])==0,
                "Get %s -> \"%s\" (actual \"%s\")", name, expect, &buf[0]);
}


#endif // TESTHELPER_H
