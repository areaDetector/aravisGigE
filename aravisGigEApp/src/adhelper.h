#ifndef ADHELPER_H
#define ADHELPER_H

/* areaDetector includes */
#include <ADDriver.h>

struct NDArrayPtr {
    NDArray *arr;
    NDArrayPtr() :arr(0) {}
    explicit NDArrayPtr(NDArray* arr) :arr(arr) {
        if(!arr)
            throw std::runtime_error("Failed to allocate NDArray");
    }
    NDArrayPtr(const NDArrayPtr& o) :arr(o.arr) {
        if(arr) arr->reserve();
    }
    ~NDArrayPtr() { reset(); }
    void reset(NDArray *p=0) {
        if(arr) arr->release();
        arr = p;
    }
    NDArray& operator*() const { return *arr; }
    NDArray* operator->() const { return arr; }
    NDArray* get() const { return arr; }
    NDArray* release() {
        NDArray *ret = arr;
        arr = 0;
        return ret;
    }
    void swap(NDArrayPtr& o) {
        std::swap(arr, o.arr);
    }
};

#endif // ADHELPER_H
