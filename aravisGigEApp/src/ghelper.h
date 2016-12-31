#ifndef GHELPER_H
#define GHELPER_H

#include <stdexcept>

extern "C" {
    #include <arv.h>
}

//! Smart-ish pointer using GObject ref counting
template<typename T>
struct GWrapper {
    GWrapper() :ptr(0) {}
    GWrapper(T* p) :ptr(p) {
        if(!p)
            throw std::bad_alloc();
    }
    GWrapper(const GWrapper& o) :ptr(o.ptr) {
        if(ptr) g_object_ref(ptr);
    }
    ~GWrapper() { reset(); }
    void reset(T* p=0) {
        if(ptr) g_object_unref(ptr);
        ptr = p;
    }
    GWrapper& operator=(const GWrapper& o) {
        if(ptr!=o.ptr) {
            if(o.ptr) g_object_ref(o.ptr);
            if(ptr)   g_object_unref(ptr);
            ptr = o.ptr;
        }
        return *this;
    }
    void swap(GWrapper& o) {
        std::swap(ptr, o.ptr);
    }
    T* get() const { return ptr; }
    operator T*() const { return ptr; }
    T* release() {
        T* ret = ptr;
        ptr = 0;
        return ret;
    }

private:
    T *ptr;
};

// Helper to ensure that GError is free'd
struct GErrorHelper {
    GError *err;
    GErrorHelper() :err(0) {}
    ~GErrorHelper() {
        if(err) g_error_free(err);
    }
    GError** get() {
        return &err;
    }
    operator GError*() const {
        return err;
    }
    GError* operator->() const {
        return err;
    }
};


#endif // GHELPER_H
