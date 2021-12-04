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

#define	MAXVAR 4

int main()
{
	XORMAP_HEAD_t eq[2 * MAXVAR];
	XORMAP_HEAD_t head;
	XORMAP var[MAXVAR];
	size_t zc = 0;
	size_t zr = 0;
	bool any;

	for (size_t x = 0; x != MAXVAR; x++)
		var[x] = XORMAP(x, false);

	for (size_t x = 0; x != 2 * MAXVAR; x++)
		TAILQ_INIT(&eq[x]);

	TAILQ_INIT(&head);

#if 1
	/* build a squarer */
	for (size_t x = 0; x != MAXVAR; x++) {
		for (size_t y = 0; y != MAXVAR; y++) {
			(new XORMAP(var[x] & var[y]))->insert_tail(&eq[x + y]);
		}
	}
#else
	/* build multiply by three */
	for (size_t x = 0; x != MAXVAR; x++) {
		(new XORMAP(var[x]))->insert_tail(&eq[x]);
		if (x != 0)
			(new XORMAP(var[x - 1]))->insert_tail(&eq[x]);
	}
	(new XORMAP(var[MAXVAR - 1]))->insert_tail(&eq[MAXVAR]);
#endif

	/* a + b = 0 */
	for (size_t x = 0; x != 2 * MAXVAR; x++)
		(new XORMAP(x + MAXVAR, false))->insert_tail(&eq[x]);

	/* build the logic expression for the adder */
repeat:
	any = false;
	for (size_t x = 0; x != 2 * MAXVAR; x++) {
		XORMAP *xn;
		XORMAP *xm;

		for (XORMAP *xa = TAILQ_FIRST(&eq[x]); xa; xa = xn) {
			xn = xa->next();

			for (XORMAP *xb = xa->next(); xb; xb = xm) {
				xm = xb->next();

				any = true;

				XORMAP *xc = new XORMAP(*xa & *xb);
				if (xc->defactor().isZero()) {
					printf("ZERO CARRY\n");
					zc++;
					delete xc;
				} else {
					if (x + 1 >= 2 * MAXVAR)
						delete xc;
					else
						xc->insert_tail(&eq[x + 1]).defactor();
				}
				*xa ^= *xb;
				delete xb->remove(&eq[x]);
				if (xb == xn)
					xn = xm;
				if (xa->isZero()) {
					printf("ZERO REMAINDER\n");
					zr++;
					delete xa->remove(&eq[x]);
					goto next;
				}
			}
		}
	next:;
	}

	if (any)
		goto repeat;

#if 0
	for (size_t x = 0; x != 2 * MAXVAR; x++) {
		XORMAP *xa = TAILQ_FIRST(&eq[x]);
		if (xa == 0)
			(new XORMAP(x + MAXVAR, false))->insert_tail(&eq[x]);
		else
			*xa ^= XORMAP(x + MAXVAR, false);
	}
#endif

	printf("ZERO CARRY: %zd\n", zc);
	printf("ZERO REMAINDER: %zd\n", zr);

	/* set all equations equal to zero */
	for (size_t x = 0; x != 2 * MAXVAR; x++)
		TAILQ_CONCAT(&head, &eq[x], entry);

	hpsat_find_all_ored(&head);
	hpsat_sort_or(&head);

	while (hpsat_simplify_insert(&head)) {
		printf("LOOP\n");

		for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
			xa->print(); printf(" || \n");
		}
	}

	/* print all solutions */
	printf("HEAD = \n");

	uint8_t sol[MAXVAR + 2 * MAXVAR] = {};

	while (1) {
		size_t a, b;

		for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
			if (xa->expand_all(sol))
				goto not_found;
		}
		a = b = 0;
		for (uint8_t x = 0; x != MAXVAR; x++)
			a += sol[x] << x;
		for (uint8_t x = 0; x != 2 * MAXVAR; x++)
			b += sol[MAXVAR + x] << x;
		printf("SOL %zd %zd\n", a, b);
not_found:;
		for (uint8_t x = 0; x != (MAXVAR + 2 * MAXVAR); x++) {
			sol[x] ^= 1;
			if (sol[x])
				break;
			if (x == (MAXVAR + 2 * MAXVAR - 1))
				goto done;
		}
	}
done:;

	/* print final equation */
	for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
		xa->print(); printf(" || \n");
	}

	hpsat_free(&head);

	return (0);
}
