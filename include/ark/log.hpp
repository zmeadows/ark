#pragma once

#include <string.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <vector>

#include "ark/third_party/COLORS.h"

// by default, no logging enabled (except for critical)
#define ARK_LOG_EVERYTHING(MSG) \
    do {                        \
    } while (false)
#define ARK_LOG_VERBOSE(MSG) \
    do {                     \
    } while (false)
#define ARK_LOG_DEBUG(MSG) \
    do {                   \
    } while (false)
#define ARK_LOG_INFO(MSG) \
    do {                  \
    } while (false)
#define ARK_LOG_WARNING(MSG) \
    do {                     \
    } while (false)

#define __SHORT_FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// TODO: record all me
#ifndef ARK_LOG_DISABLE_COLOR
#define __DO_ARK_LOG(COLOR, label, MSG)                                                            \
    std::cout << COLOR("ark log (" label ")") << " " << __SHORT_FILENAME__ << "::" << __FUNCTION__ \
              << ":" << __LINE__ << " -> " << MSG << std::endl;
#else
#define __DO_ARK_LOG(COLOR, label, MSG)                                                                \
    std::cout << "ark log (" label ")"                                                                 \
              << " " << __SHORT_FILENAME__ << "::" << __FUNCTION__ << ":" << __LINE__ << " -> " << MSG \
              << std::endl;
#endif

// enable different log level depending on user-defined preference
#ifdef ARK_LOG_LEVEL_EVERYTHING
#undef ARK_LOG_EVERYTHING
#undef ARK_LOG_VERBOSE
#undef ARK_LOG_DEBUG
#undef ARK_LOG_INFO
#undef ARK_LOG_WARNING
#define ARK_LOG_EVERYTHING(MSG) __DO_ARK_LOG(YEL, "everything", MSG)
#define ARK_LOG_VERBOSE(MSG) __DO_ARK_LOG(CYN, "verbose", MSG)
#define ARK_LOG_DEBUG(MSG) __DO_ARK_LOG(GRN, "debug", MSG)
#define ARK_LOG_INFO(MSG) __DO_ARK_LOG(BLU, "info", MSG)
#define ARK_LOG_WARNING(MSG) __DO_ARK_LOG(MAG, "warning", MSG)
#endif

#ifdef ARK_LOG_LEVEL_VERBOSE
#undef ARK_LOG_VERBOSE
#undef ARK_LOG_DEBUG
#undef ARK_LOG_INFO
#undef ARK_LOG_WARNING
#define ARK_LOG_VERBOSE(MSG) __DO_ARK_LOG(CYN, "verbose", MSG)
#define ARK_LOG_DEBUG(MSG) __DO_ARK_LOG(GRN, "debug", MSG)
#define ARK_LOG_INFO(MSG) __DO_ARK_LOG(BLU, "info", MSG)
#define ARK_LOG_WARNING(MSG) __DO_ARK_LOG(MAG, "warning", MSG)
#endif

#ifdef ARK_LOG_LEVEL_DEBUG
#undef ARK_LOG_DEBUG
#undef ARK_LOG_INFO
#undef ARK_LOG_WARNING
#define ARK_LOG_DEBUG(MSG) __DO_ARK_LOG(GRN, "debug", MSG)
#define ARK_LOG_INFO(MSG) __DO_ARK_LOG(BLU, "info", MSG)
#define ARK_LOG_WARNING(MSG) __DO_ARK_LOG(MAG, "warning", MSG)
#endif

#ifdef ARK_LOG_LEVEL_INFO
#undef ARK_LOG_INFO
#undef ARK_LOG_WARNING
#define ARK_LOG_INFO(MSG) __DO_ARK_LOG(BLU, "info", MSG)
#define ARK_LOG_WARNING(MSG) __DO_ARK_LOG(MAG, "warning", MSG)
#endif

#ifdef ARK_LOG_LEVEL_WARNING
#undef ARK_LOG_WARNING
#define ARK_LOG_WARNING(MSG) __DO_ARK_LOG(MAG, "warning", MSG)
#endif

// always enable critical log messages
#define ARK_LOG_CRITICAL(MSG) __DO_ARK_LOG(RED, "CRITICAL", MSG)
