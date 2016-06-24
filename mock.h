#ifndef _MOCK_H_
#define _MOCK_H_

#include <stdarg.h>



#include "assert_out_ns_vim.h"
#include "begin_ns_vim.h"


#define Warn(msg) \
     do { \
        char _errMsg[200]; \
        snprintf(_errMsg, 200, "Warn:\n%s\nFile: %s\nLine: %d\nFunc:%s\n", \
                msg, __FILE__, __LINE__, __FUNCTION__); \
        MessageBox(s_hwnd,_errMsg, "Warn", MB_OK); \
    } while (0);

#define Error(msg) \
     do { \
        char _errMsg[200]; \
        snprintf(_errMsg, 200, "Error:\n%s\nFile: %s\nLine: %d\nFunc:%s\n", \
                msg, __FILE__, __LINE__, __FUNCTION__); \
        MessageBox(s_hwnd,_errMsg, "Error", MB_OK); \
        exit(13); \
    } while (0);
#define NotFinished() Error("Not Finished.")

extern void fnError(const char *msg);
extern void fnWarn(const char *msg);
extern void fnError2(const char *msg1, const char* msg2);
extern void fnWarn2(const char* msg1, const char* msg2);
extern void fnWarnf(const char* fmt, ...);
extern void fnErrorf(const char* fmt, ...);



#include "end_ns_vim.h"
#endif // _MOCK_H_

