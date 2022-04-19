 /*-
 * Copyright (c) 2022 Hans Petter Selasky. All rights reserved.
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

hprsat_val_t hprsat_global_mod;
hprsat_val_t *hprsat_global_sqrt;

void
hprsat_set_global_modulus(hprsat_val_t mod)
{
	hprsat_global_mod = mod;

	delete [] hprsat_global_sqrt;
	hprsat_global_sqrt = new hprsat_val_t [mod];

	memset(hprsat_global_sqrt, 0, sizeof(hprsat_global_sqrt[0]) * mod);

	for (hprsat_val_t x = 0; x != (mod / 2); x++)
		hprsat_global_sqrt[(x * x) % mod] = x;
}

void
hprsat_do_inverse(hprsat_val_t val, hprsat_val_t mod, hprsat_val_t &r)
{
	hprsat_val_t exp = mod - 2;

	r = 1;

	while (exp != 0) {
		if (exp & 1) {
			r *= val;
			r %= mod;
		}
		exp /= 2;
		val *= val;
		val %= mod;
	}
}
