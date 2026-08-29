/* Primitive types and layout assertions, shared by every translation unit.
 *
 * The single most common way to get a byte match wrong here is a struct field
 * at the wrong offset: the code compiles, runs, and differs by one immediate.
 * So layout is asserted at COMPILE time. A wrong offset becomes
 *
 *     error C2118: negative subscript
 *
 * which is caught before anything is compared, instead of surfacing later as
 * a one-word diff that reads like a scheduling problem.
 *
 * cl.exe 15.00.8153 is a C++03 compiler -- no static_assert, no <cstdint> --
 * so the classic negative-array-size trick does the work. Verified against
 * this toolchain in both directions: a correct offset compiles, a wrong one
 * fails with C2118.
 */

#ifndef TOS_TYPES_H
#define TOS_TYPES_H

#include <stddef.h>

typedef unsigned char       u8;
typedef signed char         s8;
typedef unsigned short      u16;
typedef short               s16;
typedef unsigned int        u32;
typedef int                 s32;
typedef unsigned __int64    u64;
typedef __int64             s64;
typedef float               f32;
typedef double              f64;

/* Assert that `field` of `type` sits at `off`. Place one per known field.
 * The typedef name has to be unique in the translation unit, hence the
 * type and field in it. */
#define ASSERT_OFFSET(type, field, off) \
    typedef char tos_off_##type##_##field[(offsetof(type, field) == (off)) ? 1 : -1]

/* Assert a whole struct's size, where the size is actually known -- from an
 * allocation, an array stride, or a `mulli` in an index computation. Do NOT
 * write one of these from a guess: an unasserted size is honest, a wrong
 * asserted size is a lie that compiles. */
#define ASSERT_SIZE(type, bytes) \
    typedef char tos_size_##type[(sizeof(type) == (bytes)) ? 1 : -1]

/* Padding for a region whose contents are not known yet. Spelled out rather
 * than hidden behind a macro so the offsets stay readable in the struct. */

#endif /* TOS_TYPES_H */
