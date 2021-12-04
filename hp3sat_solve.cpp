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

bool
hpsat_solve(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv, hpsat_var_t vm)
{
	XORMAP_HEAD_t temp;

	XORMAP *xa;
	XORMAP *xb;
	XORMAP *xn;

	bool do_loop;

	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		if (xa->isXorConst())
			continue;
		if (xa->count() > 1 || xa->isOne() || hpsat_numvar(&xa->head) > 2)
			goto solve;
	}

	do {
		hpsat_sort_or(xhead);

		hpsat_simplify_all(xhead, pderiv, vm);

		hpsat_underiv(xhead, pderiv);

		do_loop = hpsat_simplify_deriv(xhead, pderiv);

		hpsat_underiv(xhead, pderiv);
	} while (do_loop);

solve:
	TAILQ_INIT(&temp);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		if (xa->isXorConst()) {
			xa->remove(xhead)->insert_tail(&temp);
		} else {
			(new XORMAP(xa->toBitMap().toXorMap()))->insert_tail(&temp);
			delete xa->remove(xhead);
		}
	}

	TAILQ_CONCAT(xhead, &temp, entry);

	while (hpsat_simplify_xormap(xhead, pderiv))
		;

	uint8_t taken[(vm + 7) / 8];
	memset(taken, 0, sizeof(taken));

	for (xa = TAILQ_FIRST(pderiv); xa != 0; xa = xa->next()) {
		hpsat_var_t v = xa->maxVar();
		taken[v / 8] |= (1 << (v % 8));

		for (xa = xa->next(); !xa->isZero(); xa = xa->next())
			;
	}

	for (hpsat_var_t v = vm; v--; ) {
		if (taken[v / 8] & (1 << (v % 8)))
			continue;

		XORMAP_HEAD_t ahead;
		XORMAP_HEAD_t thead;

		TAILQ_INIT(&ahead);
		TAILQ_INIT(&thead);

		for (xa = TAILQ_FIRST(xhead); xa != 0; xa = xn) {
			xn = xa->next();
			if (xa->contains(v))
				xa->remove(xhead)->insert_tail(&ahead);

		}

		while (hpsat_simplify_xormap(&ahead, 0))
			;

		hpsat_find_ored(&ahead);
		hpsat_find_anded(&ahead);

		for (xa = TAILQ_FIRST(&ahead); xa != 0; xa = xn) {
			xn = xa->next();

			for (xb = xa->next(); xb != 0; xb = xb->next()) {
				if (xb->isZero())
					continue;
				xa->xored(*xb, v, &thead);
			}
			if (xa->isZero())
				delete xa->remove(&ahead);
		}

		hpsat_sort_or(&thead);

		hpsat_find_ored(&thead);
		hpsat_find_anded(&thead);

		TAILQ_CONCAT(xhead, &thead, entry);

		(new XORMAP(false))->insert_tail(&ahead);
		(new XORMAP(v,false))->insert_head(&ahead);

		TAILQ_CONCAT(&ahead, pderiv, entry);
		TAILQ_CONCAT(pderiv, &ahead, entry);
	}
	return (TAILQ_FIRST(xhead) != 0);
}

bool
hpsat_solve_callback(XORMAP *xm, uint8_t *psol, hpsat_solve_callback_t *cb, void *arg)
{
	XORMAP *xa;

	if (xm == NULL)
		return (cb(arg, psol));

	const hpsat_var_t v = xm->first()->first()->pvar[0];

	for (psol[v] = 0; psol[v] != 2; psol[v]++) {
		for (xa = xm->next(); !xa->isZero(); xa = xa->next()) {
			if (xa->expand_all(psol))
				goto next;
		}
		if (hpsat_solve_callback(xa->next(), psol, cb, arg) == false)
			return (false);
	next:;
	}
	return (true);
}

static bool
hpsat_solve_do_count(void *arg, uint8_t *)
{
	(*(size_t *)arg)++;
	return (true);
}

size_t
hpsat_solve_count(XORMAP_HEAD_t *xhead, uint8_t *psol)
{
	size_t retval = 0;
	hpsat_solve_callback(TAILQ_FIRST(xhead), psol, hpsat_solve_do_count, &retval);
	return (retval);
}

bool
hpsat_solve_first(XORMAP_HEAD_t *xhead, uint8_t *psol)
{
	bool retval = true;

	/* compute the derivated XORs in the right order */
	for (XORMAP *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		const hpsat_var_t v = xa->first()->first()->pvar[0];
		printf("c VAR %zd\n", v);
		for (xa = xa->next(); !xa->isZero(); xa = xa->next()) {
			printf("c "); xa->print(); printf("\n");
			psol[v] ^= xa->expand_all(psol);
		}
	}

	/* double check solution */
	for (XORMAP *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		for (xa = xa->next(); !xa->isZero(); xa = xa->next()) {
			if (xa->expand_all(psol))
				retval = false;
		}
	}
	return (retval);
}

void
hpsat_solve_strip(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv, hpsat_var_t vmin, hpsat_var_t vmax)
{
	if (vmax < vmin)
		vmax = vmin;

	hpsat_var_t stats[vmax];

	memset(stats, 0, sizeof(stats));

	XORMAP *pvar[vmax];

	memset(pvar, 0, sizeof(pvar));

	for (XORMAP *xa = TAILQ_FIRST(pderiv); xa; xa = xa->next()) {
		stats[xa->maxVar()] = 1;
		for (xa = xa->next(); !xa->isZero(); xa = xa->next())
			;
	}

	/* add missing variable definitions */
	for (hpsat_var_t v = 0; v != vmin; v++) {
		if (stats[v] != 0)
			continue;
		/* variable can have any value */
		(new XORMAP(false))->insert_head(pderiv);
		(new XORMAP(v, false))->insert_head(pderiv);
	}

	hpsat_var_t count = vmin;
	hpsat_var_t index = 0;

	/* remove not needed equations, first step */
	for (XORMAP *xa = TAILQ_FIRST(pderiv); xa; xa = xa->next()) {
		if (count == 0) {
			while (xa) {
				XORMAP *xn = xa->next();
				delete xa->remove(pderiv);
				xa = xn;
			}
			break;
		} else {
			assert(index < vmax);
			const hpsat_var_t v = xa->maxVar();
			if (v < vmin)
				count--;
			pvar[index++] = xa;
			for (xa = xa->next(); !xa->isZero(); xa = xa->next())
				;
		}
	}

	memset(stats, 0, sizeof(stats));

	/* find all dependencies */
	for (hpsat_var_t i = index; i--; ) {
		XORMAP *xa = pvar[i];
		const hpsat_var_t v = xa->maxVar();
		if (v < vmin || stats[v] != 0) {
			stats[v] = 1;
			for (xa = xa->next(); !xa->isZero(); xa = xa->next()) {
				for (ANDMAP *pa = TAILQ_FIRST(&xa->head); pa; pa = pa->next()) {
					for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
						for (size_t x = 0; x != bm->nvar; x++)
							stats[bm->pvar[x]] = 1;
					}
				}
			}
		} else {
			XORMAP *xn;

			while (!xa->isZero()) {
				xn = xa->next();
				delete xa->remove(pderiv);
				xa = xn;
			}
			xn = xa->next();
			delete xa->remove(pderiv);
			xa = xn;
		}
	}

	/* find remaining variables, if any */
	for (XORMAP *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		for (ANDMAP *pa = TAILQ_FIRST(&xa->head); pa; pa = pa->next()) {
			for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
				for (size_t x = 0; x != bm->nvar; x++)
					stats[bm->pvar[x]] = 1;
			}
		}
	}

	/* remove variable gaps */
	for (hpsat_var_t i = index = vmin; i != vmax; i++) {
		if (stats[i] != 0)
			stats[i] = index++;
	}
	/* keep variables below "vmin" as-is */
	for (hpsat_var_t i = 0; i != vmin; i++)
		stats[i] = i;

	/* optimize the variable numbers */
	for (XORMAP *xa = TAILQ_FIRST(pderiv); xa; xa = xa->next()) {
		for (ANDMAP *pa = TAILQ_FIRST(&xa->head); pa; pa = pa->next()) {
			for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
				for (size_t x = 0; x != bm->nvar; x++)
					bm->pvar[x] = stats[bm->pvar[x]];
			}
		}
	}

	/* optimize the variable numbers */
	for (XORMAP *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		for (ANDMAP *pa = TAILQ_FIRST(&xa->head); pa; pa = pa->next()) {
			for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
				for (size_t x = 0; x != bm->nvar; x++)
					bm->pvar[x] = stats[bm->pvar[x]];
			}
		}
	}
}

void
hpsat_underiv(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv)
{
	XORMAP *xa;
	XORMAP *xn;

	for (xa = TAILQ_FIRST(pderiv); xa != 0; ) {
		xn = xa->next();
		delete xa->remove(pderiv);
		xa = xn;

		while (!xa->isZero()) {
			xn = xa->next();
			xa->remove(pderiv)->insert_head(xhead);
			xa = xn;
		}
		xn = xa->next();
		delete xa->remove(pderiv);
		xa = xn;
	}
}
