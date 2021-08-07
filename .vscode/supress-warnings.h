#define PATH_MAX 1024

/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _KERN_MIPS_TYPES_H_
#define _KERN_MIPS_TYPES_H_

/*
 * Machine-dependent types visible to userland.
 * (Kernel-only types should go in mips/types.h.)
 * 32-bit MIPS version.
 *
 * See kern/types.h for an explanation of the underscores.
 */


/* Sized integer types, with convenient short names */
typedef char      __i8;                 /* 8-bit signed integer */
typedef short     __i16;                /* 16-bit signed integer */
typedef int       __i32;                /* 32-bit signed integer */
typedef long long __i64;                /* 64-bit signed integer */

typedef unsigned char      __u8;        /* 8-bit unsigned integer */
typedef unsigned short     __u16;       /* 16-bit unsigned integer */
typedef unsigned int       __u32;       /* 32-bit unsigned integer */
typedef unsigned long long __u64;       /* 64-bit unsigned integer */

/* Further standard C types */
typedef long __intptr_t;                /* Signed pointer-sized integer */
typedef unsigned long __uintptr_t;      /* Unsigned pointer-sized integer */

/*
 * Since we're a 32-bit platform, size_t, ssize_t, and ptrdiff_t can
 * correctly be either (unsigned) int or (unsigned) long. However, if we
 * don't define it to the same one gcc is using, gcc will get
 * upset. If you switch compilers and see otherwise unexplicable type
 * errors involving size_t, try changing this.
 */
#if 1
typedef unsigned __size_t;              /* Size of a memory region */
typedef int __ssize_t;                  /* Signed type of same size */
typedef int __ptrdiff_t;                /* Difference of two pointers */
#else
typedef unsigned long __size_t;         /* Size of a memory region */
typedef long __ssize_t;                 /* Signed type of same size */
typedef long __ptrdiff_t;               /* Difference of two pointers */
#endif

/* Number of bits per byte. */
#define __CHAR_BIT  8


#endif /* _KERN_MIPS_TYPES_H_ */

#ifndef _KERN_TYPES_H_
#define _KERN_TYPES_H_

typedef __u32 __blkcnt_t;  /* Count of blocks */
typedef __u32 __blksize_t; /* Size of an I/O block */
typedef __u64 __counter_t; /* Event counter */
typedef __u32 __daddr_t;   /* Disk block number */
typedef __u32 __dev_t;     /* Hardware device ID */
typedef __u32 __fsid_t;    /* Filesystem ID */
typedef __i32 __gid_t;     /* Group ID */
typedef __u32 __in_addr_t; /* Internet address */
typedef __u32 __in_port_t; /* Internet port number */
typedef __u32 __ino_t;     /* Inode number */
typedef __u32 __mode_t;    /* File access mode */
typedef __u16 __nlink_t;   /* Number of links (intentionally only 16 bits) */
typedef __i64 __off_t;     /* Offset within file */
typedef __i32 __pid_t;     /* Process ID */
typedef __u64 __rlim_t;    /* Resource limit quantity */
typedef __u8 __sa_family_t;/* Socket address family */
typedef __i64 __time_t;    /* Time in seconds */
typedef __i32 __uid_t;     /* User ID */

typedef int __nfds_t;    /* Number of file handles */
typedef int __socklen_t;   /* Socket-related length */

/* See note in <stdarg.h> */
#ifdef __GNUC__
typedef __builtin_va_list __va_list;
#endif


#endif /* _KERN_TYPES_H_ */

typedef __size_t size_t;

#define NULL ((void *)0)

typedef __ssize_t ssize_t;
typedef __ptrdiff_t ptrdiff_t;

/* ...and machine-independent from <kern/types.h>. */
typedef __blkcnt_t blkcnt_t;
typedef __blksize_t blksize_t;
typedef __daddr_t daddr_t;
typedef __dev_t dev_t;
typedef __fsid_t fsid_t;
typedef __gid_t gid_t;
typedef __in_addr_t in_addr_t;
typedef __in_port_t in_port_t;
typedef __ino_t ino_t;
typedef __mode_t mode_t;
typedef __nlink_t nlink_t;
typedef __off_t off_t;
typedef __pid_t pid_t;
typedef __rlim_t rlim_t;
typedef __sa_family_t sa_family_t;
typedef __time_t time_t;
typedef __uid_t uid_t;

typedef __nfds_t nfds_t;
typedef __socklen_t socklen_t;

#define CHAR_BIT __CHAR_BIT

struct __userptr { char _dummy; };
typedef struct __userptr *userptr_t;
typedef const struct __userptr *const_userptr_t;

typedef __i8 int8_t;
typedef __i16 int16_t;
typedef __i32 int32_t;
typedef __i64 int64_t;
typedef __u8 uint8_t;
typedef __u16 uint16_t;
typedef __u32 uint32_t;
typedef __u64 uint64_t;
typedef __size_t size_t;
typedef __ssize_t ssize_t;
typedef __intptr_t intptr_t;
typedef __uintptr_t uintptr_t;
typedef __ptrdiff_t ptrdiff_t;

/* ...and machine-independent from <kern/types.h>. */
typedef __blkcnt_t blkcnt_t;
typedef __blksize_t blksize_t;
typedef __daddr_t daddr_t;
typedef __dev_t dev_t;
typedef __fsid_t fsid_t;
typedef __gid_t gid_t;
typedef __in_addr_t in_addr_t;
typedef __in_port_t in_port_t;
typedef __ino_t ino_t;
typedef __mode_t mode_t;
typedef __nlink_t nlink_t;
typedef __off_t off_t;
typedef __pid_t pid_t;
typedef __rlim_t rlim_t;
typedef __sa_family_t sa_family_t;
typedef __time_t time_t;
typedef __uid_t uid_t;

typedef __nfds_t nfds_t;
typedef __socklen_t socklen_t;

/*
 * Number of bits per byte.
 */

#define CHAR_BIT __CHAR_BIT

/*
 * Null pointer.
 */

#define NULL ((void *)0)

/*
 * Boolean.
 */
typedef _Bool bool;
#define true  1
#define false 0

typedef __u32 paddr_t;
typedef __u32 vaddr_t;

#define _KERNEL

#define OPT_A3
#define UW
