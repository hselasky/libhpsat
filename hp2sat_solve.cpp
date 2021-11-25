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

#include "hp2sat.h"

static void
hpsat_underiv(ANDMAP_HEAD_t *phead, ANDMAP_HEAD_t *pderiv)
{
	ANDMAP *pa;
	ANDMAP *pn;

	for (pa = TAILQ_FIRST(pderiv); pa != 0; ) {
		pn = pa->next();
		delete pa->remove(pderiv);
		pa = pn;

		while (!pa->isZero()) {
			pn = pa->next();
			pa->remove(pderiv)->insert_head(phead);
			pa = pn;
		}
		pn = pa->next();
		delete pa->remove(pderiv);
		pa = pn;
	}
}

static void
hpsat_multiply(ANDMAP_HEAD_t *ph0, ANDMAP_HEAD_t *ph1, ANDMAP_HEAD_t *ph2)
{
	size_t count[2] = {};
	ANDMAP_HEAD_t temp;
	ANDMAP *pa;
	ANDMAP *pb;
	ANDMAP *pc;

	TAILQ_INIT(&temp);

	for (pa = TAILQ_FIRST(ph0); pa != 0; pa = pa->next())
		count[0]++;
	for (pa = TAILQ_FIRST(ph1); pa != 0; pa = pa->next())
		count[1]++;

	if (count[0] < count[1]) {
		for (pa = TAILQ_FIRST(ph0); pa != 0; pa = pa->next()) {
			for (pb = TAILQ_FIRST(ph1); pb != 0; pb = pb->next()) {
				pc = new ANDMAP(*pa & *pb);
				if (pc->isZero())
					delete pc;
				else
					pc->insert_tail(&temp);
			}
			hpsat_sort_or(&temp);
		}
	} else {
		for (pa = TAILQ_FIRST(ph1); pa != 0; pa = pa->next()) {
			for (pb = TAILQ_FIRST(ph0); pb != 0; pb = pb->next()) {
				pc = new ANDMAP(*pa & *pb);
				if (pc->isZero())
					delete pc;
				else
					pc->insert_tail(&temp);
			}
			hpsat_sort_or(&temp);
		}
	}

	TAILQ_CONCAT(ph2, &temp, entry);
	hpsat_sort_or(ph2);
}

bool
hpsat_solve(ANDMAP_HEAD_t *phead, ANDMAP_HEAD_t *pderiv, hpsat_var_t vm)
{
	ANDMAP_HEAD_t ahead[HPSAT_VAR_ST_MAX];
	ANDMAP *pa;
	ANDMAP *pn;

	hpsat_underiv(phead, pderiv);

	hpsat_sort_or(phead);

	for (uint8_t x = 0; x != HPSAT_VAR_ST_MAX; x++)
		TAILQ_INIT(ahead + x);

	for (hpsat_var_t v = vm; v--; ) {

		for (pa = TAILQ_FIRST(phead); pa != 0; pa = pn) {
			pn = pa->next();
			if (pa->contains(v * HPSAT_VAR_ST_MAX)) {
				uint8_t st;

				if (pa->bitmaps[0].contains(v * HPSAT_VAR_ST_MAX))
					st = pa->bitmaps[0].getSt();
				else
					st = pa->bitmaps[1].getSt();

				pa->expand(v * HPSAT_VAR_ST_MAX, st);

				if (pa->isZero())
					delete pa->remove(phead);
				else
					pa->remove(phead)->insert_tail(ahead + st);
			}
		}

		for (uint8_t x = 0; x != HPSAT_VAR_ST_MAX; x++) {
			for (uint8_t y = x + 1; y != HPSAT_VAR_ST_MAX; y++) {
				hpsat_multiply(ahead + x, ahead + y, phead);
			}
		}

		(new ANDMAP())->insert_head(pderiv);

		for (uint8_t x = 0; x != HPSAT_VAR_ST_MAX; x++) {
			for (pa = TAILQ_FIRST(ahead + x); pa; pa = pn) {
				pn = pa->next();
				*pa &= ANDMAP(v * HPSAT_VAR_ST_MAX + x);
				pa->remove(ahead + x)->insert_head(pderiv);
			}
		}
		(new ANDMAP(v * HPSAT_VAR_ST_MAX))->insert_head(pderiv);
	}
	return (TAILQ_FIRST(phead) != 0);
}

bool
hpsat_solve_callback(ANDMAP *xm, uint8_t *psol, hpsat_solve_callback_t *cb, void *arg)
{
	if (xm == NULL)
		return (cb(arg, psol));

	const hpsat_var_t v = xm->bitmaps[0].state / HPSAT_VAR_ST_MAX;

	for (psol[v] = 0; psol[v] != HPSAT_VAR_ST_MAX; psol[v]++) {
		ANDMAP *pa;

		for (pa = xm->next(); !pa->isZero(); pa = pa->next()) {
			if (pa->expand_all(psol))
				goto next;
		}

		if (hpsat_solve_callback(pa->next(), psol, cb, arg))
			return (true);
	next:;
	}
	return (false);
}

static bool
hpsat_solve_do_count(void *arg, uint8_t *)
{
	(*(size_t *)arg)++;
	return (false);
}

size_t
hpsat_solve_count(ANDMAP_HEAD_t *phead, uint8_t *psol)
{
	size_t retval = 0;
	hpsat_solve_callback(TAILQ_FIRST(phead), psol, hpsat_solve_do_count, &retval);
	return (retval);
}

static bool
hpsat_solve_do_first(void *, uint8_t *)
{
	return (true);
}

bool
hpsat_solve_first(ANDMAP_HEAD_t *phead, uint8_t *psol)
{
	bool retval = true;

	hpsat_solve_callback(TAILQ_FIRST(phead), psol, hpsat_solve_do_first, 0);

	/* double check solution */
	for (ANDMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		for (pa = pa->next(); !pa->isZero(); pa = pa->next()) {
			if (pa->expand_all(psol))
				retval = false;
		}
	}
	return (retval);
}
