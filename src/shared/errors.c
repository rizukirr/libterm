#include "libterm/libterm.h"

const char *lt_strerror(int code) {
    switch (code) {
        case LT_OK:                    return "ok";
        case LT_ERR:                   return "generic error";
        case LT_ERR_NEED_MORE:         return "need more input";
        case LT_ERR_INIT_ALREADY:      return "already initialized";
        case LT_ERR_INIT_OPEN:         return "failed to open terminal";
        case LT_ERR_MEM:               return "out of memory";
        case LT_ERR_NO_EVENT:          return "no event";
        case LT_ERR_NO_TERM:           return "no terminal";
        case LT_ERR_NOT_INIT:          return "not initialized";
        case LT_ERR_OUT_OF_BOUNDS:     return "coordinates out of bounds";
        case LT_ERR_READ:              return "read error";
        case LT_ERR_UNSUPPORTED_TERM:  return "unsupported terminal";
        default:                       return "unknown error";
    }
}
