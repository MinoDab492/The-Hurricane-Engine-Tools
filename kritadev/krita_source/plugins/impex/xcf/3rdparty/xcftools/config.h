#ifndef _WIN32
#define HAVE_NETINET_IN_H 1
#endif

#ifdef _MSC_VER
#if _MSC_VER < 1800
#define HAVE_INTTYPES_H 0
#else
#   define HAVE_INTTYPES_H 1
#endif
#else
#define HAVE_INTTYPES_H 1
#endif

#define PACKAGE_STRING "krita"
