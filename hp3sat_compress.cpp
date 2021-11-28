/*-
 * Copyright (c) 2021 Hans Petter Selasky. All rights reserved.
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

static void
hpsat_compress_insert(BITMAP_HEAD_t *pfrom, BITMAP_HEAD_t *pto, hpsat_var_t v)
{
	if (TAILQ_FIRST(pfrom) == 0) {
		(new BITMAP(v, false))->insert_tail(pto);
	} else {
		for (BITMAP *ba = TAILQ_FIRST(pfrom); ba; ba = ba->next())
			(new BITMAP(*ba))->insert_tail(pto);
	}
}

void
hpsat_compress(XORMAP_HEAD_t *phead, ANDMAP_HEAD_t *pderiv)
{
	hpsat_var_t vm = hpsat_maxvar(phead) + 1;
	XORMAP_HEAD_t deriv;
	hpsat_var_t map[2];
	XORMAP *xa;
	XORMAP *xn;
	ANDMAP *pa;
	ANDMAP *pn;
	BITMAP *ba;
	BITMAP *bb;
	bool any;

	TAILQ_INIT(&deriv);

	while (1) {
		for (xa = TAILQ_FIRST(phead); xa; xa = xa->next()) {
			for (ANDMAP *pa = xa->first(); pa; pa = pa->next()) {
				if (pa->count() > 1) {
					map[0] = pa->first()->minVar();
					map[1] = pa->first()->next()->minVar();
					if (map[0] == map[1]) {
						hpsat_var_t v = pa->first()->maxVar();
						if (v != map[0])
							map[0] = v;
					}
					if (map[0] == map[1]) {
						hpsat_var_t v = pa->first()->next()->maxVar();
						if (v != map[0])
							map[0] = v;
					}
					assert(map[0] != map[1]);
					goto next;
				}
			}
		}

		any = false;

		for (xa = TAILQ_FIRST(phead); xa; xa = xn->next()) {
			xn = xa->next();
			if (xn == 0)
				break;
			*xn |= *xa;
			xn->defactor();
			any = true;
			delete xa->remove(phead);
			xa = xn->next();
		}

		if (any)
			continue;
		break;
next:;
		any = false;

		XORMAP temp(ANDMAP(map[0],false) & ANDMAP(map[1],false));
		hpsat_var_t vn = vm++;

		printf("v%zd ", vn);
		temp.print(); printf("\n");

		(new XORMAP(vn,false))->insert_tail(&deriv);
		(new XORMAP(temp ^ XORMAP(vn, false)))->insert_tail(&deriv);
		(new XORMAP(false))->insert_tail(&deriv);

		for (xa = TAILQ_FIRST(phead); xa; xa = xa->next()) {
			for (pa = xa->first(); pa; pa = pn) {
				pn = pa->next();

				for (ba = pa->first(); ba; ba = ba->next()) {
					if (ba->contains(map[0]) == false)
						continue;
					for (bb = pa->first(); bb; bb = bb->next()) {
						if (bb == ba)
							continue;
						if (bb->contains(map[1]) == false)
							continue;
						goto found;
					}
					break;
				}
				continue;
			found:;
				ANDMAP *array[4] = {
				    new ANDMAP(*pa),
				    new ANDMAP(*pa),
				    new ANDMAP(*pa),
				    new ANDMAP(*pa)
				};

				array[0]->expand(map[0], 0).expand(map[1], 0);
				array[1]->expand(map[0], 0).expand(map[1], 1);
				array[2]->expand(map[0], 1).expand(map[1], 0);
				array[3]->expand(map[0], 1).expand(map[1], 1);

				if (array[0]->isZero())
					delete array[0];
				else
					array[0]->insert_tail(&xa->head);

				if (array[1]->isZero())
					delete array[1];
				else
					array[1]->insert_tail(&xa->head);

				if (array[2]->isZero())
					delete array[2];
				else
					array[2]->insert_tail(&xa->head);

				if (array[3]->isZero())
					delete array[3];
				else {
					(new BITMAP(vn, false))->insert_tail(&array[3]->head);
					array[3]->sort().insert_tail(&xa->head);
				}

				delete pa->remove(&xa->head);
			}
			xa->sort();
		}
	}

	xa = TAILQ_FIRST(phead);
	if (xa == 0) {
		hpsat_free(&deriv);
		return;
	}
	assert(xa->isXorConst());

	printf("REMAINDER\n");
	for (xa = TAILQ_FIRST(phead); xa; xa = xa->next()) {
		xa->print(); printf("\n");
	}

	BITMAP_HEAD_t head[vm];

	for (hpsat_var_t x = 0; x != vm; x++)
		TAILQ_INIT(&head[x]);

	for (xa = TAILQ_FIRST(&deriv); xa; xa = xa->next()) {
		hpsat_var_t v = xa->minVar();

		for (xa = xa->next(); !xa->isZero(); xa = xa->next()) {
			hpsat_var_t vt = HPSAT_VAR_MAX;
			size_t n = 0;
			hpsat_var_t other[2] = {};

			xa->print(); printf("\n");
			while (1) {
				vt = xa->maxVar(vt);
				if (vt == HPSAT_VAR_MIN)
					break;
				if (vt == v)
					continue;
				other[n++] = vt;
			}
			assert(n == 2);

			assert(TAILQ_FIRST(&head[v]) == 0);

			hpsat_compress_insert(&head[other[0]], &head[v], other[0]);
			hpsat_compress_insert(&head[other[1]], &head[v], other[1]);

			hpsat_sort_and(&head[v]);

			printf("VAR v%zd = \n", v);
			for (ba = TAILQ_FIRST(&head[v]); ba; ba = ba->next()) {
				ba->print(); printf("\n");
			}
		}
	}

	ba = TAILQ_FIRST(phead)->first()->first();

	if (ba->isInverted())
		(new ANDMAP(true))->insert_tail(pderiv);

	for (size_t x = 0; x != ba->nvar; x++) {
		const hpsat_var_t v = ba->pvar[x];
		(pa = new ANDMAP(true))->insert_tail(pderiv);
		TAILQ_CONCAT(&pa->head, &head[v], entry);
	}

	hpsat_sort_xor_accumulate(pderiv);

	for (hpsat_var_t x = 0; x != vm; x++)
		hpsat_free(&head[x]);

	hpsat_free(phead);

	TAILQ_CONCAT(phead, &deriv, entry);
}
