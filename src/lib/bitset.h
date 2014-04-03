//File: bitset.h
//Date: Thu Mar 27 20:59:36 2014 +0800
//Author: Yuxin Wu <ppwwyyxxc@gmail.com>

#pragma once

#include <emmintrin.h>
#include <cstdint>
#include <string.h>
#include "debugutils.h"

extern __m128i lut[] __attribute__((aligned(16)));
extern uint8_t POPCOUNT_4bit[16] __attribute__((aligned(16)));

inline int get_len_from_bit(int nbit) {
	int l = ((nbit - 1) >> 7) + 1;
	if (l % 4 == 0) return l;
	return ((l >> 2) + 1) << 2;
}


inline uint32_t ssse3_popcount3(uint8_t* buffer, int chunks16) {
	static char MASK_4bit[16] = {0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf, 0xf};

	uint32_t result;

	__asm__ volatile ("movdqu (%%rax), %%xmm7" : : "a" (POPCOUNT_4bit));
	__asm__ volatile ("movdqu (%%rax), %%xmm6" : : "a" (MASK_4bit));
	__asm__ volatile ("pxor    %%xmm5, %%xmm5" : : ); // xmm5 -- global accumulator

	result = 0;

	int k, n, i;

	i = 0;
	while (chunks16 > 0) {
		// max(POPCOUNT_8bit) = 8, thus byte-wise addition could be done
		// for floor(255/8) = 31 iterations
#define MAX (7*4)
		if (chunks16 > MAX) {
			k = MAX;
			chunks16 -= MAX;
		} else {
			k = chunks16;
			chunks16 = 0;
		}
#undef MAX
		__asm__ volatile ("pxor %xmm4, %xmm4"); // xmm4 -- local accumulator
		for (n=0; n < k; n+=4) {
#define body(index) \
			__asm__ volatile( \
					"movdqa	  (%%rax), %%xmm0	\n" \
					"movdqa    %%xmm0, %%xmm1	\n" \
					"psrlw         $4, %%xmm1	\n" \
					"pand      %%xmm6, %%xmm0	\n" \
					"pand      %%xmm6, %%xmm1	\n" \
					"movdqa    %%xmm7, %%xmm2	\n" \
					"movdqa    %%xmm7, %%xmm3	\n" \
					"pshufb    %%xmm0, %%xmm2	\n" \
					"pshufb    %%xmm1, %%xmm3	\n" \
					"paddb     %%xmm2, %%xmm4	\n" \
					"paddb     %%xmm3, %%xmm4	\n" \
					: : "a" (&buffer[index]));

			body(i);
			body(i + 1*16);
			body(i + 2*16);
			body(i + 3*16);
#undef body
			i += 4*16;
		}

		// update global accumulator (two 32-bits counters)
		__asm__ volatile (
				"pxor	%xmm0, %xmm0		\n"
				"psadbw	%xmm0, %xmm4		\n"
				"paddd	%xmm4, %xmm5		\n"
				);
	}

	// finally add together 32-bits counters stored in global accumulator
	__asm__ volatile (
			"movhlps   %%xmm5, %%xmm0	\n"
			"paddd     %%xmm5, %%xmm0	\n"
			"movd      %%xmm0, %%rax	\n"
			: "=a" (result)
			);
	return result;
}


inline void sse2_sub_arr(__m128i*  dst,
		const __m128i*  src,
		const __m128i*  src_end) {
	__m128i xmm1, xmm2;
	do {
		_mm_prefetch((const char*)(src)+512,  _MM_HINT_NTA);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_andnot_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_andnot_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_andnot_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_andnot_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);

	} while (src < src_end);
}

inline void sse2_or_arr(__m128i* dst,
		const __m128i* src,
		const __m128i* src_end) {
	__m128i xmm1, xmm2;
	do {
		_mm_prefetch((const char*)(src)+512,  _MM_HINT_NTA);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_or_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_or_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_or_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);

		xmm1 = _mm_load_si128(src++);
		xmm2 = _mm_load_si128(dst);
		xmm1 = _mm_or_si128(xmm1, xmm2);
		_mm_store_si128(dst++, xmm1);
	} while (src < src_end);
}

class Bitset {
	public:
		__m128i* data;

		Bitset(int len) { data = (__m128i*)calloc(len, sizeof(__m128i)); }

		~Bitset() { free(data); }

		inline void set(int k) {
			int idx = k >> 7;
			int pos = k % 128;
			data[idx] = _mm_or_si128(data[idx],	_mm_load_si128(lut + pos));
		}

		inline void and_not_arr(const Bitset& r, int len) {
			sse2_sub_arr(data, r.data, r.data + len);
		}

		inline void or_arr(const Bitset& r, int len) {
			sse2_or_arr(data, r.data, r.data + len);
		}

		inline int count(int len) {
			return ssse3_popcount3((uint8_t*)data, len);
		}

		inline void reset(int len) {
			memset(data, 0, len * sizeof(__m128i));
		}
};