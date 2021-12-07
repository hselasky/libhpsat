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
#define	TYPE 1

static void
flip(hpsat_var_t &v)
{
#if TYPE != 0
	if (v < MAXVAR)
		v += MAXVAR;
	else if (v < 2 * MAXVAR)
		v -= MAXVAR;
#endif
}

static void
symmetry(XORMAP *xa, XORMAP *xd)
{
	xa->sort();
	for (ANDMAP *pa = xa->first(); pa; pa = pa->next()) {
		if (pa->isZero()) {
			continue;
		} else {
			ANDMAP *pb = new ANDMAP(true);

			for (BITMAP *ba = pa->first(); ba; ba = ba->next()) {
				assert(ba->isXorConst());
				BITMAP *bb = new BITMAP(ba->isInverted());

				for (size_t x = 0; x != ba->nvar; x++) {
					hpsat_var_t v = ba->pvar[x];
					flip(v);
					*bb ^= BITMAP(v, false);
				}
				bb->sort().insert_tail(&pb->head);
			}

			pb->insert_tail(&xd->head);
		}
	}
	xd->sort();
}

static size_t
category(XORMAP *xa)
{
	size_t retval = 0;

	for (ANDMAP *pa = xa->first(); pa; pa = pa->next()) {
		size_t sz = pa->count();
		if (sz > retval)
			retval = sz;
	}

	XORMAP xd(false);
	symmetry(xa, &xd);
	if (xa->compare(xd) != 0)
		retval += MAXVAR * 4;
	return (retval);
}

int main()
{
	XORMAP_HEAD_t eq[2 * MAXVAR];
	XORMAP_HEAD_t head;
	XORMAP_HEAD_t last[2 * 4 * MAXVAR];
	XORMAP var[2 * MAXVAR];

	for (size_t x = 0; x != 2 * MAXVAR; x++)
		var[x] = XORMAP(x, false);

	for (size_t x = 0; x != 2 * MAXVAR; x++)
		TAILQ_INIT(&eq[x]);

	for (size_t x = 0; x != 2 * 4 * MAXVAR; x++)
		TAILQ_INIT(&last[x]);

	TAILQ_INIT(&head);

	switch (TYPE) {
	case 0:
		printf("c Building squarer\n");
		for (size_t x = 0; x != MAXVAR; x++) {
			for (size_t y = 0; y != MAXVAR; y++) {
				(new XORMAP(var[x] & var[y]))->insert_tail(&eq[x + y]);
			}
		}
		break;
	case 1:
		printf("c Building multiplier\n");
		for (size_t x = 0; x != MAXVAR; x++) {
			for (size_t y = 0; y != MAXVAR; y++) {
				(new XORMAP(var[x] & var[y + MAXVAR]))->insert_tail(&eq[x + y]);
			}
		}
		break;
	case 2:
		printf("c Building adder\n");
		for (size_t x = 0; x != MAXVAR; x++) {
			(new XORMAP(var[x]))->insert_tail(&eq[x]);
			(new XORMAP(var[x + MAXVAR]))->insert_tail(&eq[x]);
		}
		break;
	default:
		break;
	}

	printf("c Building adder logic\n");
	hpsat_build_multi_adder_logic(eq, 2 * MAXVAR);

	printf("c Inserting solution\n");
	for (size_t x = 0; x != 2 * MAXVAR; x++) {
		XORMAP *xa = TAILQ_FIRST(&eq[x]);
		if (xa == 0)
			(new XORMAP(2 * MAXVAR + x, false))->insert_tail(&eq[x]);
		else
			(new ANDMAP(2 * MAXVAR + x, false))->insert_tail(&xa->head);

		TAILQ_CONCAT(&head, &eq[x], entry);
	}
	hpsat_sort_or(&head);
	hpsat_find_all_ored(&head);

	printf("START\n");
	for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
		xa->print(); printf(" || \n");
	}

	while (hpsat_simplify_insert(&head)) {
		printf("LOOP\n");
		hpsat_sort_or(&head);

		for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
			xa->print(); printf(" || \n");
		}
	}

	for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
		XORMAP *xn = new XORMAP();
		symmetry(xa, xn);
		if (*xa != *xn)
			xn->insert_head(&head);
		else
			delete xn;
	}

	while (hpsat_simplify_insert(&head)) {
		printf("LOOP_2\n");
		hpsat_sort_or(&head);

		for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
			xa->print(); printf(" || \n");
		}
	}

#if 1
	for (XORMAP *xa = TAILQ_FIRST(&head), *xn; xa; xa = xn) {
		xn = xa->next();
		xa->remove(&head)->insert_tail(&last[category(xa)]);
	}

	for (size_t x = 0; x != 2 * 4 * MAXVAR; x++)
		TAILQ_CONCAT(&head, &last[x], entry);

	hpsat_simplify_redundant(&head);
#endif

	/* print all solutions */
	printf("HEAD = \n");

	uint8_t sol[4 * MAXVAR] = {};

	while (1) {
		size_t a, b, c, d;

		for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
			if (xa->expand_all(sol))
				goto not_found;
		}
		a = b = c = d = 0;
		for (uint8_t x = 0; x != 2 * MAXVAR; x++)
			a += sol[x] << x;
		for (uint8_t x = 0; x != MAXVAR; x++)
			c += sol[x] << x;
		for (uint8_t x = 0; x != MAXVAR; x++)
			d += sol[x + MAXVAR] << x;
		for (uint8_t x = 0; x != 2 * MAXVAR; x++)
			b += sol[2 * MAXVAR + x] << x;

		if (c * d != b)
			printf("BUG ");

		printf("SOL %zd (%zd,%zd) %zd REF=%zd,%zd,%zd\n", a, c, d, b, a * a, c * d, c + d);
not_found:;
		for (uint8_t x = 0; x != (4 * MAXVAR); x++) {
			sol[x] ^= 1;
			if (sol[x])
				break;
			if (x == (4 * MAXVAR - 1))
				goto done;
		}
	}
done:;
	size_t cc = 0;

	/* print final equation */
	for (XORMAP *xa = TAILQ_FIRST(&head); xa; xa = xa->next()) {
		xa->print(); printf(" || CAT(%zd)\n", category(xa));
		cc++;
	}
	printf("NUM=%zd\n", cc);

	hpsat_free(&head);

	return (0);
}
