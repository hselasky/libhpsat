/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
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

#include "hp3sat.h"

#define	LMAX 18
#define	MAX (1 << LMAX)

static bool
function(uint32_t x, uint8_t bit)
{
	const uint32_t mask = (1U << (LMAX / 3)) - 1U;
	x = ((x & mask) * (x & mask)) ^ (x >> (LMAX / 3));
	return ((x >> bit) & 1);
}

int main()
{
	BITMAP_HEAD_t head;

	TAILQ_INIT(&head);

	hpsat_var_t nvar = LMAX;

	for (uint8_t bit = 0; bit != LMAX; bit++) {
		BITMAP *pm = new BITMAP();

		pm->addVarSequence(0, LMAX);

		for (uint32_t x = 0; x != MAX; x++) {
			if (function(x, bit))
				pm->set(x);
			else
				pm->clear(x);
		}

		pm->sort().insert_tail(&head);
	}

	size_t num = 0;
	for (BITMAP *pn = TAILQ_FIRST(&head); pn; pn = pn->next()) {
		pn->print(); printf(" # EQ %zd\n", num++);
	}

	hpsat_var_t ovar = nvar;

	nvar = (nvar + nvar * nvar) / 2;

	XORMAP_HEAD_t ahead;
	TAILQ_INIT(&ahead);
	hpsat_bitmap_to_xormap(&head, &ahead);
	hpsat_sort_or(&ahead);

	/* add helper variables */
	for (hpsat_var_t x = 0, z = 0; x != ovar; x++) {
		for (hpsat_var_t y = x + 1; y != ovar; y++, z++) {
			(new XORMAP((XORMAP(x, false) & XORMAP(y, false)) ^
			    XORMAP(ovar + z, false)))->insert_tail(&ahead);
		}
	}

	XORMAP_HEAD_t shead;
	TAILQ_INIT(&shead);

	hpsat_solve(&ahead, &shead, &nvar);
	hpsat_solve_strip(&ahead, &shead, LMAX, nvar);

	uint8_t solution[nvar];
	memset(solution, 0, sizeof(solution));

	if (hpsat_solve_first(&shead, solution))
		printf("SOLVED\n");

	uint32_t value = 0;
	for (uint32_t x = 0; x != LMAX; x++)
		value |= (uint32_t)solution[x] << x;

	printf("VALUE = %d\n", value);

	hpsat_free(&head);
	hpsat_free(&ahead);
	hpsat_free(&shead);
	return (0);
}
