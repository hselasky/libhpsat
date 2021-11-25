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

#define	LMAX 8
#define	MAX (1 << LMAX)

static bool
function(uint32_t x, uint8_t bit)
{
#if 0
	x = (x & 15) * (x >> 4);
	x ^= 3 * 5;
	return ((x >> bit) & 1);
#else
	return (x % 3) & 1;
	return ((x * x) & MAX) ? 1 : 0;
#endif
}

int main()
{
	BITMAP_HEAD_t head;

	TAILQ_INIT(&head);

	hpsat_var_t nvar = LMAX;

	for (uint8_t bit = 0; bit != 2 * LMAX; bit++) {
		BITMAP *pm = new BITMAP();

		pm->addVarSequence(0, LMAX);

		for (uint32_t x = 0; x != MAX; x++) {
			if (function(x, bit))
				pm->set(x);
			else
				pm->clear(x);
		}

		BITMAP_HEAD_t hh;

		TAILQ_INIT(&hh);

		pm->sort().insert_tail(&hh);

		hpsat_demux(&hh, &nvar);

		TAILQ_CONCAT(&head, &hh, entry);
	}

	do {
		hpsat_simplify_or(&head);
	} while (hpsat_squash_or(&head));

	size_t num = 0;
	for (BITMAP *pn = TAILQ_FIRST(&head); pn; pn = pn->next()) {
		pn->print(); printf(" # EQ %zd\n", num++);
	}

	uint8_t solution[nvar];
	memset(solution, 0, sizeof(solution));

	XORMAP_HEAD_t ahead;
	TAILQ_INIT(&ahead);

	hpsat_bitmap_to_xormap(&head, &ahead);

	XORMAP_HEAD_t shead;
	TAILQ_INIT(&shead);

	hpsat_solve(&ahead, &shead, nvar);

	if (hpsat_solve_first(&shead, solution))
		printf("SOLVED\n");

	uint32_t value = 0;
	for (uint32_t x = 0; x != LMAX; x++)
		value |= (uint32_t)solution[x] << x;

	printf("VALUE = %d\n", value);

	hpsat_free_bitmaps(&head);
	hpsat_free_xormaps(&ahead);
	hpsat_free_xormaps(&shead);
	return (0);
}
