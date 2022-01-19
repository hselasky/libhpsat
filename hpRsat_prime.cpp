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

class hprsat_prime;
typedef TAILQ_CLASS_HEAD(, hprsat_prime) hprsat_prime_head_t;
typedef TAILQ_CLASS_ENTRY(hprsat_prime) hprsat_prime_entry_t;

class hprsat_prime {
public:
	hprsat_prime_entry_t entry;
	hprsat_val_t mod;

	hprsat_prime *insert_tail(hprsat_prime_head_t *phead) {
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (this);
	};
	hprsat_prime *remove(hprsat_prime_head_t *phead) {
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	};
	hprsat_prime *next() {
		return (TAILQ_NEXT(this, entry));
	};
};

hprsat_val_t hprsat_global_modulus;
static hprsat_prime_head_t hprsat_prime_head = TAILQ_HEAD_INITIALIZER(hprsat_prime_head);

static void
hprsat_free(hprsat_prime_head_t *phead)
{
	for (hprsat_prime *pa; (pa = TAILQ_FIRST(phead)); )
		delete pa->remove(phead);
}

void
hprsat_set_global_modulus(ADD_HEAD_t *phead)
{
	hprsat_val_t min;
	hprsat_val_t max;
	hprsat_val_t range = 0;

	hprsat_free(&hprsat_prime_head);

	/* Compute the maximum range for each equation. */
	for (ADD *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		min = max = 0;

		for (MUL *ma = pa->first(); ma; ma = ma->next()) {
			if (ma->factor_lin < 0)
				min += ma->factor_lin;
			else
				max += ma->factor_lin;
		}

		max -= min;
		if (range < max)
			range = max;
	}

	hprsat_global_modulus = 2;

	/*
	 * Compute a prime number big-enough to hold the range:
	 */
	hprsat_val_t counter = 1;
top:
	counter++;

	if (hprsat_global_modulus < range) {
		for (hprsat_prime *pa = TAILQ_FIRST(&hprsat_prime_head); pa; pa = pa->next()) {
			if ((counter % pa->mod) == 0) {
				/* Test this non-prime number. */
				hprsat_val_t r;
				hprsat_val_t b = 2;
				hprsat_val_t e = counter * hprsat_global_modulus;
				hprsat_val_t m = e + 1;

				mpz_powm(r.get_mpz_t(), b.get_mpz_t(),
					 e.get_mpz_t(), m.get_mpz_t());

				/* Check if ring divides (P-1). */
				if (r == 1) {
					e /= 2;
					mpz_powm(r.get_mpz_t(), b.get_mpz_t(),
						 e.get_mpz_t(), m.get_mpz_t());

					/* Check if ring does not divide (P-1) / 2. */
					if (r != 1)
						hprsat_global_modulus *= counter;
				}

				/* Get next non-prime number. */
				goto top;
			}
		}
		/* Accumulate and skip all prime numbers. */
		(new hprsat_prime())->insert_tail(&hprsat_prime_head)->mod = counter;
		goto top;
	}

	/* Clean up all the temporary primes. */
	hprsat_free(&hprsat_prime_head);

	/* Get our prime. */
	hprsat_global_modulus++;
}

void
hprsat_do_global_modulus(hprsat_val_t &val)
{
	if (hprsat_global_modulus != 0) {
		val %= hprsat_global_modulus;
		if (val < 0)
			val += hprsat_global_modulus;
	}
}
