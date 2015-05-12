#ifndef _BYTEORDER_H_
#define _BYTEORDER_H_

/*仅提供编译时的字节序支持*/
#define htobe8(x) (x)
#define htole8(x) (x)
#define be8toh(x) (x)
#define le8toh(x) (x)

#ifdef __GNUC__

#if defined(linux) || defined(__linux) || defined(__linux__)

#include <endian.h>

#ifndef htobe16
#include <byteswap.h>
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define htobe16(x) __bswap_16 (x)
#  define htole16(x) (x)
#  define be16toh(x) __bswap_16 (x)
#  define le16toh(x) (x)

#  define htobe32(x) __bswap_32 (x)
#  define htole32(x) (x)
#  define be32toh(x) __bswap_32 (x)
#  define le32toh(x) (x)

//#  if __GLIBC_HAVE_LONG_LONG
#   define htobe64(x) __bswap_64 (x)
#   define htole64(x) (x)
#   define be64toh(x) __bswap_64 (x)
#   define le64toh(x) (x)
//#  endif

# else
#  define htobe16(x) (x)
#  define htole16(x) __bswap_16 (x)
#  define be16toh(x) (x)
#  define le16toh(x) __bswap_16 (x)

#  define htobe32(x) (x)
#  define htole32(x) __bswap_32 (x)
#  define be32toh(x) (x)
#  define le32toh(x) __bswap_32 (x)

//#  if __GLIBC_HAVE_LONG_LONG
#   define htobe64(x) (x)
#   define htole64(x) __bswap_64 (x)
#   define be64toh(x) (x)
#   define le64toh(x) __bswap_64 (x)
//#  endif
# endif
#endif

#elif defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/endian.h>

#elif defined(__OpenBSD__)
#include <sys/types.h>
#define be16toh(x) betoh16(x)
#define le16toh(x) letoh16(x)

#define be32toh(x) betoh32(x)
#define le32toh(x) letoh32(x)

#define be64toh(x) betoh64(x)
#define le64toh(x) letoh64(x)

#elif defined(__APPLE__) && defined(__MACH__) || defined(_AIX)
#include <sys/types.h>
# ifdef _AIX
#include <dumprestor.h>
#define __DARWIN_OSSwapInt16(x) swap_card2(x)
#define __DARWIN_OSSwapInt32(x) swap_card4(x)
#define __DARWIN_OSSwapInt64(x) swap_card8(x)
# endif
# if BYTE_ORDER == LITTLE_ENDIAN
#  define htobe16(x) __DARWIN_OSSwapInt16 (x)
#  define htole16(x) (x)
#  define be16toh(x) __DARWIN_OSSwapInt16 (x)
#  define le16toh(x) (x)

#  define htobe32(x) __DARWIN_OSSwapInt32 (x)
#  define htole32(x) (x)
#  define be32toh(x) __DARWIN_OSSwapInt32 (x)
#  define le32toh(x) (x)

//#  if __GLIBC_HAVE_LONG_LONG
#   define htobe64(x) __DARWIN_OSSwapInt64 (x)
#   define htole64(x) (x)
#   define be64toh(x) __DARWIN_OSSwapInt64 (x)
#   define le64toh(x) (x)
//#  endif

# else
#  define htobe16(x) (x)
#  define htole16(x) __DARWIN_OSSwapInt16 (x)
#  define be16toh(x) (x)
#  define le16toh(x) __DARWIN_OSSwapInt16 (x)

#  define htobe32(x) (x)
#  define htole32(x) __DARWIN_OSSwapInt32 (x)
#  define be32toh(x) (x)
#  define le32toh(x) __DARWIN_OSSwapInt32 (x)

//#  if __GLIBC_HAVE_LONG_LONG
#   define htobe64(x) (x)
#   define htole64(x) __DARWIN_OSSwapInt64 (x)
#   define be64toh(x) (x)
#   define le64toh(x) __DARWIN_OSSwapInt64 (x)
//#  endif
# endif
#else
#error os not support
#endif

#elif defined(_WIN32)

/*windows is little endian only*/
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#define __FLOAT_WORD_ORDER __BYTE_ORDER 

#define htobe16(host_16bits) htons(host_16bits)
#define htole16(host_16bits) (host_16bits)
#define be16toh(big_endian_16bits) ntohs(big_endian_16bits)
#define le16toh(little_endian_16bits) (little_endian_16bits)

#define htobe32(host_32bits) htonl(host_32bits)
#define htole32(host_32bits) (host_32bits)
#define be32toh(big_endian_32bits) ntohl(big_endian_32bits)
#define le32toh(little_endian_32bits) (little_endian_32bits)

#define htobe64(host_64bits)((((uint64_t)htobe32((uint32_t)host_64bits))<<32) | htobe32((uint32_t)(host_64bits>>32)))
#define htole64(host_64bits) (host_64bits)
#define be64toh(big_endian_64bits)((((uint64_t)be32toh((uint32_t)big_endian_64bits))<<32) | be32toh((uint32_t)(big_endian_64bits>>32)))
#define le64toh(little_endian_64bits) (little_endian_64bits)

#else
#error os not support
#endif

#endif
