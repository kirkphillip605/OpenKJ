/* config.h. Generated for vendored TagLib build. */

#ifndef TAGLIB_CONFIG_H
#define TAGLIB_CONFIG_H

#if defined(__GNUC__) || defined(__clang__)
#define HAVE_GCC_BYTESWAP 1
#endif

#if defined(__GLIBC__)
#define HAVE_GLIBC_BYTESWAP 1
#endif

#if defined(_MSC_VER)
#define HAVE_MSC_BYTESWAP 1
#define HAVE_ISO_STRDUP 1
#endif

#if defined(__APPLE__)
#define HAVE_MAC_BYTESWAP 1
#endif

#endif
