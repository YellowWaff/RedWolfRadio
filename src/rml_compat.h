// SPDX-License-Identifier: GPL-3.0-only
// ABI layout derived from Republic Mod Loader, revision
// 57e567e5cccaa3c38aee7e9d599d2443d2460b10, Source/runtime/tesmio_compat.cpp.
#pragma once
#include <stddef.h>
struct TsmPluginInfo { const char* name; const char* version; };
struct TsmHost {
    unsigned apiVersion, structSize;
    void* exeModule;
    unsigned char* exeBase;
    size_t exeSize;
    void* engineModule;
    const char* baseDir;
    const char* pluginDir;
    void (*log)(const char*, ...);
    void** (*findIatSlot)(void*, const char*, const char*);
    int (*patchIat)(void*, const char*, const char*, void*, void**, const char*);
    int (*installInlineHook)(void*, void*, void**, const unsigned char*, size_t, const char*);
    unsigned char* (*allocNear)(unsigned char*, size_t);
    int (*readablePtr)(const void*, size_t);
    long (*faultFilter)(const char*, void*);
    int (*configInt)(const char*, const char*, const char*, int);
    int (*configString)(const char*, const char*, const char*, char*, int, const char*);
    int (*provide)(const char*, unsigned, const void*);
    const void* (*consume)(const char*, unsigned);
    const char* vfsRoot;
};
static_assert(sizeof(TsmHost) == 0x98, "Republic compatibility ABI must be x64 API 4");
static_assert(offsetof(TsmHost, patchIat) == 0x48);
