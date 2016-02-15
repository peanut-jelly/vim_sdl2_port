#ifndef _NS_VIMPEANUT
#define _NS_VIMPEANUT 1

#ifndef ASSERT_INSIDE_NS_VIMPEANUT
#  define ASSERT_INSIDE_NS_VIMPEANUT() \
    do { \
        if (!_NS_VIMPEANUT) { \
            fprintf(stderr, \
                    "Assertion failed.\n" \
                    "assert we are inside namespace VimPeanut" \
                    "File: %s\n" \
                    "Line: %d\n" \
                    , __FILE__, __LINE__); \
            exit(10); \
        } \
    } while (0);

namespace VimPeanut {
    // end of the namespace is in end_NS_VimPeanut.h

#endif

