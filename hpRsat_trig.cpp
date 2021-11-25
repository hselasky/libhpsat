/*-
 * Copyright (c) 2021-2022 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hpRsat.h"

/*
 * The following functions generates the full sinus and cosinus
 * waveform from [0 to 360> degrees, using the full 32-bit integer
 * range.
 */
ADD
hprsat_sin_32(uint32_t angle, uint32_t mask, hprsat_var_t var)
{
	return (hprsat_cos_32(angle + (3 << 30), mask, var));
}

ADD
hprsat_cos_32(uint32_t angle, uint32_t mask, hprsat_var_t var)
{
	uint32_t x = angle;
	uint8_t num;

	if (mask == -1U) {
		/* Handle special cases, if any */
		switch (x) {
		case 0xFFFFFFFFU:
		case 0x00000000U:
			return (ADD(1.0));
		case 0x3FFFFFFFU:
		case 0x40000000U:
		case 0xBFFFFFFFU:
		case 0xC0000000U:
			return (ADD(0.0));
		case 0x7FFFFFFFU:
		case 0x80000000U:
			return (ADD(-1.0));
		}
	}

	ADD retval;

	/*
	 * Apply so-called "Gray-coding":
	 * See also: https://en.wikipedia.org/wiki/Gray_code
	 */
	for (uint32_t m = 1U << 31; m != 1; m /= 2) {
		if (x & m)
			x ^= (m - 1);
	}

	/* Find first set bit */
	for (num = 0; num != 30; num++) {
		if (x & (1U << num)) {
			num++;
			break;
		}
	}

	/* Compute the rest of the square-root series */
	for (; num != 30; num++) {
		if (mask & (1U << num)) {
			if (x & (1U << num))
				retval = ADD(0.5) - retval * ADD(0.5);
			else
				retval = ADD(0.5) + retval * ADD(0.5);
		} else {
			retval = ADD(0.5) + retval * (ADD(0.5) - ADD(1.0, var++));
		}
		retval.doSqrt();
	}

	/* Check if halfway */
	if (mask & (1ULL << 30)) {
		if (x & (1ULL << 30))
			retval.negate();
	} else {
		retval *= (ADD(1.0) - ADD(2.0, var++));
	}
	return (retval);
}

CADD
hprsat_cosinus_32(uint32_t angle, uint32_t mask, hprsat_var_t var)
{
	CADD temp;

	temp.x = hprsat_cos_32(angle, mask, var);
	temp.y = hprsat_sin_32(angle, mask, var);

	return (temp);
}
