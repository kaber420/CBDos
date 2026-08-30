#pragma once
#include "../ConfigHelix.h"
#include "cbdos/log.hpp"

// Logging Implementation
#if HELIX_LOGGING_ACTIVE
    #define TAG_HELIX "libhelix"
    #define LOGD_HELIX(...) CBD_LOG_D(TAG_HELIX,__VA_ARGS__);
    #define LOGI_HELIX(...) CBD_LOG_I(TAG_HELIX,__VA_ARGS__);
    #define LOGW_HELIX(...) CBD_LOG_W(TAG_HELIX,__VA_ARGS__);
    #define LOGE_HELIX(...) CBD_LOG_E(TAG_HELIX,__VA_ARGS__);
#else
    // Remove all log statments from the code
    #define LOGD_HELIX(...) 
    #define LOGI_HELIX(...) 
    #define LOGW_HELIX(...) 
    #define LOGE_HELIX(...) 
#endif

