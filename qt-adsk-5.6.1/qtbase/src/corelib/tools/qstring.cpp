/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Copyright (C) 2015 Intel Corporation
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qstringlist.h"
#include "qregexp.h"
#include "qregularexpression.h"
#include "qunicodetables_p.h"
#ifndef QT_NO_TEXTCODEC
#include <qtextcodec.h>
#endif
#include <private/qutfcodec_p.h>
#include "qsimd_p.h"
#include <qnumeric.h>
#include <qdatastream.h>
#include <qlist.h>
#include "qlocale.h"
#include "qlocale_p.h"
#include "qstringbuilder.h"
#include "qstringmatcher.h"
#include "qvarlengtharray.h"
#include "qtools_p.h"
#include "qdebug.h"
#include "qendian.h"
#include "qcollator.h"

#ifdef Q_OS_MAC
#include <private/qcore_mac_p.h>
#endif

#include <private/qfunctions_p.h>

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "qchar.cpp"
#include "qstringmatcher.cpp"
#include "qstringiterator_p.h"
#include "qstringalgorithms_p.h"
#include "qthreadstorage.h"

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#  ifdef Q_OS_WINCE
#    include <winnls.h>
#  endif
#endif

#ifdef truncate
#  undef truncate
#endif

#ifndef LLONG_MAX
#define LLONG_MAX qint64_C(9223372036854775807)
#endif
#ifndef LLONG_MIN
#define LLONG_MIN (-LLONG_MAX - qint64_C(1))
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX quint64_C(18446744073709551615)
#endif

#define IS_RAW_DATA(d) ((d)->offset != sizeof(QStringData))

QT_BEGIN_NAMESPACE

/*
 * Note on the use of SIMD in qstring.cpp:
 *
 * Several operations with strings are improved with the use of SIMD code,
 * since they are repetitive. For MIPS, we have hand-written assembly code
 * outside of qstring.cpp targeting MIPS DSP and MIPS DSPr2. For ARM and for
 * x86, we can only use intrinsics and therefore everything is contained in
 * qstring.cpp. We need to use intrinsics only for those platforms due to the
 * different compilers and toolchains used, which have different syntax for
 * assembly sources.
 *
 * ** SSE notes: **
 *
 * Whenever multiple alternatives are equivalent or near so, we prefer the one
 * using instructions from SSE2, since SSE2 is guaranteed to be enabled for all
 * 64-bit builds and we enable it for 32-bit builds by default. Use of higher
 * SSE versions should be done when there's a clear performance benefit and
 * requires fallback code to SSE2, if it exists.
 *
 * Performance measurement in the past shows that most strings are short in
 * size and, therefore, do not benefit from alignment prologues. That is,
 * trying to find a 16-byte-aligned boundary to operate on is often more
 * expensive than executing the unaligned operation directly. In addition, note
 * that the QString private data is designed so that the data is stored on
 * 16-byte boundaries if the system malloc() returns 16-byte aligned pointers
 * on its own (64-bit glibc on Linux does; 32-bit glibc on Linux returns them
 * 50% of the time), so skipping the alignment prologue is actually optimizing
 * for the common case.
 */

#if defined(__mips_dsp)
// From qstring_mips_dsp_asm.S
extern "C" void qt_fromlatin1_mips_asm_unroll4 (ushort*, const char*, uint);
extern "C" void qt_fromlatin1_mips_asm_unroll8 (ushort*, const char*, uint);
extern "C" void qt_toLatin1_mips_dsp_asm(uchar *dst, const ushort *src, int length);
#endif

// internal
int qFindString(const QChar *haystack, int haystackLen, int from,
    const QChar *needle, int needleLen, Qt::CaseSensitivity cs);
int qFindStringBoyerMoore(const QChar *haystack, int haystackLen, int from,
    const QChar *needle, int needleLen, Qt::CaseSensitivity cs);
static inline int qt_last_index_of(const QChar *haystack, int haystackLen, QChar needle,
                                   int from, Qt::CaseSensitivity cs);
static inline int qt_string_count(const QChar *haystack, int haystackLen,
                                  const QChar *needle, int needleLen,
                                  Qt::CaseSensitivity cs);
static inline int qt_string_count(const QChar *haystack, int haystackLen,
                                  QChar needle, Qt::CaseSensitivity cs);
static inline int qt_find_latin1_string(const QChar *hay, int size, QLatin1String needle,
                                        int from, Qt::CaseSensitivity cs);
static inline bool qt_starts_with(const QChar *haystack, int haystackLen,
                                  const QChar *needle, int needleLen, Qt::CaseSensitivity cs);
static inline bool qt_starts_with(const QChar *haystack, int haystackLen,
                                  QLatin1String needle, Qt::CaseSensitivity cs);
static inline bool qt_ends_with(const QChar *haystack, int haystackLen,
                                const QChar *needle, int needleLen, Qt::CaseSensitivity cs);
static inline bool qt_ends_with(const QChar *haystack, int haystackLen,
                                QLatin1String needle, Qt::CaseSensitivity cs);

#if defined(Q_COMPILER_LAMBDA) && !defined(__OPTIMIZE_SIZE__)
namespace {
template <uint MaxCount> struct UnrollTailLoop
{
    template <typename RetType, typename Functor1, typename Functor2>
    static inline RetType exec(int count, RetType returnIfExited, Functor1 loopCheck, Functor2 returnIfFailed, int i = 0)
    {
        /* equivalent to:
         *   while (count--) {
         *       if (loopCheck(i))
         *           return returnIfFailed(i);
         *   }
         *   return returnIfExited;
         */

        if (!count)
            return returnIfExited;

        bool check = loopCheck(i);
        if (check) {
            const RetType &retval = returnIfFailed(i);
            return retval;
        }

        return UnrollTailLoop<MaxCount - 1>::exec(count - 1, returnIfExited, loopCheck, returnIfFailed, i + 1);
    }

    template <typename Functor>
    static inline void exec(int count, Functor code)
    {
        /* equivalent to:
         *   for (int i = 0; i < count; ++i)
         *       code(i);
         */
        exec(count, 0, [=](int i) -> bool { code(i); return false; }, [](int) { return 0; });
    }
};
template <> template <typename RetType, typename Functor1, typename Functor2>
inline RetType UnrollTailLoop<0>::exec(int, RetType returnIfExited, Functor1, Functor2, int)
{
    return returnIfExited;
}
}
#endif

// conversion between Latin 1 and UTF-16
void qt_from_latin1(ushort *dst, const char *str, size_t size) Q_DECL_NOTHROW
{
    /* SIMD:
     * Unpacking with SSE has been shown to improve performance on recent CPUs
     * The same method gives no improvement with NEON.
     */
#if defined(__SSE2__)
    const char *e = str + size;
    qptrdiff offset = 0;

    // we're going to read str[offset..offset+15] (16 bytes)
    for ( ; str + offset + 15 < e; offset += 16) {
        const __m128i chunk = _mm_loadu_si128((const __m128i*)(str + offset)); // load
#ifdef __AVX2__
        // zero extend to an YMM register
        const __m256i extended = _mm256_cvtepu8_epi16(chunk);

        // store
        _mm256_storeu_si256((__m256i*)(dst + offset), extended);
#else
        const __m128i nullMask = _mm_set1_epi32(0);

        // unpack the first 8 bytes, padding with zeros
        const __m128i firstHalf = _mm_unpacklo_epi8(chunk, nullMask);
        _mm_storeu_si128((__m128i*)(dst + offset), firstHalf); // store

        // unpack the last 8 bytes, padding with zeros
        const __m128i secondHalf = _mm_unpackhi_epi8 (chunk, nullMask);
        _mm_storeu_si128((__m128i*)(dst + offset + 8), secondHalf); // store
#endif
    }

    size = size % 16;
    dst += offset;
    str += offset;
#  if defined(Q_COMPILER_LAMBDA) && !defined(__OPTIMIZE_SIZE__)
    return UnrollTailLoop<15>::exec(int(size), [=](int i) { dst[i] = (uchar)str[i]; });
#  endif
#endif
#if defined(__mips_dsp)
    if (size > 20)
        qt_fromlatin1_mips_asm_unroll8(dst, str, size);
    else
        qt_fromlatin1_mips_asm_unroll4(dst, str, size);
#else
    while (size--)
        *dst++ = (uchar)*str++;
#endif
}

#if defined(__SSE2__)
static inline __m128i mergeQuestionMarks(__m128i chunk)
{
    const __m128i questionMark = _mm_set1_epi16('?');

# ifdef __SSE4_2__
    // compare the unsigned shorts for the range 0x0100-0xFFFF
    // note on the use of _mm_cmpestrm:
    //  The MSDN documentation online (http://technet.microsoft.com/en-us/library/bb514080.aspx)
    //  says for range search the following:
    //    For each character c in a, determine whether b0 <= c <= b1 or b2 <= c <= b3
    //
    //  However, all examples on the Internet, including from Intel
    //  (see http://software.intel.com/en-us/articles/xml-parsing-accelerator-with-intel-streaming-simd-extensions-4-intel-sse4/)
    //  put the range to be searched first
    //
    //  Disassembly and instruction-level debugging with GCC and ICC show
    //  that they are doing the right thing. Inverting the arguments in the
    //  instruction does cause a bunch of test failures.

    const __m128i rangeMatch = _mm_cvtsi32_si128(0xffff0100);
    const __m128i offLimitMask = _mm_cmpestrm(rangeMatch, 2, chunk, 8,
            _SIDD_UWORD_OPS | _SIDD_CMP_RANGES | _SIDD_UNIT_MASK);

    // replace the non-Latin 1 characters in the chunk with question marks
    chunk = _mm_blendv_epi8(chunk, questionMark, offLimitMask);
# else
    // SSE has no compare instruction for unsigned comparison.
    // The variables must be shiffted + 0x8000 to be compared
    const __m128i signedBitOffset = _mm_set1_epi16(short(0x8000));
    const __m128i thresholdMask = _mm_set1_epi16(short(0xff + 0x8000));

    const __m128i signedChunk = _mm_add_epi16(chunk, signedBitOffset);
    const __m128i offLimitMask = _mm_cmpgt_epi16(signedChunk, thresholdMask);

#  ifdef __SSE4_1__
    // replace the non-Latin 1 characters in the chunk with question marks
    chunk = _mm_blendv_epi8(chunk, questionMark, offLimitMask);
#  else
    // offLimitQuestionMark contains '?' for each 16 bits that was off-limit
    // the 16 bits that were correct contains zeros
    const __m128i offLimitQuestionMark = _mm_and_si128(offLimitMask, questionMark);

    // correctBytes contains the bytes that were in limit
    // the 16 bits that were off limits contains zeros
    const __m128i correctBytes = _mm_andnot_si128(offLimitMask, chunk);

    // merge offLimitQuestionMark and correctBytes to have the result
    chunk = _mm_or_si128(correctBytes, offLimitQuestionMark);
#  endif
# endif
    return chunk;
}
#endif

static void qt_to_latin1(uchar *dst, const ushort *src, int length)
{
#if defined(__SSE2__)
    uchar *e = dst + length;
    qptrdiff offset = 0;

    // we're going to write to dst[offset..offset+15] (16 bytes)
    for ( ; dst + offset + 15 < e; offset += 16) {
        __m128i chunk1 = _mm_loadu_si128((const __m128i*)(src + offset)); // load
        chunk1 = mergeQuestionMarks(chunk1);

        __m128i chunk2 = _mm_loadu_si128((const __m128i*)(src + offset + 8)); // load
        chunk2 = mergeQuestionMarks(chunk2);

        // pack the two vector to 16 x 8bits elements
        const __m128i result = _mm_packus_epi16(chunk1, chunk2);
        _mm_storeu_si128((__m128i*)(dst + offset), result); // store
    }

    length = length % 16;
    dst += offset;
    src += offset;

#  if defined(Q_COMPILER_LAMBDA) && !defined(__OPTIMIZE_SIZE__)
    return UnrollTailLoop<15>::exec(length, [=](int i) { dst[i] = (src[i]>0xff) ? '?' : (uchar) src[i]; });
#  endif
#elif defined(__ARM_NEON__)
    // Refer to the documentation of the SSE2 implementation
    // this use eactly the same method as for SSE except:
    // 1) neon has unsigned comparison
    // 2) packing is done to 64 bits (8 x 8bits component).
    if (length >= 16) {
        const int chunkCount = length >> 3; // divided by 8
        const uint16x8_t questionMark = vdupq_n_u16('?'); // set
        const uint16x8_t thresholdMask = vdupq_n_u16(0xff); // set
        for (int i = 0; i < chunkCount; ++i) {
            uint16x8_t chunk = vld1q_u16((uint16_t *)src); // load
            src += 8;

            const uint16x8_t offLimitMask = vcgtq_u16(chunk, thresholdMask); // chunk > thresholdMask
            const uint16x8_t offLimitQuestionMark = vandq_u16(offLimitMask, questionMark); // offLimitMask & questionMark
            const uint16x8_t correctBytes = vbicq_u16(chunk, offLimitMask); // !offLimitMask & chunk
            chunk = vorrq_u16(correctBytes, offLimitQuestionMark); // correctBytes | offLimitQuestionMark
            const uint8x8_t result = vmovn_u16(chunk); // narrowing move->packing
            vst1_u8(dst, result); // store
            dst += 8;
        }
        length = length % 8;
    }
#endif
#if defined(__mips_dsp)
    qt_toLatin1_mips_dsp_asm(dst, src, length);
#else
    while (length--) {
        *dst++ = (*src>0xff) ? '?' : (uchar) *src;
        ++src;
    }
#endif
}

// Unicode case-insensitive comparison
static int ucstricmp(const ushort *a, const ushort *ae, const ushort *b, const ushort *be)
{
    if (a == b)
        return (ae - be);
    if (a == 0)
        return 1;
    if (b == 0)
        return -1;

    const ushort *e = ae;
    if (be - b < ae - a)
        e = a + (be - b);

    uint alast = 0;
    uint blast = 0;
    while (a < e) {
//         qDebug() << hex << alast << blast;
//         qDebug() << hex << "*a=" << *a << "alast=" << alast << "folded=" << foldCase (*a, alast);
//         qDebug() << hex << "*b=" << *b << "blast=" << blast << "folded=" << foldCase (*b, blast);
        int diff = foldCase(*a, alast) - foldCase(*b, blast);
        if ((diff))
            return diff;
        ++a;
        ++b;
    }
    if (a == ae) {
        if (b == be)
            return 0;
        return -1;
    }
    return 1;
}

// Case-insensitive comparison between a Unicode string and a QLatin1String
static int ucstricmp(const ushort *a, const ushort *ae, const uchar *b, const uchar *be)
{
    if (a == 0) {
        if (b == 0)
            return 0;
        return 1;
    }
    if (b == 0)
        return -1;

    const ushort *e = ae;
    if (be - b < ae - a)
        e = a + (be - b);

    while (a < e) {
        int diff = foldCase(*a) - foldCase(*b);
        if ((diff))
            return diff;
        ++a;
        ++b;
    }
    if (a == ae) {
        if (b == be)
            return 0;
        return -1;
    }
    return 1;
}

#if defined(__mips_dsp)
// From qstring_mips_dsp_asm.S
extern "C" int qt_ucstrncmp_mips_dsp_asm(const ushort *a,
                                         const ushort *b,
                                         unsigned len);
#endif

// Unicode case-sensitive compare two same-sized strings
static int ucstrncmp(const QChar *a, const QChar *b, int l)
{
#if defined(__mips_dsp)
    if (l >= 8) {
        return qt_ucstrncmp_mips_dsp_asm(reinterpret_cast<const ushort*>(a),
                                         reinterpret_cast<const ushort*>(b),
                                         l);
    }
#endif // __mips_dsp
#ifdef __SSE2__
    const char *ptr = reinterpret_cast<const char*>(a);
    qptrdiff distance = reinterpret_cast<const char*>(b) - ptr;
    a += l & ~7;
    b += l & ~7;
    l &= 7;

    // we're going to read ptr[0..15] (16 bytes)
    for ( ; ptr + 15 < reinterpret_cast<const char *>(a); ptr += 16) {
        __m128i a_data = _mm_loadu_si128((const __m128i*)ptr);
        __m128i b_data = _mm_loadu_si128((const __m128i*)(ptr + distance));
        __m128i result = _mm_cmpeq_epi16(a_data, b_data);
        uint mask = ~_mm_movemask_epi8(result);
        if (ushort(mask)) {
            // found a different byte
            uint idx = uint(_bit_scan_forward(mask));
            return reinterpret_cast<const QChar *>(ptr + idx)->unicode()
                    - reinterpret_cast<const QChar *>(ptr + distance + idx)->unicode();
        }
    }
#  if defined(Q_COMPILER_LAMBDA) && !defined(__OPTIMIZE_SIZE__)
    const auto &lambda = [=](int i) -> int {
        return reinterpret_cast<const QChar *>(ptr)[i].unicode()
                - reinterpret_cast<const QChar *>(ptr + distance)[i].unicode();
    };
    return UnrollTailLoop<7>::exec(l, 0, lambda, lambda);
#  endif
#endif
    if (!l)
        return 0;

    union {
        const QChar *w;
        const quint32 *d;
        quintptr value;
    } sa, sb;
    sa.w = a;
    sb.w = b;

    // check alignment
    if ((sa.value & 2) == (sb.value & 2)) {
        // both addresses have the same alignment
        if (sa.value & 2) {
            // both addresses are not aligned to 4-bytes boundaries
            // compare the first character
            if (*sa.w != *sb.w)
                return sa.w->unicode() - sb.w->unicode();
            --l;
            ++sa.w;
            ++sb.w;

            // now both addresses are 4-bytes aligned
        }

        // both addresses are 4-bytes aligned
        // do a fast 32-bit comparison
        const quint32 *e = sa.d + (l >> 1);
        for ( ; sa.d != e; ++sa.d, ++sb.d) {
            if (*sa.d != *sb.d) {
                if (*sa.w != *sb.w)
                    return sa.w->unicode() - sb.w->unicode();
                return sa.w[1].unicode() - sb.w[1].unicode();
            }
        }

        // do we have a tail?
        return (l & 1) ? sa.w->unicode() - sb.w->unicode() : 0;
    } else {
        // one of the addresses isn't 4-byte aligned but the other is
        const QChar *e = sa.w + l;
        for ( ; sa.w != e; ++sa.w, ++sb.w) {
            if (*sa.w != *sb.w)
                return sa.w->unicode() - sb.w->unicode();
        }
    }
    return 0;
}

static int ucstrncmp(const QChar *a, const uchar *c, int l)
{
    const ushort *uc = reinterpret_cast<const ushort *>(a);
    const ushort *e = uc + l;

#ifdef __SSE2__
    __m128i nullmask = _mm_setzero_si128();
    qptrdiff offset = 0;

    // we're going to read uc[offset..offset+15] (32 bytes)
    // and c[offset..offset+15] (16 bytes)
    for ( ; uc + offset + 15 < e; offset += 16) {
        // similar to fromLatin1_helper:
        // load 16 bytes of Latin 1 data
        __m128i chunk = _mm_loadu_si128((const __m128i*)(c + offset));

#  ifdef __AVX2__
        // expand Latin 1 data via zero extension
        __m256i ldata = _mm256_cvtepu8_epi16(chunk);

        // load UTF-16 data and compare
        __m256i ucdata = _mm256_loadu_si256((const __m256i*)(uc + offset));
        __m256i result = _mm256_cmpeq_epi16(ldata, ucdata);

        uint mask = ~_mm256_movemask_epi8(result);
#  else
        // expand via unpacking
        __m128i firstHalf = _mm_unpacklo_epi8(chunk, nullmask);
        __m128i secondHalf = _mm_unpackhi_epi8(chunk, nullmask);

        // load UTF-16 data and compare
        __m128i ucdata1 = _mm_loadu_si128((const __m128i*)(uc + offset));
        __m128i ucdata2 = _mm_loadu_si128((const __m128i*)(uc + offset + 8));
        __m128i result1 = _mm_cmpeq_epi16(firstHalf, ucdata1);
        __m128i result2 = _mm_cmpeq_epi16(secondHalf, ucdata2);

        uint mask = ~(_mm_movemask_epi8(result1) | _mm_movemask_epi8(result2) << 16);
#  endif
        if (mask) {
            // found a different character
            uint idx = uint(_bit_scan_forward(mask));
            return uc[offset + idx / 2] - c[offset + idx / 2];
        }
    }

#  ifdef Q_PROCESSOR_X86_64
    enum { MaxTailLength = 7 };
    // we'll read uc[offset..offset+7] (16 bytes) and c[offset..offset+7] (8 bytes)
    if (uc + offset + 7 < e) {
        // same, but we're using an 8-byte load
        __m128i chunk = _mm_cvtsi64_si128(qFromUnaligned<long long>(c + offset));
        __m128i secondHalf = _mm_unpacklo_epi8(chunk, nullmask);

        __m128i ucdata = _mm_loadu_si128((const __m128i*)(uc + offset));
        __m128i result = _mm_cmpeq_epi16(secondHalf, ucdata);
        uint mask = ~_mm_movemask_epi8(result);
        if (ushort(mask)) {
            // found a different character
            uint idx = uint(_bit_scan_forward(mask));
            return uc[offset + idx / 2] - c[offset + idx / 2];
        }

        // still matched
        offset += 8;
    }
#  else
    // 32-bit, we can't do MOVQ to load 8 bytes
    enum { MaxTailLength = 15 };
#  endif

    // reset uc and c
    uc += offset;
    c += offset;

#  if defined(Q_COMPILER_LAMBDA) && !defined(__OPTIMIZE_SIZE__)
    const auto &lambda = [=](int i) { return uc[i] - ushort(c[i]); };
    return UnrollTailLoop<MaxTailLength>::exec(e - uc, 0, lambda, lambda);
#  endif
#endif

    while (uc < e) {
        int diff = *uc - *c;
        if (diff)
            return diff;
        uc++, c++;
    }

    return 0;
}

// Unicode case-sensitive comparison
static int ucstrcmp(const QChar *a, int alen, const QChar *b, int blen)
{
    if (a == b && alen == blen)
        return 0;
    int l = qMin(alen, blen);
    int cmp = ucstrncmp(a, b, l);
    return cmp ? cmp : (alen-blen);
}

// Unicode case-insensitive compare two same-sized strings
static int ucstrnicmp(const ushort *a, const ushort *b, int l)
{
    return ucstricmp(a, a + l, b, b + l);
}

static bool qMemEquals(const quint16 *a, const quint16 *b, int length)
{
    if (a == b || !length)
        return true;

    return ucstrncmp(reinterpret_cast<const QChar *>(a), reinterpret_cast<const QChar *>(b), length) == 0;
}

static int ucstrcmp(const QChar *a, int alen, const uchar *b, int blen)
{
    int l = qMin(alen, blen);
    int cmp = ucstrncmp(a, b, l);
    return cmp ? cmp : (alen-blen);
}

/*!
    \internal

    Returns the index position of the first occurrence of the
    character \a ch in the string given by \a str and \a len,
    searching forward from index
    position \a from. Returns -1 if \a ch could not be found.
*/
static int findChar(const QChar *str, int len, QChar ch, int from,
    Qt::CaseSensitivity cs)
{
    const ushort *s = (const ushort *)str;
    ushort c = ch.unicode();
    if (from < 0)
        from = qMax(from + len, 0);
    if (from < len) {
        const ushort *n = s + from;
        const ushort *e = s + len;
        if (cs == Qt::CaseSensitive) {
#ifdef __SSE2__
            __m128i mch = _mm_set1_epi32(c | (c << 16));

            // we're going to read n[0..7] (16 bytes)
            for (const ushort *next = n + 8; next <= e; n = next, next += 8) {
                __m128i data = _mm_loadu_si128((const __m128i*)n);
                __m128i result = _mm_cmpeq_epi16(data, mch);
                uint mask = _mm_movemask_epi8(result);
                if (ushort(mask)) {
                    // found a match
                    // same as: return n - s + _bit_scan_forward(mask) / 2
                    return (reinterpret_cast<const char *>(n) - reinterpret_cast<const char *>(s)
                            + _bit_scan_forward(mask)) >> 1;
                }
            }

#  if defined(Q_COMPILER_LAMBDA) && !defined(__OPTIMIZE_SIZE__)
            return UnrollTailLoop<7>::exec(e - n, -1,
                                           [=](int i) { return n[i] == c; },
                                           [=](int i) { return n - s + i; });
#  endif
#endif
            --n;
            while (++n != e)
                if (*n == c)
                    return  n - s;
        } else {
            c = foldCase(c);
            --n;
            while (++n != e)
                if (foldCase(*n) == c)
                    return  n - s;
        }
    }
    return -1;
}

#define REHASH(a) \
    if (sl_minus_1 < sizeof(uint) * CHAR_BIT)  \
        hashHaystack -= uint(a) << sl_minus_1; \
    hashHaystack <<= 1

inline bool qIsUpper(char ch)
{
    return ch >= 'A' && ch <= 'Z';
}

inline bool qIsDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

inline char qToLower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 'a';
    else
        return ch;
}


const QString::Null QString::null = { };

/*!
  \macro QT_RESTRICTED_CAST_FROM_ASCII
  \relates QString

  Defining this macro disables most automatic conversions from source
  literals and 8-bit data to unicode QStrings, but allows the use of
  the \c{QChar(char)} and \c{QString(const char (&ch)[N]} constructors,
  and the \c{QString::operator=(const char (&ch)[N])} assignment operator
  giving most of the type-safety benefits of QT_NO_CAST_FROM_ASCII
  but does not require user code to wrap character and string literals
  with QLatin1Char, QLatin1String or similar.

  Using this macro together with source strings outside the 7-bit range,
  non-literals, or literals with embedded NUL characters is undefined.

  \sa QT_NO_CAST_FROM_ASCII, QT_NO_CAST_TO_ASCII
*/

/*!
  \macro QT_NO_CAST_FROM_ASCII
  \relates QString

  Disables automatic conversions from 8-bit strings (char *) to unicode QStrings

  \sa QT_NO_CAST_TO_ASCII, QT_RESTRICTED_CAST_FROM_ASCII, QT_NO_CAST_FROM_BYTEARRAY
*/

/*!
  \macro QT_NO_CAST_TO_ASCII
  \relates QString

  disables automatic conversion from QString to 8-bit strings (char *)

  \sa QT_NO_CAST_FROM_ASCII, QT_RESTRICTED_CAST_FROM_ASCII, QT_NO_CAST_FROM_BYTEARRAY
*/

/*!
  \macro QT_ASCII_CAST_WARNINGS
  \internal
  \relates QString

  This macro can be defined to force a warning whenever a function is
  called that automatically converts between unicode and 8-bit encodings.

  Note: This only works for compilers that support warnings for
  deprecated API.

  \sa QT_NO_CAST_TO_ASCII, QT_NO_CAST_FROM_ASCII, QT_RESTRICTED_CAST_FROM_ASCII
*/

/*!
    \class QCharRef
    \inmodule QtCore
    \reentrant
    \brief The QCharRef class is a helper class for QString.

    \internal

    \ingroup string-processing

    When you get an object of type QCharRef, if you can assign to it,
    the assignment will apply to the character in the string from
    which you got the reference. That is its whole purpose in life.
    The QCharRef becomes invalid once modifications are made to the
    string: if you want to keep the character, copy it into a QChar.

    Most of the QChar member functions also exist in QCharRef.
    However, they are not explicitly documented here.

    \sa QString::operator[](), QString::at(), QChar
*/

/*!
    \class QString
    \inmodule QtCore
    \reentrant

    \brief The QString class provides a Unicode character string.

    \ingroup tools
    \ingroup shared
    \ingroup string-processing

    QString stores a string of 16-bit \l{QChar}s, where each QChar
    corresponds one Unicode 4.0 character. (Unicode characters
    with code values above 65535 are stored using surrogate pairs,
    i.e., two consecutive \l{QChar}s.)

    \l{Unicode} is an international standard that supports most of the
    writing systems in use today. It is a superset of US-ASCII (ANSI
    X3.4-1986) and Latin-1 (ISO 8859-1), and all the US-ASCII/Latin-1
    characters are available at the same code positions.

    Behind the scenes, QString uses \l{implicit sharing}
    (copy-on-write) to reduce memory usage and to avoid the needless
    copying of data. This also helps reduce the inherent overhead of
    storing 16-bit characters instead of 8-bit characters.

    In addition to QString, Qt also provides the QByteArray class to
    store raw bytes and traditional 8-bit '\\0'-terminated strings.
    For most purposes, QString is the class you want to use. It is
    used throughout the Qt API, and the Unicode support ensures that
    your applications will be easy to translate if you want to expand
    your application's market at some point. The two main cases where
    QByteArray is appropriate are when you need to store raw binary
    data, and when memory conservation is critical (like in embedded
    systems).

    \tableofcontents

    \section1 Initializing a String

    One way to initialize a QString is simply to pass a \c{const char
    *} to its constructor. For example, the following code creates a
    QString of size 5 containing the data "Hello":

    \snippet qstring/main.cpp 0

    QString converts the \c{const char *} data into Unicode using the
    fromUtf8() function.

    In all of the QString functions that take \c{const char *}
    parameters, the \c{const char *} is interpreted as a classic
    C-style '\\0'-terminated string encoded in UTF-8. It is legal for
    the \c{const char *} parameter to be 0.

    You can also provide string data as an array of \l{QChar}s:

    \snippet qstring/main.cpp 1

    QString makes a deep copy of the QChar data, so you can modify it
    later without experiencing side effects. (If for performance
    reasons you don't want to take a deep copy of the character data,
    use QString::fromRawData() instead.)

    Another approach is to set the size of the string using resize()
    and to initialize the data character per character. QString uses
    0-based indexes, just like C++ arrays. To access the character at
    a particular index position, you can use \l operator[](). On
    non-const strings, \l operator[]() returns a reference to a
    character that can be used on the left side of an assignment. For
    example:

    \snippet qstring/main.cpp 2

    For read-only access, an alternative syntax is to use the at()
    function:

    \snippet qstring/main.cpp 3

    The at() function can be faster than \l operator[](), because it
    never causes a \l{deep copy} to occur. Alternatively, use the
    left(), right(), or mid() functions to extract several characters
    at a time.

    A QString can embed '\\0' characters (QChar::Null). The size()
    function always returns the size of the whole string, including
    embedded '\\0' characters.

    After a call to the resize() function, newly allocated characters
    have undefined values. To set all the characters in the string to
    a particular value, use the fill() function.

    QString provides dozens of overloads designed to simplify string
    usage. For example, if you want to compare a QString with a string
    literal, you can write code like this and it will work as expected:

    \snippet qstring/main.cpp 4

    You can also pass string literals to functions that take QStrings
    as arguments, invoking the QString(const char *)
    constructor. Similarly, you can pass a QString to a function that
    takes a \c{const char *} argument using the \l qPrintable() macro
    which returns the given QString as a \c{const char *}. This is
    equivalent to calling <QString>.toLocal8Bit().constData().

    \section1 Manipulating String Data

    QString provides the following basic functions for modifying the
    character data: append(), prepend(), insert(), replace(), and
    remove(). For example:

    \snippet qstring/main.cpp 5

    If you are building a QString gradually and know in advance
    approximately how many characters the QString will contain, you
    can call reserve(), asking QString to preallocate a certain amount
    of memory. You can also call capacity() to find out how much
    memory QString actually allocated.

    The replace() and remove() functions' first two arguments are the
    position from which to start erasing and the number of characters
    that should be erased.  If you want to replace all occurrences of
    a particular substring with another, use one of the two-parameter
    replace() overloads.

    A frequent requirement is to remove whitespace characters from a
    string ('\\n', '\\t', ' ', etc.). If you want to remove whitespace
    from both ends of a QString, use the trimmed() function. If you
    want to remove whitespace from both ends and replace multiple
    consecutive whitespaces with a single space character within the
    string, use simplified().

    If you want to find all occurrences of a particular character or
    substring in a QString, use the indexOf() or lastIndexOf()
    functions. The former searches forward starting from a given index
    position, the latter searches backward. Both return the index
    position of the character or substring if they find it; otherwise,
    they return -1.  For example, here's a typical loop that finds all
    occurrences of a particular substring:

    \snippet qstring/main.cpp 6

    QString provides many functions for converting numbers into
    strings and strings into numbers. See the arg() functions, the
    setNum() functions, the number() static functions, and the
    toInt(), toDouble(), and similar functions.

    To get an upper- or lowercase version of a string use toUpper() or
    toLower().

    Lists of strings are handled by the QStringList class. You can
    split a string into a list of strings using the split() function,
    and join a list of strings into a single string with an optional
    separator using QStringList::join(). You can obtain a list of
    strings from a string list that contain a particular substring or
    that match a particular QRegExp using the QStringList::filter()
    function.

    \section1 Querying String Data

    If you want to see if a QString starts or ends with a particular
    substring use startsWith() or endsWith(). If you simply want to
    check whether a QString contains a particular character or
    substring, use the contains() function. If you want to find out
    how many times a particular character or substring occurs in the
    string, use count().

    QStrings can be compared using overloaded operators such as \l
    operator<(), \l operator<=(), \l operator==(), \l operator>=(),
    and so on.  Note that the comparison is based exclusively on the
    numeric Unicode values of the characters. It is very fast, but is
    not what a human would expect; the QString::localeAwareCompare()
    function is a better choice for sorting user-interface strings.

    To obtain a pointer to the actual character data, call data() or
    constData(). These functions return a pointer to the beginning of
    the QChar data. The pointer is guaranteed to remain valid until a
    non-const function is called on the QString.

    \section1 Converting Between 8-Bit Strings and Unicode Strings

    QString provides the following three functions that return a
    \c{const char *} version of the string as QByteArray: toUtf8(),
    toLatin1(), and toLocal8Bit().

    \list
    \li toLatin1() returns a Latin-1 (ISO 8859-1) encoded 8-bit string.
    \li toUtf8() returns a UTF-8 encoded 8-bit string. UTF-8 is a
       superset of US-ASCII (ANSI X3.4-1986) that supports the entire
       Unicode character set through multibyte sequences.
    \li toLocal8Bit() returns an 8-bit string using the system's local
       encoding.
    \endlist

    To convert from one of these encodings, QString provides
    fromLatin1(), fromUtf8(), and fromLocal8Bit(). Other
    encodings are supported through the QTextCodec class.

    As mentioned above, QString provides a lot of functions and
    operators that make it easy to interoperate with \c{const char *}
    strings. But this functionality is a double-edged sword: It makes
    QString more convenient to use if all strings are US-ASCII or
    Latin-1, but there is always the risk that an implicit conversion
    from or to \c{const char *} is done using the wrong 8-bit
    encoding. To minimize these risks, you can turn off these implicit
    conversions by defining the following two preprocessor symbols:

    \list
    \li \c QT_NO_CAST_FROM_ASCII disables automatic conversions from
       C string literals and pointers to Unicode.
    \li \c QT_RESTRICTED_CAST_FROM_ASCII allows automatic conversions
       from C characters and character arrays, but disables automatic
       conversions from character pointers to Unicode.
    \li \c QT_NO_CAST_TO_ASCII disables automatic conversion from QString
       to C strings.
    \endlist

    One way to define these preprocessor symbols globally for your
    application is to add the following entry to your \l {Creating Project Files}{qmake project file}:

    \snippet code/src_corelib_tools_qstring.cpp 0

    You then need to explicitly call fromUtf8(), fromLatin1(),
    or fromLocal8Bit() to construct a QString from an
    8-bit string, or use the lightweight QLatin1String class, for
    example:

    \snippet code/src_corelib_tools_qstring.cpp 1

    Similarly, you must call toLatin1(), toUtf8(), or
    toLocal8Bit() explicitly to convert the QString to an 8-bit
    string.  (Other encodings are supported through the QTextCodec
    class.)

    \table 100 %
    \header
    \li Note for C Programmers

    \row
    \li
    Due to C++'s type system and the fact that QString is
    \l{implicitly shared}, QStrings may be treated like \c{int}s or
    other basic types. For example:

    \snippet qstring/main.cpp 7

    The \c result variable, is a normal variable allocated on the
    stack. When \c return is called, and because we're returning by
    value, the copy constructor is called and a copy of the string is
    returned. No actual copying takes place thanks to the implicit
    sharing.

    \endtable

    \section1 Distinction Between Null and Empty Strings

    For historical reasons, QString distinguishes between a null
    string and an empty string. A \e null string is a string that is
    initialized using QString's default constructor or by passing
    (const char *)0 to the constructor. An \e empty string is any
    string with size 0. A null string is always empty, but an empty
    string isn't necessarily null:

    \snippet qstring/main.cpp 8

    All functions except isNull() treat null strings the same as empty
    strings. For example, toUtf8().constData() returns a pointer to a
    '\\0' character for a null string (\e not a null pointer), and
    QString() compares equal to QString(""). We recommend that you
    always use the isEmpty() function and avoid isNull().

    \section1 Argument Formats

    In member functions where an argument \e format can be specified
    (e.g., arg(), number()), the argument \e format can be one of the
    following:

    \table
    \header \li Format \li Meaning
    \row \li \c e \li format as [-]9.9e[+|-]999
    \row \li \c E \li format as [-]9.9E[+|-]999
    \row \li \c f \li format as [-]9.9
    \row \li \c g \li use \c e or \c f format, whichever is the most concise
    \row \li \c G \li use \c E or \c f format, whichever is the most concise
    \endtable

    A \e precision is also specified with the argument \e format. For
    the 'e', 'E', and 'f' formats, the \e precision represents the
    number of digits \e after the decimal point. For the 'g' and 'G'
    formats, the \e precision represents the maximum number of
    significant digits (trailing zeroes are omitted).

    \section1 More Efficient String Construction

    Many strings are known at compile time. But the trivial
    constructor QString("Hello"), will copy the contents of the string,
    treating the contents as Latin-1. To avoid this one can use the
    QStringLiteral macro to directly create the required data at compile
    time. Constructing a QString out of the literal does then not cause
    any overhead at runtime.

    A slightly less efficient way is to use QLatin1String. This class wraps
    a C string literal, precalculates it length at compile time and can
    then be used for faster comparison with QStrings and conversion to
    QStrings than a regular C string literal.

    Using the QString \c{'+'} operator, it is easy to construct a
    complex string from multiple substrings. You will often write code
    like this:

    \snippet qstring/stringbuilder.cpp 0

    There is nothing wrong with either of these string constructions,
    but there are a few hidden inefficiencies. Beginning with Qt 4.6,
    you can eliminate them.

    First, multiple uses of the \c{'+'} operator usually means
    multiple memory allocations. When concatenating \e{n} substrings,
    where \e{n > 2}, there can be as many as \e{n - 1} calls to the
    memory allocator.

    In 4.6, an internal template class \c{QStringBuilder} has been
    added along with a few helper functions. This class is marked
    internal and does not appear in the documentation, because you
    aren't meant to instantiate it in your code. Its use will be
    automatic, as described below. The class is found in
    \c {src/corelib/tools/qstringbuilder.cpp} if you want to have a
    look at it.

    \c{QStringBuilder} uses expression templates and reimplements the
    \c{'%'} operator so that when you use \c{'%'} for string
    concatenation instead of \c{'+'}, multiple substring
    concatenations will be postponed until the final result is about
    to be assigned to a QString. At this point, the amount of memory
    required for the final result is known. The memory allocator is
    then called \e{once} to get the required space, and the substrings
    are copied into it one by one.

    Additional efficiency is gained by inlining and reduced reference
    counting (the QString created from a \c{QStringBuilder} typically
    has a ref count of 1, whereas QString::append() needs an extra
    test).

    There are three ways you can access this improved method of string
    construction. The straightforward way is to include
    \c{QStringBuilder} wherever you want to use it, and use the
    \c{'%'} operator instead of \c{'+'} when concatenating strings:

    \snippet qstring/stringbuilder.cpp 5

    A more global approach which is the most convenient but
    not entirely source compatible, is to this define in your
    .pro file:

    \snippet qstring/stringbuilder.cpp 3

    and the \c{'+'} will automatically be performed as the
    \c{QStringBuilder} \c{'%'} everywhere.

    \sa fromRawData(), QChar, QLatin1String, QByteArray, QStringRef
*/

/*!
    \enum QString::SplitBehavior

    This enum specifies how the split() function should behave with
    respect to empty strings.

    \value KeepEmptyParts  If a field is empty, keep it in the result.
    \value SkipEmptyParts  If a field is empty, don't include it in the result.

    \sa split()
*/

/*! \typedef QString::ConstIterator

    Qt-style synonym for QString::const_iterator.
*/

/*! \typedef QString::Iterator

    Qt-style synonym for QString::iterator.
*/

/*! \typedef QString::const_iterator

    This typedef provides an STL-style const iterator for QString.

    \sa QString::iterator
*/

/*! \typedef QString::iterator

    The QString::iterator typedef provides an STL-style non-const
    iterator for QString.

    \sa QString::const_iterator
*/

/*! \typedef QString::const_reverse_iterator
    \since 5.6

    This typedef provides an STL-style const reverse iterator for QString.

    \sa QString::reverse_iterator, QString::const_iterator
*/

/*! \typedef QString::reverse_iterator
    \since 5.6

    This typedef provides an STL-style non-const reverse iterator for QString.

    \sa QString::const_reverse_iterator, QString::iterator
*/

/*!
    \typedef QString::size_type

    The QString::size_type typedef provides an STL-style type for sizes (int).
*/

/*!
    \typedef QString::difference_type

    The QString::size_type typedef provides an STL-style type for difference between pointers.
*/

/*!
    \typedef QString::const_reference

    This typedef provides an STL-style const reference for a QString element (QChar).
*/
/*!
    \typedef QString::reference

    This typedef provides an STL-style
    reference for a QString element (QChar).
*/

/*!
    \typedef QString::const_pointer

    The QString::const_pointer typedef provides an STL-style
    const pointer to a QString element (QChar).
*/
/*!
    \typedef QString::pointer

    The QString::const_pointer typedef provides an STL-style
    pointer to a QString element (QChar).
*/

/*!
    \typedef QString::value_type

    This typedef provides an STL-style value type for QString.
*/

/*! \fn QString::iterator QString::begin()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the first character in
    the string.

    \sa constBegin(), end()
*/

/*! \fn QString::const_iterator QString::begin() const

    \overload begin()
*/

/*! \fn QString::const_iterator QString::cbegin() const
    \since 5.0

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character
    in the string.

    \sa begin(), cend()
*/

/*! \fn QString::const_iterator QString::constBegin() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the first character
    in the string.

    \sa begin(), constEnd()
*/

/*! \fn QString::iterator QString::end()

    Returns an \l{STL-style iterators}{STL-style iterator} pointing to the imaginary character
    after the last character in the string.

    \sa begin(), constEnd()
*/

/*! \fn QString::const_iterator QString::end() const

    \overload end()
*/

/*! \fn QString::const_iterator QString::cend() const
    \since 5.0

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    \sa cbegin(), end()
*/

/*! \fn QString::const_iterator QString::constEnd() const

    Returns a const \l{STL-style iterators}{STL-style iterator} pointing to the imaginary
    character after the last character in the list.

    \sa constBegin(), end()
*/

/*! \fn QString::reverse_iterator QString::rbegin()
    \since 5.6

    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the string, in reverse order.

    \sa begin(), crbegin(), rend()
*/

/*! \fn QString::const_reverse_iterator QString::rbegin() const
    \since 5.6
    \overload
*/

/*! \fn QString::const_reverse_iterator QString::crbegin() const
    \since 5.6

    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to the first
    character in the string, in reverse order.

    \sa begin(), rbegin(), rend()
*/

/*! \fn QString::reverse_iterator QString::rend()
    \since 5.6

    Returns a \l{STL-style iterators}{STL-style} reverse iterator pointing to one past
    the last character in the string, in reverse order.

    \sa end(), crend(), rbegin()
*/

/*! \fn QString::const_reverse_iterator QString::rend() const
    \since 5.6
    \overload
*/

/*! \fn QString::const_reverse_iterator QString::crend() const
    \since 5.6

    Returns a const \l{STL-style iterators}{STL-style} reverse iterator pointing to one
    past the last character in the string, in reverse order.

    \sa end(), rend(), rbegin()
*/

/*!
    \fn QString::QString()

    Constructs a null string. Null strings are also empty.

    \sa isEmpty()
*/

/*!
    \fn QString::QString(QString &&other)

    Move-constructs a QString instance, making it point at the same
    object that \a other was pointing to.

    \since 5.2
*/

/*! \fn QString::QString(const char *str)

    Constructs a string initialized with the 8-bit string \a str. The
    given const char pointer is converted to Unicode using the
    fromUtf8() function.

    You can disable this constructor by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    \note Defining QT_RESTRICTED_CAST_FROM_ASCII also disables
    this constructor, but enables a \c{QString(const char (&ch)[N])}
    constructor instead. Using non-literal input, or input with
    embedded NUL characters, or non-7-bit characters is undefined
    in this case.

    \sa fromLatin1(), fromLocal8Bit(), fromUtf8()
*/

/*! \fn QString QString::fromStdString(const std::string &str)

    Returns a copy of the \a str string. The given string is converted
    to Unicode using the fromUtf8() function.

    \sa fromLatin1(), fromLocal8Bit(), fromUtf8(), QByteArray::fromStdString()
*/

/*! \fn QString QString::fromStdWString(const std::wstring &str)

    Returns a copy of the \a str string. The given string is assumed
    to be encoded in utf16 if the size of wchar_t is 2 bytes (e.g. on
    windows) and ucs4 if the size of wchar_t is 4 bytes (most Unix
    systems).

    \sa fromUtf16(), fromLatin1(), fromLocal8Bit(), fromUtf8(), fromUcs4(), fromStdU16String(), fromStdU32String()
*/

/*! \fn QString QString::fromWCharArray(const wchar_t *string, int size)
    \since 4.2

    Returns a copy of the \a string, where the encoding of \a string depends on
    the size of wchar. If wchar is 4 bytes, the \a string is interpreted as UCS-4,
    if wchar is 2 bytes it is interpreted as UTF-16.

    If \a size is -1 (default), the \a string has to be 0 terminated.

    \sa fromUtf16(), fromLatin1(), fromLocal8Bit(), fromUtf8(), fromUcs4(), fromStdWString()
*/

/*! \fn std::wstring QString::toStdWString() const

    Returns a std::wstring object with the data contained in this
    QString. The std::wstring is encoded in utf16 on platforms where
    wchar_t is 2 bytes wide (e.g. windows) and in ucs4 on platforms
    where wchar_t is 4 bytes wide (most Unix systems).

    This method is mostly useful to pass a QString to a function
    that accepts a std::wstring object.

    \sa utf16(), toLatin1(), toUtf8(), toLocal8Bit(), toStdU16String(), toStdU32String()
*/

int QString::toUcs4_helper(const ushort *uc, int length, uint *out)
{
    int count = 0;

    QStringIterator i(reinterpret_cast<const QChar *>(uc), reinterpret_cast<const QChar *>(uc + length));
    while (i.hasNext())
        out[count++] = i.next();

    return count;
}

/*! \fn int QString::toWCharArray(wchar_t *array) const
  \since 4.2

  Fills the \a array with the data contained in this QString object.
  The array is encoded in UTF-16 on platforms where
  wchar_t is 2 bytes wide (e.g. windows) and in UCS-4 on platforms
  where wchar_t is 4 bytes wide (most Unix systems).

  \a array has to be allocated by the caller and contain enough space to
  hold the complete string (allocating the array with the same length as the
  string is always sufficient).

  This function returns the actual length of the string in \a array.

  \note This function does not append a null character to the array.

  \sa utf16(), toUcs4(), toLatin1(), toUtf8(), toLocal8Bit(), toStdWString()
*/

/*! \fn QString::QString(const QString &other)

    Constructs a copy of \a other.

    This operation takes \l{constant time}, because QString is
    \l{implicitly shared}. This makes returning a QString from a
    function very fast. If a shared instance is modified, it will be
    copied (copy-on-write), and that takes \l{linear time}.

    \sa operator=()
*/

/*!
    Constructs a string initialized with the first \a size characters
    of the QChar array \a unicode.

    If \a unicode is 0, a null string is constructed.

    If \a size is negative, \a unicode is assumed to point to a nul-terminated
    array and its length is determined dynamically. The terminating
    nul-character is not considered part of the string.

    QString makes a deep copy of the string data. The unicode data is copied as
    is and the Byte Order Mark is preserved if present.

    \sa fromRawData()
*/
QString::QString(const QChar *unicode, int size)
{
   if (!unicode) {
        d = Data::sharedNull();
    } else {
        if (size < 0) {
            size = 0;
            while (!unicode[size].isNull())
                ++size;
        }
        if (!size) {
            d = Data::allocate(0);
        } else {
            d = Data::allocate(size + 1);
            Q_CHECK_PTR(d);
            d->size = size;
            memcpy(d->data(), unicode, size * sizeof(QChar));
            d->data()[size] = '\0';
        }
    }
}

/*!
    Constructs a string of the given \a size with every character set
    to \a ch.

    \sa fill()
*/
QString::QString(int size, QChar ch)
{
   if (size <= 0) {
        d = Data::allocate(0);
    } else {
        d = Data::allocate(size + 1);
        Q_CHECK_PTR(d);
        d->size = size;
        d->data()[size] = '\0';
        ushort *i = d->data() + size;
        ushort *b = d->data();
        const ushort value = ch.unicode();
        while (i != b)
           *--i = value;
    }
}

/*! \fn QString::QString(int size, Qt::Initialization)
  \internal

  Constructs a string of the given \a size without initializing the
  characters. This is only used in \c QStringBuilder::toString().
*/
QString::QString(int size, Qt::Initialization)
{
    d = Data::allocate(size + 1);
    Q_CHECK_PTR(d);
    d->size = size;
    d->data()[size] = '\0';
}

/*! \fn QString::QString(QLatin1String str)

    Constructs a copy of the Latin-1 string \a str.

    \sa fromLatin1()
*/

/*!
    Constructs a string of size 1 containing the character \a ch.
*/
QString::QString(QChar ch)
{
    d = Data::allocate(2);
    Q_CHECK_PTR(d);
    d->size = 1;
    d->data()[0] = ch.unicode();
    d->data()[1] = '\0';
}

/*! \fn QString::QString(const QByteArray &ba)

    Constructs a string initialized with the byte array \a ba. The
    given byte array is converted to Unicode using fromUtf8(). Stops
    copying at the first 0 character, otherwise copies the entire byte
    array.

    You can disable this constructor by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    \sa fromLatin1(), fromLocal8Bit(), fromUtf8()
*/

/*! \fn QString::QString(const Null &)
    \internal
*/

/*! \fn QString::QString(QStringDataPtr)
    \internal
*/

/*! \fn QString &QString::operator=(const Null &)
    \internal
*/

/*!
  \fn QString::~QString()

    Destroys the string.
*/


/*! \fn void QString::swap(QString &other)
    \since 4.8

    Swaps string \a other with this string. This operation is very fast and
    never fails.
*/

/*! \fn void QString::detach()

    \internal
*/

/*! \fn bool QString::isDetached() const

    \internal
*/

/*! \fn bool QString::isSharedWith(const QString &other) const

    \internal
*/

/*!
    Sets the size of the string to \a size characters.

    If \a size is greater than the current size, the string is
    extended to make it \a size characters long with the extra
    characters added to the end. The new characters are uninitialized.

    If \a size is less than the current size, characters are removed
    from the end.

    Example:

    \snippet qstring/main.cpp 45

    If you want to append a certain number of identical characters to
    the string, use \l operator+=() as follows rather than resize():

    \snippet qstring/main.cpp 46

    If you want to expand the string so that it reaches a certain
    width and fill the new positions with a particular character, use
    the leftJustified() function:

    If \a size is negative, it is equivalent to passing zero.

    \snippet qstring/main.cpp 47

    \sa truncate(), reserve()
*/

void QString::resize(int size)
{
    if (size < 0)
        size = 0;

    if (IS_RAW_DATA(d) && !d->ref.isShared() && size < d->size) {
        d->size = size;
        return;
    }

    if (d->ref.isShared() || uint(size) + 1u > d->alloc)
        reallocData(uint(size) + 1u, true);
    if (d->alloc) {
        d->size = size;
        d->data()[size] = '\0';
    }
}

/*! \fn int QString::capacity() const

    Returns the maximum number of characters that can be stored in
    the string without forcing a reallocation.

    The sole purpose of this function is to provide a means of fine
    tuning QString's memory usage. In general, you will rarely ever
    need to call this function. If you want to know how many
    characters are in the string, call size().

    \sa reserve(), squeeze()
*/

/*!
    \fn void QString::reserve(int size)

    Attempts to allocate memory for at least \a size characters. If
    you know in advance how large the string will be, you can call
    this function, and if you resize the string often you are likely
    to get better performance. If \a size is an underestimate, the
    worst that will happen is that the QString will be a bit slower.

    The sole purpose of this function is to provide a means of fine
    tuning QString's memory usage. In general, you will rarely ever
    need to call this function. If you want to change the size of the
    string, call resize().

    This function is useful for code that needs to build up a long
    string and wants to avoid repeated reallocation. In this example,
    we want to add to the string until some condition is \c true, and
    we're fairly sure that size is large enough to make a call to
    reserve() worthwhile:

    \snippet qstring/main.cpp 44

    \sa squeeze(), capacity()
*/

/*!
    \fn void QString::squeeze()

    Releases any memory not required to store the character data.

    The sole purpose of this function is to provide a means of fine
    tuning QString's memory usage. In general, you will rarely ever
    need to call this function.

    \sa reserve(), capacity()
*/

void QString::reallocData(uint alloc, bool grow)
{
    if (grow) {
        if (alloc > (uint(MaxAllocSize) - sizeof(Data)) / sizeof(QChar))
            qBadAlloc();
        alloc = qAllocMore(alloc * sizeof(QChar), sizeof(Data)) / sizeof(QChar);
    }

    if (d->ref.isShared() || IS_RAW_DATA(d)) {
        Data::AllocationOptions allocOptions(d->capacityReserved ? Data::CapacityReserved : 0);
        Data *x = Data::allocate(alloc, allocOptions);
        Q_CHECK_PTR(x);
        x->size = qMin(int(alloc) - 1, d->size);
        ::memcpy(x->data(), d->data(), x->size * sizeof(QChar));
        x->data()[x->size] = 0;
        if (!d->ref.deref())
            Data::deallocate(d);
        d = x;
    } else {
        Data *p = static_cast<Data *>(::realloc(d, sizeof(Data) + alloc * sizeof(QChar)));
        Q_CHECK_PTR(p);
        d = p;
        d->alloc = alloc;
        d->offset = sizeof(QStringData);
    }
}

void QString::expand(int i)
{
    int sz = d->size;
    resize(qMax(i + 1, sz));
    if (d->size - 1 > sz) {
        ushort *n = d->data() + d->size - 1;
        ushort *e = d->data() + sz;
        while (n != e)
           * --n = ' ';
    }
}

/*! \fn void QString::clear()

    Clears the contents of the string and makes it null.

    \sa resize(), isNull()
*/

/*! \fn QString &QString::operator=(const QString &other)

    Assigns \a other to this string and returns a reference to this
    string.
*/

QString &QString::operator=(const QString &other) Q_DECL_NOTHROW
{
    other.d->ref.ref();
    if (!d->ref.deref())
        Data::deallocate(d);
    d = other.d;
    return *this;
}

/*!
    \fn QString &QString::operator=(QString &&other)

    Move-assigns \a other to this QString instance.

    \since 5.2
*/

/*! \fn QString &QString::operator=(QLatin1String str)

    \overload operator=()

    Assigns the Latin-1 string \a str to this string.
*/
QString &QString::operator=(QLatin1String other)
{
    if (isDetached() && other.size() <= capacity()) { // assumes d->alloc == 0 → !isDetached() (sharedNull)
        d->size = other.size();
        d->data()[other.size()] = 0;
        qt_from_latin1(d->data(), other.latin1(), other.size());
    } else {
        *this = fromLatin1(other.latin1(), other.size());
    }
    return *this;
}

/*! \fn QString &QString::operator=(const QByteArray &ba)

    \overload operator=()

    Assigns \a ba to this string. The byte array is converted to Unicode
    using the fromUtf8() function. This function stops conversion at the
    first NUL character found, or the end of the \a ba byte array.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn QString &QString::operator=(const char *str)

    \overload operator=()

    Assigns \a str to this string. The const char pointer is converted
    to Unicode using the fromUtf8() function.

    You can disable this operator by defining \c QT_NO_CAST_FROM_ASCII
    or \c QT_RESTRICTED_CAST_FROM_ASCII when you compile your applications.
    This can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

*/

/*! \fn QString &QString::operator=(char ch)

    \overload operator=()

    Assigns character \a ch to this string. Note that the character is
    converted to Unicode using the fromLatin1() function, unlike other 8-bit
    functions that operate on UTF-8 data.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \overload operator=()

    Sets the string to contain the single character \a ch.
*/
QString &QString::operator=(QChar ch)
{
    if (isDetached() && capacity() >= 1) { // assumes d->alloc == 0 → !isDetached() (sharedNull)
        // re-use existing capacity:
        ushort *dat = d->data();
        dat[0] = ch.unicode();
        dat[1] = 0;
        d->size = 1;
    } else {
        operator=(QString(ch));
    }
    return *this;
}

/*!
     \fn QString& QString::insert(int position, const QString &str)

    Inserts the string \a str at the given index \a position and
    returns a reference to this string.

    Example:

    \snippet qstring/main.cpp 26

    If the given \a position is greater than size(), the array is
    first extended using resize().

    \sa append(), prepend(), replace(), remove()
*/


/*!
    \fn QString& QString::insert(int position, const QStringRef &str)
    \since 5.5
    \overload insert()

    Inserts the string reference \a str at the given index \a position and
    returns a reference to this string.

    If the given \a position is greater than size(), the array is
    first extended using resize().
*/


/*!
    \fn QString& QString::insert(int position, const char *str)
    \since 5.5
    \overload insert()

    Inserts the C string \a str at the given index \a position and
    returns a reference to this string.

    If the given \a position is greater than size(), the array is
    first extended using resize().

    This function is not available when QT_NO_CAST_FROM_ASCII is
    defined.
*/


/*!
    \fn QString& QString::insert(int position, const QByteArray &str)
    \since 5.5
    \overload insert()

    Inserts the byte array \a str at the given index \a position and
    returns a reference to this string.

    If the given \a position is greater than size(), the array is
    first extended using resize().

    This function is not available when QT_NO_CAST_FROM_ASCII is
    defined.
*/


/*!
    \fn QString &QString::insert(int position, QLatin1String str)
    \overload insert()

    Inserts the Latin-1 string \a str at the given index \a position.
*/
QString &QString::insert(int i, QLatin1String str)
{
    const char *s = str.latin1();
    if (i < 0 || !s || !(*s))
        return *this;

    int len = str.size();
    expand(qMax(d->size, i) + len - 1);

    ::memmove(d->data() + i + len, d->data() + i, (d->size - i - len) * sizeof(QChar));
    qt_from_latin1(d->data() + i, s, uint(len));
    return *this;
}

/*!
    \fn QString& QString::insert(int position, const QChar *unicode, int size)
    \overload insert()

    Inserts the first \a size characters of the QChar array \a unicode
    at the given index \a position in the string.
*/
QString& QString::insert(int i, const QChar *unicode, int size)
{
    if (i < 0 || size <= 0)
        return *this;

    const ushort *s = (const ushort *)unicode;
    if (s >= d->data() && s < d->data() + d->alloc) {
        // Part of me - take a copy
        ushort *tmp = static_cast<ushort *>(::malloc(size * sizeof(QChar)));
        Q_CHECK_PTR(tmp);
        memcpy(tmp, s, size * sizeof(QChar));
        insert(i, reinterpret_cast<const QChar *>(tmp), size);
        ::free(tmp);
        return *this;
    }

    expand(qMax(d->size, i) + size - 1);

    ::memmove(d->data() + i + size, d->data() + i, (d->size - i - size) * sizeof(QChar));
    memcpy(d->data() + i, s, size * sizeof(QChar));
    return *this;
}

/*!
    \fn QString& QString::insert(int position, QChar ch)
    \overload insert()

    Inserts \a ch at the given index \a position in the string.
*/

QString& QString::insert(int i, QChar ch)
{
    if (i < 0)
        i += d->size;
    if (i < 0)
        return *this;
    expand(qMax(i, d->size));
    ::memmove(d->data() + i + 1, d->data() + i, (d->size - i - 1) * sizeof(QChar));
    d->data()[i] = ch.unicode();
    return *this;
}

/*!
    Appends the string \a str onto the end of this string.

    Example:

    \snippet qstring/main.cpp 9

    This is the same as using the insert() function:

    \snippet qstring/main.cpp 10

    The append() function is typically very fast (\l{constant time}),
    because QString preallocates extra space at the end of the string
    data so it can grow without reallocating the entire string each
    time.

    \sa operator+=(), prepend(), insert()
*/
QString &QString::append(const QString &str)
{
    if (str.d != Data::sharedNull()) {
        if (d == Data::sharedNull()) {
            operator=(str);
        } else {
            if (d->ref.isShared() || uint(d->size + str.d->size) + 1u > d->alloc)
                reallocData(uint(d->size + str.d->size) + 1u, true);
            memcpy(d->data() + d->size, str.d->data(), str.d->size * sizeof(QChar));
            d->size += str.d->size;
            d->data()[d->size] = '\0';
        }
    }
    return *this;
}

/*!
  \overload append()
  \since 5.0

  Appends \a len characters from the QChar array \a str to this string.
*/
QString &QString::append(const QChar *str, int len)
{
    if (str && len > 0) {
        if (d->ref.isShared() || uint(d->size + len) + 1u > d->alloc)
            reallocData(uint(d->size + len) + 1u, true);
        memcpy(d->data() + d->size, str, len * sizeof(QChar));
        d->size += len;
        d->data()[d->size] = '\0';
    }
    return *this;
}

/*!
  \overload append()

  Appends the Latin-1 string \a str to this string.
*/
QString &QString::append(QLatin1String str)
{
    const char *s = str.latin1();
    if (s) {
        int len = str.size();
        if (d->ref.isShared() || uint(d->size + len) + 1u > d->alloc)
            reallocData(uint(d->size + len) + 1u, true);
        ushort *i = d->data() + d->size;
        qt_from_latin1(i, s, uint(len));
        i[len] = '\0';
        d->size += len;
    }
    return *this;
}

/*! \fn QString &QString::append(const QByteArray &ba)

    \overload append()

    Appends the byte array \a ba to this string. The given byte array
    is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*! \fn QString &QString::append(const char *str)

    \overload append()

    Appends the string \a str to this string. The given const char
    pointer is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*!
    \overload append()

    Appends the character \a ch to this string.
*/
QString &QString::append(QChar ch)
{
    if (d->ref.isShared() || uint(d->size) + 2u > d->alloc)
        reallocData(uint(d->size) + 2u, true);
    d->data()[d->size++] = ch.unicode();
    d->data()[d->size] = '\0';
    return *this;
}

/*! \fn QString &QString::prepend(const QString &str)

    Prepends the string \a str to the beginning of this string and
    returns a reference to this string.

    Example:

    \snippet qstring/main.cpp 36

    \sa append(), insert()
*/

/*! \fn QString &QString::prepend(QLatin1String str)

    \overload prepend()

    Prepends the Latin-1 string \a str to this string.
*/

/*! \fn QString &QString::prepend(const QChar *str, int len)
    \since 5.5
    \overload prepend()

    Prepends \a len characters from the QChar array \a str to this string and
    returns a reference to this string.
*/

/*! \fn QString &QString::prepend(const QStringRef &str)
    \since 5.5
    \overload prepend()

    Prepends the string reference \a str to the beginning of this string and
    returns a reference to this string.
*/

/*! \fn QString &QString::prepend(const QByteArray &ba)

    \overload prepend()

    Prepends the byte array \a ba to this string. The byte array is
    converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn QString &QString::prepend(const char *str)

    \overload prepend()

    Prepends the string \a str to this string. The const char pointer
    is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn QString &QString::prepend(QChar ch)

    \overload prepend()

    Prepends the character \a ch to this string.
*/

/*!
  \fn QString &QString::remove(int position, int n)

  Removes \a n characters from the string, starting at the given \a
  position index, and returns a reference to the string.

  If the specified \a position index is within the string, but \a
  position + \a n is beyond the end of the string, the string is
  truncated at the specified \a position.

  \snippet qstring/main.cpp 37

  \sa insert(), replace()
*/
QString &QString::remove(int pos, int len)
{
    if (pos < 0)  // count from end of string
        pos += d->size;
    if (uint(pos) >= uint(d->size)) {
        // range problems
    } else if (len >= d->size - pos) {
        resize(pos); // truncate
    } else if (len > 0) {
        detach();
        memmove(d->data() + pos, d->data() + pos + len,
                (d->size - pos - len + 1) * sizeof(ushort));
        d->size -= len;
    }
    return *this;
}

/*!
  Removes every occurrence of the given \a str string in this
  string, and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is
  case sensitive; otherwise the search is case insensitive.

  This is the same as \c replace(str, "", cs).

  \sa replace()
*/
QString &QString::remove(const QString &str, Qt::CaseSensitivity cs)
{
    if (str.d->size) {
        int i = 0;
        while ((i = indexOf(str, i, cs)) != -1)
            remove(i, str.d->size);
    }
    return *this;
}

/*!
  Removes every occurrence of the character \a ch in this string, and
  returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet qstring/main.cpp 38

  This is the same as \c replace(ch, "", cs).

  \sa replace()
*/
QString &QString::remove(QChar ch, Qt::CaseSensitivity cs)
{
    int i = 0;
    ushort c = ch.unicode();
    if (cs == Qt::CaseSensitive) {
        while (i < d->size)
            if (d->data()[i] == ch)
                remove(i, 1);
            else
                i++;
    } else {
        c = foldCase(c);
        while (i < d->size)
            if (foldCase(d->data()[i]) == c)
                remove(i, 1);
            else
                i++;
    }
    return *this;
}

/*!
  \fn QString &QString::remove(const QRegExp &rx)

  Removes every occurrence of the regular expression \a rx in the
  string, and returns a reference to the string. For example:

  \snippet qstring/main.cpp 39

  \sa indexOf(), lastIndexOf(), replace()
*/

/*!
  \fn QString &QString::remove(const QRegularExpression &re)
  \since 5.0

  Removes every occurrence of the regular expression \a re in the
  string, and returns a reference to the string. For example:

  \snippet qstring/main.cpp 96

  \sa indexOf(), lastIndexOf(), replace()
*/

/*!
  \fn QString &QString::replace(int position, int n, const QString &after)

  Replaces \a n characters beginning at index \a position with
  the string \a after and returns a reference to this string.

  \note If the specified \a position index is within the string,
  but \a position + \a n goes outside the strings range,
  then \a n will be adjusted to stop at the end of the string.

  Example:

  \snippet qstring/main.cpp 40

  \sa insert(), remove()
*/
QString &QString::replace(int pos, int len, const QString &after)
{
    QString copy = after;
    return replace(pos, len, copy.constData(), copy.length());
}

/*!
  \fn QString &QString::replace(int position, int n, const QChar *unicode, int size)
  \overload replace()
  Replaces \a n characters beginning at index \a position with the
  first \a size characters of the QChar array \a unicode and returns a
  reference to this string.
*/
QString &QString::replace(int pos, int len, const QChar *unicode, int size)
{
    if (uint(pos) > uint(d->size))
        return *this;
    if (len > d->size - pos)
        len = d->size - pos;

    uint index = pos;
    replace_helper(&index, 1, len, unicode, size);
    return *this;
}

/*!
  \fn QString &QString::replace(int position, int n, QChar after)
  \overload replace()

  Replaces \a n characters beginning at index \a position with the
  character \a after and returns a reference to this string.
*/
QString &QString::replace(int pos, int len, QChar after)
{
    return replace(pos, len, &after, 1);
}

/*!
  \overload replace()
  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet qstring/main.cpp 41

  \note The replacement text is not rescanned after it is inserted.

  Example:

  \snippet qstring/main.cpp 86
*/
QString &QString::replace(const QString &before, const QString &after, Qt::CaseSensitivity cs)
{
    return replace(before.constData(), before.size(), after.constData(), after.size(), cs);
}

/*!
  \internal
 */
void QString::replace_helper(uint *indices, int nIndices, int blen, const QChar *after, int alen)
{
    // copy *after in case it lies inside our own d->data() area
    // (which we could possibly invalidate via a realloc or corrupt via memcpy operations.)
    QChar *afterBuffer = const_cast<QChar *>(after);
    if (after >= reinterpret_cast<QChar *>(d->data()) && after < reinterpret_cast<QChar *>(d->data()) + d->size) {
        afterBuffer = static_cast<QChar *>(::malloc(alen*sizeof(QChar)));
        Q_CHECK_PTR(afterBuffer);
        ::memcpy(afterBuffer, after, alen*sizeof(QChar));
    }

    QT_TRY {
        if (blen == alen) {
            // replace in place
            detach();
            for (int i = 0; i < nIndices; ++i)
                memcpy(d->data() + indices[i], afterBuffer, alen * sizeof(QChar));
        } else if (alen < blen) {
            // replace from front
            detach();
            uint to = indices[0];
            if (alen)
                memcpy(d->data()+to, after, alen*sizeof(QChar));
            to += alen;
            uint movestart = indices[0] + blen;
            for (int i = 1; i < nIndices; ++i) {
                int msize = indices[i] - movestart;
                if (msize > 0) {
                    memmove(d->data() + to, d->data() + movestart, msize * sizeof(QChar));
                    to += msize;
                }
                if (alen) {
                    memcpy(d->data() + to, afterBuffer, alen*sizeof(QChar));
                    to += alen;
                }
                movestart = indices[i] + blen;
            }
            int msize = d->size - movestart;
            if (msize > 0)
                memmove(d->data() + to, d->data() + movestart, msize * sizeof(QChar));
            resize(d->size - nIndices*(blen-alen));
        } else {
            // replace from back
            int adjust = nIndices*(alen-blen);
            int newLen = d->size + adjust;
            int moveend = d->size;
            resize(newLen);

            while (nIndices) {
                --nIndices;
                int movestart = indices[nIndices] + blen;
                int insertstart = indices[nIndices] + nIndices*(alen-blen);
                int moveto = insertstart + alen;
                memmove(d->data() + moveto, d->data() + movestart,
                        (moveend - movestart)*sizeof(QChar));
                memcpy(d->data() + insertstart, afterBuffer, alen*sizeof(QChar));
                moveend = movestart-blen;
            }
        }
    } QT_CATCH(const std::bad_alloc &) {
        if (afterBuffer != after)
            ::free(afterBuffer);
        QT_RETHROW;
    }
    if (afterBuffer != after)
        ::free(afterBuffer);
}

/*!
  \since 4.5
  \overload replace()

  Replaces each occurrence in this string of the first \a blen
  characters of \a before with the first \a alen characters of \a
  after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.
*/
QString &QString::replace(const QChar *before, int blen,
                          const QChar *after, int alen,
                          Qt::CaseSensitivity cs)
{
    if (d->size == 0) {
        if (blen)
            return *this;
    } else {
        if (cs == Qt::CaseSensitive && before == after && blen == alen)
            return *this;
    }
    if (alen == 0 && blen == 0)
        return *this;

    QStringMatcher matcher(before, blen, cs);

    int index = 0;
    while (1) {
        uint indices[1024];
        uint pos = 0;
        while (pos < 1023) {
            index = matcher.indexIn(*this, index);
            if (index == -1)
                break;
            indices[pos++] = index;
            index += blen;
            // avoid infinite loop
            if (!blen)
                index++;
        }
        if (!pos)
            break;

        replace_helper(indices, pos, blen, after, alen);

        if (index == -1)
            break;
        // index has to be adjusted in case we get back into the loop above.
        index += pos*(alen-blen);
    }

    return *this;
}

/*!
  \overload replace()
  Replaces every occurrence of the character \a ch in the string with
  \a after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.
*/
QString& QString::replace(QChar ch, const QString &after, Qt::CaseSensitivity cs)
{
    if (after.d->size == 0)
        return remove(ch, cs);

    if (after.d->size == 1)
        return replace(ch, after.d->data()[0], cs);

    if (d->size == 0)
        return *this;

    ushort cc = (cs == Qt::CaseSensitive ? ch.unicode() : ch.toCaseFolded().unicode());

    int index = 0;
    while (1) {
        uint indices[1024];
        uint pos = 0;
        if (cs == Qt::CaseSensitive) {
            while (pos < 1023 && index < d->size) {
                if (d->data()[index] == cc)
                    indices[pos++] = index;
                index++;
            }
        } else {
            while (pos < 1023 && index < d->size) {
                if (QChar::toCaseFolded(d->data()[index]) == cc)
                    indices[pos++] = index;
                index++;
            }
        }
        if (!pos)
            break;

        replace_helper(indices, pos, 1, after.constData(), after.d->size);

        if (index == -1)
            break;
        // index has to be adjusted in case we get back into the loop above.
        index += pos*(after.d->size - 1);
    }
    return *this;
}

/*!
  \overload replace()
  Replaces every occurrence of the character \a before with the
  character \a after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.
*/
QString& QString::replace(QChar before, QChar after, Qt::CaseSensitivity cs)
{
    ushort a = after.unicode();
    ushort b = before.unicode();
    if (d->size) {
        detach();
        ushort *i = d->data();
        const ushort *e = i + d->size;
        if (cs == Qt::CaseSensitive) {
            for (; i != e; ++i)
                if (*i == b)
                    *i = a;
        } else {
            b = foldCase(b);
            for (; i != e; ++i)
                if (foldCase(*i) == b)
                    *i = a;
        }
    }
    return *this;
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
QString &QString::replace(QLatin1String before, QLatin1String after, Qt::CaseSensitivity cs)
{
    int alen = after.size();
    int blen = before.size();
    QVarLengthArray<ushort> a(alen);
    QVarLengthArray<ushort> b(blen);
    qt_from_latin1(a.data(), after.latin1(), alen);
    qt_from_latin1(b.data(), before.latin1(), blen);
    return replace((const QChar *)b.data(), blen, (const QChar *)a.data(), alen, cs);
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
QString &QString::replace(QLatin1String before, const QString &after, Qt::CaseSensitivity cs)
{
    int blen = before.size();
    QVarLengthArray<ushort> b(blen);
    qt_from_latin1(b.data(), before.latin1(), blen);
    return replace((const QChar *)b.data(), blen, after.constData(), after.d->size, cs);
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence of the string \a before with the string \a
  after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
QString &QString::replace(const QString &before, QLatin1String after, Qt::CaseSensitivity cs)
{
    int alen = after.size();
    QVarLengthArray<ushort> a(alen);
    qt_from_latin1(a.data(), after.latin1(), alen);
    return replace(before.constData(), before.d->size, (const QChar *)a.data(), alen, cs);
}

/*!
  \since 4.5
  \overload replace()

  Replaces every occurrence of the character \a c with the string \a
  after and returns a reference to this string.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \note The text is not rescanned after a replacement.
*/
QString &QString::replace(QChar c, QLatin1String after, Qt::CaseSensitivity cs)
{
    int alen = after.size();
    QVarLengthArray<ushort> a(alen);
    qt_from_latin1(a.data(), after.latin1(), alen);
    return replace(&c, 1, (const QChar *)a.data(), alen, cs);
}


/*!
  \relates QString
  Returns \c true if string \a s1 is equal to string \a s2; otherwise
  returns \c false.

  The comparison is based exclusively on the numeric Unicode values of
  the characters and is very fast, but is not what a human would
  expect. Consider sorting user-interface strings with
  localeAwareCompare().
*/
bool operator==(const QString &s1, const QString &s2)
{
    if (s1.d->size != s2.d->size)
        return false;

    return qMemEquals(s1.d->data(), s2.d->data(), s1.d->size);
}

/*!
    \overload operator==()
    Returns \c true if this string is equal to \a other; otherwise
    returns \c false.
*/
bool QString::operator==(QLatin1String other) const
{
    if (d->size != other.size())
        return false;

    if (!other.size())
        return isEmpty();

    return compare_helper(data(), size(), other, Qt::CaseSensitive) == 0;
}

/*! \fn bool QString::operator==(const QByteArray &other) const

    \overload operator==()

    The \a other byte array is converted to a QString using the
    fromUtf8() function. This function stops conversion at the
    first NUL character found, or the end of the byte array.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    Returns \c true if this string is lexically equal to the parameter
    string \a other. Otherwise returns \c false.
*/

/*! \fn bool QString::operator==(const char *other) const

    \overload operator==()

    The \a other const char pointer is converted to a QString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
   \relates QString
    Returns \c true if string \a s1 is lexically less than string
    \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.
*/
bool operator<(const QString &s1, const QString &s2)
{
    return ucstrcmp(s1.constData(), s1.length(), s2.constData(), s2.length()) < 0;
}
/*!
   \overload operator<()

    Returns \c true if this string is lexically less than the parameter
    string called \a other; otherwise returns \c false.
*/
bool QString::operator<(QLatin1String other) const
{
    const uchar *c = (const uchar *) other.latin1();
    if (!c || *c == 0)
        return false;

    return compare_helper(data(), size(), other, Qt::CaseSensitive) < 0;
}

/*! \fn bool QString::operator<(const QByteArray &other) const

    \overload operator<()

    The \a other byte array is converted to a QString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn bool QString::operator<(const char *other) const

    Returns \c true if this string is lexically less than string \a other.
    Otherwise returns \c false.

    \overload operator<()

    The \a other const char pointer is converted to a QString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn bool operator<=(const QString &s1, const QString &s2)

    \relates QString

    Returns \c true if string \a s1 is lexically less than or equal to
    string \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*! \fn bool QString::operator<=(QLatin1String other) const

    Returns \c true if this string is lexically less than or equal to
    parameter string \a other. Otherwise returns \c false.

    \overload operator<=()
*/

/*! \fn bool QString::operator<=(const QByteArray &other) const

    \overload operator<=()

    The \a other byte array is converted to a QString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn bool QString::operator<=(const char *other) const

    \overload operator<=()

    The \a other const char pointer is converted to a QString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn bool operator>(const QString &s1, const QString &s2)
    \relates QString

    Returns \c true if string \a s1 is lexically greater than string \a s2;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*!
   \overload operator>()

    Returns \c true if this string is lexically greater than the parameter
    string \a other; otherwise returns \c false.
*/
bool QString::operator>(QLatin1String other) const
{
    const uchar *c = (const uchar *) other.latin1();
    if (!c || *c == '\0')
        return !isEmpty();

    return compare_helper(data(), size(), other, Qt::CaseSensitive) > 0;
}

/*! \fn bool QString::operator>(const QByteArray &other) const

    \overload operator>()

    The \a other byte array is converted to a QString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn bool QString::operator>(const char *other) const

    \overload operator>()

    The \a other const char pointer is converted to a QString using
    the fromUtf8() function.

    You can disable this operator by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*! \fn bool operator>=(const QString &s1, const QString &s2)
    \relates QString

    Returns \c true if string \a s1 is lexically greater than or equal to
    string \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*! \fn bool QString::operator>=(QLatin1String other) const

    Returns \c true if this string is lexically greater than or equal to parameter
    string \a other. Otherwise returns \c false.

    \overload operator>=()
*/

/*! \fn bool QString::operator>=(const QByteArray &other) const

    \overload operator>=()

    The \a other byte array is converted to a QString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded in
    the byte array, they will be included in the transformation.

    You can disable this operator by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*! \fn bool QString::operator>=(const char *other) const

    \overload operator>=()

    The \a other const char pointer is converted to a QString using
    the fromUtf8() function.

    You can disable this operator by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*! \fn bool operator!=(const QString &s1, const QString &s2)
    \relates QString

    Returns \c true if string \a s1 is not equal to string \a s2;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    localeAwareCompare().
*/

/*! \fn bool QString::operator!=(QLatin1String other) const

    Returns \c true if this string is not equal to parameter string \a other.
    Otherwise returns \c false.

    \overload operator!=()
*/

/*! \fn bool QString::operator!=(const QByteArray &other) const

    \overload operator!=()

    The \a other byte array is converted to a QString using the
    fromUtf8() function. If any NUL characters ('\\0') are embedded
    in the byte array, they will be included in the transformation.

    You can disable this operator by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*! \fn bool QString::operator!=(const char *other) const

    \overload operator!=()

    The \a other const char pointer is converted to a QString using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
  Returns the index position of the first occurrence of the string \a
  str in this string, searching forward from index position \a
  from. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet qstring/main.cpp 24

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa lastIndexOf(), contains(), count()
*/
int QString::indexOf(const QString &str, int from, Qt::CaseSensitivity cs) const
{
    return qFindString(unicode(), length(), from, str.unicode(), str.length(), cs);
}

/*!
  \since 4.5
  Returns the index position of the first occurrence of the string \a
  str in this string, searching forward from index position \a
  from. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet qstring/main.cpp 24

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa lastIndexOf(), contains(), count()
*/

int QString::indexOf(QLatin1String str, int from, Qt::CaseSensitivity cs) const
{
    return qt_find_latin1_string(unicode(), size(), str, from, cs);
}

int qFindString(
    const QChar *haystack0, int haystackLen, int from,
    const QChar *needle0, int needleLen, Qt::CaseSensitivity cs)
{
    const int l = haystackLen;
    const int sl = needleLen;
    if (from < 0)
        from += l;
    if (uint(sl + from) > (uint)l)
        return -1;
    if (!sl)
        return from;
    if (!l)
        return -1;

    if (sl == 1)
        return findChar(haystack0, haystackLen, needle0[0], from, cs);

    /*
        We use the Boyer-Moore algorithm in cases where the overhead
        for the skip table should pay off, otherwise we use a simple
        hash function.
    */
    if (l > 500 && sl > 5)
        return qFindStringBoyerMoore(haystack0, haystackLen, from,
            needle0, needleLen, cs);

    /*
        We use some hashing for efficiency's sake. Instead of
        comparing strings, we compare the hash value of str with that
        of a part of this QString. Only if that matches, we call
        ucstrncmp() or ucstrnicmp().
    */
    const ushort *needle = (const ushort *)needle0;
    const ushort *haystack = (const ushort *)haystack0 + from;
    const ushort *end = (const ushort *)haystack0 + (l-sl);
    const uint sl_minus_1 = sl - 1;
    uint hashNeedle = 0, hashHaystack = 0;
    int idx;

    if (cs == Qt::CaseSensitive) {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = ((hashNeedle<<1) + needle[idx]);
            hashHaystack = ((hashHaystack<<1) + haystack[idx]);
        }
        hashHaystack -= haystack[sl_minus_1];

        while (haystack <= end) {
            hashHaystack += haystack[sl_minus_1];
            if (hashHaystack == hashNeedle
                 && ucstrncmp((const QChar *)needle, (const QChar *)haystack, sl) == 0)
                return haystack - (const ushort *)haystack0;

            REHASH(*haystack);
            ++haystack;
        }
    } else {
        const ushort *haystack_start = (const ushort *)haystack0;
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = (hashNeedle<<1) + foldCase(needle + idx, needle);
            hashHaystack = (hashHaystack<<1) + foldCase(haystack + idx, haystack_start);
        }
        hashHaystack -= foldCase(haystack + sl_minus_1, haystack_start);

        while (haystack <= end) {
            hashHaystack += foldCase(haystack + sl_minus_1, haystack_start);
            if (hashHaystack == hashNeedle && ucstrnicmp(needle, haystack, sl) == 0)
                return haystack - (const ushort *)haystack0;

            REHASH(foldCase(haystack, haystack_start));
            ++haystack;
        }
    }
    return -1;
}

/*!
    \overload indexOf()

    Returns the index position of the first occurrence of the
    character \a ch in the string, searching forward from index
    position \a from. Returns -1 if \a ch could not be found.
*/
int QString::indexOf(QChar ch, int from, Qt::CaseSensitivity cs) const
{
    return findChar(unicode(), length(), ch, from, cs);
}

/*!
    \since 4.8

    \overload indexOf()

    Returns the index position of the first occurrence of the string
    reference \a str in this string, searching forward from index
    position \a from. Returns -1 if \a str is not found.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.
*/
int QString::indexOf(const QStringRef &str, int from, Qt::CaseSensitivity cs) const
{
    return qFindString(unicode(), length(), from, str.unicode(), str.length(), cs);
}

static int lastIndexOfHelper(const ushort *haystack, int from, const ushort *needle, int sl, Qt::CaseSensitivity cs)
{
    /*
        See indexOf() for explanations.
    */

    const ushort *end = haystack;
    haystack += from;
    const uint sl_minus_1 = sl - 1;
    const ushort *n = needle+sl_minus_1;
    const ushort *h = haystack+sl_minus_1;
    uint hashNeedle = 0, hashHaystack = 0;
    int idx;

    if (cs == Qt::CaseSensitive) {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = ((hashNeedle<<1) + *(n-idx));
            hashHaystack = ((hashHaystack<<1) + *(h-idx));
        }
        hashHaystack -= *haystack;

        while (haystack >= end) {
            hashHaystack += *haystack;
            if (hashHaystack == hashNeedle
                 && ucstrncmp((const QChar *)needle, (const QChar *)haystack, sl) == 0)
                return haystack - end;
            --haystack;
            REHASH(haystack[sl]);
        }
    } else {
        for (idx = 0; idx < sl; ++idx) {
            hashNeedle = ((hashNeedle<<1) + foldCase(n-idx, needle));
            hashHaystack = ((hashHaystack<<1) + foldCase(h-idx, end));
        }
        hashHaystack -= foldCase(haystack, end);

        while (haystack >= end) {
            hashHaystack += foldCase(haystack, end);
            if (hashHaystack == hashNeedle && ucstrnicmp(needle, haystack, sl) == 0)
                return haystack - end;
            --haystack;
            REHASH(foldCase(haystack + sl, end));
        }
    }
    return -1;
}

/*!
  Returns the index position of the last occurrence of the string \a
  str in this string, searching backward from index position \a
  from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet qstring/main.cpp 29

  \sa indexOf(), contains(), count()
*/
int QString::lastIndexOf(const QString &str, int from, Qt::CaseSensitivity cs) const
{
    const int sl = str.d->size;
    if (sl == 1)
        return lastIndexOf(QChar(str.d->data()[0]), from, cs);

    const int l = d->size;
    if (from < 0)
        from += l;
    int delta = l-sl;
    if (from == l && sl == 0)
        return from;
    if (uint(from) >= uint(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    return lastIndexOfHelper(d->data(), from, str.d->data(), str.d->size, cs);
}

/*!
  \since 4.5
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string \a
  str in this string, searching backward from index position \a
  from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  Example:

  \snippet qstring/main.cpp 29

  \sa indexOf(), contains(), count()
*/
int QString::lastIndexOf(QLatin1String str, int from, Qt::CaseSensitivity cs) const
{
    const int sl = str.size();
    if (sl == 1)
        return lastIndexOf(QLatin1Char(str.latin1()[0]), from, cs);

    const int l = d->size;
    if (from < 0)
        from += l;
    int delta = l-sl;
    if (from == l && sl == 0)
        return from;
    if (uint(from) >= uint(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    QVarLengthArray<ushort> s(sl);
    qt_from_latin1(s.data(), str.latin1(), sl);

    return lastIndexOfHelper(d->data(), from, s.data(), sl, cs);
}

/*!
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the character
  \a ch, searching backward from position \a from.
*/
int QString::lastIndexOf(QChar ch, int from, Qt::CaseSensitivity cs) const
{
    return qt_last_index_of(unicode(), size(), ch, from, cs);
}

/*!
  \since 4.8
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string
  reference \a str in this string, searching backward from index
  position \a from. If \a from is -1 (default), the search starts at
  the last character; if \a from is -2, at the next to last character
  and so on. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa indexOf(), contains(), count()
*/
int QString::lastIndexOf(const QStringRef &str, int from, Qt::CaseSensitivity cs) const
{
    const int sl = str.size();
    if (sl == 1)
        return lastIndexOf(str.at(0), from, cs);

    const int l = d->size;
    if (from < 0)
        from += l;
    int delta = l - sl;
    if (from == l && sl == 0)
        return from;
    if (uint(from) >= uint(l) || delta < 0)
    return -1;
    if (from > delta)
        from = delta;

    return lastIndexOfHelper(d->data(), from, reinterpret_cast<const ushort*>(str.unicode()),
                             str.size(), cs);
}


#if !(defined(QT_NO_REGEXP) && defined(QT_NO_REGULAREXPRESSION))
struct QStringCapture
{
    int pos;
    int len;
    int no;
};
Q_DECLARE_TYPEINFO(QStringCapture, Q_PRIMITIVE_TYPE);
#endif

#ifndef QT_NO_REGEXP

/*!
  \overload replace()

  Replaces every occurrence of the regular expression \a rx in the
  string with \a after. Returns a reference to the string. For
  example:

  \snippet qstring/main.cpp 42

  For regular expressions containing \l{capturing parentheses},
  occurrences of \b{\\1}, \b{\\2}, ..., in \a after are replaced
  with \a{rx}.cap(1), cap(2), ...

  \snippet qstring/main.cpp 43

  \sa indexOf(), lastIndexOf(), remove(), QRegExp::cap()
*/
QString& QString::replace(const QRegExp &rx, const QString &after)
{
    QRegExp rx2(rx);

    if (isEmpty() && rx2.indexIn(*this) == -1)
        return *this;

    reallocData(uint(d->size) + 1u);

    int index = 0;
    int numCaptures = rx2.captureCount();
    int al = after.length();
    QRegExp::CaretMode caretMode = QRegExp::CaretAtZero;

    if (numCaptures > 0) {
        const QChar *uc = after.unicode();
        int numBackRefs = 0;

        for (int i = 0; i < al - 1; i++) {
            if (uc[i] == QLatin1Char('\\')) {
                int no = uc[i + 1].digitValue();
                if (no > 0 && no <= numCaptures)
                    numBackRefs++;
            }
        }

        /*
            This is the harder case where we have back-references.
        */
        if (numBackRefs > 0) {
            QVarLengthArray<QStringCapture, 16> captures(numBackRefs);
            int j = 0;

            for (int i = 0; i < al - 1; i++) {
                if (uc[i] == QLatin1Char('\\')) {
                    int no = uc[i + 1].digitValue();
                    if (no > 0 && no <= numCaptures) {
                        QStringCapture capture;
                        capture.pos = i;
                        capture.len = 2;

                        if (i < al - 2) {
                            int secondDigit = uc[i + 2].digitValue();
                            if (secondDigit != -1 && ((no * 10) + secondDigit) <= numCaptures) {
                                no = (no * 10) + secondDigit;
                                ++capture.len;
                            }
                        }

                        capture.no = no;
                        captures[j++] = capture;
                    }
                }
            }

            while (index <= length()) {
                index = rx2.indexIn(*this, index, caretMode);
                if (index == -1)
                    break;

                QString after2(after);
                for (j = numBackRefs - 1; j >= 0; j--) {
                    const QStringCapture &capture = captures[j];
                    after2.replace(capture.pos, capture.len, rx2.cap(capture.no));
                }

                replace(index, rx2.matchedLength(), after2);
                index += after2.length();

                // avoid infinite loop on 0-length matches (e.g., QRegExp("[a-z]*"))
                if (rx2.matchedLength() == 0)
                    ++index;

                caretMode = QRegExp::CaretWontMatch;
            }
            return *this;
        }
    }

    /*
        This is the simple and optimized case where we don't have
        back-references.
    */
    while (index != -1) {
        struct {
            int pos;
            int length;
        } replacements[2048];

        int pos = 0;
        int adjust = 0;
        while (pos < 2047) {
            index = rx2.indexIn(*this, index, caretMode);
            if (index == -1)
                break;
            int ml = rx2.matchedLength();
            replacements[pos].pos = index;
            replacements[pos++].length = ml;
            index += ml;
            adjust += al - ml;
            // avoid infinite loop
            if (!ml)
                index++;
        }
        if (!pos)
            break;
        replacements[pos].pos = d->size;
        int newlen = d->size + adjust;

        // to continue searching at the right position after we did
        // the first round of replacements
        if (index != -1)
            index += adjust;
        QString newstring;
        newstring.reserve(newlen + 1);
        QChar *newuc = newstring.data();
        QChar *uc = newuc;
        int copystart = 0;
        int i = 0;
        while (i < pos) {
            int copyend = replacements[i].pos;
            int size = copyend - copystart;
            memcpy(uc, d->data() + copystart, size * sizeof(QChar));
            uc += size;
            memcpy(uc, after.d->data(), al * sizeof(QChar));
            uc += al;
            copystart = copyend + replacements[i].length;
            i++;
        }
        memcpy(uc, d->data() + copystart, (d->size - copystart) * sizeof(QChar));
        newstring.resize(newlen);
        *this = newstring;
        caretMode = QRegExp::CaretWontMatch;
    }
    return *this;
}
#endif

#ifndef QT_NO_REGULAREXPRESSION
#ifndef QT_BOOTSTRAPPED
/*!
  \overload replace()
  \since 5.0

  Replaces every occurrence of the regular expression \a re in the
  string with \a after. Returns a reference to the string. For
  example:

  \snippet qstring/main.cpp 87

  For regular expressions containing capturing groups,
  occurrences of \b{\\1}, \b{\\2}, ..., in \a after are replaced
  with the string captured by the corresponding capturing group.

  \snippet qstring/main.cpp 88

  \sa indexOf(), lastIndexOf(), remove(), QRegularExpression, QRegularExpressionMatch
*/
QString &QString::replace(const QRegularExpression &re, const QString &after)
{
    if (!re.isValid()) {
        qWarning("QString::replace: invalid QRegularExpression object");
        return *this;
    }

    const QString copy(*this);
    QRegularExpressionMatchIterator iterator = re.globalMatch(copy);
    if (!iterator.hasNext()) // no matches at all
        return *this;

    reallocData(uint(d->size) + 1u);

    int numCaptures = re.captureCount();

    // 1. build the backreferences vector, holding where the backreferences
    // are in the replacement string
    QVector<QStringCapture> backReferences;
    const int al = after.length();
    const QChar *ac = after.unicode();

    for (int i = 0; i < al - 1; i++) {
        if (ac[i] == QLatin1Char('\\')) {
            int no = ac[i + 1].digitValue();
            if (no > 0 && no <= numCaptures) {
                QStringCapture backReference;
                backReference.pos = i;
                backReference.len = 2;

                if (i < al - 2) {
                    int secondDigit = ac[i + 2].digitValue();
                    if (secondDigit != -1 && ((no * 10) + secondDigit) <= numCaptures) {
                        no = (no * 10) + secondDigit;
                        ++backReference.len;
                    }
                }

                backReference.no = no;
                backReferences.append(backReference);
            }
        }
    }

    // 2. iterate on the matches. For every match, copy in chunks
    // - the part before the match
    // - the after string, with the proper replacements for the backreferences

    int newLength = 0; // length of the new string, with all the replacements
    int lastEnd = 0;
    QVector<QStringRef> chunks;
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        int len;
        // add the part before the match
        len = match.capturedStart() - lastEnd;
        if (len > 0) {
            chunks << copy.midRef(lastEnd, len);
            newLength += len;
        }

        lastEnd = 0;
        // add the after string, with replacements for the backreferences
        foreach (const QStringCapture &backReference, backReferences) {
            // part of "after" before the backreference
            len = backReference.pos - lastEnd;
            if (len > 0) {
                chunks << after.midRef(lastEnd, len);
                newLength += len;
            }

            // backreference itself
            len = match.capturedLength(backReference.no);
            if (len > 0) {
                chunks << copy.midRef(match.capturedStart(backReference.no), len);
                newLength += len;
            }

            lastEnd = backReference.pos + backReference.len;
        }

        // add the last part of the after string
        len = after.length() - lastEnd;
        if (len > 0) {
            chunks << after.midRef(lastEnd, len);
            newLength += len;
        }

        lastEnd = match.capturedEnd();
    }

    // 3. trailing string after the last match
    if (copy.length() > lastEnd) {
        chunks << copy.midRef(lastEnd);
        newLength += copy.length() - lastEnd;
    }

    // 4. assemble the chunks together
    resize(newLength);
    int i = 0;
    QChar *uc = data();
    foreach (const QStringRef &chunk, chunks) {
        int len = chunk.length();
        memcpy(uc + i, chunk.unicode(), len * sizeof(QChar));
        i += len;
    }

    return *this;
}
#endif // QT_BOOTSTRAPPED
#endif // QT_NO_REGULAREXPRESSION

/*!
    Returns the number of (potentially overlapping) occurrences of
    the string \a str in this string.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa contains(), indexOf()
*/

int QString::count(const QString &str, Qt::CaseSensitivity cs) const
{
    return qt_string_count(unicode(), size(), str.unicode(), str.size(), cs);
}

/*!
    \overload count()

    Returns the number of occurrences of character \a ch in the string.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa contains(), indexOf()
*/

int QString::count(QChar ch, Qt::CaseSensitivity cs) const
{
    return qt_string_count(unicode(), size(), ch, cs);
    }

/*!
    \since 4.8
    \overload count()
    Returns the number of (potentially overlapping) occurrences of the
    string reference \a str in this string.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa contains(), indexOf()
*/
int QString::count(const QStringRef &str, Qt::CaseSensitivity cs) const
{
    return qt_string_count(unicode(), size(), str.unicode(), str.size(), cs);
}


/*! \fn bool QString::contains(const QString &str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const

    Returns \c true if this string contains an occurrence of the string
    \a str; otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    Example:
    \snippet qstring/main.cpp 17

    \sa indexOf(), count()
*/

/*! \fn bool QString::contains(QLatin1String str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
    \since 5.3

    \overload contains()

    Returns \c true if this string contains an occurrence of the latin-1 string
    \a str; otherwise returns \c false.
*/

/*! \fn bool QString::contains(QChar ch, Qt::CaseSensitivity cs = Qt::CaseSensitive) const

    \overload contains()

    Returns \c true if this string contains an occurrence of the
    character \a ch; otherwise returns \c false.
*/

/*! \fn bool QString::contains(const QStringRef &str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
    \since 4.8

    Returns \c true if this string contains an occurrence of the string
    reference \a str; otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
*/

/*! \fn bool QString::contains(const QRegExp &rx) const

    \overload contains()

    Returns \c true if the regular expression \a rx matches somewhere in
    this string; otherwise returns \c false.
*/

/*! \fn bool QString::contains(QRegExp &rx) const
    \overload contains()
    \since 4.5

    Returns \c true if the regular expression \a rx matches somewhere in
    this string; otherwise returns \c false.

    If there is a match, the \a rx regular expression will contain the
    matched captures (see QRegExp::matchedLength, QRegExp::cap).
*/

#ifndef QT_NO_REGEXP
/*!
    \overload indexOf()

    Returns the index position of the first match of the regular
    expression \a rx in the string, searching forward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    Example:

    \snippet qstring/main.cpp 25
*/
int QString::indexOf(const QRegExp& rx, int from) const
{
    QRegExp rx2(rx);
    return rx2.indexIn(*this, from);
}

/*!
    \overload indexOf()
    \since 4.5

    Returns the index position of the first match of the regular
    expression \a rx in the string, searching forward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    If there is a match, the \a rx regular expression will contain the
    matched captures (see QRegExp::matchedLength, QRegExp::cap).

    Example:

    \snippet qstring/main.cpp 25
*/
int QString::indexOf(QRegExp& rx, int from) const
{
    return rx.indexIn(*this, from);
}

/*!
    \overload lastIndexOf()

    Returns the index position of the last match of the regular
    expression \a rx in the string, searching backward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    Example:

    \snippet qstring/main.cpp 30
*/
int QString::lastIndexOf(const QRegExp& rx, int from) const
{
    QRegExp rx2(rx);
    return rx2.lastIndexIn(*this, from);
}

/*!
    \overload lastIndexOf()
    \since 4.5

    Returns the index position of the last match of the regular
    expression \a rx in the string, searching backward from index
    position \a from. Returns -1 if \a rx didn't match anywhere.

    If there is a match, the \a rx regular expression will contain the
    matched captures (see QRegExp::matchedLength, QRegExp::cap).

    Example:

    \snippet qstring/main.cpp 30
*/
int QString::lastIndexOf(QRegExp& rx, int from) const
{
    return rx.lastIndexIn(*this, from);
}

/*!
    \overload count()

    Returns the number of times the regular expression \a rx matches
    in the string.

    This function counts overlapping matches, so in the example
    below, there are four instances of "ana" or "ama":

    \snippet qstring/main.cpp 18

*/
int QString::count(const QRegExp& rx) const
{
    QRegExp rx2(rx);
    int count = 0;
    int index = -1;
    int len = length();
    while (index < len - 1) {                 // count overlapping matches
        index = rx2.indexIn(*this, index + 1);
        if (index == -1)
            break;
        count++;
    }
    return count;
}
#endif // QT_NO_REGEXP

#ifndef QT_NO_REGULAREXPRESSION
#ifndef QT_BOOTSTRAPPED
/*!
    \overload indexOf()
    \since 5.0

    Returns the index position of the first match of the regular
    expression \a re in the string, searching forward from index
    position \a from. Returns -1 if \a re didn't match anywhere.

    Example:

    \snippet qstring/main.cpp 93
*/
int QString::indexOf(const QRegularExpression& re, int from) const
{
    return indexOf(re, from, Q_NULLPTR);
}

/*!
    \overload
    \since 5.5

    Returns the index position of the first match of the regular
    expression \a re in the string, searching forward from index
    position \a from. Returns -1 if \a re didn't match anywhere.

    If the match is successful and \a rmatch is not a null pointer, it also
    writes the results of the match into the QRegularExpressionMatch object
    pointed to by \a rmatch.

    Example:

    \snippet qstring/main.cpp 99
*/
int QString::indexOf(const QRegularExpression &re, int from, QRegularExpressionMatch *rmatch) const
{
    if (!re.isValid()) {
        qWarning("QString::indexOf: invalid QRegularExpression object");
        return -1;
    }

    QRegularExpressionMatch match = re.match(*this, from);
    if (match.hasMatch()) {
        const int ret = match.capturedStart();
        if (rmatch)
            *rmatch = qMove(match);
        return ret;
    }

    return -1;
}

/*!
    \overload lastIndexOf()
    \since 5.0

    Returns the index position of the last match of the regular
    expression \a re in the string, which starts before the index
    position \a from. Returns -1 if \a re didn't match anywhere.

    Example:

    \snippet qstring/main.cpp 94
*/
int QString::lastIndexOf(const QRegularExpression &re, int from) const
{
    return lastIndexOf(re, from, Q_NULLPTR);
}

/*!
    \overload
    \since 5.5

    Returns the index position of the last match of the regular
    expression \a re in the string, which starts before the index
    position \a from. Returns -1 if \a re didn't match anywhere.

    If the match is successful and \a rmatch is not a null pointer, it also
    writes the results of the match into the QRegularExpressionMatch object
    pointed to by \a rmatch.

    Example:

    \snippet qstring/main.cpp 100
*/
int QString::lastIndexOf(const QRegularExpression &re, int from, QRegularExpressionMatch *rmatch) const
{
    if (!re.isValid()) {
        qWarning("QString::lastIndexOf: invalid QRegularExpression object");
        return -1;
    }

    int endpos = (from < 0) ? (size() + from + 1) : (from + 1);
    QRegularExpressionMatchIterator iterator = re.globalMatch(*this);
    int lastIndex = -1;
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        int start = match.capturedStart();
        if (start < endpos) {
            lastIndex = start;
            if (rmatch)
                *rmatch = qMove(match);
        } else {
            break;
        }
    }

    return lastIndex;
}

/*! \overload contains()
    \since 5.0

    Returns \c true if the regular expression \a re matches somewhere in
    this string; otherwise returns \c false.
*/
bool QString::contains(const QRegularExpression &re) const
{
    return contains(re, Q_NULLPTR);
}

/*!
    \overload contains()
    \since 5.1

    Returns \c true if the regular expression \a re matches somewhere in this
    string; otherwise returns \c false.

    If the match is successful and \a match is not a null pointer, it also
    writes the results of the match into the QRegularExpressionMatch object
    pointed to by \a match.

    \sa QRegularExpression::match()
*/

bool QString::contains(const QRegularExpression &re, QRegularExpressionMatch *match) const
{
    if (!re.isValid()) {
        qWarning("QString::contains: invalid QRegularExpression object");
        return false;
    }
    QRegularExpressionMatch m = re.match(*this);
    bool hasMatch = m.hasMatch();
    if (hasMatch && match)
        *match = qMove(m);
    return hasMatch;
}

/*!
    \overload count()
    \since 5.0

    Returns the number of times the regular expression \a re matches
    in the string.

    This function counts overlapping matches, so in the example
    below, there are four instances of "ana" or "ama":

    \snippet qstring/main.cpp 95
*/
int QString::count(const QRegularExpression &re) const
{
    if (!re.isValid()) {
        qWarning("QString::count: invalid QRegularExpression object");
        return 0;
    }
    int count = 0;
    int index = -1;
    int len = length();
    while (index < len - 1) {
        QRegularExpressionMatch match = re.match(*this, index + 1);
        if (!match.hasMatch())
            break;
        index = match.capturedStart();
        count++;
    }
    return count;
}
#endif // QT_BOOTSTRAPPED
#endif // QT_NO_REGULAREXPRESSION

/*! \fn int QString::count() const

    \overload count()

    Same as size().
*/


/*!
    \enum QString::SectionFlag

    This enum specifies flags that can be used to affect various
    aspects of the section() function's behavior with respect to
    separators and empty fields.

    \value SectionDefault Empty fields are counted, leading and
    trailing separators are not included, and the separator is
    compared case sensitively.

    \value SectionSkipEmpty Treat empty fields as if they don't exist,
    i.e. they are not considered as far as \e start and \e end are
    concerned.

    \value SectionIncludeLeadingSep Include the leading separator (if
    any) in the result string.

    \value SectionIncludeTrailingSep Include the trailing separator
    (if any) in the result string.

    \value SectionCaseInsensitiveSeps Compare the separator
    case-insensitively.

    \sa section()
*/

/*!
    \fn QString QString::section(QChar sep, int start, int end = -1, SectionFlags flags) const

    This function returns a section of the string.

    This string is treated as a sequence of fields separated by the
    character, \a sep. The returned string consists of the fields from
    position \a start to position \a end inclusive. If \a end is not
    specified, all fields from position \a start to the end of the
    string are included. Fields are numbered 0, 1, 2, etc., counting
    from the left, and -1, -2, etc., counting from right to left.

    The \a flags argument can be used to affect some aspects of the
    function's behavior, e.g. whether to be case sensitive, whether
    to skip empty fields and how to deal with leading and trailing
    separators; see \l{SectionFlags}.

    \snippet qstring/main.cpp 52

    If \a start or \a end is negative, we count fields from the right
    of the string, the right-most field being -1, the one from
    right-most field being -2, and so on.

    \snippet qstring/main.cpp 53

    \sa split()
*/

/*!
    \overload section()

    \snippet qstring/main.cpp 51
    \snippet qstring/main.cpp 54

    \sa split()
*/

QString QString::section(const QString &sep, int start, int end, SectionFlags flags) const
{
    const QVector<QStringRef> sections = splitRef(sep, KeepEmptyParts,
                                                  (flags & SectionCaseInsensitiveSeps) ? Qt::CaseInsensitive : Qt::CaseSensitive);
    const int sectionsSize = sections.size();
    if (!(flags & SectionSkipEmpty)) {
        if (start < 0)
            start += sectionsSize;
        if (end < 0)
            end += sectionsSize;
    } else {
        int skip = 0;
        for (int k = 0; k < sectionsSize; ++k) {
            if (sections.at(k).isEmpty())
                skip++;
        }
        if (start < 0)
            start += sectionsSize - skip;
        if (end < 0)
            end += sectionsSize - skip;
    }
    if (start >= sectionsSize || end < 0 || start > end)
        return QString();

    QString ret;
    int first_i = start, last_i = end;
    for (int x = 0, i = 0; x <= end && i < sectionsSize; ++i) {
        const QStringRef &section = sections.at(i);
        const bool empty = section.isEmpty();
        if (x >= start) {
            if(x == start)
                first_i = i;
            if(x == end)
                last_i = i;
            if (x > start && i > 0)
                ret += sep;
            ret += section;
        }
        if (!empty || !(flags & SectionSkipEmpty))
            x++;
    }
    if ((flags & SectionIncludeLeadingSep) && first_i > 0)
        ret.prepend(sep);
    if ((flags & SectionIncludeTrailingSep) && last_i < sectionsSize - 1)
        ret += sep;
    return ret;
}

#if !(defined(QT_NO_REGEXP) && defined(QT_NO_REGULAREXPRESSION))
class qt_section_chunk {
public:
    qt_section_chunk() {}
    qt_section_chunk(int l, QStringRef s) : length(l), string(qMove(s)) {}
    int length;
    QStringRef string;
};
Q_DECLARE_TYPEINFO(qt_section_chunk, Q_MOVABLE_TYPE);

static QString extractSections(const QVector<qt_section_chunk> &sections,
                               int start,
                               int end,
                               QString::SectionFlags flags)
{
    const int sectionsSize = sections.size();

    if (!(flags & QString::SectionSkipEmpty)) {
        if (start < 0)
            start += sectionsSize;
        if (end < 0)
            end += sectionsSize;
    } else {
        int skip = 0;
        for (int k = 0; k < sectionsSize; ++k) {
            const qt_section_chunk &section = sections.at(k);
            if (section.length == section.string.length())
                skip++;
        }
        if (start < 0)
            start += sectionsSize - skip;
        if (end < 0)
            end += sectionsSize - skip;
    }
    if (start >= sectionsSize || end < 0 || start > end)
        return QString();

    QString ret;
    int x = 0;
    int first_i = start, last_i = end;
    for (int i = 0; x <= end && i < sectionsSize; ++i) {
        const qt_section_chunk &section = sections.at(i);
        const bool empty = (section.length == section.string.length());
        if (x >= start) {
            if (x == start)
                first_i = i;
            if (x == end)
                last_i = i;
            if (x != start)
                ret += section.string;
            else
                ret += section.string.mid(section.length);
        }
        if (!empty || !(flags & QString::SectionSkipEmpty))
            x++;
    }

    if ((flags & QString::SectionIncludeLeadingSep) && first_i >= 0) {
        const qt_section_chunk &section = sections.at(first_i);
        ret.prepend(section.string.left(section.length));
    }

    if ((flags & QString::SectionIncludeTrailingSep)
        && last_i < sectionsSize - 1) {
        const qt_section_chunk &section = sections.at(last_i+1);
        ret += section.string.left(section.length);
    }

    return ret;
}
#endif

#ifndef QT_NO_REGEXP
/*!
    \overload section()

    This string is treated as a sequence of fields separated by the
    regular expression, \a reg.

    \snippet qstring/main.cpp 55

    \warning Using this QRegExp version is much more expensive than
    the overloaded string and character versions.

    \sa split(), simplified()
*/
QString QString::section(const QRegExp &reg, int start, int end, SectionFlags flags) const
{
    const QChar *uc = unicode();
    if(!uc)
        return QString();

    QRegExp sep(reg);
    sep.setCaseSensitivity((flags & SectionCaseInsensitiveSeps) ? Qt::CaseInsensitive
                                                                : Qt::CaseSensitive);

    QVector<qt_section_chunk> sections;
    int n = length(), m = 0, last_m = 0, last_len = 0;
    while ((m = sep.indexIn(*this, m)) != -1) {
        sections.append(qt_section_chunk(last_len, QStringRef(this, last_m, m - last_m)));
        last_m = m;
        last_len = sep.matchedLength();
        m += qMax(sep.matchedLength(), 1);
    }
    sections.append(qt_section_chunk(last_len, QStringRef(this, last_m, n - last_m)));

    return extractSections(sections, start, end, flags);
}
#endif

#ifndef QT_NO_REGULAREXPRESSION
#ifndef QT_BOOTSTRAPPED
/*!
    \overload section()
    \since 5.0

    This string is treated as a sequence of fields separated by the
    regular expression, \a re.

    \snippet qstring/main.cpp 89

    \warning Using this QRegularExpression version is much more expensive than
    the overloaded string and character versions.

    \sa split(), simplified()
*/
QString QString::section(const QRegularExpression &re, int start, int end, SectionFlags flags) const
{
    if (!re.isValid()) {
        qWarning("QString::section: invalid QRegularExpression object");
        return QString();
    }

    const QChar *uc = unicode();
    if (!uc)
        return QString();

    QRegularExpression sep(re);
    if (flags & SectionCaseInsensitiveSeps)
        sep.setPatternOptions(sep.patternOptions() | QRegularExpression::CaseInsensitiveOption);

    QVector<qt_section_chunk> sections;
    int n = length(), m = 0, last_m = 0, last_len = 0;
    QRegularExpressionMatchIterator iterator = sep.globalMatch(*this);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        m = match.capturedStart();
        sections.append(qt_section_chunk(last_len, QStringRef(this, last_m, m - last_m)));
        last_m = m;
        last_len = match.capturedLength();
    }
    sections.append(qt_section_chunk(last_len, QStringRef(this, last_m, n - last_m)));

    return extractSections(sections, start, end, flags);
}
#endif // QT_BOOTSTRAPPED
#endif // QT_NO_REGULAREXPRESSION

/*!
    Returns a substring that contains the \a n leftmost characters
    of the string.

    The entire string is returned if \a n is greater than or equal
    to size(), or less than zero.

    \snippet qstring/main.cpp 31

    \sa right(), mid(), startsWith()
*/
QString QString::left(int n)  const
{
    if (uint(n) >= uint(d->size))
        return *this;
    return QString((const QChar*) d->data(), n);
}

/*!
    Returns a substring that contains the \a n rightmost characters
    of the string.

    The entire string is returned if \a n is greater than or equal
    to size(), or less than zero.

    \snippet qstring/main.cpp 48

    \sa left(), mid(), endsWith()
*/
QString QString::right(int n) const
{
    if (uint(n) >= uint(d->size))
        return *this;
    return QString((const QChar*) d->data() + d->size - n, n);
}

/*!
    Returns a string that contains \a n characters of this string,
    starting at the specified \a position index.

    Returns a null string if the \a position index exceeds the
    length of the string. If there are less than \a n characters
    available in the string starting at the given \a position, or if
    \a n is -1 (default), the function returns all characters that
    are available from the specified \a position.

    Example:

    \snippet qstring/main.cpp 34

    \sa left(), right()
*/

QString QString::mid(int position, int n) const
{
    using namespace QtPrivate;
    switch (QContainerImplHelper::mid(d->size, &position, &n)) {
    case QContainerImplHelper::Null:
        return QString();
    case QContainerImplHelper::Empty:
    {
        QStringDataPtr empty = { Data::allocate(0) };
        return QString(empty);
    }
    case QContainerImplHelper::Full:
        return *this;
    case QContainerImplHelper::Subset:
        return QString((const QChar*)d->data() + position, n);
    }
    Q_UNREACHABLE();
    return QString();
}

/*!
    Returns \c true if the string starts with \a s; otherwise returns
    \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \snippet qstring/main.cpp 65

    \sa endsWith()
*/
bool QString::startsWith(const QString& s, Qt::CaseSensitivity cs) const
{
    return qt_starts_with(isNull() ? 0 : unicode(), size(),
                          s.isNull() ? 0 : s.unicode(), s.size(), cs);
}

/*!
  \overload startsWith()
 */
bool QString::startsWith(QLatin1String s, Qt::CaseSensitivity cs) const
{
    return qt_starts_with(isNull() ? 0 : unicode(), size(), s, cs);
}

/*!
  \overload startsWith()

  Returns \c true if the string starts with \a c; otherwise returns
  \c false.
*/
bool QString::startsWith(QChar c, Qt::CaseSensitivity cs) const
{
    return d->size
           && (cs == Qt::CaseSensitive
               ? d->data()[0] == c
               : foldCase(d->data()[0]) == foldCase(c.unicode()));
}

/*!
    \since 4.8
    \overload
    Returns \c true if the string starts with the string reference \a s;
    otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa endsWith()
*/
bool QString::startsWith(const QStringRef &s, Qt::CaseSensitivity cs) const
{
    return qt_starts_with(isNull() ? 0 : unicode(), size(),
                          s.isNull() ? 0 : s.unicode(), s.size(), cs);
}

/*!
    Returns \c true if the string ends with \a s; otherwise returns
    \c false.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \snippet qstring/main.cpp 20

    \sa startsWith()
*/
bool QString::endsWith(const QString& s, Qt::CaseSensitivity cs) const
{
    return qt_ends_with(isNull() ? 0 : unicode(), size(),
                        s.isNull() ? 0 : s.unicode(), s.size(), cs);
    }

/*!
    \since 4.8
    \overload endsWith()
    Returns \c true if the string ends with the string reference \a s;
    otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa startsWith()
*/
bool QString::endsWith(const QStringRef &s, Qt::CaseSensitivity cs) const
{
    return qt_ends_with(isNull() ? 0 : unicode(), size(),
                        s.isNull() ? 0 : s.unicode(), s.size(), cs);
}


/*!
    \overload endsWith()
*/
bool QString::endsWith(QLatin1String s, Qt::CaseSensitivity cs) const
{
    return qt_ends_with(isNull() ? 0 : unicode(), size(), s, cs);
}

/*!
  Returns \c true if the string ends with \a c; otherwise returns
  \c false.

  \overload endsWith()
 */
bool QString::endsWith(QChar c, Qt::CaseSensitivity cs) const
{
    return d->size
           && (cs == Qt::CaseSensitive
               ? d->data()[d->size - 1] == c
               : foldCase(d->data()[d->size - 1]) == foldCase(c.unicode()));
}

QByteArray QString::toLatin1_helper(const QString &string)
{
    if (Q_UNLIKELY(string.isNull()))
        return QByteArray();

    return toLatin1_helper(string.constData(), string.length());
}

QByteArray QString::toLatin1_helper(const QChar *data, int length)
{
    QByteArray ba(length, Qt::Uninitialized);

    // since we own the only copy, we're going to const_cast the constData;
    // that avoids an unnecessary call to detach() and expansion code that will never get used
    qt_to_latin1(reinterpret_cast<uchar *>(const_cast<char *>(ba.constData())),
                 reinterpret_cast<const ushort *>(data), length);
    return ba;
}

QByteArray QString::toLatin1_helper_inplace(QString &s)
{
    if (!s.isDetached())
        return s.toLatin1();

    // We can return our own buffer to the caller.
    // Conversion to Latin-1 always shrinks the buffer by half.
    const ushort *data = reinterpret_cast<const ushort *>(s.constData());
    uint length = s.size();

    // Swap the d pointers.
    // Kids, avert your eyes. Don't try this at home.
    QArrayData *ba_d = s.d;

    // multiply the allocated capacity by sizeof(ushort)
    ba_d->alloc *= sizeof(ushort);

    // reset ourselves to QString()
    s.d = QString().d;

    // do the in-place conversion
    uchar *dst = reinterpret_cast<uchar *>(ba_d->data());
    qt_to_latin1(dst, data, length);
    dst[length] = '\0';

    QByteArrayDataPtr badptr = { ba_d };
    return QByteArray(badptr);
}


/*!
    \fn QByteArray QString::toLatin1() const

    Returns a Latin-1 representation of the string as a QByteArray.

    The returned byte array is undefined if the string contains non-Latin1
    characters. Those characters may be suppressed or replaced with a
    question mark.

    \sa fromLatin1(), toUtf8(), toLocal8Bit(), QTextCodec
*/

/*!
    \fn QByteArray QString::toAscii() const
    \deprecated
    Returns an 8-bit representation of the string as a QByteArray.

    This function does the same as toLatin1().

    Note that, despite the name, this function does not necessarily return an US-ASCII
    (ANSI X3.4-1986) string and its result may not be US-ASCII compatible.

    \sa fromAscii(), toLatin1(), toUtf8(), toLocal8Bit(), QTextCodec
*/

/*!
    \fn QByteArray QString::toLocal8Bit() const

    Returns the local 8-bit representation of the string as a
    QByteArray. The returned byte array is undefined if the string
    contains characters not supported by the local 8-bit encoding.

    QTextCodec::codecForLocale() is used to perform the conversion from
    Unicode. If the locale encoding could not be determined, this function
    does the same as toLatin1().

    If this string contains any characters that cannot be encoded in the
    locale, the returned byte array is undefined. Those characters may be
    suppressed or replaced by another.

    \sa fromLocal8Bit(), toLatin1(), toUtf8(), QTextCodec
*/

QByteArray QString::toLocal8Bit_helper(const QChar *data, int size)
{
#ifndef QT_NO_TEXTCODEC
    QTextCodec *localeCodec = QTextCodec::codecForLocale();
    if (localeCodec)
        return localeCodec->fromUnicode(data, size);
#endif // QT_NO_TEXTCODEC
    return toLatin1_helper(data, size);
}


/*!
    \fn QByteArray QString::toUtf8() const

    Returns a UTF-8 representation of the string as a QByteArray.

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like QString.

    \sa fromUtf8(), toLatin1(), toLocal8Bit(), QTextCodec
*/

QByteArray QString::toUtf8_helper(const QString &str)
{
    if (str.isNull())
        return QByteArray();

    return QUtf8::convertFromUnicode(str.constData(), str.length());
}

/*!
    \since 4.2

    Returns a UCS-4/UTF-32 representation of the string as a QVector<uint>.

    UCS-4 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UCS-4. Any invalid sequence of code units in
    this string is replaced by the Unicode's replacement character
    (QChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned vector is not NUL terminated.

    \sa fromUtf8(), toUtf8(), toLatin1(), toLocal8Bit(), QTextCodec, fromUcs4(), toWCharArray()
*/
QVector<uint> QString::toUcs4() const
{
    QVector<uint> v(length());
    uint *a = v.data();
    int len = toUcs4_helper(d->data(), length(), a);
    v.resize(len);
    return v;
}

QString::Data *QString::fromLatin1_helper(const char *str, int size)
{
    Data *d;
    if (!str) {
        d = Data::sharedNull();
    } else if (size == 0 || (!*str && size < 0)) {
        d = Data::allocate(0);
    } else {
        if (size < 0)
            size = qstrlen(str);
        d = Data::allocate(size + 1);
        Q_CHECK_PTR(d);
        d->size = size;
        d->data()[size] = '\0';
        ushort *dst = d->data();

        qt_from_latin1(dst, str, uint(size));
    }
    return d;
}

QString::Data *QString::fromAscii_helper(const char *str, int size)
{
    QString s = fromUtf8(str, size);
    s.d->ref.ref();
    return s.d;
}

/*! \fn QString QString::fromLatin1(const char *str, int size)
    Returns a QString initialized with the first \a size characters
    of the Latin-1 string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    \sa toLatin1(), fromUtf8(), fromLocal8Bit()
*/

/*!
    \fn QString QString::fromLatin1(const QByteArray &str)
    \overload
    \since 5.0

    Returns a QString initialized with the Latin-1 string \a str.
*/

/*! \fn QString QString::fromLocal8Bit(const char *str, int size)
    Returns a QString initialized with the first \a size characters
    of the 8-bit string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    QTextCodec::codecForLocale() is used to perform the conversion.

    \sa toLocal8Bit(), fromLatin1(), fromUtf8()
*/

/*!
    \fn QString QString::fromLocal8Bit(const QByteArray &str)
    \overload
    \since 5.0

    Returns a QString initialized with the 8-bit string \a str.
*/
QString QString::fromLocal8Bit_helper(const char *str, int size)
{
    if (!str)
        return QString();
    if (size == 0 || (!*str && size < 0)) {
        QStringDataPtr empty = { Data::allocate(0) };
        return QString(empty);
    }
#if !defined(QT_NO_TEXTCODEC)
    if (size < 0)
        size = qstrlen(str);
    QTextCodec *codec = QTextCodec::codecForLocale();
    if (codec)
        return codec->toUnicode(str, size);
#endif // !QT_NO_TEXTCODEC
    return fromLatin1(str, size);
}

/*! \fn QString QString::fromAscii(const char *, int size);
    \deprecated

    Returns a QString initialized with the first \a size characters
    from the string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    This function does the same as fromLatin1().

    \sa toAscii(), fromLatin1(), fromUtf8(), fromLocal8Bit()
*/

/*!
    \fn QString QString::fromAscii(const QByteArray &str)
    \deprecated
    \overload
    \since 5.0

    Returns a QString initialized with the string \a str.
*/

/*! \fn QString QString::fromUtf8(const char *str, int size)
    Returns a QString initialized with the first \a size bytes
    of the UTF-8 string \a str.

    If \a size is -1 (default), it is taken to be strlen(\a
    str).

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like QString. However, invalid sequences are possible with UTF-8
    and, if any such are found, they will be replaced with one or more
    "replacement characters", or suppressed. These include non-Unicode
    sequences, non-characters, overlong sequences or surrogate codepoints
    encoded into UTF-8.

    This function can be used to process incoming data incrementally as long as
    all UTF-8 characters are terminated within the incoming data. Any
    unterminated characters at the end of the string will be replaced or
    suppressed. In order to do stateful decoding, please use \l QTextDecoder.

    \sa toUtf8(), fromLatin1(), fromLocal8Bit()
*/

/*!
    \fn QString QString::fromUtf8(const QByteArray &str)
    \overload
    \since 5.0

    Returns a QString initialized with the UTF-8 string \a str.
*/
QString QString::fromUtf8_helper(const char *str, int size)
{
    if (!str)
        return QString();

    Q_ASSERT(size != -1);
    return QUtf8::convertToUnicode(str, size);
}

/*!
    Returns a QString initialized with the first \a size characters
    of the Unicode string \a unicode (ISO-10646-UTF-16 encoded).

    If \a size is -1 (default), \a unicode must be terminated
    with a 0.

    This function checks for a Byte Order Mark (BOM). If it is missing,
    host byte order is assumed.

    This function is slow compared to the other Unicode conversions.
    Use QString(const QChar *, int) or QString(const QChar *) if possible.

    QString makes a deep copy of the Unicode data.

    \sa utf16(), setUtf16(), fromStdU16String()
*/
QString QString::fromUtf16(const ushort *unicode, int size)
{
    if (!unicode)
        return QString();
    if (size < 0) {
        size = 0;
        while (unicode[size] != 0)
            ++size;
    }
    return QUtf16::convertToUnicode((const char *)unicode, size*2, 0);
}


/*!
    \since 4.2

    Returns a QString initialized with the first \a size characters
    of the Unicode string \a unicode (ISO-10646-UCS-4 encoded).

    If \a size is -1 (default), \a unicode must be terminated
    with a 0.

    \sa toUcs4(), fromUtf16(), utf16(), setUtf16(), fromWCharArray(), fromStdU32String()
*/
QString QString::fromUcs4(const uint *unicode, int size)
{
    if (!unicode)
        return QString();
    if (size < 0) {
        size = 0;
        while (unicode[size] != 0)
            ++size;
    }
    return QUtf32::convertToUnicode((const char *)unicode, size*4, 0);
}

/*!
    Resizes the string to \a size characters and copies \a unicode
    into the string.

    If \a unicode is 0, nothing is copied, but the string is still
    resized to \a size.

    \sa unicode(), setUtf16()
*/
QString& QString::setUnicode(const QChar *unicode, int size)
{
     resize(size);
     if (unicode && size)
         memcpy(d->data(), unicode, size * sizeof(QChar));
     return *this;
}

/*!
    \fn QString &QString::setUtf16(const ushort *unicode, int size)

    Resizes the string to \a size characters and copies \a unicode
    into the string.

    If \a unicode is 0, nothing is copied, but the string is still
    resized to \a size.

    Note that unlike fromUtf16(), this function does not consider BOMs and
    possibly differing byte ordering.

    \sa utf16(), setUnicode()
*/

/*!
    \fn QString QString::simplified() const

    Returns a string that has whitespace removed from the start
    and the end, and that has each sequence of internal whitespace
    replaced with a single space.

    Whitespace means any character for which QChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    Example:

    \snippet qstring/main.cpp 57

    \sa trimmed()
*/
QString QString::simplified_helper(const QString &str)
{
    return QStringAlgorithms<const QString>::simplified_helper(str);
}

QString QString::simplified_helper(QString &str)
{
    return QStringAlgorithms<QString>::simplified_helper(str);
}

/*!
    \fn QString QString::trimmed() const

    Returns a string that has whitespace removed from the start and
    the end.

    Whitespace means any character for which QChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    Example:

    \snippet qstring/main.cpp 82

    Unlike simplified(), trimmed() leaves internal whitespace alone.

    \sa simplified()
*/
QString QString::trimmed_helper(const QString &str)
{
    return QStringAlgorithms<const QString>::trimmed_helper(str);
}

QString QString::trimmed_helper(QString &str)
{
    return QStringAlgorithms<QString>::trimmed_helper(str);
}

/*! \fn const QChar QString::at(int position) const

    Returns the character at the given index \a position in the
    string.

    The \a position must be a valid index position in the string
    (i.e., 0 <= \a position < size()).

    \sa operator[]()
*/

/*!
    \fn QCharRef QString::operator[](int position)

    Returns the character at the specified \a position in the string as a
    modifiable reference.

    Example:

    \snippet qstring/main.cpp 85

    The return value is of type QCharRef, a helper class for QString.
    When you get an object of type QCharRef, you can use it as if it
    were a QChar &. If you assign to it, the assignment will apply to
    the character in the QString from which you got the reference.

    \sa at()
*/

/*!
    \fn const QChar QString::operator[](int position) const

    \overload operator[]()
*/

/*! \fn QCharRef QString::operator[](uint position)

\overload operator[]()

Returns the character at the specified \a position in the string as a
modifiable reference.
*/

/*! \fn const QChar QString::operator[](uint position) const
    Equivalent to \c at(position).
\overload operator[]()
*/

/*!
    \fn void QString::truncate(int position)

    Truncates the string at the given \a position index.

    If the specified \a position index is beyond the end of the
    string, nothing happens.

    Example:

    \snippet qstring/main.cpp 83

    If \a position is negative, it is equivalent to passing zero.

    \sa chop(), resize(), left(), QStringRef::truncate()
*/

void QString::truncate(int pos)
{
    if (pos < d->size)
        resize(pos);
}


/*!
    Removes \a n characters from the end of the string.

    If \a n is greater than or equal to size(), the result is an
    empty string.

    Example:
    \snippet qstring/main.cpp 15

    If you want to remove characters from the \e beginning of the
    string, use remove() instead.

    \sa truncate(), resize(), remove()
*/
void QString::chop(int n)
{
    if (n > 0)
        resize(d->size - n);
}

/*!
    Sets every character in the string to character \a ch. If \a size
    is different from -1 (default), the string is resized to \a
    size beforehand.

    Example:

    \snippet qstring/main.cpp 21

    \sa resize()
*/

QString& QString::fill(QChar ch, int size)
{
    resize(size < 0 ? d->size : size);
    if (d->size) {
        QChar *i = (QChar*)d->data() + d->size;
        QChar *b = (QChar*)d->data();
        while (i != b)
           *--i = ch;
    }
    return *this;
}

/*!
    \fn int QString::length() const

    Returns the number of characters in this string.  Equivalent to
    size().

    \sa resize()
*/

/*!
    \fn int QString::size() const

    Returns the number of characters in this string.

    The last character in the string is at position size() - 1. In
    addition, QString ensures that the character at position size()
    is always '\\0', so that you can use the return value of data()
    and constData() as arguments to functions that expect
    '\\0'-terminated strings.

    Example:

    \snippet qstring/main.cpp 58

    \sa isEmpty(), resize()
*/

/*! \fn bool QString::isNull() const

    Returns \c true if this string is null; otherwise returns \c false.

    Example:

    \snippet qstring/main.cpp 28

    Qt makes a distinction between null strings and empty strings for
    historical reasons. For most applications, what matters is
    whether or not a string contains any data, and this can be
    determined using the isEmpty() function.

    \sa isEmpty()
*/

/*! \fn bool QString::isEmpty() const

    Returns \c true if the string has no characters; otherwise returns
    \c false.

    Example:

    \snippet qstring/main.cpp 27

    \sa size()
*/

/*! \fn QString &QString::operator+=(const QString &other)

    Appends the string \a other onto the end of this string and
    returns a reference to this string.

    Example:

    \snippet qstring/main.cpp 84

    This operation is typically very fast (\l{constant time}),
    because QString preallocates extra space at the end of the string
    data so it can grow without reallocating the entire string each
    time.

    \sa append(), prepend()
*/

/*! \fn QString &QString::operator+=(QLatin1String str)

    \overload operator+=()

    Appends the Latin-1 string \a str to this string.
*/

/*! \fn QString &QString::operator+=(const QByteArray &ba)

    \overload operator+=()

    Appends the byte array \a ba to this string. The byte array is converted
    to Unicode using the fromUtf8() function. If any NUL characters ('\\0')
    are embedded in the \a ba byte array, they will be included in the
    transformation.

    You can disable this function by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn QString &QString::operator+=(const char *str)

    \overload operator+=()

    Appends the string \a str to this string. The const char pointer
    is converted to Unicode using the fromUtf8() function.

    You can disable this function by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*! \fn QString &QString::operator+=(const QStringRef &str)

    \overload operator+=()

    Appends the string section referenced by \a str to this string.
*/

/*! \fn QString &QString::operator+=(char ch)

    \overload operator+=()

    Appends the character \a ch to this string. Note that the character is
    converted to Unicode using the fromLatin1() function, unlike other 8-bit
    functions that operate on UTF-8 data.

    You can disable this function by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*! \fn QString &QString::operator+=(QChar ch)

    \overload operator+=()

    Appends the character \a ch to the string.
*/

/*! \fn QString &QString::operator+=(QChar::SpecialCharacter c)

    \overload operator+=()

    \internal
*/

/*!
    \fn bool operator==(const char *s1, const QString &s2)

    \overload  operator==()
    \relates QString

    Returns \c true if \a s1 is equal to \a s2; otherwise returns \c false.
    Note that no string is equal to \a s1 being 0.

    Equivalent to \c {s1 != 0 && compare(s1, s2) == 0}.

    \sa QString::compare()
*/

/*!
    \fn bool operator!=(const char *s1, const QString &s2)
    \relates QString

    Returns \c true if \a s1 is not equal to \a s2; otherwise returns
    \c false.

    For \a s1 != 0, this is equivalent to \c {compare(} \a s1, \a s2
    \c {) != 0}. Note that no string is equal to \a s1 being 0.

    \sa QString::compare()
*/

/*!
    \fn bool operator<(const char *s1, const QString &s2)
    \relates QString

    Returns \c true if \a s1 is lexically less than \a s2; otherwise
    returns \c false.  For \a s1 != 0, this is equivalent to \c
    {compare(s1, s2) < 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.

    \sa QString::compare()
*/

/*!
    \fn bool operator<=(const char *s1, const QString &s2)
    \relates QString

    Returns \c true if \a s1 is lexically less than or equal to \a s2;
    otherwise returns \c false.  For \a s1 != 0, this is equivalent to \c
    {compare(s1, s2) <= 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    QString::localeAwareCompare().

    \sa QString::compare()
*/

/*!
    \fn bool operator>(const char *s1, const QString &s2)
    \relates QString

    Returns \c true if \a s1 is lexically greater than \a s2; otherwise
    returns \c false.  Equivalent to \c {compare(s1, s2) > 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.

    \sa QString::compare()
*/

/*!
    \fn bool operator>=(const char *s1, const QString &s2)
    \relates QString

    Returns \c true if \a s1 is lexically greater than or equal to \a s2;
    otherwise returns \c false.  For \a s1 != 0, this is equivalent to \c
    {compare(s1, s2) >= 0}.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.
*/

/*!
    \fn const QString operator+(const QString &s1, const QString &s2)
    \relates QString

    Returns a string which is the result of concatenating \a s1 and \a
    s2.
*/

/*!
    \fn const QString operator+(const QString &s1, const char *s2)
    \relates QString

    Returns a string which is the result of concatenating \a s1 and \a
    s2 (\a s2 is converted to Unicode using the QString::fromUtf8()
    function).

    \sa QString::fromUtf8()
*/

/*!
    \fn const QString operator+(const char *s1, const QString &s2)
    \relates QString

    Returns a string which is the result of concatenating \a s1 and \a
    s2 (\a s1 is converted to Unicode using the QString::fromUtf8()
    function).

    \sa QString::fromUtf8()
*/

/*!
    \fn const QString operator+(const QString &s, char ch)
    \relates QString

    Returns a string which is the result of concatenating the string
    \a s and the character \a ch.
*/

/*!
    \fn const QString operator+(char ch, const QString &s)
    \relates QString

    Returns a string which is the result of concatenating the
    character \a ch and the string \a s.
*/

/*!
    \fn int QString::compare(const QString &s1, const QString &s2, Qt::CaseSensitivity cs)
    \since 4.2

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    If \a cs is Qt::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Case sensitive comparison is based exclusively on the numeric
    Unicode values of the characters and is very fast, but is not what
    a human would expect.  Consider sorting user-visible strings with
    localeAwareCompare().

    \snippet qstring/main.cpp 16

    \sa operator==(), operator<(), operator>()
*/

/*!
    \fn int QString::compare(const QString &s1, QLatin1String s2, Qt::CaseSensitivity cs)
    \since 4.2
    \overload compare()

    Performs a comparison of \a s1 and \a s2, using the case
    sensitivity setting \a cs.
*/

/*!
    \fn int QString::compare(QLatin1String s1, const QString &s2, Qt::CaseSensitivity cs = Qt::CaseSensitive)

    \since 4.2
    \overload compare()

    Performs a comparison of \a s1 and \a s2, using the case
    sensitivity setting \a cs.
*/


/*!
    \overload compare()
    \since 4.2

    Lexically compares this string with the \a other string and
    returns an integer less than, equal to, or greater than zero if
    this string is less than, equal to, or greater than the other
    string.

    Same as compare(*this, \a other, \a cs).
*/
int QString::compare(const QString &other, Qt::CaseSensitivity cs) const
{
    if (cs == Qt::CaseSensitive)
        return ucstrcmp(constData(), length(), other.constData(), other.length());
    return ucstricmp(d->data(), d->data() + d->size, other.d->data(), other.d->data() + other.d->size);
}

/*!
    \internal
    \since 4.5
*/
int QString::compare_helper(const QChar *data1, int length1, const QChar *data2, int length2,
                            Qt::CaseSensitivity cs)
{
    if (cs == Qt::CaseSensitive)
        return ucstrcmp(data1, length1, data2, length2);
    const ushort *s1 = reinterpret_cast<const ushort *>(data1);
    const ushort *s2 = reinterpret_cast<const ushort *>(data2);
    return ucstricmp(s1, s1 + length1, s2, s2 + length2);
}

/*!
    \overload compare()
    \since 4.2

    Same as compare(*this, \a other, \a cs).
*/
int QString::compare(QLatin1String other, Qt::CaseSensitivity cs) const
{
    return compare_helper(unicode(), length(), other, cs);
}

/*!
  \fn int QString::compare(const QStringRef &ref, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
  \overload compare()

  Compares the string reference, \a ref, with the string and returns
  an integer less than, equal to, or greater than zero if the string
  is less than, equal to, or greater than \a ref.
*/

/*!
    \internal
    \since 5.0
*/
int QString::compare_helper(const QChar *data1, int length1, const char *data2, int length2,
                            Qt::CaseSensitivity cs)
{
    // ### optimize me
    const QString s2 = QString::fromUtf8(data2, length2 == -1 ? (data2 ? int(strlen(data2)) : -1) : length2);
    return compare_helper(data1, length1, s2.constData(), s2.size(), cs);
}

/*!
  \fn int QString::compare(const QString &s1, const QStringRef &s2, Qt::CaseSensitivity cs = Qt::CaseSensitive)
  \overload compare()
*/

/*!
    \internal
    \since 4.5
*/
int QString::compare_helper(const QChar *data1, int length1, QLatin1String s2,
                            Qt::CaseSensitivity cs)
{
    const ushort *uc = reinterpret_cast<const ushort *>(data1);
    const ushort *uce = uc + length1;
    const uchar *c = (const uchar *)s2.latin1();

    if (!c)
        return length1;

    if (cs == Qt::CaseSensitive) {
        return ucstrcmp(data1, length1, c, s2.size());
    } else {
        return ucstricmp(uc, uce, c, c + s2.size());
    }
}

/*!
    \fn int QString::localeAwareCompare(const QString & s1, const QString & s2)

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    On OS X and iOS this function compares according the
    "Order for sorted lists" setting in the International preferences panel.

    \sa compare(), QLocale
*/

/*!
    \fn int QString::localeAwareCompare(const QStringRef &other) const
    \since 4.5
    \overload localeAwareCompare()

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    Same as \c {localeAwareCompare(*this, other)}.
*/

/*!
    \fn int QString::localeAwareCompare(const QString &s1, const QStringRef &s2)
    \since 4.5
    \overload localeAwareCompare()

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.
*/


#if !defined(CSTR_LESS_THAN)
#define CSTR_LESS_THAN    1
#define CSTR_EQUAL        2
#define CSTR_GREATER_THAN 3
#endif

/*!
    \overload localeAwareCompare()

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    Same as \c {localeAwareCompare(*this, other)}.
*/
int QString::localeAwareCompare(const QString &other) const
{
    return localeAwareCompare_helper(constData(), length(), other.constData(), other.length());
}

#if defined(QT_USE_ICU) && !defined(Q_OS_WIN32) && !defined(Q_OS_WINCE) && !defined (Q_OS_MAC)
Q_GLOBAL_STATIC(QThreadStorage<QCollator>, defaultCollator)
#endif

/*!
    \internal
    \since 4.5
*/
int QString::localeAwareCompare_helper(const QChar *data1, int length1,
                                       const QChar *data2, int length2)
{
    // do the right thing for null and empty
    if (length1 == 0 || length2 == 0)
        return ucstrcmp(data1, length1, data2, length2);

#if defined(Q_OS_WIN32) || defined(Q_OS_WINCE)
#ifndef Q_OS_WINRT
    int res = CompareString(GetUserDefaultLCID(), 0, (wchar_t*)data1, length1, (wchar_t*)data2, length2);
#else
    int res = CompareStringEx(LOCALE_NAME_USER_DEFAULT, 0, (LPCWSTR)data1, length1, (LPCWSTR)data2, length2, NULL, NULL, 0);
#endif

    switch (res) {
    case CSTR_LESS_THAN:
        return -1;
    case CSTR_GREATER_THAN:
        return 1;
    default:
        return 0;
    }
#elif defined (Q_OS_MAC)
    // Use CFStringCompare for comparing strings on Mac. This makes Qt order
    // strings the same way as native applications do, and also respects
    // the "Order for sorted lists" setting in the International preferences
    // panel.
    const CFStringRef thisString =
        CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
            reinterpret_cast<const UniChar *>(data1), length1, kCFAllocatorNull);
    const CFStringRef otherString =
        CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
            reinterpret_cast<const UniChar *>(data2), length2, kCFAllocatorNull);

    const int result = CFStringCompare(thisString, otherString, kCFCompareLocalized);
    CFRelease(thisString);
    CFRelease(otherString);
    return result;
#elif defined(QT_USE_ICU)
    if (!defaultCollator()->hasLocalData())
        defaultCollator()->setLocalData(QCollator());
    return defaultCollator()->localData().compare(data1, length1, data2, length2);
#elif defined(Q_OS_UNIX)
    // declared in <string.h>
    int delta = strcoll(toLocal8Bit_helper(data1, length1).constData(), toLocal8Bit_helper(data2, length2).constData());
    if (delta == 0)
        delta = ucstrcmp(data1, length1, data2, length2);
    return delta;
#else
    return ucstrcmp(data1, length1, data2, length2);
#endif
}


/*!
    \fn const QChar *QString::unicode() const

    Returns a '\\0'-terminated Unicode representation of the string.
    The result remains valid until the string is modified.

    \sa utf16()
*/

/*!
    \fn const ushort *QString::utf16() const

    Returns the QString as a '\\0\'-terminated array of unsigned
    shorts. The result remains valid until the string is modified.

    The returned string is in host byte order.

    \sa unicode()
*/

const ushort *QString::utf16() const
{
    if (IS_RAW_DATA(d)) {
        // ensure '\0'-termination for ::fromRawData strings
        const_cast<QString*>(this)->reallocData(uint(d->size) + 1u);
    }
    return d->data();
}

/*!
    Returns a string of size \a width that contains this string
    padded by the \a fill character.

    If \a truncate is \c false and the size() of the string is more than
    \a width, then the returned string is a copy of the string.

    \snippet qstring/main.cpp 32

    If \a truncate is \c true and the size() of the string is more than
    \a width, then any characters in a copy of the string after
    position \a width are removed, and the copy is returned.

    \snippet qstring/main.cpp 33

    \sa rightJustified()
*/

QString QString::leftJustified(int width, QChar fill, bool truncate) const
{
    QString result;
    int len = length();
    int padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        if (len)
            memcpy(result.d->data(), d->data(), sizeof(QChar)*len);
        QChar *uc = (QChar*)result.d->data() + len;
        while (padlen--)
           * uc++ = fill;
    } else {
        if (truncate)
            result = left(width);
        else
            result = *this;
    }
    return result;
}

/*!
    Returns a string of size() \a width that contains the \a fill
    character followed by the string. For example:

    \snippet qstring/main.cpp 49

    If \a truncate is \c false and the size() of the string is more than
    \a width, then the returned string is a copy of the string.

    If \a truncate is true and the size() of the string is more than
    \a width, then the resulting string is truncated at position \a
    width.

    \snippet qstring/main.cpp 50

    \sa leftJustified()
*/

QString QString::rightJustified(int width, QChar fill, bool truncate) const
{
    QString result;
    int len = length();
    int padlen = width - len;
    if (padlen > 0) {
        result.resize(len+padlen);
        QChar *uc = (QChar*)result.d->data();
        while (padlen--)
           * uc++ = fill;
        if (len)
            memcpy(uc, d->data(), sizeof(QChar)*len);
    } else {
        if (truncate)
            result = left(width);
        else
            result = *this;
    }
    return result;
}

/*!
    \fn QString QString::toLower() const

    Returns a lowercase copy of the string.

    \snippet qstring/main.cpp 75

    The case conversion will always happen in the 'C' locale. For locale dependent
    case folding use QLocale::toLower()

    \sa toUpper(), QLocale::toLower()
*/

namespace QUnicodeTables {
/*
    \internal
    Converts the \a str string starting from the position pointed to by the \a
    it iterator, using the Unicode case traits \c Traits, and returns the
    result. The input string must not be empty (the convertCase function below
    guarantees that).

    The string type \c{T} is also a template and is either \c{const QString} or
    \c{QString}. This function can do both copy-conversion and in-place
    conversion depending on the state of the \a str parameter:
    \list
       \li \c{T} is \c{const QString}: copy-convert
       \li \c{T} is \c{QString} and its refcount != 1: copy-convert
       \li \c{T} is \c{QString} and its refcount == 1: in-place convert
    \endlist

    In copy-convert mode, the local variable \c{s} is detached from the input
    \a str. In the in-place convert mode, \a str is in moved-from state (which
    this function requires to be a valid, empty string) and \c{s} contains the
    only copy of the string, without reallocation (thus, \a it is still valid).

    There's one pathological case left: when the in-place conversion needs to
    reallocate memory to grow the buffer. In that case, we need to adjust the \a
    it pointer.
 */
template <typename Traits, typename T>
Q_NEVER_INLINE
static QString detachAndConvertCase(T &str, QStringIterator it)
{
    Q_ASSERT(!str.isEmpty());
    QString s = qMove(str);             // will copy if T is const QString
    QChar *pp = s.begin() + it.index(); // will detach if necessary

    do {
        uint uc = it.nextUnchecked();

        const QUnicodeTables::Properties *prop = qGetProp(uc);
        signed short caseDiff = Traits::caseDiff(prop);

        if (Q_UNLIKELY(Traits::caseSpecial(prop))) {
            const ushort *specialCase = specialCaseMap + caseDiff;
            ushort length = *specialCase++;

            if (Q_LIKELY(length == 1)) {
                *pp++ = QChar(*specialCase);
            } else {
                // slow path: the string is growing
                int inpos = it.index() - 1;
                int outpos = pp - s.constBegin();

                s.replace(outpos, 1, reinterpret_cast<const QChar *>(specialCase), length);
                pp = const_cast<QChar *>(s.constBegin()) + outpos + length;

                // do we need to adjust the input iterator too?
                // if it is pointing to s's data, str is empty
                if (str.isEmpty())
                    it = QStringIterator(s.constBegin(), inpos + length, s.constEnd());
            }
        } else if (Q_UNLIKELY(QChar::requiresSurrogates(uc))) {
            // so far, case convertion never changes planes (guaranteed by the qunicodetables generator)
            pp++;
            *pp++ = QChar::lowSurrogate(uc + caseDiff);
        } else {
            *pp++ = QChar(uc + caseDiff);
        }
    } while (it.hasNext());

    return s;
}

template <typename Traits, typename T>
static QString convertCase(T &str)
{
    const QChar *p = str.constBegin();
    const QChar *e = p + str.size();

    // this avoids out of bounds check in the loop
    while (e != p && e[-1].isHighSurrogate())
        --e;

    QStringIterator it(p, e);
    while (it.hasNext()) {
        uint uc = it.nextUnchecked();
        if (Traits::caseDiff(qGetProp(uc))) {
            it.recedeUnchecked();
            return detachAndConvertCase<Traits>(str, it);
        }
    }
    return qMove(str);
}
} // namespace QUnicodeTables

QString QString::toLower_helper(const QString &str)
{
    return QUnicodeTables::convertCase<QUnicodeTables::LowercaseTraits>(str);
}

QString QString::toLower_helper(QString &str)
{
    return QUnicodeTables::convertCase<QUnicodeTables::LowercaseTraits>(str);
}

/*!
    \fn QString QString::toCaseFolded() const

    Returns the case folded equivalent of the string. For most Unicode
    characters this is the same as toLower().
*/

QString QString::toCaseFolded_helper(const QString &str)
{
    return QUnicodeTables::convertCase<QUnicodeTables::CasefoldTraits>(str);
}

QString QString::toCaseFolded_helper(QString &str)
{
    return QUnicodeTables::convertCase<QUnicodeTables::CasefoldTraits>(str);
}

/*!
    \fn QString QString::toUpper() const

    Returns an uppercase copy of the string.

    \snippet qstring/main.cpp 81

    The case conversion will always happen in the 'C' locale. For locale dependent
    case folding use QLocale::toUpper()

    \sa toLower(), QLocale::toLower()
*/

QString QString::toUpper_helper(const QString &str)
{
    return QUnicodeTables::convertCase<QUnicodeTables::UppercaseTraits>(str);
}

QString QString::toUpper_helper(QString &str)
{
    return QUnicodeTables::convertCase<QUnicodeTables::UppercaseTraits>(str);
}

/*!
    \obsolete

    Use asprintf(), arg() or QTextStream instead.
*/
QString &QString::sprintf(const char *cformat, ...)
{
    va_list ap;
    va_start(ap, cformat);
    *this = vasprintf(cformat, ap);
    va_end(ap);
    return *this;
}

// ### Qt 6: Consider whether this function shouldn't be removed See task 202871.
/*!
    \since 5.5

    Safely builds a formatted string from the format string \a cformat
    and an arbitrary list of arguments.

    The format string supports the conversion specifiers, length modifiers,
    and flags provided by printf() in the standard C++ library. The \a cformat
    string and \c{%s} arguments must be UTF-8 encoded.

    \note The \c{%lc} escape sequence expects a unicode character of type
    \c char16_t, or \c ushort (as returned by QChar::unicode()).
    The \c{%ls} escape sequence expects a pointer to a zero-terminated array
    of unicode characters of type \c char16_t, or ushort (as returned by
    QString::utf16()). This is at odds with the printf() in the standard C++
    library, which defines \c {%lc} to print a wchar_t and \c{%ls} to print
    a \c{wchar_t*}, and might also produce compiler warnings on platforms
    where the size of \c {wchar_t} is not 16 bits.

    \warning We do not recommend using QString::asprintf() in new Qt
    code. Instead, consider using QTextStream or arg(), both of
    which support Unicode strings seamlessly and are type-safe.
    Here's an example that uses QTextStream:

    \snippet qstring/main.cpp 64

    For \l {QObject::tr()}{translations}, especially if the strings
    contains more than one escape sequence, you should consider using
    the arg() function instead. This allows the order of the
    replacements to be controlled by the translator.

    \sa arg()
*/

QString QString::asprintf(const char *cformat, ...)
{
    va_list ap;
    va_start(ap, cformat);
    const QString s = vasprintf(cformat, ap);
    va_end(ap);
    return s;
}

/*!
    \obsolete

    Use vasprintf(), arg() or QTextStream instead.
*/
QString &QString::vsprintf(const char *cformat, va_list ap)
{
    return *this = vasprintf(cformat, ap);
}

/*!
    \fn QString::vasprintf(const char *cformat, va_list ap)
    \since 5.5

    Equivalent method to asprintf(), but takes a va_list \a ap
    instead a list of variable arguments. See the asprintf()
    documentation for an explanation of \a cformat.

    This method does not call the va_end macro, the caller
    is responsible to call va_end on \a ap.

    \sa asprintf()
*/

QString QString::vasprintf(const char *cformat, va_list ap)
{
    if (!cformat || !*cformat) {
        // Qt 1.x compat
        return fromLatin1("");
    }

    // Parse cformat

    QString result;
    const char *c = cformat;
    for (;;) {
        // Copy non-escape chars to result
        const char *cb = c;
        while (*c != '\0' && *c != '%')
            c++;
        result.append(QString::fromUtf8(cb, (int)(c - cb)));

        if (*c == '\0')
            break;

        // Found '%'
        const char *escape_start = c;
        ++c;

        if (*c == '\0') {
            result.append(QLatin1Char('%')); // a % at the end of the string - treat as non-escape text
            break;
        }
        if (*c == '%') {
            result.append(QLatin1Char('%')); // %%
            ++c;
            continue;
        }

        // Parse flag characters
        uint flags = 0;
        bool no_more_flags = false;
        do {
            switch (*c) {
                case '#': flags |= QLocaleData::Alternate; break;
                case '0': flags |= QLocaleData::ZeroPadded; break;
                case '-': flags |= QLocaleData::LeftAdjusted; break;
                case ' ': flags |= QLocaleData::BlankBeforePositive; break;
                case '+': flags |= QLocaleData::AlwaysShowSign; break;
                case '\'': flags |= QLocaleData::ThousandsGroup; break;
                default: no_more_flags = true; break;
            }

            if (!no_more_flags)
                ++c;
        } while (!no_more_flags);

        if (*c == '\0') {
            result.append(QLatin1String(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse field width
        int width = -1; // -1 means unspecified
        if (qIsDigit(*c)) {
            QString width_str;
            while (*c != '\0' && qIsDigit(*c))
                width_str.append(QLatin1Char(*c++));

            // can't be negative - started with a digit
            // contains at least one digit
            width = width_str.toInt();
        }
        else if (*c == '*') {
            width = va_arg(ap, int);
            if (width < 0)
                width = -1; // treat all negative numbers as unspecified
            ++c;
        }

        if (*c == '\0') {
            result.append(QLatin1String(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse precision
        int precision = -1; // -1 means unspecified
        if (*c == '.') {
            ++c;
            if (qIsDigit(*c)) {
                QString precision_str;
                while (*c != '\0' && qIsDigit(*c))
                    precision_str.append(QLatin1Char(*c++));

                // can't be negative - started with a digit
                // contains at least one digit
                precision = precision_str.toInt();
            }
            else if (*c == '*') {
                precision = va_arg(ap, int);
                if (precision < 0)
                    precision = -1; // treat all negative numbers as unspecified
                ++c;
            }
        }

        if (*c == '\0') {
            result.append(QLatin1String(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse the length modifier
        enum LengthMod { lm_none, lm_hh, lm_h, lm_l, lm_ll, lm_L, lm_j, lm_z, lm_t };
        LengthMod length_mod = lm_none;
        switch (*c) {
            case 'h':
                ++c;
                if (*c == 'h') {
                    length_mod = lm_hh;
                    ++c;
                }
                else
                    length_mod = lm_h;
                break;

            case 'l':
                ++c;
                if (*c == 'l') {
                    length_mod = lm_ll;
                    ++c;
                }
                else
                    length_mod = lm_l;
                break;

            case 'L':
                ++c;
                length_mod = lm_L;
                break;

            case 'j':
                ++c;
                length_mod = lm_j;
                break;

            case 'z':
            case 'Z':
                ++c;
                length_mod = lm_z;
                break;

            case 't':
                ++c;
                length_mod = lm_t;
                break;

            default: break;
        }

        if (*c == '\0') {
            result.append(QLatin1String(escape_start)); // incomplete escape, treat as non-escape text
            break;
        }

        // Parse the conversion specifier and do the conversion
        QString subst;
        switch (*c) {
            case 'd':
            case 'i': {
                qint64 i;
                switch (length_mod) {
                    case lm_none: i = va_arg(ap, int); break;
                    case lm_hh: i = va_arg(ap, int); break;
                    case lm_h: i = va_arg(ap, int); break;
                    case lm_l: i = va_arg(ap, long int); break;
                    case lm_ll: i = va_arg(ap, qint64); break;
                    case lm_j: i = va_arg(ap, long int); break;
                    case lm_z: i = va_arg(ap, size_t); break;
                    case lm_t: i = va_arg(ap, int); break;
                    default: i = 0; break;
                }
                subst = QLocaleData::c()->longLongToString(i, precision, 10, width, flags);
                ++c;
                break;
            }
            case 'o':
            case 'u':
            case 'x':
            case 'X': {
                quint64 u;
                switch (length_mod) {
                    case lm_none: u = va_arg(ap, uint); break;
                    case lm_hh: u = va_arg(ap, uint); break;
                    case lm_h: u = va_arg(ap, uint); break;
                    case lm_l: u = va_arg(ap, ulong); break;
                    case lm_ll: u = va_arg(ap, quint64); break;
                    case lm_z: u = va_arg(ap, size_t); break;
                    default: u = 0; break;
                }

                if (qIsUpper(*c))
                    flags |= QLocaleData::CapitalEorX;

                int base = 10;
                switch (qToLower(*c)) {
                    case 'o':
                        base = 8; break;
                    case 'u':
                        base = 10; break;
                    case 'x':
                        base = 16; break;
                    default: break;
                }
                subst = QLocaleData::c()->unsLongLongToString(u, precision, base, width, flags);
                ++c;
                break;
            }
            case 'E':
            case 'e':
            case 'F':
            case 'f':
            case 'G':
            case 'g':
            case 'A':
            case 'a': {
                double d;
                if (length_mod == lm_L)
                    d = va_arg(ap, long double); // not supported - converted to a double
                else
                    d = va_arg(ap, double);

                if (qIsUpper(*c))
                    flags |= QLocaleData::CapitalEorX;

                QLocaleData::DoubleForm form = QLocaleData::DFDecimal;
                switch (qToLower(*c)) {
                    case 'e': form = QLocaleData::DFExponent; break;
                    case 'a':                             // not supported - decimal form used instead
                    case 'f': form = QLocaleData::DFDecimal; break;
                    case 'g': form = QLocaleData::DFSignificantDigits; break;
                    default: break;
                }
                subst = QLocaleData::c()->doubleToString(d, precision, form, width, flags);
                ++c;
                break;
            }
            case 'c': {
                if (length_mod == lm_l)
                    subst = QChar((ushort) va_arg(ap, int));
                else
                    subst = QLatin1Char((uchar) va_arg(ap, int));
                ++c;
                break;
            }
            case 's': {
                if (length_mod == lm_l) {
                    const ushort *buff = va_arg(ap, const ushort*);
                    const ushort *ch = buff;
                    while (*ch != 0)
                        ++ch;
                    subst.setUtf16(buff, ch - buff);
                } else
                    subst = QString::fromUtf8(va_arg(ap, const char*));
                if (precision != -1)
                    subst.truncate(precision);
                ++c;
                break;
            }
            case 'p': {
                void *arg = va_arg(ap, void*);
                const quint64 i = reinterpret_cast<quintptr>(arg);
                flags |= QLocaleData::Alternate;
                subst = QLocaleData::c()->unsLongLongToString(i, precision, 16, width, flags);
                ++c;
                break;
            }
            case 'n':
                switch (length_mod) {
                    case lm_hh: {
                        signed char *n = va_arg(ap, signed char*);
                        *n = result.length();
                        break;
                    }
                    case lm_h: {
                        short int *n = va_arg(ap, short int*);
                        *n = result.length();
                            break;
                    }
                    case lm_l: {
                        long int *n = va_arg(ap, long int*);
                        *n = result.length();
                        break;
                    }
                    case lm_ll: {
                        qint64 *n = va_arg(ap, qint64*);
                        volatile uint tmp = result.length(); // egcs-2.91.66 gets internal
                        *n = tmp;                             // compiler error without volatile
                        break;
                    }
                    default: {
                        int *n = va_arg(ap, int*);
                        *n = result.length();
                        break;
                    }
                }
                ++c;
                break;

            default: // bad escape, treat as non-escape text
                for (const char *cc = escape_start; cc != c; ++cc)
                    result.append(QLatin1Char(*cc));
                continue;
        }

        if (flags & QLocaleData::LeftAdjusted)
            result.append(subst.leftJustified(width));
        else
            result.append(subst.rightJustified(width));
    }

    return result;
}

/*!
    Returns the string converted to a \c{long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toLongLong()

    Example:

    \snippet qstring/main.cpp 74

    \sa number(), toULongLong(), toInt(), QLocale::toLongLong()
*/

qint64 QString::toLongLong(bool *ok, int base) const
{
    return toIntegral_helper<qlonglong>(constData(), size(), ok, base);
}

qlonglong QString::toIntegral_helper(const QChar *data, int len, bool *ok, int base)
{
#if defined(QT_CHECK_RANGE)
    if (base != 0 && (base < 2 || base > 36)) {
        qWarning("QString::toULongLong: Invalid base (%d)", base);
        base = 10;
    }
#endif

    return QLocaleData::c()->stringToLongLong(data, len, base, ok, QLocaleData::FailOnGroupSeparators);
}


/*!
    Returns the string converted to an \c{unsigned long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toULongLong()

    Example:

    \snippet qstring/main.cpp 79

    \sa number(), toLongLong(), QLocale::toULongLong()
*/

quint64 QString::toULongLong(bool *ok, int base) const
{
    return toIntegral_helper<qulonglong>(constData(), size(), ok, base);
}

qulonglong QString::toIntegral_helper(const QChar *data, uint len, bool *ok, int base)
{
#if defined(QT_CHECK_RANGE)
    if (base != 0 && (base < 2 || base > 36)) {
        qWarning("QString::toULongLong: Invalid base (%d)", base);
        base = 10;
    }
#endif

    return QLocaleData::c()->stringToUnsLongLong(data, len, base, ok, QLocaleData::FailOnGroupSeparators);
}

/*!
    \fn long QString::toLong(bool *ok, int base) const

    Returns the string converted to a \c long using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toLongLong()

    Example:

    \snippet qstring/main.cpp 73

    \sa number(), toULong(), toInt(), QLocale::toInt()
*/

long QString::toLong(bool *ok, int base) const
{
    return toIntegral_helper<long>(constData(), size(), ok, base);
}

/*!
    \fn ulong QString::toULong(bool *ok, int base) const

    Returns the string converted to an \c{unsigned long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toULongLong()

    Example:

    \snippet qstring/main.cpp 78

    \sa number(), QLocale::toUInt()
*/

ulong QString::toULong(bool *ok, int base) const
{
    return toIntegral_helper<ulong>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to an \c int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toInt()

    Example:

    \snippet qstring/main.cpp 72

    \sa number(), toUInt(), toDouble(), QLocale::toInt()
*/

int QString::toInt(bool *ok, int base) const
{
    return toIntegral_helper<int>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toUInt()

    Example:

    \snippet qstring/main.cpp 77

    \sa number(), toInt(), QLocale::toUInt()
*/

uint QString::toUInt(bool *ok, int base) const
{
    return toIntegral_helper<uint>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to a \c short using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toShort()

    Example:

    \snippet qstring/main.cpp 76

    \sa number(), toUShort(), toInt(), QLocale::toShort()
*/

short QString::toShort(bool *ok, int base) const
{
    return toIntegral_helper<short>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned short} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toUShort()

    Example:

    \snippet qstring/main.cpp 80

    \sa number(), toShort(), QLocale::toUShort()
*/

ushort QString::toUShort(bool *ok, int base) const
{
    return toIntegral_helper<ushort>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to a \c double value.

    Returns 0.0 if the conversion fails.

    If a conversion error occurs, \c{*}\a{ok} is set to \c false;
    otherwise \c{*}\a{ok} is set to \c true.

    \snippet qstring/main.cpp 66

    \warning The QString content may only contain valid numerical characters which includes the plus/minus sign, the characters g and e used in scientific notation, and the decimal point. Including the unit or additional characters leads to a conversion error.

    \snippet qstring/main.cpp 67

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toDouble()

    \snippet qstring/main.cpp 68

    For historical reasons, this function does not handle
    thousands group separators. If you need to convert such numbers,
    use QLocale::toDouble().

    \snippet qstring/main.cpp 69

    \sa number(), QLocale::setDefault(), QLocale::toDouble(), trimmed()
*/

double QString::toDouble(bool *ok) const
{
    return QLocaleData::c()->stringToDouble(constData(), size(), ok, QLocaleData::FailOnGroupSeparators);
}

/*!
    Returns the string converted to a \c float value.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true. Returns 0.0 if the conversion fails.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toFloat()

    Example:

    \snippet qstring/main.cpp 71

    \sa number(), toDouble(), toInt(), QLocale::toFloat()
*/

float QString::toFloat(bool *ok) const
{
    return QLocaleData::convertDoubleToFloat(toDouble(ok), ok);
}

/*! \fn QString &QString::setNum(int n, int base)

    Sets the string to the printed value of \a n in the specified \a
    base, and returns a reference to the string.

    The base is 10 by default and must be between 2 and 36. For bases
    other than 10, \a n is treated as an unsigned integer.

    \snippet qstring/main.cpp 56

   The formatting always uses QLocale::C, i.e., English/UnitedStates.
   To get a localized string representation of a number, use
   QLocale::toString() with the appropriate locale.
*/

/*! \fn QString &QString::setNum(uint n, int base)

    \overload
*/

/*! \fn QString &QString::setNum(long n, int base)

    \overload
*/

/*! \fn QString &QString::setNum(ulong n, int base)

    \overload
*/

/*!
    \overload
*/
QString &QString::setNum(qlonglong n, int base)
{
    return *this = number(n, base);
}

/*!
    \overload
*/
QString &QString::setNum(qulonglong n, int base)
{
    return *this = number(n, base);
}

/*! \fn QString &QString::setNum(short n, int base)

    \overload
*/

/*! \fn QString &QString::setNum(ushort n, int base)

    \overload
*/

/*!
    \fn QString &QString::setNum(double n, char format, int precision)
    \overload

    Sets the string to the printed value of \a n, formatted according
    to the given \a format and \a precision, and returns a reference
    to the string.

    The \a format can be 'e', 'E', 'f', 'g' or 'G' (see
    \l{Argument Formats} for an explanation of the formats).

    The formatting always uses QLocale::C, i.e., English/UnitedStates.
    To get a localized string representation of a number, use
    QLocale::toString() with the appropriate locale.
*/

QString &QString::setNum(double n, char f, int prec)
{
    return *this = number(n, f, prec);
}

/*!
    \fn QString &QString::setNum(float n, char format, int precision)
    \overload

    Sets the string to the printed value of \a n, formatted according
    to the given \a format and \a precision, and returns a reference
    to the string.

    The formatting always uses QLocale::C, i.e., English/UnitedStates.
    To get a localized string representation of a number, use
    QLocale::toString() with the appropriate locale.
*/


/*!
    \fn QString QString::number(long n, int base)

    Returns a string equivalent of the number \a n according to the
    specified \a base.

    The base is 10 by default and must be between 2
    and 36. For bases other than 10, \a n is treated as an
    unsigned integer.

    The formatting always uses QLocale::C, i.e., English/UnitedStates.
    To get a localized string representation of a number, use
    QLocale::toString() with the appropriate locale.

    \snippet qstring/main.cpp 35

    \sa setNum()
*/

QString QString::number(long n, int base)
{
    return number(qlonglong(n), base);
}

/*!
  \fn QString QString::number(ulong n, int base)

    \overload
*/
QString QString::number(ulong n, int base)
{
    return number(qulonglong(n), base);
}

/*!
    \overload
*/
QString QString::number(int n, int base)
{
    return number(qlonglong(n), base);
}

/*!
    \overload
*/
QString QString::number(uint n, int base)
{
    return number(qulonglong(n), base);
}

/*!
    \overload
*/
QString QString::number(qlonglong n, int base)
{
#if defined(QT_CHECK_RANGE)
    if (base < 2 || base > 36) {
        qWarning("QString::setNum: Invalid base (%d)", base);
        base = 10;
    }
#endif
    return QLocaleData::c()->longLongToString(n, -1, base);
}

/*!
    \overload
*/
QString QString::number(qulonglong n, int base)
{
#if defined(QT_CHECK_RANGE)
    if (base < 2 || base > 36) {
        qWarning("QString::setNum: Invalid base (%d)", base);
        base = 10;
    }
#endif
    return QLocaleData::c()->unsLongLongToString(n, -1, base);
}


/*!
    \fn QString QString::number(double n, char format, int precision)

    Returns a string equivalent of the number \a n, formatted
    according to the specified \a format and \a precision. See
    \l{Argument Formats} for details.

    Unlike QLocale::toString(), this function does not honor the
    user's locale settings.

    \sa setNum(), QLocale::toString()
*/
QString QString::number(double n, char f, int prec)
{
    QLocaleData::DoubleForm form = QLocaleData::DFDecimal;
    uint flags = 0;

    if (qIsUpper(f))
        flags = QLocaleData::CapitalEorX;
    f = qToLower(f);

    switch (f) {
        case 'f':
            form = QLocaleData::DFDecimal;
            break;
        case 'e':
            form = QLocaleData::DFExponent;
            break;
        case 'g':
            form = QLocaleData::DFSignificantDigits;
            break;
        default:
#if defined(QT_CHECK_RANGE)
            qWarning("QString::setNum: Invalid format char '%c'", f);
#endif
            break;
    }

    return QLocaleData::c()->doubleToString(n, prec, form, -1, flags);
}

namespace {
template<class ResultList, class StringSource>
static ResultList splitString(const StringSource &source, const QChar *sep,
                              QString::SplitBehavior behavior, Qt::CaseSensitivity cs, const int separatorSize)
{
    ResultList list;
    int start = 0;
    int end;
    int extra = 0;
    while ((end = qFindString(source.constData(), source.size(), start + extra, sep, separatorSize, cs)) != -1) {
        if (start != end || behavior == QString::KeepEmptyParts)
            list.append(source.mid(start, end - start));
        start = end + separatorSize;
        extra = (separatorSize == 0 ? 1 : 0);
    }
    if (start != source.size() || behavior == QString::KeepEmptyParts)
        list.append(source.mid(start, -1));
    return list;
}

} // namespace

/*!
    Splits the string into substrings wherever \a sep occurs, and
    returns the list of those strings. If \a sep does not match
    anywhere in the string, split() returns a single-element list
    containing this string.

    \a cs specifies whether \a sep should be matched case
    sensitively or case insensitively.

    If \a behavior is QString::SkipEmptyParts, empty entries don't
    appear in the result. By default, empty entries are kept.

    Example:

    \snippet qstring/main.cpp 62

    \sa QStringList::join(), section()
*/
QStringList QString::split(const QString &sep, SplitBehavior behavior, Qt::CaseSensitivity cs) const
{
    return splitString<QStringList>(*this, sep.constData(), behavior, cs, sep.size());
}

/*!
    Splits the string into substring references wherever \a sep occurs, and
    returns the list of those strings. If \a sep does not match
    anywhere in the string, splitRef() returns a single-element vector
    containing this string reference.

    \a cs specifies whether \a sep should be matched case
    sensitively or case insensitively.

    If \a behavior is QString::SkipEmptyParts, empty entries don't
    appear in the result. By default, empty entries are kept.

    \note All references are valid as long this string is alive. Destroying this
    string will cause all references be dangling pointers.

    \since 5.4
    \sa QStringRef split()
*/
QVector<QStringRef> QString::splitRef(const QString &sep, SplitBehavior behavior, Qt::CaseSensitivity cs) const
{
    return splitString<QVector<QStringRef> >(QStringRef(this), sep.constData(), behavior, cs, sep.size());
}
/*!
    \overload
*/
QStringList QString::split(QChar sep, SplitBehavior behavior, Qt::CaseSensitivity cs) const
{
    return splitString<QStringList>(*this, &sep, behavior, cs, 1);
}

/*!
    \overload
    \since 5.4
*/
QVector<QStringRef> QString::splitRef(QChar sep, SplitBehavior behavior, Qt::CaseSensitivity cs) const
{
    return splitString<QVector<QStringRef> >(QStringRef(this), &sep, behavior, cs, 1);
}

/*!
    Splits the string into substrings references wherever \a sep occurs, and
    returns the list of those strings. If \a sep does not match
    anywhere in the string, split() returns a single-element vector
    containing this string reference.

    \a cs specifies whether \a sep should be matched case
    sensitively or case insensitively.

    If \a behavior is QString::SkipEmptyParts, empty entries don't
    appear in the result. By default, empty entries are kept.

    \note All references are valid as long this string is alive. Destroying this
    string will cause all references be dangling pointers.

    \since 5.4
*/
QVector<QStringRef> QStringRef::split(const QString &sep, QString::SplitBehavior behavior, Qt::CaseSensitivity cs) const
{
    return splitString<QVector<QStringRef> >(*this, sep.constData(), behavior, cs, sep.size());
}

/*!
    \overload
    \since 5.4
*/
QVector<QStringRef> QStringRef::split(QChar sep, QString::SplitBehavior behavior, Qt::CaseSensitivity cs) const
{
    return splitString<QVector<QStringRef> >(*this, &sep, behavior, cs, 1);
}

#ifndef QT_NO_REGEXP
namespace {
template<class ResultList, typename MidMethod>
static ResultList splitString(const QString &source, MidMethod mid, const QRegExp &rx, QString::SplitBehavior behavior)
{
    QRegExp rx2(rx);
    ResultList list;
    int start = 0;
    int extra = 0;
    int end;
    while ((end = rx2.indexIn(source, start + extra)) != -1) {
        int matchedLen = rx2.matchedLength();
        if (start != end || behavior == QString::KeepEmptyParts)
            list.append((source.*mid)(start, end - start));
        start = end + matchedLen;
        extra = (matchedLen == 0) ? 1 : 0;
    }
    if (start != source.size() || behavior == QString::KeepEmptyParts)
        list.append((source.*mid)(start, -1));
    return list;
}
} // namespace

/*!
    \overload

    Splits the string into substrings wherever the regular expression
    \a rx matches, and returns the list of those strings. If \a rx
    does not match anywhere in the string, split() returns a
    single-element list containing this string.

    Here's an example where we extract the words in a sentence
    using one or more whitespace characters as the separator:

    \snippet qstring/main.cpp 59

    Here's a similar example, but this time we use any sequence of
    non-word characters as the separator:

    \snippet qstring/main.cpp 60

    Here's a third example where we use a zero-length assertion,
    \b{\\b} (word boundary), to split the string into an
    alternating sequence of non-word and word tokens:

    \snippet qstring/main.cpp 61

    \sa QStringList::join(), section()
*/
QStringList QString::split(const QRegExp &rx, SplitBehavior behavior) const
{
    return splitString<QStringList>(*this, &QString::mid, rx, behavior);
}

/*!
    \overload
    \since 5.4

    Splits the string into substring references wherever the regular expression
    \a rx matches, and returns the list of those strings. If \a rx
    does not match anywhere in the string, splitRef() returns a
    single-element vector containing this string reference.

    \note All references are valid as long this string is alive. Destroying this
    string will cause all references be dangling pointers.

    \sa QStringRef split()
*/
QVector<QStringRef> QString::splitRef(const QRegExp &rx, SplitBehavior behavior) const
{
    return splitString<QVector<QStringRef> >(*this, &QString::midRef, rx, behavior);
}
#endif

#ifndef QT_NO_REGULAREXPRESSION
#ifndef QT_BOOTSTRAPPED
namespace {
template<class ResultList, typename MidMethod>
static ResultList splitString(const QString &source, MidMethod mid, const QRegularExpression &re,
                              QString::SplitBehavior behavior)
{
    ResultList list;
    if (!re.isValid()) {
        qWarning("QString::split: invalid QRegularExpression object");
        return list;
    }

    int start = 0;
    int end = 0;
    QRegularExpressionMatchIterator iterator = re.globalMatch(source);
    while (iterator.hasNext()) {
        QRegularExpressionMatch match = iterator.next();
        end = match.capturedStart();
        if (start != end || behavior == QString::KeepEmptyParts)
            list.append((source.*mid)(start, end - start));
        start = match.capturedEnd();
    }

    if (start != source.size() || behavior == QString::KeepEmptyParts)
        list.append((source.*mid)(start, -1));

    return list;
}
} // namespace

/*!
    \overload
    \since 5.0

    Splits the string into substrings wherever the regular expression
    \a re matches, and returns the list of those strings. If \a re
    does not match anywhere in the string, split() returns a
    single-element list containing this string.

    Here's an example where we extract the words in a sentence
    using one or more whitespace characters as the separator:

    \snippet qstring/main.cpp 90

    Here's a similar example, but this time we use any sequence of
    non-word characters as the separator:

    \snippet qstring/main.cpp 91

    Here's a third example where we use a zero-length assertion,
    \b{\\b} (word boundary), to split the string into an
    alternating sequence of non-word and word tokens:

    \snippet qstring/main.cpp 92

    \sa QStringList::join(), section()
*/
QStringList QString::split(const QRegularExpression &re, SplitBehavior behavior) const
{
    return splitString<QStringList>(*this, &QString::mid, re, behavior);
}

/*!
    \overload
    \since 5.4

    Splits the string into substring references wherever the regular expression
    \a re matches, and returns the list of those strings. If \a re
    does not match anywhere in the string, splitRef() returns a
    single-element vector containing this string reference.

    \note All references are valid as long this string is alive. Destroying this
    string will cause all references be dangling pointers.

    \sa split() QStringRef
*/
QVector<QStringRef> QString::splitRef(const QRegularExpression &re, SplitBehavior behavior) const
{
    return splitString<QVector<QStringRef> >(*this, &QString::midRef, re, behavior);
}
#endif // QT_BOOTSTRAPPED
#endif // QT_NO_REGULAREXPRESSION

/*!
    \enum QString::NormalizationForm

    This enum describes the various normalized forms of Unicode text.

    \value NormalizationForm_D  Canonical Decomposition
    \value NormalizationForm_C  Canonical Decomposition followed by Canonical Composition
    \value NormalizationForm_KD  Compatibility Decomposition
    \value NormalizationForm_KC  Compatibility Decomposition followed by Canonical Composition

    \sa normalized(),
        {http://www.unicode.org/reports/tr15/}{Unicode Standard Annex #15}
*/

/*!
    \since 4.5

    Returns a copy of this string repeated the specified number of \a times.

    If \a times is less than 1, an empty string is returned.

    Example:

    \code
        QString str("ab");
        str.repeated(4);            // returns "abababab"
    \endcode
*/
QString QString::repeated(int times) const
{
    if (d->size == 0)
        return *this;

    if (times <= 1) {
        if (times == 1)
            return *this;
        return QString();
    }

    const int resultSize = times * d->size;

    QString result;
    result.reserve(resultSize);
    if (result.d->alloc != uint(resultSize) + 1u)
        return QString(); // not enough memory

    memcpy(result.d->data(), d->data(), d->size * sizeof(ushort));

    int sizeSoFar = d->size;
    ushort *end = result.d->data() + sizeSoFar;

    const int halfResultSize = resultSize >> 1;
    while (sizeSoFar <= halfResultSize) {
        memcpy(end, result.d->data(), sizeSoFar * sizeof(ushort));
        end += sizeSoFar;
        sizeSoFar <<= 1;
    }
    memcpy(end, result.d->data(), (resultSize - sizeSoFar) * sizeof(ushort));
    result.d->data()[resultSize] = '\0';
    result.d->size = resultSize;
    return result;
}

void qt_string_normalize(QString *data, QString::NormalizationForm mode, QChar::UnicodeVersion version, int from)
{
    bool simple = true;
    const QChar *p = data->constData();
    int len = data->length();
    for (int i = from; i < len; ++i) {
        if (p[i].unicode() >= 0x80) {
            simple = false;
            if (i > from)
                from = i - 1;
            break;
        }
    }
    if (simple)
        return;

    if (version == QChar::Unicode_Unassigned) {
        version = QChar::currentUnicodeVersion();
    } else if (int(version) <= NormalizationCorrectionsVersionMax) {
        const QString &s = *data;
        QChar *d = 0;
        for (int i = 0; i < NumNormalizationCorrections; ++i) {
            const NormalizationCorrection &n = uc_normalization_corrections[i];
            if (n.version > version) {
                int pos = from;
                if (QChar::requiresSurrogates(n.ucs4)) {
                    ushort ucs4High = QChar::highSurrogate(n.ucs4);
                    ushort ucs4Low = QChar::lowSurrogate(n.ucs4);
                    ushort oldHigh = QChar::highSurrogate(n.old_mapping);
                    ushort oldLow = QChar::lowSurrogate(n.old_mapping);
                    while (pos < s.length() - 1) {
                        if (s.at(pos).unicode() == ucs4High && s.at(pos + 1).unicode() == ucs4Low) {
                            if (!d)
                                d = data->data();
                            d[pos] = QChar(oldHigh);
                            d[++pos] = QChar(oldLow);
                        }
                        ++pos;
                    }
                } else {
                    while (pos < s.length()) {
                        if (s.at(pos).unicode() == n.ucs4) {
                            if (!d)
                                d = data->data();
                            d[pos] = QChar(n.old_mapping);
                        }
                        ++pos;
                    }
                }
            }
        }
    }

    if (normalizationQuickCheckHelper(data, mode, from, &from))
        return;

    decomposeHelper(data, mode < QString::NormalizationForm_KD, version, from);

    canonicalOrderHelper(data, version, from);

    if (mode == QString::NormalizationForm_D || mode == QString::NormalizationForm_KD)
        return;

    composeHelper(data, version, from);
}

/*!
    Returns the string in the given Unicode normalization \a mode,
    according to the given \a version of the Unicode standard.
*/
QString QString::normalized(QString::NormalizationForm mode, QChar::UnicodeVersion version) const
{
    QString copy = *this;
    qt_string_normalize(&copy, mode, version, 0);
    return copy;
}


struct ArgEscapeData
{
    int min_escape;            // lowest escape sequence number
    int occurrences;           // number of occurrences of the lowest escape sequence number
    int locale_occurrences;    // number of occurrences of the lowest escape sequence number that
                               // contain 'L'
    int escape_len;            // total length of escape sequences which will be replaced
};

static ArgEscapeData findArgEscapes(const QString &s)
{
    const QChar *uc_begin = s.unicode();
    const QChar *uc_end = uc_begin + s.length();

    ArgEscapeData d;

    d.min_escape = INT_MAX;
    d.occurrences = 0;
    d.escape_len = 0;
    d.locale_occurrences = 0;

    const QChar *c = uc_begin;
    while (c != uc_end) {
        while (c != uc_end && c->unicode() != '%')
            ++c;

        if (c == uc_end)
            break;
        const QChar *escape_start = c;
        if (++c == uc_end)
            break;

        bool locale_arg = false;
        if (c->unicode() == 'L') {
            locale_arg = true;
            if (++c == uc_end)
                break;
        }

        int escape = c->digitValue();
        if (escape == -1)
            continue;

        ++c;

        if (c != uc_end) {
            int next_escape = c->digitValue();
            if (next_escape != -1) {
                escape = (10 * escape) + next_escape;
                ++c;
            }
        }

        if (escape > d.min_escape)
            continue;

        if (escape < d.min_escape) {
            d.min_escape = escape;
            d.occurrences = 0;
            d.escape_len = 0;
            d.locale_occurrences = 0;
        }

        ++d.occurrences;
        if (locale_arg)
            ++d.locale_occurrences;
        d.escape_len += c - escape_start;
    }
    return d;
}

static QString replaceArgEscapes(const QString &s, const ArgEscapeData &d, int field_width,
                                 const QString &arg, const QString &larg, QChar fillChar = QLatin1Char(' '))
{
    const QChar *uc_begin = s.unicode();
    const QChar *uc_end = uc_begin + s.length();

    int abs_field_width = qAbs(field_width);
    int result_len = s.length()
                     - d.escape_len
                     + (d.occurrences - d.locale_occurrences)
                     *qMax(abs_field_width, arg.length())
                     + d.locale_occurrences
                     *qMax(abs_field_width, larg.length());

    QString result(result_len, Qt::Uninitialized);
    QChar *result_buff = (QChar*) result.unicode();

    QChar *rc = result_buff;
    const QChar *c = uc_begin;
    int repl_cnt = 0;
    while (c != uc_end) {
        /* We don't have to check if we run off the end of the string with c,
           because as long as d.occurrences > 0 we KNOW there are valid escape
           sequences. */

        const QChar *text_start = c;

        while (c->unicode() != '%')
            ++c;

        const QChar *escape_start = c++;

        bool locale_arg = false;
        if (c->unicode() == 'L') {
            locale_arg = true;
            ++c;
        }

        int escape = c->digitValue();
        if (escape != -1) {
            if (c + 1 != uc_end && (c + 1)->digitValue() != -1) {
                escape = (10 * escape) + (c + 1)->digitValue();
                ++c;
            }
        }

        if (escape != d.min_escape) {
            memcpy(rc, text_start, (c - text_start)*sizeof(QChar));
            rc += c - text_start;
        }
        else {
            ++c;

            memcpy(rc, text_start, (escape_start - text_start)*sizeof(QChar));
            rc += escape_start - text_start;

            uint pad_chars;
            if (locale_arg)
                pad_chars = qMax(abs_field_width, larg.length()) - larg.length();
            else
                pad_chars = qMax(abs_field_width, arg.length()) - arg.length();

            if (field_width > 0) { // left padded
                for (uint i = 0; i < pad_chars; ++i)
                    (rc++)->unicode() = fillChar.unicode();
            }

            if (locale_arg) {
                memcpy(rc, larg.unicode(), larg.length()*sizeof(QChar));
                rc += larg.length();
            }
            else {
                memcpy(rc, arg.unicode(), arg.length()*sizeof(QChar));
                rc += arg.length();
            }

            if (field_width < 0) { // right padded
                for (uint i = 0; i < pad_chars; ++i)
                    (rc++)->unicode() = fillChar.unicode();
            }

            if (++repl_cnt == d.occurrences) {
                memcpy(rc, c, (uc_end - c)*sizeof(QChar));
                rc += uc_end - c;
                Q_ASSERT(rc - result_buff == result_len);
                c = uc_end;
            }
        }
    }
    Q_ASSERT(rc == result_buff + result_len);

    return result;
}

/*!
  Returns a copy of this string with the lowest numbered place marker
  replaced by string \a a, i.e., \c %1, \c %2, ..., \c %99.

  \a fieldWidth specifies the minimum amount of space that argument \a
  a shall occupy. If \a a requires less space than \a fieldWidth, it
  is padded to \a fieldWidth with character \a fillChar.  A positive
  \a fieldWidth produces right-aligned text. A negative \a fieldWidth
  produces left-aligned text.

  This example shows how we might create a \c status string for
  reporting progress while processing a list of files:

  \snippet qstring/main.cpp 11

  First, \c arg(i) replaces \c %1. Then \c arg(total) replaces \c
  %2. Finally, \c arg(fileName) replaces \c %3.

  One advantage of using arg() over asprintf() is that the order of the
  numbered place markers can change, if the application's strings are
  translated into other languages, but each arg() will still replace
  the lowest numbered unreplaced place marker, no matter where it
  appears. Also, if place marker \c %i appears more than once in the
  string, the arg() replaces all of them.

  If there is no unreplaced place marker remaining, a warning message
  is output and the result is undefined. Place marker numbers must be
  in the range 1 to 99.
*/
QString QString::arg(const QString &a, int fieldWidth, QChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        qWarning("QString::arg: Argument missing: %s, %s", toLocal8Bit().data(),
                  a.toLocal8Bit().data());
        return *this;
    }
    return replaceArgEscapes(*this, d, fieldWidth, a, a, fillChar);
}

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2) const
  \overload arg()

  This is the same as \c {str.arg(a1).arg(a2)}, except that the
  strings \a a1 and \a a2 are replaced in one pass. This can make a
  difference if \a a1 contains e.g. \c{%1}:

  \snippet qstring/main.cpp 13

  A similar problem occurs when the numbered place markers are not
  white space separated:

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 97

  Let's look at the substitutions:
  \list
  \li First, \c Hello replaces \c {%1} so the string becomes \c {"Hello%3%2"}.
  \li Then, \c 20 replaces \c {%2} so the string becomes \c {"Hello%320"}.
  \li Since the maximum numbered place marker value is 99, \c 50 replaces \c {%32}.
  \endlist
  Thus the string finally becomes \c {"Hello500"}.

  In such cases, the following yields the expected results:

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 98
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3) const
  \overload arg()

  This is the same as calling \c str.arg(a1).arg(a2).arg(a3), except
  that the strings \a a1, \a a2 and \a a3 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4)}, except that the strings \a
  a1, \a a2, \a a3 and \a a4 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5)}, except that the strings
  \a a1, \a a2, \a a3, \a a4, and \a a5 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6))}, except that
  the strings \a a1, \a a2, \a a3, \a a4, \a a5, and \a a6 are
  replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6, const QString& a7) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6,
  and \a a7 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6, const QString& a7, const QString& a8) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6, \a
  a7, and \a a8 are replaced in one pass.
*/

/*!
  \fn QString QString::arg(const QString& a1, const QString& a2, const QString& a3, const QString& a4, const QString& a5, const QString& a6, const QString& a7, const QString& a8, const QString& a9) const
  \overload arg()

  This is the same as calling \c
  {str.arg(a1).arg(a2).arg(a3).arg(a4).arg(a5).arg(a6).arg(a7).arg(a8).arg(a9)},
  except that the strings \a a1, \a a2, \a a3, \a a4, \a a5, \a a6, \a
  a7, \a a8, and \a a9 are replaced in one pass.
*/

/*! \fn QString QString::arg(int a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  The \a a argument is expressed in base \a base, which is 10 by
  default and must be between 2 and 36. For bases other than 10, \a a
  is treated as an unsigned integer.

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale, set by QLocale::setDefault(). If no default
  locale was specified, the "C" locale is used. The 'L' flag is
  ignored if \a base is not 10.

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 14

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn QString QString::arg(uint a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn QString QString::arg(long a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a a argument is expressed in the given \a base, which is 10 by
  default and must be between 2 and 36.

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale. The default locale is determined from the
  system's locale settings at application startup. It can be changed
  using QLocale::setDefault(). The 'L' flag is ignored if \a base is
  not 10.

  \snippet qstring/main.cpp 12
  \snippet qstring/main.cpp 14

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*! \fn QString QString::arg(ulong a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a to a string. The base must be between 2 and 36, with 8
  giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/
QString QString::arg(qlonglong a, int fieldWidth, int base, QChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        qWarning() << "QString::arg: Argument missing:" << *this << ',' << a;
        return *this;
    }

    unsigned flags = QLocaleData::NoFlags;
    if (fillChar == QLatin1Char('0'))
        flags = QLocaleData::ZeroPadded;

    QString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = QLocaleData::c()->longLongToString(a, -1, base, fieldWidth, flags);

    QString locale_arg;
    if (d.locale_occurrences > 0) {
        QLocale locale;
        if (!(locale.numberOptions() & QLocale::OmitGroupSeparator))
            flags |= QLocaleData::ThousandsGroup;
        locale_arg = locale.d->m_data->longLongToString(a, -1, base, fieldWidth, flags);
    }

    return replaceArgEscapes(*this, d, fieldWidth, arg, locale_arg, fillChar);
}

/*!
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. \a base must be between 2 and 36, with 8
  giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/
QString QString::arg(qulonglong a, int fieldWidth, int base, QChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        qWarning() << "QString::arg: Argument missing:" << *this << ',' << a;
        return *this;
    }

    unsigned flags = QLocaleData::NoFlags;
    if (fillChar == QLatin1Char('0'))
        flags = QLocaleData::ZeroPadded;

    QString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = QLocaleData::c()->unsLongLongToString(a, -1, base, fieldWidth, flags);

    QString locale_arg;
    if (d.locale_occurrences > 0) {
        QLocale locale;
        if (!(locale.numberOptions() & QLocale::OmitGroupSeparator))
            flags |= QLocaleData::ThousandsGroup;
        locale_arg = locale.d->m_data->unsLongLongToString(a, -1, base, fieldWidth, flags);
    }

    return replaceArgEscapes(*this, d, fieldWidth, arg, locale_arg, fillChar);
}

/*!
  \overload arg()

  \fn QString QString::arg(short a, int fieldWidth, int base, QChar fillChar) const

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
  \fn QString QString::arg(ushort a, int fieldWidth, int base, QChar fillChar) const
  \overload arg()

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar. A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  The \a base argument specifies the base to use when converting the
  integer \a a into a string. The base must be between 2 and 36, with
  8 giving octal, 10 decimal, and 16 hexadecimal numbers.

  If \a fillChar is '0' (the number 0, ASCII 48), the locale's zero is
  used. For negative numbers, zero padding might appear before the
  minus sign.
*/

/*!
    \overload arg()
*/
QString QString::arg(QChar a, int fieldWidth, QChar fillChar) const
{
    QString c;
    c += a;
    return arg(c, fieldWidth, fillChar);
}

/*!
  \overload arg()

  The \a a argument is interpreted as a Latin-1 character.
*/
QString QString::arg(char a, int fieldWidth, QChar fillChar) const
{
    QString c;
    c += QLatin1Char(a);
    return arg(c, fieldWidth, fillChar);
}

/*!
  \fn QString QString::arg(double a, int fieldWidth, char format, int precision, QChar fillChar) const
  \overload arg()

  Argument \a a is formatted according to the specified \a format and
  \a precision. See \l{Argument Formats} for details.

  \a fieldWidth specifies the minimum amount of space that \a a is
  padded to and filled with the character \a fillChar.  A positive
  value produces right-aligned text; a negative value produces
  left-aligned text.

  \snippet code/src_corelib_tools_qstring.cpp 2

  The '%' can be followed by an 'L', in which case the sequence is
  replaced with a localized representation of \a a. The conversion
  uses the default locale, set by QLocale::setDefault(). If no
  default locale was specified, the "C" locale is used.

  If \a fillChar is '0' (the number 0, ASCII 48), this function will
  use the locale's zero to pad. For negative numbers, the zero padding
  will probably appear before the minus sign.

  \sa QLocale::toString()
*/
QString QString::arg(double a, int fieldWidth, char fmt, int prec, QChar fillChar) const
{
    ArgEscapeData d = findArgEscapes(*this);

    if (d.occurrences == 0) {
        qWarning("QString::arg: Argument missing: %s, %g", toLocal8Bit().data(), a);
        return *this;
    }

    unsigned flags = QLocaleData::NoFlags;
    if (fillChar == QLatin1Char('0'))
        flags = QLocaleData::ZeroPadded;

    if (qIsUpper(fmt))
        flags |= QLocaleData::CapitalEorX;
    fmt = qToLower(fmt);

    QLocaleData::DoubleForm form = QLocaleData::DFDecimal;
    switch (fmt) {
    case 'f':
        form = QLocaleData::DFDecimal;
        break;
    case 'e':
        form = QLocaleData::DFExponent;
        break;
    case 'g':
        form = QLocaleData::DFSignificantDigits;
        break;
    default:
#if defined(QT_CHECK_RANGE)
        qWarning("QString::arg: Invalid format char '%c'", fmt);
#endif
        break;
    }

    QString arg;
    if (d.occurrences > d.locale_occurrences)
        arg = QLocaleData::c()->doubleToString(a, prec, form, fieldWidth, flags);

    QString locale_arg;
    if (d.locale_occurrences > 0) {
        QLocale locale;

        if (!(locale.numberOptions() & QLocale::OmitGroupSeparator))
            flags |= QLocaleData::ThousandsGroup;
        locale_arg = locale.d->m_data->doubleToString(a, prec, form, fieldWidth, flags);
    }

    return replaceArgEscapes(*this, d, fieldWidth, arg, locale_arg, fillChar);
}

static int getEscape(const QChar *uc, int *pos, int len, int maxNumber = 999)
{
    int i = *pos;
    ++i;
    if (i < len && uc[i] == QLatin1Char('L'))
        ++i;
    if (i < len) {
        int escape = uc[i].unicode() - '0';
        if (uint(escape) >= 10U)
            return -1;
        ++i;
        while (i < len) {
            int digit = uc[i].unicode() - '0';
            if (uint(digit) >= 10U)
                break;
            escape = (escape * 10) + digit;
            ++i;
        }
        if (escape <= maxNumber) {
            *pos = i;
            return escape;
        }
    }
    return -1;
}

/*
    Algorithm for multiArg:

    1. Parse the string as a sequence of verbatim text and placeholders (%L?\d{,3}).
       The L is parsed and accepted for compatibility with non-multi-arg, but since
       multiArg only accepts strings as replacements, the localization request can
       be safely ignored.
    2. The result of step (1) is a list of (string-ref,int)-tuples. The string-ref
       either points at text to be copied verbatim (in which case the int is -1),
       or, initially, at the textual representation of the placeholder. In that case,
       the int contains the numerical number as parsed from the placeholder.
    3. Next, collect all the non-negative ints found, sort them in ascending order and
       remove duplicates.
       3a. If the result has more entires than multiArg() was given replacement strings,
           we have found placeholders we can't satisfy with replacement strings. That is
           fine (there could be another .arg() call coming after this one), so just
           truncate the result to the number of actual multiArg() replacement strings.
       3b. If the result has less entries than multiArg() was given replacement strings,
           the string is missing placeholders. This is an error that the user should be
           warned about.
    4. The result of step (3) is a mapping from the index of any replacement string to
       placeholder number. This is the wrong way around, but since placeholder
       numbers could get as large as 999, while we typically don't have more than 9
       replacement strings, we trade 4K of sparsely-used memory for doing a reverse lookup
       each time we need to map a placeholder number to a replacement string index
       (that's a linear search; but still *much* faster than using an associative container).
    5. Next, for each of the tuples found in step (1), do the following:
       5a. If the int is negative, do nothing.
       5b. Otherwise, if the int is found in the result of step (3) at index I, replace
           the string-ref with a string-ref for the (complete) I'th replacement string.
       5c. Otherwise, do nothing.
    6. Concatenate all string refs into a single result string.
*/

namespace {
struct Part
{
    Part() : stringRef(), number(0) {}
    Part(const QString &s, int pos, int len, int num = -1) Q_DECL_NOTHROW
        : stringRef(&s, pos, len), number(num) {}

    QStringRef stringRef;
    int number;
};
} // unnamed namespace

template <>
class QTypeInfo<Part> : public QTypeInfoMerger<Part, QStringRef, int> {}; // Q_DECLARE_METATYPE


namespace {

enum { ExpectedParts = 32 };

typedef QVarLengthArray<Part, ExpectedParts> ParseResult;
typedef QVarLengthArray<int, ExpectedParts/2> ArgIndexToPlaceholderMap;

static ParseResult parseMultiArgFormatString(const QString &s)
{
    ParseResult result;

    const QChar *uc = s.constData();
    const int len = s.size();
    const int end = len - 1;
    int i = 0;
    int last = 0;

    while (i < end) {
        if (uc[i] == QLatin1Char('%')) {
            int percent = i;
            int number = getEscape(uc, &i, len);
            if (number != -1) {
                if (last != percent)
                    result.push_back(Part(s, last, percent - last)); // literal text (incl. failed placeholders)
                result.push_back(Part(s, percent, i - percent, number));  // parsed placeholder
                last = i;
                continue;
            }
        }
        ++i;
    }

    if (last < len)
        result.push_back(Part(s, last, len - last)); // trailing literal text

    return result;
}

static ArgIndexToPlaceholderMap makeArgIndexToPlaceholderMap(const ParseResult &parts)
{
    ArgIndexToPlaceholderMap result;

    for (ParseResult::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it) {
        if (it->number >= 0)
            result.push_back(it->number);
    }

    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()),
                 result.end());

    return result;
}

static int resolveStringRefsAndReturnTotalSize(ParseResult &parts, const ArgIndexToPlaceholderMap &argIndexToPlaceholderMap, const QString *args[])
{
    int totalSize = 0;
    for (ParseResult::iterator pit = parts.begin(), end = parts.end(); pit != end; ++pit) {
        if (pit->number != -1) {
            const ArgIndexToPlaceholderMap::const_iterator ait
                    = std::find(argIndexToPlaceholderMap.begin(), argIndexToPlaceholderMap.end(), pit->number);
            if (ait != argIndexToPlaceholderMap.end())
                pit->stringRef = QStringRef(args[ait - argIndexToPlaceholderMap.begin()]);
        }
        totalSize += pit->stringRef.size();
    }
    return totalSize;
}

} // unnamed namespace

QString QString::multiArg(int numArgs, const QString **args) const
{
    // Step 1-2 above
    ParseResult parts = parseMultiArgFormatString(*this);

    // 3-4
    ArgIndexToPlaceholderMap argIndexToPlaceholderMap = makeArgIndexToPlaceholderMap(parts);

    if (argIndexToPlaceholderMap.size() > numArgs) // 3a
        argIndexToPlaceholderMap.resize(numArgs);
    else if (argIndexToPlaceholderMap.size() < numArgs) // 3b
        qWarning("QString::arg: %d argument(s) missing in %s",
                 numArgs - argIndexToPlaceholderMap.size(), toLocal8Bit().data());

    // 5
    const int totalSize = resolveStringRefsAndReturnTotalSize(parts, argIndexToPlaceholderMap, args);

    // 6:
    QString result(totalSize, Qt::Uninitialized);
    QChar *out = result.data();

    for (ParseResult::const_iterator it = parts.begin(), end = parts.end(); it != end; ++it) {
        if (const int sz = it->stringRef.size()) {
            memcpy(out, it->stringRef.constData(), sz * sizeof(QChar));
            out += sz;
        }
    }

    return result;
}


/*! \fn QString QString::fromCFString(CFStringRef string)
    \since 5.2

    Constructs a new QString containing a copy of the \a string CFString.

    \note this function is only available on OS X and iOS.
*/

/*! \fn CFStringRef QString::toCFString() const
    \since 5.2

    Creates a CFString from a QString. The caller owns the CFString and is
    responsible for releasing it.

    \note this function is only available on OS X and iOS.
*/

/*! \fn QString QString::fromNSString(const NSString *string)
    \since 5.2

    Constructs a new QString containing a copy of the \a string NSString.

    \note this function is only available on OS X and iOS.
*/

/*! \fn NSString QString::toNSString() const
    \since 5.2

    Creates a NSString from a QString. The NSString is autoreleased.

    \note this function is only available on OS X and iOS.
*/

/*! \fn bool QString::isSimpleText() const

    \internal
*/
bool QString::isSimpleText() const
{
    const ushort *p = d->data();
    const ushort * const end = p + d->size;
    while (p < end) {
        ushort uc = *p;
        // sort out regions of complex text formatting
        if (uc > 0x058f && (uc < 0x1100 || uc > 0xfb0f)) {
            return false;
        }
        p++;
    }

    return true;
}

/*! \fn bool QString::isRightToLeft() const

    Returns \c true if the string is read right to left.
*/
bool QString::isRightToLeft() const
{
    const ushort *p = d->data();
    const ushort * const end = p + d->size;
    while (p < end) {
        uint ucs4 = *p;
        if (QChar::isHighSurrogate(ucs4) && p < end - 1) {
            ushort low = p[1];
            if (QChar::isLowSurrogate(low)) {
                ucs4 = QChar::surrogateToUcs4(ucs4, low);
                ++p;
            }
        }
        switch (QChar::direction(ucs4))
        {
        case QChar::DirL:
            return false;
        case QChar::DirR:
        case QChar::DirAL:
            return true;
        default:
            break;
        }
        ++p;
    }
    return false;
}

/*! \fn QChar *QString::data()

    Returns a pointer to the data stored in the QString. The pointer
    can be used to access and modify the characters that compose the
    string. For convenience, the data is '\\0'-terminated.

    Example:

    \snippet qstring/main.cpp 19

    Note that the pointer remains valid only as long as the string is
    not modified by other means. For read-only access, constData() is
    faster because it never causes a \l{deep copy} to occur.

    \sa constData(), operator[]()
*/

/*! \fn const QChar *QString::data() const

    \overload
*/

/*! \fn const QChar *QString::constData() const

    Returns a pointer to the data stored in the QString. The pointer
    can be used to access the characters that compose the string. For
    convenience, the data is '\\0'-terminated.

    Note that the pointer remains valid only as long as the string is
    not modified.

    \sa data(), operator[]()
*/

/*! \fn void QString::push_front(const QString &other)

    This function is provided for STL compatibility, prepending the
    given \a other string to the beginning of this string. It is
    equivalent to \c prepend(other).

    \sa prepend()
*/

/*! \fn void QString::push_front(QChar ch)

    \overload

    Prepends the given \a ch character to the beginning of this string.
*/

/*! \fn void QString::push_back(const QString &other)

    This function is provided for STL compatibility, appending the
    given \a other string onto the end of this string. It is
    equivalent to \c append(other).

    \sa append()
*/

/*! \fn void QString::push_back(QChar ch)

    \overload

    Appends the given \a ch character onto the end of this string.
*/

/*!
    \fn std::string QString::toStdString() const

    Returns a std::string object with the data contained in this
    QString. The Unicode data is converted into 8-bit characters using
    the toUtf8() function.

    This method is mostly useful to pass a QString to a function
    that accepts a std::string object.

    \sa toLatin1(), toUtf8(), toLocal8Bit(), QByteArray::toStdString()
*/

/*!
    Constructs a QString that uses the first \a size Unicode characters
    in the array \a unicode. The data in \a unicode is \e not
    copied. The caller must be able to guarantee that \a unicode will
    not be deleted or modified as long as the QString (or an
    unmodified copy of it) exists.

    Any attempts to modify the QString or copies of it will cause it
    to create a deep copy of the data, ensuring that the raw data
    isn't modified.

    Here's an example of how we can use a QRegExp on raw data in
    memory without requiring to copy the data into a QString:

    \snippet qstring/main.cpp 22
    \snippet qstring/main.cpp 23

    \warning A string created with fromRawData() is \e not
    '\\0'-terminated, unless the raw data contains a '\\0' character
    at position \a size. This means unicode() will \e not return a
    '\\0'-terminated string (although utf16() does, at the cost of
    copying the raw data).

    \sa fromUtf16(), setRawData()
*/
QString QString::fromRawData(const QChar *unicode, int size)
{
    Data *x;
    if (!unicode) {
        x = Data::sharedNull();
    } else if (!size) {
        x = Data::allocate(0);
    } else {
        x = Data::fromRawData(reinterpret_cast<const ushort *>(unicode), size);
        Q_CHECK_PTR(x);
    }
    QStringDataPtr dataPtr = { x };
    return QString(dataPtr);
}

/*!
    \since 4.7

    Resets the QString to use the first \a size Unicode characters
    in the array \a unicode. The data in \a unicode is \e not
    copied. The caller must be able to guarantee that \a unicode will
    not be deleted or modified as long as the QString (or an
    unmodified copy of it) exists.

    This function can be used instead of fromRawData() to re-use
    existings QString objects to save memory re-allocations.

    \sa fromRawData()
*/
QString &QString::setRawData(const QChar *unicode, int size)
{
    if (d->ref.isShared() || d->alloc) {
        *this = fromRawData(unicode, size);
    } else {
        if (unicode) {
            d->size = size;
            d->offset = reinterpret_cast<const char *>(unicode) - reinterpret_cast<char *>(d);
        } else {
            d->offset = sizeof(QStringData);
            d->size = 0;
        }
    }
    return *this;
}

/*! \fn QString QString::fromStdU16String(const std::u16string &str)
    \since 5.5

    Returns a copy of the \a str string. The given string is assumed
    to be encoded in UTF-16.

    \sa fromUtf16(), fromStdWString(), fromStdU32String()
*/

/*!
    \fn std::u16string QString::toStdU16String() const
    \since 5.5

    Returns a std::u16string object with the data contained in this
    QString. The Unicode data is the same as returned by the utf16()
    method.

    \sa utf16(), toStdWString(), toStdU32String()
*/

/*! \fn QString QString::fromStdU32String(const std::u32string &str)
    \since 5.5

    Returns a copy of the \a str string. The given string is assumed
    to be encoded in UCS-4.

    \sa fromUcs4(), fromStdWString(), fromStdU16String()
*/

/*!
    \fn std::u32string QString::toStdU32String() const
    \since 5.5

    Returns a std::u32string object with the data contained in this
    QString. The Unicode data is the same as returned by the toUcs4()
    method.

    \sa toUcs4(), toStdWString(), toStdU16String()
*/

/*! \class QLatin1String
    \inmodule QtCore
    \brief The QLatin1String class provides a thin wrapper around an US-ASCII/Latin-1 encoded string literal.

    \ingroup string-processing
    \reentrant

    Many of QString's member functions are overloaded to accept
    \c{const char *} instead of QString. This includes the copy
    constructor, the assignment operator, the comparison operators,
    and various other functions such as \l{QString::insert()}{insert()}, \l{QString::replace()}{replace()},
    and \l{QString::indexOf()}{indexOf()}. These functions
    are usually optimized to avoid constructing a QString object for
    the \c{const char *} data. For example, assuming \c str is a
    QString,

    \snippet code/src_corelib_tools_qstring.cpp 3

    is much faster than

    \snippet code/src_corelib_tools_qstring.cpp 4

    because it doesn't construct four temporary QString objects and
    make a deep copy of the character data.

    Applications that define \c QT_NO_CAST_FROM_ASCII (as explained
    in the QString documentation) don't have access to QString's
    \c{const char *} API. To provide an efficient way of specifying
    constant Latin-1 strings, Qt provides the QLatin1String, which is
    just a very thin wrapper around a \c{const char *}. Using
    QLatin1String, the example code above becomes

    \snippet code/src_corelib_tools_qstring.cpp 5

    This is a bit longer to type, but it provides exactly the same
    benefits as the first version of the code, and is faster than
    converting the Latin-1 strings using QString::fromLatin1().

    Thanks to the QString(QLatin1String) constructor,
    QLatin1String can be used everywhere a QString is expected. For
    example:

    \snippet code/src_corelib_tools_qstring.cpp 6

    \note If the function you're calling with a QLatin1String
    argument isn't actually overloaded to take QLatin1String, the
    implicit conversion to QString will trigger a memory allocation,
    which is usually what you want to avoid by using QLatin1String
    in the first place. In those cases, using QStringLiteral may be
    the better option.

    \sa QString, QLatin1Char, {QStringLiteral()}{QStringLiteral}
*/

/*! \fn QLatin1String::QLatin1String()
    \since 5.6

    Constructs a QLatin1String object that stores a nullptr.
*/

/*! \fn QLatin1String::QLatin1String(const char *str)

    Constructs a QLatin1String object that stores \a str.

    The string data is \e not copied. The caller must be able to
    guarantee that \a str will not be deleted or modified as long as
    the QLatin1String object exists.

    \sa latin1()
*/

/*! \fn QLatin1String::QLatin1String(const char *str, int size)

    Constructs a QLatin1String object that stores \a str with \a size.

    The string data is \e not copied. The caller must be able to
    guarantee that \a str will not be deleted or modified as long as
    the QLatin1String object exists.

    \sa latin1()
*/

/*! \fn QLatin1String::QLatin1String(const QByteArray &str)

    Constructs a QLatin1String object that stores \a str.

    The string data is \e not copied. The caller must be able to
    guarantee that \a str will not be deleted or modified as long as
    the QLatin1String object exists.

    \sa latin1()
*/

/*! \fn const char *QLatin1String::latin1() const

    Returns the Latin-1 string stored in this object.
*/

/*! \fn const char *QLatin1String::data() const

    Returns the Latin-1 string stored in this object.
*/

/*! \fn int QLatin1String::size() const

    Returns the size of the Latin-1 string stored in this object.
*/

/*! \fn bool QLatin1String::operator==(const QString &other) const

    Returns \c true if this string is equal to string \a other;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    QString::localeAwareCompare().
*/

/*!
    \fn bool QLatin1String::operator==(const char *other) const
    \since 4.3
    \overload

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \fn bool QLatin1String::operator==(const QByteArray &other) const
    \since 5.0
    \overload

    The \a other byte array is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn bool QLatin1String::operator!=(const QString &other) const

    Returns \c true if this string is not equal to string \a other;
    otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    QString::localeAwareCompare().
*/

/*!
    \fn bool QLatin1String::operator!=(const char *other) const
    \since 4.3
    \overload operator!=()

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \fn bool QLatin1String::operator!=(const QByteArray &other) const
    \since 5.0
    \overload operator!=()

    The \a other byte array is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \fn bool QLatin1String::operator>(const QString &other) const

    Returns \c true if this string is lexically greater than string \a
    other; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    QString::localeAwareCompare().
*/

/*!
    \fn bool QLatin1String::operator>(const char *other) const
    \since 4.3
    \overload

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*!
    \fn bool QLatin1String::operator>(const QByteArray &other) const
    \since 5.0
    \overload

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c QT_NO_CAST_FROM_ASCII
    when you compile your applications. This can be useful if you want
    to ensure that all user-visible strings go through QObject::tr(),
    for example.
*/

/*!
    \fn bool QLatin1String::operator<(const QString &other) const

    Returns \c true if this string is lexically less than the \a other
    string; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.
*/

/*!
    \fn bool QLatin1String::operator<(const char *other) const
    \since 4.3
    \overload

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \fn bool QLatin1String::operator<(const QByteArray &other) const
    \since 5.0
    \overload

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \fn bool QLatin1String::operator>=(const QString &other) const

    Returns \c true if this string is lexically greater than or equal
    to string \a other; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    QString::localeAwareCompare().
*/

/*!
    \fn bool QLatin1String::operator>=(const char *other) const
    \since 4.3
    \overload

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \fn bool QLatin1String::operator>=(const QByteArray &other) const
    \since 5.0
    \overload

    The \a other array is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*! \fn bool QLatin1String::operator<=(const QString &other) const

    Returns \c true if this string is lexically less than or equal
    to string \a other; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings with
    QString::localeAwareCompare().
*/

/*!
    \fn bool QLatin1String::operator<=(const char *other) const
    \since 4.3
    \overload

    The \a other const char pointer is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/

/*!
    \fn bool QLatin1String::operator<=(const QByteArray &other) const
    \since 5.0
    \overload

    The \a other array is converted to a QString using
    the QString::fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.
*/


/*! \fn bool operator==(QLatin1String s1, QLatin1String s2)
   \relates QLatin1String

   Returns \c true if string \a s1 is lexically equal to string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator!=(QLatin1String s1, QLatin1String s2)
   \relates QLatin1String

   Returns \c true if string \a s1 is lexically unequal to string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator<(QLatin1String s1, QLatin1String s2)
   \relates QLatin1String

   Returns \c true if string \a s1 is lexically smaller than string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator<=(QLatin1String s1, QLatin1String s2)
   \relates QLatin1String

   Returns \c true if string \a s1 is lexically smaller than or equal to string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator>(QLatin1String s1, QLatin1String s2)
   \relates QLatin1String

   Returns \c true if string \a s1 is lexically greater than string \a s2; otherwise
   returns \c false.
*/
/*! \fn bool operator>=(QLatin1String s1, QLatin1String s2)
   \relates QLatin1String

   Returns \c true if string \a s1 is lexically greater than or equal to
   string \a s2; otherwise returns \c false.
*/


#if !defined(QT_NO_DATASTREAM) || (defined(QT_BOOTSTRAPPED) && !defined(QT_BUILD_QMAKE))
/*!
    \fn QDataStream &operator<<(QDataStream &stream, const QString &string)
    \relates QString

    Writes the given \a string to the specified \a stream.

    \sa {Serializing Qt Data Types}
*/

QDataStream &operator<<(QDataStream &out, const QString &str)
{
    if (out.version() == 1) {
        out << str.toLatin1();
    } else {
        if (!str.isNull() || out.version() < 3) {
            if ((out.byteOrder() == QDataStream::BigEndian) == (QSysInfo::ByteOrder == QSysInfo::BigEndian)) {
                out.writeBytes(reinterpret_cast<const char *>(str.unicode()), sizeof(QChar) * str.length());
            } else {
                QVarLengthArray<ushort> buffer(str.length());
                const ushort *data = reinterpret_cast<const ushort *>(str.constData());
                for (int i = 0; i < str.length(); i++) {
                    buffer[i] = qbswap(*data);
                    ++data;
                }
                out.writeBytes(reinterpret_cast<const char *>(buffer.data()), sizeof(ushort) * buffer.size());
            }
        } else {
            // write null marker
            out << (quint32)0xffffffff;
        }
    }
    return out;
}

/*!
    \fn QDataStream &operator>>(QDataStream &stream, QString &string)
    \relates QString

    Reads a string from the specified \a stream into the given \a string.

    \sa {Serializing Qt Data Types}
*/

QDataStream &operator>>(QDataStream &in, QString &str)
{
    if (in.version() == 1) {
        QByteArray l;
        in >> l;
        str = QString::fromLatin1(l);
    } else {
        quint32 bytes = 0;
        in >> bytes;                                  // read size of string
        if (bytes == 0xffffffff) {                    // null string
            str.clear();
        } else if (bytes > 0) {                       // not empty
            if (bytes & 0x1) {
                str.clear();
                in.setStatus(QDataStream::ReadCorruptData);
                return in;
            }

            const quint32 Step = 1024 * 1024;
            quint32 len = bytes / 2;
            quint32 allocated = 0;

            while (allocated < len) {
                int blockSize = qMin(Step, len - allocated);
                str.resize(allocated + blockSize);
                if (in.readRawData(reinterpret_cast<char *>(str.data()) + allocated * 2,
                                   blockSize * 2) != blockSize * 2) {
                    str.clear();
                    in.setStatus(QDataStream::ReadPastEnd);
                    return in;
                }
                allocated += blockSize;
            }

            if ((in.byteOrder() == QDataStream::BigEndian)
                    != (QSysInfo::ByteOrder == QSysInfo::BigEndian)) {
                ushort *data = reinterpret_cast<ushort *>(str.data());
                while (len--) {
                    *data = qbswap(*data);
                    ++data;
                }
            }
        } else {
            str = QString(QLatin1String(""));
        }
    }
    return in;
}
#endif // QT_NO_DATASTREAM




/*!
    \class QStringRef
    \inmodule QtCore
    \since 4.3
    \brief The QStringRef class provides a thin wrapper around QString substrings.
    \reentrant
    \ingroup tools
    \ingroup string-processing

    QStringRef provides a read-only subset of the QString API.

    A string reference explicitly references a portion of a string()
    with a given size(), starting at a specific position(). Calling
    toString() returns a copy of the data as a real QString instance.

    This class is designed to improve the performance of substring
    handling when manipulating substrings obtained from existing QString
    instances. QStringRef avoids the memory allocation and reference
    counting overhead of a standard QString by simply referencing a
    part of the original string. This can prove to be advantageous in
    low level code, such as that used in a parser, at the expense of
    potentially more complex code.

    For most users, there are no semantic benefits to using QStringRef
    instead of QString since QStringRef requires attention to be paid
    to memory management issues, potentially making code more complex
    to write and maintain.

    \warning A QStringRef is only valid as long as the referenced
    string exists. If the original string is deleted, the string
    reference points to an invalid memory location.

    We suggest that you only use this class in stable code where profiling
    has clearly identified that performance improvements can be made by
    replacing standard string operations with the optimized substring
    handling provided by this class.

    \sa {Implicitly Shared Classes}
*/

/*!
    \typedef QStringRef::size_type
    \internal
*/

/*!
    \typedef QStringRef::value_type
    \internal
*/

/*!
    \typedef QStringRef::const_pointer
    \internal
*/

/*!
    \typedef QStringRef::const_reference
    \internal
*/

/*!
    \typedef QStringRef::const_iterator
    \internal
*/

/*!
 \fn QStringRef::QStringRef()

 Constructs an empty string reference.
*/

/*! \fn QStringRef::QStringRef(const QString *string, int position, int length)

Constructs a string reference to the range of characters in the given
\a string specified by the starting \a position and \a length in characters.

\warning This function exists to improve performance as much as possible,
and performs no bounds checking. For program correctness, \a position and
\a length must describe a valid substring of \a string.

This means that the starting \a position must be positive or 0 and smaller
than \a string's length, and \a length must be positive or 0 but smaller than
the string's length minus the starting \a position;
i.e, 0 <= position < string->length() and
0 <= length <= string->length() - position must both be satisfied.
*/

/*! \fn QStringRef::QStringRef(const QString *string)

Constructs a string reference to the given \a string.
*/

/*! \fn QStringRef::QStringRef(const QStringRef &other)

Constructs a copy of the \a other string reference.
 */
/*!
\fn QStringRef::~QStringRef()

Destroys the string reference.

Since this class is only used to refer to string data, and does not take
ownership of it, no memory is freed when instances are destroyed.
*/


/*!
    \fn int QStringRef::position() const

    Returns the starting position in the referenced string that is referred to
    by the string reference.

    \sa size(), string()
*/

/*!
    \fn int QStringRef::size() const

    Returns the number of characters referred to by the string reference.
    Equivalent to length() and count().

    \sa position(), string()
*/
/*!
    \fn int QStringRef::count() const
    Returns the number of characters referred to by the string reference.
    Equivalent to size() and length().

    \sa position(), string()
*/
/*!
    \fn int QStringRef::length() const
    Returns the number of characters referred to by the string reference.
    Equivalent to size() and count().

    \sa position(), string()
*/


/*!
    \fn bool QStringRef::isEmpty() const

    Returns \c true if the string reference has no characters; otherwise returns
    \c false.

    A string reference is empty if its size is zero.

    \sa size()
*/

/*!
    \fn bool QStringRef::isNull() const

    Returns \c true if string() returns a null pointer or a pointer to a
    null string; otherwise returns \c true.

    \sa size()
*/

/*!
    \fn const QString *QStringRef::string() const

    Returns a pointer to the string referred to by the string reference, or
    0 if it does not reference a string.

    \sa unicode()
*/


/*!
    \fn const QChar *QStringRef::unicode() const

    Returns a Unicode representation of the string reference. Since
    the data stems directly from the referenced string, it is not
    null-terminated unless the string reference includes the string's
    null terminator.

    \sa string()
*/

/*!
    \fn const QChar *QStringRef::data() const

    Same as unicode().
*/

/*!
    \fn const QChar *QStringRef::begin() const
    \since 5.4

    Same as unicode().
*/

/*!
    \fn const QChar *QStringRef::cbegin() const
    \since 5.4

    Same as unicode().
*/

/*!
    \fn const QChar *QStringRef::end() const
    \since 5.4

    Returns a pointer to one character past the last one in this string.
    (It is the same as \c {unicode() + size()}.)
*/

/*!
    \fn const QChar *QStringRef::cend() const
    \since 5.4

    Returns a pointer to one character past the last one in this string.
    (It is the same as \c {unicode() + size()}.)
*/

/*!
    \fn const QChar *QStringRef::constData() const

    Same as unicode().
*/

/*!
    Returns a copy of the string reference as a QString object.

    If the string reference is not a complete reference of the string
    (meaning that position() is 0 and size() equals string()->size()),
    this function will allocate a new string to return.

    \sa string()
*/

QString QStringRef::toString() const {
    if (!m_string)
        return QString();
    if (m_size && m_position == 0 && m_size == m_string->size())
        return *m_string;
    return QString(m_string->unicode() + m_position, m_size);
}


/*! \relates QStringRef

   Returns \c true if string reference \a s1 is lexically equal to string reference \a s2; otherwise
   returns \c false.
*/
bool operator==(const QStringRef &s1,const QStringRef &s2)
{ return (s1.size() == s2.size() &&
          qMemEquals((const ushort *)s1.unicode(), (const ushort *)s2.unicode(), s1.size()));
}

/*! \relates QStringRef

   Returns \c true if string \a s1 is lexically equal to string reference \a s2; otherwise
   returns \c false.
*/
bool operator==(const QString &s1,const QStringRef &s2)
{ return (s1.size() == s2.size() &&
          qMemEquals((const ushort *)s1.unicode(), (const ushort *)s2.unicode(), s1.size()));
}

/*! \relates QStringRef

   Returns \c true if string  \a s1 is lexically equal to string reference \a s2; otherwise
   returns \c false.
*/
bool operator==(QLatin1String s1, const QStringRef &s2)
{
    if (s1.size() != s2.size())
        return false;

    const uchar *c = reinterpret_cast<const uchar *>(s1.latin1());
    if (!c)
        return s2.isEmpty();
    return ucstrncmp(s2.unicode(), c, s2.size()) == 0;
}

/*!
   \relates QStringRef

    Returns \c true if string reference \a s1 is lexically less than
    string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.
*/
bool operator<(const QStringRef &s1,const QStringRef &s2)
{
    return ucstrcmp(s1.constData(), s1.length(), s2.constData(), s2.length()) < 0;
}

/*!\fn bool operator<=(const QStringRef &s1,const QStringRef &s2)

   \relates QStringRef

    Returns \c true if string reference \a s1 is lexically less than
    or equal to string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.
*/

/*!\fn bool operator>=(const QStringRef &s1,const QStringRef &s2)

   \relates QStringRef

    Returns \c true if string reference \a s1 is lexically greater than
    or equal to string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.
*/

/*!\fn bool operator>(const QStringRef &s1,const QStringRef &s2)

   \relates QStringRef

    Returns \c true if string reference \a s1 is lexically greater than
    string reference \a s2; otherwise returns \c false.

    The comparison is based exclusively on the numeric Unicode values
    of the characters and is very fast, but is not what a human would
    expect. Consider sorting user-interface strings using the
    QString::localeAwareCompare() function.
*/


/*!
    \fn const QChar QStringRef::at(int position) const

    Returns the character at the given index \a position in the
    string reference.

    The \a position must be a valid index position in the string
    (i.e., 0 <= \a position < size()).
*/

/*!
    \fn void QStringRef::clear()

    Clears the contents of the string reference by making it null and empty.

    \sa isEmpty(), isNull()
*/

/*!
    \fn QStringRef &QStringRef::operator=(const QStringRef &other)

    Assigns the \a other string reference to this string reference, and
    returns the result.
*/

/*!
    \fn QStringRef &QStringRef::operator=(const QString *string)

    Constructs a string reference to the given \a string and assigns it to
    this string reference, returning the result.
*/

/*!
    \fn bool QStringRef::operator==(const char * s) const

    \overload operator==()

    The \a s byte array is converted to a QStringRef using the
    fromUtf8() function. This function stops conversion at the
    first NUL character found, or the end of the byte array.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    Returns \c true if this string is lexically equal to the parameter
    string \a s. Otherwise returns \c false.

*/

/*!
    \fn bool QStringRef::operator!=(const char * s) const

    \overload operator!=()

    The \a s const char pointer is converted to a QStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    Returns \c true if this string is not lexically equal to the parameter
    string \a s. Otherwise returns \c false.
*/

/*!
    \fn bool QStringRef::operator<(const char * s) const

    \overload operator<()

    The \a s const char pointer is converted to a QStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    Returns \c true if this string is lexically smaller than the parameter
    string \a s. Otherwise returns \c false.
*/

/*!
    \fn bool QStringRef::operator<=(const char * s) const

    \overload operator<=()

    The \a s const char pointer is converted to a QStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    Returns \c true if this string is lexically smaller than or equal to the parameter
    string \a s. Otherwise returns \c false.
*/

/*!
    \fn bool QStringRef::operator>(const char * s) const


    \overload operator>()

    The \a s const char pointer is converted to a QStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    Returns \c true if this string is lexically greater than the parameter
    string \a s. Otherwise returns \c false.
*/

/*!
    \fn bool QStringRef::operator>= (const char * s) const

    \overload operator>=()

    The \a s const char pointer is converted to a QStringRef using
    the fromUtf8() function.

    You can disable this operator by defining \c
    QT_NO_CAST_FROM_ASCII when you compile your applications. This
    can be useful if you want to ensure that all user-visible strings
    go through QObject::tr(), for example.

    Returns \c true if this string is lexically greater than or equal to the
    parameter string \a s. Otherwise returns \c false.
*/
/*!
    \typedef QString::Data
    \internal
*/

/*!
    \typedef QString::DataPtr
    \internal
*/

/*!
    \fn DataPtr & QString::data_ptr()
    \internal
*/



/*!  Appends the string reference to \a string, and returns a new
reference to the combined string data.
 */
QStringRef QStringRef::appendTo(QString *string) const
{
    if (!string)
        return QStringRef();
    int pos = string->size();
    string->insert(pos, unicode(), size());
    return QStringRef(string, pos, size());
}

/*!
    \fn int QStringRef::compare(const QStringRef &s1, const QString &s2, Qt::CaseSensitivity cs = Qt::CaseSensitive)
    \since 4.5

    Compares the string \a s1 with the string \a s2 and returns an
    integer less than, equal to, or greater than zero if \a s1
    is less than, equal to, or greater than \a s2.

    If \a cs is Qt::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.
*/

/*!
    \fn int QStringRef::compare(const QStringRef &s1, const QStringRef &s2, Qt::CaseSensitivity cs = Qt::CaseSensitive)
    \since 4.5
    \overload

    Compares the string \a s1 with the string \a s2 and returns an
    integer less than, equal to, or greater than zero if \a s1
    is less than, equal to, or greater than \a s2.

    If \a cs is Qt::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.
*/

/*!
    \fn int QStringRef::compare(const QStringRef &s1, QLatin1String s2, Qt::CaseSensitivity cs = Qt::CaseSensitive)
    \since 4.5
    \overload

    Compares the string \a s1 with the string \a s2 and returns an
    integer less than, equal to, or greater than zero if \a s1
    is less than, equal to, or greater than \a s2.

    If \a cs is Qt::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.
*/

/*!
    \overload
    \fn int QStringRef::compare(const QString &other, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
    \since 4.5

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    If \a cs is Qt::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Equivalent to \c {compare(*this, other, cs)}.

    \sa QString::compare()
*/

/*!
    \overload
    \fn int QStringRef::compare(const QStringRef &other, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
    \since 4.5

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    If \a cs is Qt::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Equivalent to \c {compare(*this, other, cs)}.

    \sa QString::compare()
*/

/*!
    \overload
    \fn int QStringRef::compare(QLatin1String other, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
    \since 4.5

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    If \a cs is Qt::CaseSensitive, the comparison is case sensitive;
    otherwise the comparison is case insensitive.

    Equivalent to \c {compare(*this, other, cs)}.

    \sa QString::compare()
*/

/*!
    \fn int QStringRef::localeAwareCompare(const QStringRef &s1, const QString & s2)
    \since 4.5

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

    On OS X and iOS, this function compares according the
    "Order for sorted lists" setting in the International prefereces panel.

    \sa compare(), QLocale
*/

/*!
    \fn int QStringRef::localeAwareCompare(const QStringRef &s1, const QStringRef & s2)
    \since 4.5
    \overload

    Compares \a s1 with \a s2 and returns an integer less than, equal
    to, or greater than zero if \a s1 is less than, equal to, or
    greater than \a s2.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.

*/

/*!
    \fn int QStringRef::localeAwareCompare(const QString &other) const
    \since 4.5
    \overload

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.
*/

/*!
    \fn int QStringRef::localeAwareCompare(const QStringRef &other) const
    \since 4.5
    \overload

    Compares this string with the \a other string and returns an
    integer less than, equal to, or greater than zero if this string
    is less than, equal to, or greater than the \a other string.

    The comparison is performed in a locale- and also
    platform-dependent manner. Use this function to present sorted
    lists of strings to the user.
*/

/*!
    \fn QString &QString::append(const QStringRef &reference)
    \since 4.4

    Appends the given string \a reference to this string and returns the result.
 */
QString &QString::append(const QStringRef &str)
{
    if (str.string() == this) {
        str.appendTo(this);
    } else if (str.string()) {
        int oldSize = size();
        resize(oldSize + str.size());
        memcpy(data() + oldSize, str.unicode(), str.size() * sizeof(QChar));
    }
    return *this;
}

/*!
    \fn QStringRef::left(int n) const
    \since 5.2

    Returns a substring reference to the \a n leftmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \sa right(), mid(), startsWith()
*/
QStringRef QStringRef::left(int n) const
{
    if (uint(n) >= uint(m_size))
        return *this;
    return QStringRef(m_string, m_position, n);
}

/*!
    \since 4.4

    Returns a substring reference to the \a n leftmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \snippet qstring/main.cpp leftRef

    \sa left(), rightRef(), midRef(), startsWith()
*/
QStringRef QString::leftRef(int n)  const
{
    if (uint(n) >= uint(d->size))
        n = d->size;
    return QStringRef(this, 0, n);
}

/*!
    \fn QStringRef::right(int n) const
    \since 5.2

    Returns a substring reference to the \a n rightmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \sa left(), mid(), endsWith()
*/
QStringRef QStringRef::right(int n) const
{
    if (uint(n) >= uint(m_size))
        return *this;
    return QStringRef(m_string, m_size - n + m_position, n);
}

/*!
    \since 4.4

    Returns a substring reference to the \a n rightmost characters
    of the string.

    If \a n is greater than or equal to size(), or less than zero,
    a reference to the entire string is returned.

    \snippet qstring/main.cpp rightRef

    \sa right(), leftRef(), midRef(), endsWith()
*/
QStringRef QString::rightRef(int n) const
{
    if (uint(n) >= uint(d->size))
        n = d->size;
    return QStringRef(this, d->size - n, n);
}

/*!
    \fn QStringRef::mid(int position, int n = -1) const
    \since 5.2

    Returns a substring reference to \a n characters of this string,
    starting at the specified \a position.

    If the \a position exceeds the length of the string, a null
    reference is returned.

    If there are less than \a n characters available in the string,
    starting at the given \a position, or if \a n is -1 (default), the
    function returns all characters from the specified \a position
    onwards.

    \sa left(), right()
*/
QStringRef QStringRef::mid(int pos, int n) const
{
    using namespace QtPrivate;
    switch (QContainerImplHelper::mid(m_size, &pos, &n)) {
    case QContainerImplHelper::Null:
        return QStringRef();
    case QContainerImplHelper::Empty:
        return QStringRef(m_string, 0, 0);
    case QContainerImplHelper::Full:
        return *this;
    case QContainerImplHelper::Subset:
        return QStringRef(m_string, pos + m_position, n);
    }
    Q_UNREACHABLE();
    return QStringRef();
}

/*!
    \since 4.4

    Returns a substring reference to \a n characters of this string,
    starting at the specified \a position.

    If the \a position exceeds the length of the string, a null
    reference is returned.

    If there are less than \a n characters available in the string,
    starting at the given \a position, or if \a n is -1 (default), the
    function returns all characters from the specified \a position
    onwards.

    Example:

    \snippet qstring/main.cpp midRef

    \sa mid(), leftRef(), rightRef()
*/
QStringRef QString::midRef(int position, int n) const
{
    using namespace QtPrivate;
    switch (QContainerImplHelper::mid(d->size, &position, &n)) {
    case QContainerImplHelper::Null:
        return QStringRef();
    case QContainerImplHelper::Empty:
        return QStringRef(this, 0, 0);
    case QContainerImplHelper::Full:
        return QStringRef(this, 0, d->size);
    case QContainerImplHelper::Subset:
        return QStringRef(this, position, n);
    }
    Q_UNREACHABLE();
    return QStringRef();
}

/*!
    \fn void QStringRef::truncate(int position)
    \since 5.6

    Truncates the string at the given \a position index.

    If the specified \a position index is beyond the end of the
    string, nothing happens.

    If \a position is negative, it is equivalent to passing zero.

    \sa QString::truncate()
*/

/*!
  \since 4.8

  Returns the index position of the first occurrence of the string \a
  str in this string reference, searching forward from index position
  \a from. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa QString::indexOf(), lastIndexOf(), contains(), count()
*/
int QStringRef::indexOf(const QString &str, int from, Qt::CaseSensitivity cs) const
{
    return qFindString(unicode(), length(), from, str.unicode(), str.length(), cs);
}

/*!
    \since 4.8
    \overload indexOf()

    Returns the index position of the first occurrence of the
    character \a ch in the string reference, searching forward from
    index position \a from. Returns -1 if \a ch could not be found.

    \sa QString::indexOf(), lastIndexOf(), contains(), count()
*/
int QStringRef::indexOf(QChar ch, int from, Qt::CaseSensitivity cs) const
{
    return findChar(unicode(), length(), ch, from, cs);
}

/*!
  \since 4.8

  Returns the index position of the first occurrence of the string \a
  str in this string reference, searching forward from index position
  \a from. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  If \a from is -1, the search starts at the last character; if it is
  -2, at the next to last character and so on.

  \sa QString::indexOf(), lastIndexOf(), contains(), count()
*/
int QStringRef::indexOf(QLatin1String str, int from, Qt::CaseSensitivity cs) const
{
    return qt_find_latin1_string(unicode(), size(), str, from, cs);
}

/*!
    \since 4.8

    \overload indexOf()

    Returns the index position of the first occurrence of the string
    reference \a str in this string reference, searching forward from
    index position \a from. Returns -1 if \a str is not found.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa QString::indexOf(), lastIndexOf(), contains(), count()
*/
int QStringRef::indexOf(const QStringRef &str, int from, Qt::CaseSensitivity cs) const
{
    return qFindString(unicode(), size(), from, str.unicode(), str.size(), cs);
}

/*!
  \since 4.8

  Returns the index position of the last occurrence of the string \a
  str in this string reference, searching backward from index position
  \a from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa QString::lastIndexOf(), indexOf(), contains(), count()
*/
int QStringRef::lastIndexOf(const QString &str, int from, Qt::CaseSensitivity cs) const
{
    const int sl = str.size();
    if (sl == 1)
        return lastIndexOf(str.at(0), from, cs);

    const int l = size();;
    if (from < 0)
        from += l;
    int delta = l - sl;
    if (from == l && sl == 0)
        return from;
    if (uint(from) >= uint(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    return lastIndexOfHelper(reinterpret_cast<const ushort*>(unicode()), from,
                             reinterpret_cast<const ushort*>(str.unicode()), str.size(), cs);
}

/*!
  \since 4.8
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the character
  \a ch, searching backward from position \a from.

  \sa QString::lastIndexOf(), indexOf(), contains(), count()
*/
int QStringRef::lastIndexOf(QChar ch, int from, Qt::CaseSensitivity cs) const
{
    return qt_last_index_of(unicode(), size(), ch, from, cs);
}

/*!
  \since 4.8
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string \a
  str in this string reference, searching backward from index position
  \a from. If \a from is -1 (default), the search starts at the last
  character; if \a from is -2, at the next to last character and so
  on. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa QString::lastIndexOf(), indexOf(), contains(), count()
*/
int QStringRef::lastIndexOf(QLatin1String str, int from, Qt::CaseSensitivity cs) const
{
    const int sl = str.size();
    if (sl == 1)
        return lastIndexOf(QLatin1Char(str.latin1()[0]), from, cs);

    const int l = size();
    if (from < 0)
        from += l;
    int delta = l - sl;
    if (from == l && sl == 0)
        return from;
    if (uint(from) >= uint(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    QVarLengthArray<ushort> s(sl);
    qt_from_latin1(s.data(), str.latin1(), sl);

    return lastIndexOfHelper(reinterpret_cast<const ushort*>(unicode()), from, s.data(), sl, cs);
}

/*!
  \since 4.8
  \overload lastIndexOf()

  Returns the index position of the last occurrence of the string
  reference \a str in this string reference, searching backward from
  index position \a from. If \a from is -1 (default), the search
  starts at the last character; if \a from is -2, at the next to last
  character and so on. Returns -1 if \a str is not found.

  If \a cs is Qt::CaseSensitive (default), the search is case
  sensitive; otherwise the search is case insensitive.

  \sa QString::lastIndexOf(), indexOf(), contains(), count()
*/
int QStringRef::lastIndexOf(const QStringRef &str, int from, Qt::CaseSensitivity cs) const
{
    const int sl = str.size();
    if (sl == 1)
        return lastIndexOf(str.at(0), from, cs);

    const int l = size();
    if (from < 0)
        from += l;
    int delta = l - sl;
    if (from == l && sl == 0)
        return from;
    if (uint(from) >= uint(l) || delta < 0)
        return -1;
    if (from > delta)
        from = delta;

    return lastIndexOfHelper(reinterpret_cast<const ushort*>(unicode()), from,
                             reinterpret_cast<const ushort*>(str.unicode()),
                             str.size(), cs);
}

/*!
    \since 4.8
    Returns the number of (potentially overlapping) occurrences of
    the string \a str in this string reference.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa QString::count(), contains(), indexOf()
*/
int QStringRef::count(const QString &str, Qt::CaseSensitivity cs) const
{
    return qt_string_count(unicode(), size(), str.unicode(), str.size(), cs);
}

/*!
    \since 4.8
    \overload count()

    Returns the number of occurrences of the character \a ch in the
    string reference.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa QString::count(), contains(), indexOf()
*/
int QStringRef::count(QChar ch, Qt::CaseSensitivity cs) const
{
    return qt_string_count(unicode(), size(), ch, cs);
}

/*!
    \since 4.8
    \overload count()

    Returns the number of (potentially overlapping) occurrences of the
    string reference \a str in this string reference.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa QString::count(), contains(), indexOf()
*/
int QStringRef::count(const QStringRef &str, Qt::CaseSensitivity cs) const
{
    return qt_string_count(unicode(), size(), str.unicode(), str.size(), cs);
}

/*!
    \since 4.8

    Returns \c true if the string reference starts with \a str; otherwise
    returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa QString::startsWith(), endsWith()
*/
bool QStringRef::startsWith(const QString &str, Qt::CaseSensitivity cs) const
{
    return qt_starts_with(isNull() ? 0 : unicode(), size(),
                          str.isNull() ? 0 : str.unicode(), str.size(), cs);
}

/*!
    \since 4.8
    \overload startsWith()
    \sa QString::startsWith(), endsWith()
*/
bool QStringRef::startsWith(QLatin1String str, Qt::CaseSensitivity cs) const
{
    return qt_starts_with(isNull() ? 0 : unicode(), size(), str, cs);
}

/*!
    \since 4.8
    \overload startsWith()
    \sa QString::startsWith(), endsWith()
*/
bool QStringRef::startsWith(const QStringRef &str, Qt::CaseSensitivity cs) const
{
    return qt_starts_with(isNull() ? 0 : unicode(), size(),
                          str.isNull() ? 0 : str.unicode(), str.size(), cs);
}

/*!
    \since 4.8
    \overload startsWith()

    Returns \c true if the string reference starts with \a ch; otherwise
    returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa QString::startsWith(), endsWith()
*/
bool QStringRef::startsWith(QChar ch, Qt::CaseSensitivity cs) const
{
    if (!isEmpty()) {
        const ushort *data = reinterpret_cast<const ushort*>(unicode());
        return (cs == Qt::CaseSensitive
                ? data[0] == ch
                : foldCase(data[0]) == foldCase(ch.unicode()));
    } else {
        return false;
    }
}

/*!
    \since 4.8
    Returns \c true if the string reference ends with \a str; otherwise
    returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa QString::endsWith(), startsWith()
*/
bool QStringRef::endsWith(const QString &str, Qt::CaseSensitivity cs) const
{
    return qt_ends_with(isNull() ? 0 : unicode(), size(),
                        str.isNull() ? 0 : str.unicode(), str.size(), cs);
}

/*!
    \since 4.8
    \overload endsWith()

    Returns \c true if the string reference ends with \a ch; otherwise
    returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is case
    sensitive; otherwise the search is case insensitive.

    \sa QString::endsWith(), endsWith()
*/
bool QStringRef::endsWith(QChar ch, Qt::CaseSensitivity cs) const
{
    if (!isEmpty()) {
        const ushort *data = reinterpret_cast<const ushort*>(unicode());
        const int size = length();
        return (cs == Qt::CaseSensitive
                ? data[size - 1] == ch
                : foldCase(data[size - 1]) == foldCase(ch.unicode()));
    } else {
        return false;
    }
}

/*!
    \since 4.8
    \overload endsWith()
    \sa QString::endsWith(), endsWith()
*/
bool QStringRef::endsWith(QLatin1String str, Qt::CaseSensitivity cs) const
{
    return qt_ends_with(isNull() ? 0 : unicode(), size(), str, cs);
}

/*!
    \since 4.8
    \overload endsWith()
    \sa QString::endsWith(), endsWith()
*/
bool QStringRef::endsWith(const QStringRef &str, Qt::CaseSensitivity cs) const
{
    return qt_ends_with(isNull() ? 0 : unicode(), size(),
                        str.isNull() ? 0 : str.unicode(), str.size(), cs);
}


/*! \fn bool QStringRef::contains(const QString &str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const

    \since 4.8
    Returns \c true if this string reference contains an occurrence of
    the string \a str; otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
*/

/*! \fn bool QStringRef::contains(QChar ch, Qt::CaseSensitivity cs = Qt::CaseSensitive) const

    \overload contains()
    \since 4.8

    Returns \c true if this string contains an occurrence of the
    character \a ch; otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

*/

/*! \fn bool QStringRef::contains(const QStringRef &str, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
    \overload contains()
    \since 4.8

    Returns \c true if this string reference contains an occurrence of
    the string reference \a str; otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
*/

/*! \fn bool QStringRef::contains(QLatin1String str, Qt::CaseSensitivity cs) const
    \since 4,8
    \overload contains()

    Returns \c true if this string reference contains an occurrence of
    the string \a str; otherwise returns \c false.

    If \a cs is Qt::CaseSensitive (default), the search is
    case sensitive; otherwise the search is case insensitive.

    \sa indexOf(), count()
*/

static inline int qt_last_index_of(const QChar *haystack, int haystackLen, QChar needle,
                                   int from, Qt::CaseSensitivity cs)
{
    ushort c = needle.unicode();
    if (from < 0)
        from += haystackLen;
    if (uint(from) >= uint(haystackLen))
        return -1;
    if (from >= 0) {
        const ushort *b = reinterpret_cast<const ushort*>(haystack);
        const ushort *n = b + from;
        if (cs == Qt::CaseSensitive) {
            for (; n >= b; --n)
                if (*n == c)
                    return n - b;
        } else {
            c = foldCase(c);
            for (; n >= b; --n)
                if (foldCase(*n) == c)
                    return n - b;
        }
    }
    return -1;


}

static inline int qt_string_count(const QChar *haystack, int haystackLen,
                                  const QChar *needle, int needleLen,
                                  Qt::CaseSensitivity cs)
{
    int num = 0;
    int i = -1;
    if (haystackLen > 500 && needleLen > 5) {
        QStringMatcher matcher(needle, needleLen, cs);
        while ((i = matcher.indexIn(haystack, haystackLen, i + 1)) != -1)
            ++num;
    } else {
        while ((i = qFindString(haystack, haystackLen, i + 1, needle, needleLen, cs)) != -1)
            ++num;
    }
    return num;
}

static inline int qt_string_count(const QChar *unicode, int size, QChar ch,
                                  Qt::CaseSensitivity cs)
{
    ushort c = ch.unicode();
    int num = 0;
    const ushort *b = reinterpret_cast<const ushort*>(unicode);
    const ushort *i = b + size;
    if (cs == Qt::CaseSensitive) {
        while (i != b)
            if (*--i == c)
                ++num;
    } else {
        c = foldCase(c);
        while (i != b)
            if (foldCase(*(--i)) == c)
                ++num;
    }
    return num;
}

static inline int qt_find_latin1_string(const QChar *haystack, int size,
                                        QLatin1String needle,
                                        int from, Qt::CaseSensitivity cs)
{
    if (size < needle.size())
        return -1;

    const char *latin1 = needle.latin1();
    int len = needle.size();
    QVarLengthArray<ushort> s(len);
    qt_from_latin1(s.data(), latin1, len);

    return qFindString(haystack, size, from,
                       reinterpret_cast<const QChar*>(s.constData()), len, cs);
}

static inline bool qt_starts_with(const QChar *haystack, int haystackLen,
                                  const QChar *needle, int needleLen, Qt::CaseSensitivity cs)
{
    if (!haystack)
        return !needle;
    if (haystackLen == 0)
        return needleLen == 0;
    if (needleLen > haystackLen)
        return false;

    const ushort *h = reinterpret_cast<const ushort*>(haystack);
    const ushort *n = reinterpret_cast<const ushort*>(needle);

    if (cs == Qt::CaseSensitive) {
        return qMemEquals(h, n, needleLen);
    } else {
        uint last = 0;
        uint olast = 0;
        for (int i = 0; i < needleLen; ++i)
            if (foldCase(h[i], last) != foldCase(n[i], olast))
                return false;
    }
    return true;
}

static inline bool qt_starts_with(const QChar *haystack, int haystackLen,
                                  QLatin1String needle, Qt::CaseSensitivity cs)
{
    if (!haystack)
        return !needle.latin1();
    if (haystackLen == 0)
        return !needle.latin1() || *needle.latin1() == 0;
    const int slen = needle.size();
    if (slen > haystackLen)
        return false;
    const ushort *data = reinterpret_cast<const ushort*>(haystack);
    const uchar *latin = reinterpret_cast<const uchar*>(needle.latin1());
    if (cs == Qt::CaseSensitive) {
        return ucstrncmp(haystack, latin, slen) == 0;
    } else {
        for (int i = 0; i < slen; ++i)
            if (foldCase(data[i]) != foldCase((ushort)latin[i]))
                return false;
    }
    return true;
}

static inline bool qt_ends_with(const QChar *haystack, int haystackLen,
                                const QChar *needle, int needleLen, Qt::CaseSensitivity cs)
{
    if (!haystack)
        return !needle;
    if (haystackLen == 0)
        return needleLen == 0;
    const int pos = haystackLen - needleLen;
    if (pos < 0)
        return false;

    const ushort *h = reinterpret_cast<const ushort*>(haystack);
    const ushort *n = reinterpret_cast<const ushort*>(needle);

    if (cs == Qt::CaseSensitive) {
        return qMemEquals(h + pos, n, needleLen);
    } else {
        uint last = 0;
        uint olast = 0;
        for (int i = 0; i < needleLen; i++)
            if (foldCase(h[pos+i], last) != foldCase(n[i], olast))
                return false;
    }
    return true;
}


static inline bool qt_ends_with(const QChar *haystack, int haystackLen,
                                QLatin1String needle, Qt::CaseSensitivity cs)
{
    if (!haystack)
        return !needle.latin1();
    if (haystackLen == 0)
        return !needle.latin1() || *needle.latin1() == 0;
    const int slen = needle.size();
    int pos = haystackLen - slen;
    if (pos < 0)
        return false;
    const uchar *latin = reinterpret_cast<const uchar*>(needle.latin1());
    const ushort *data = reinterpret_cast<const ushort*>(haystack);
    if (cs == Qt::CaseSensitive) {
        return ucstrncmp(haystack + pos, latin, slen) == 0;
    } else {
        for (int i = 0; i < slen; i++)
            if (foldCase(data[pos+i]) != foldCase((ushort)latin[i]))
                return false;
    }
    return true;
}

/*!
    \since 4.8

    Returns a Latin-1 representation of the string as a QByteArray.

    The returned byte array is undefined if the string contains non-Latin1
    characters. Those characters may be suppressed or replaced with a
    question mark.

    \sa toUtf8(), toLocal8Bit(), QTextCodec
*/
QByteArray QStringRef::toLatin1() const
{
    return QString::toLatin1_helper(unicode(), length());
}

/*!
    \fn QByteArray QStringRef::toAscii() const
    \since 4.8
    \deprecated

    Returns an 8-bit representation of the string as a QByteArray.

    This function does the same as toLatin1().

    Note that, despite the name, this function does not necessarily return an US-ASCII
    (ANSI X3.4-1986) string and its result may not be US-ASCII compatible.

    \sa toLatin1(), toUtf8(), toLocal8Bit(), QTextCodec
*/

/*!
    \since 4.8

    Returns the local 8-bit representation of the string as a
    QByteArray. The returned byte array is undefined if the string
    contains characters not supported by the local 8-bit encoding.

    QTextCodec::codecForLocale() is used to perform the conversion from
    Unicode. If the locale encoding could not be determined, this function
    does the same as toLatin1().

    If this string contains any characters that cannot be encoded in the
    locale, the returned byte array is undefined. Those characters may be
    suppressed or replaced by another.

    \sa toLatin1(), toUtf8(), QTextCodec
*/
QByteArray QStringRef::toLocal8Bit() const
{
#ifndef QT_NO_TEXTCODEC
    QTextCodec *localeCodec = QTextCodec::codecForLocale();
    if (localeCodec)
        return localeCodec->fromUnicode(unicode(), length());
#endif // QT_NO_TEXTCODEC
    return toLatin1();
}

/*!
    \since 4.8

    Returns a UTF-8 representation of the string as a QByteArray.

    UTF-8 is a Unicode codec and can represent all characters in a Unicode
    string like QString.

    \sa toLatin1(), toLocal8Bit(), QTextCodec
*/
QByteArray QStringRef::toUtf8() const
{
    if (isNull())
        return QByteArray();

    return QUtf8::convertFromUnicode(constData(), length());
}

/*!
    \since 4.8

    Returns a UCS-4/UTF-32 representation of the string as a QVector<uint>.

    UCS-4 is a Unicode codec and therefore it is lossless. All characters from
    this string will be encoded in UCS-4. Any invalid sequence of code units in
    this string is replaced by the Unicode's replacement character
    (QChar::ReplacementCharacter, which corresponds to \c{U+FFFD}).

    The returned vector is not NUL terminated.

    \sa toUtf8(), toLatin1(), toLocal8Bit(), QTextCodec
*/
QVector<uint> QStringRef::toUcs4() const
{
    QVector<uint> v(length());
    uint *a = v.data();
    int len = QString::toUcs4_helper(reinterpret_cast<const ushort *>(unicode()), length(), a);
    v.resize(len);
    return v;
}

/*!
    Returns a string that has whitespace removed from the start and
    the end.

    Whitespace means any character for which QChar::isSpace() returns
    \c true. This includes the ASCII characters '\\t', '\\n', '\\v',
    '\\f', '\\r', and ' '.

    Unlike QString::simplified(), trimmed() leaves internal whitespace alone.

    \since 5.1

    \sa QString::trimmed()
*/
QStringRef QStringRef::trimmed() const
{
    const QChar *begin = cbegin();
    const QChar *end = cend();
    QStringAlgorithms<const QStringRef>::trimmed_helper_positions(begin, end);
    if (begin == cbegin() && end == cend())
        return *this;
    if (begin == end)
        return QStringRef();
    int position = m_position + (begin - cbegin());
    return QStringRef(m_string, position, end - begin);
}

/*!
    Returns the string converted to a \c{long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toLongLong()

    \sa QString::toLongLong()

    \since 5.1
*/

qint64 QStringRef::toLongLong(bool *ok, int base) const
{
    return QString::toIntegral_helper<qint64>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned long long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toULongLong()

    \sa QString::toULongLong()

    \since 5.1
*/

quint64 QStringRef::toULongLong(bool *ok, int base) const
{
    return QString::toIntegral_helper<quint64>(constData(), size(), ok, base);
}

/*!
    \fn long QStringRef::toLong(bool *ok, int base) const

    Returns the string converted to a \c long using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toLong()

    \sa QString::toLong()

    \since 5.1
*/

long QStringRef::toLong(bool *ok, int base) const
{
    return QString::toIntegral_helper<long>(constData(), size(), ok, base);
}

/*!
    \fn ulong QStringRef::toULong(bool *ok, int base) const

    Returns the string converted to an \c{unsigned long} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toULongLong()

    \sa QString::toULong()

    \since 5.1
*/

ulong QStringRef::toULong(bool *ok, int base) const
{
    return QString::toIntegral_helper<ulong>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to an \c int using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toInt()

    \sa QString::toInt()

    \since 5.1
*/

int QStringRef::toInt(bool *ok, int base) const
{
    return QString::toIntegral_helper<int>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned int} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toUInt()

    \sa QString::toUInt()

    \since 5.1
*/

uint QStringRef::toUInt(bool *ok, int base) const
{
    return QString::toIntegral_helper<uint>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to a \c short using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toShort()

    \sa QString::toShort()

    \since 5.1
*/

short QStringRef::toShort(bool *ok, int base) const
{
    return QString::toIntegral_helper<short>(constData(), size(), ok, base);
}

/*!
    Returns the string converted to an \c{unsigned short} using base \a
    base, which is 10 by default and must be between 2 and 36, or 0.
    Returns 0 if the conversion fails.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true.

    If \a base is 0, the C language convention is used: If the string
    begins with "0x", base 16 is used; if the string begins with "0",
    base 8 is used; otherwise, base 10 is used.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toUShort()

    \sa QString::toUShort()

    \since 5.1
*/

ushort QStringRef::toUShort(bool *ok, int base) const
{
    return QString::toIntegral_helper<ushort>(constData(), size(), ok, base);
}


/*!
    Returns the string converted to a \c double value.

    Returns 0.0 if the conversion fails.

    If a conversion error occurs, \c{*}\a{ok} is set to \c false;
    otherwise \c{*}\a{ok} is set to \c true.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toDouble()

    For historic reasons, this function does not handle
    thousands group separators. If you need to convert such numbers,
    use QLocale::toDouble().

    \sa QString::toDouble()

    \since 5.1
*/

double QStringRef::toDouble(bool *ok) const
{
    return QLocaleData::c()->stringToDouble(constData(), size(), ok, QLocaleData::FailOnGroupSeparators);
}

/*!
    Returns the string converted to a \c float value.

    If a conversion error occurs, *\a{ok} is set to \c false; otherwise
    *\a{ok} is set to \c true. Returns 0.0 if the conversion fails.

    The string conversion will always happen in the 'C' locale. For locale
    dependent conversion use QLocale::toFloat()

    \sa QString::toFloat()

    \since 5.1
*/

float QStringRef::toFloat(bool *ok) const
{
    return QLocaleData::convertDoubleToFloat(toDouble(ok), ok);
}

/*!
    \obsolete
    \fn QString Qt::escape(const QString &plain)

    Use QString::toHtmlEscaped() instead.
*/

/*!
    \since 5.0

    Converts a plain text string to an HTML string with
    HTML metacharacters \c{<}, \c{>}, \c{&}, and \c{"} replaced by HTML
    entities.

    Example:

    \snippet code/src_corelib_tools_qstring.cpp 7
*/
QString QString::toHtmlEscaped() const
{
    QString rich;
    const int len = length();
    rich.reserve(int(len * 1.1));
    for (int i = 0; i < len; ++i) {
        if (at(i) == QLatin1Char('<'))
            rich += QLatin1String("&lt;");
        else if (at(i) == QLatin1Char('>'))
            rich += QLatin1String("&gt;");
        else if (at(i) == QLatin1Char('&'))
            rich += QLatin1String("&amp;");
        else if (at(i) == QLatin1Char('"'))
            rich += QLatin1String("&quot;");
        else
            rich += at(i);
    }
    rich.squeeze();
    return rich;
}

/*!
  \macro QStringLiteral(str)
  \relates QString

  The macro generates the data for a QString out of \a str at compile time if the compiler supports it.
  Creating a QString from it is free in this case, and the generated string data is stored in
  the read-only segment of the compiled object file.

  For compilers not supporting the creation of compile time strings, QStringLiteral will fall back to
  QString::fromUtf8().

  If you have code looking like:
  \code
  if (node.hasAttribute("http-contents-length")) //...
  \endcode
  One temporary QString will be created to be passed as the hasAttribute function parameter.
  This can be quite expensive, as it involves a memory allocation and the copy and the conversion
  of the data into QString's internal encoding.

  This can be avoided by doing
  \code
  if (node.hasAttribute(QStringLiteral("http-contents-length"))) //...
  \endcode
  Then the QString's internal data will be generated at compile time and no conversion or allocation
  will occur at runtime

  Using QStringLiteral instead of a double quoted ascii literal can significantly speed up creation
  of QString's from data known at compile time.

  If the compiler is C++11 enabled the string \a str can actually contain unicode data.

  \note There are still a few cases in which QLatin1String is more efficient than QStringLiteral:
  If it is passed to a function that has an overload that takes the QLatin1String directly, without
  conversion to QString. For instance, this is the case of QString::operator==
  \code
  if (attribute.name() == QLatin1String("http-contents-length")) //...
  \endcode

  \note There are some restrictions when using the MSVC 2010 or 2012 compilers. The example snippets
  provided here fail to compile with them.
  \list
  \li Concatenated string literals cannot be used with QStringLiteral.
  \code
  QString s = QStringLiteral("a" "b");
  \endcode
  \li QStringLiteral cannot be used to initialize lists or arrays of QString.
  \code
  QString a[] = { QStringLiteral("a"), QStringLiteral("b") };
  \endcode
  \endlist
*/

/*!
    \internal
 */
void QAbstractConcatenable::appendLatin1To(const char *a, int len, QChar *out)
{
    qt_from_latin1(reinterpret_cast<ushort *>(out), a, uint(len));
}

QT_END_NAMESPACE
