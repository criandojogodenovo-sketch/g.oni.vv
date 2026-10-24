#ifndef __khrplatform_h_
#define __khrplatform_h_

/*
 * KHR/khrplatform.h — Khronos platform header (cópia mínima canónica,
 * necessária porque os debs de dev deste ambiente não a trazem).
 * Khronos Public License — https://www.khronos.org/registry/
 */

#include <stdint.h>
#include <cstddef>

/* Khronos integer types */
typedef int32_t khronos_int32_t;
typedef uint32_t khronos_uint32_t;
typedef int64_t khronos_int64_t;
typedef uint64_t khronos_uint64_t;
typedef signed char khronos_int8_t;
typedef unsigned char khronos_uint8_t;
typedef short khronos_int16_t;
typedef unsigned short khronos_uint16_t;
typedef float khronos_float_t;

/* khronos pointer-sized types (usados por GLsizeiptr/GLintptr no gl3.h) */
typedef uintptr_t khronos_uintptr_t;
typedef ptrdiff_t khronos_intptr_t;
typedef size_t khronos_ssize_t;

/* khronos_utc utilities (não usadas pela engine) */
typedef khronos_int64_t khronos_utime_nanoseconds_t;
typedef khronos_int64_t khronos_stime_nanoseconds_t;

/* Khronos API entrypoint convention */
#ifndef KHRONOS_STATIC
#ifdef _WIN32
#define KHRONOS_APICALL __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define KHRONOS_APICALL __attribute__((visibility("default")))
#else
#define KHRONOS_APICALL
#endif
#endif

#ifndef KHRONOS_APIENTRY
#define KHRONOS_APIENTRY
#endif
#ifndef KHRONOS_APIATTRIBUTES
#define KHRONOS_APIATTRIBUTES
#endif

#endif /* __khrplatform_h_ */
