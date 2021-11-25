/*-
 * Copyright (c) 2020-2023 Hans Petter Selasky. All rights reserved.
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

#include "hpNsat.h"

static void
hpNsat_underiv(ANDMAP_HEAD_t *phead, ANDMAP_HEAD_t *pderiv)
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
hpNsat_binary_conflict(const ANDMAP_HEAD_t *ph0, const ANDMAP_HEAD_t *ph1, ANDMAP_HEAD_t *phead)
{
	ANDMAP *p[2];
	ANDMAP *pr;
	size_t old = 64;
	size_t curr = 0;

	for (p[0] = TAILQ_FIRST(ph0); p[0] != 0; p[0] = p[0]->next()) {
		for (p[1] = TAILQ_FIRST(ph1); p[1] != 0; p[1] = p[1]->next()) {
			pr = new ANDMAP(*p[0] & *p[1]);
			if (pr->isZero()) {
				delete pr;
			} else {
				pr->insert_tail(phead);

				if (++curr >= old) {
					hpNsat_sort_or(phead);

					old = 0;
					curr = 0;

					for (pr = TAILQ_FIRST(phead); pr; pr = pr->next())
						old++;
				}
			}
		}
	}
}

bool
hpNsat_solve(ANDMAP_HEAD_t *phead, ANDMAP_HEAD_t *pderiv, hpNsat_var_t vm, const hpNsat_val_t *pmax)
{
	ANDMAP_HEAD_t *mhead;
	ANDMAP_HEAD_t thead;
	ANDMAP_HEAD_t uhead;
	ANDMAP *pa;
	ANDMAP *pn;

	hpNsat_underiv(phead, pderiv);

	hpNsat_sort_or(phead);

	mhead = new ANDMAP_HEAD_t [vm];

	for (hpNsat_var_t x = 0; x != vm; x++)
		TAILQ_INIT(mhead + x);

	for (pa = TAILQ_FIRST(phead); pa != 0; pa = pn) {
		pn = pa->next();

		pa->remove(phead);
		pa->insert_tail(mhead, false);
	}

	for (hpNsat_var_t v = 0; v != vm; v++) {
		if (pmax[v] == 0)
			continue;

		ANDMAP_HEAD_t ahead[pmax[v]];

		for (hpNsat_val_t x = 0; x != pmax[v]; x++)
			TAILQ_INIT(ahead + x);

		/* N-state variable */
		for (pa = TAILQ_FIRST(mhead + v); pa != 0; pa = pn) {
			pn = pa->next();
			if (pa->contains(v) == false)
				continue;

			hpNsat_val_t st = pmax[v];

			for (size_t x = 0; x != pa->nummaps; x++) {
				if (pa->bitmaps[x].contains(v))
					st = pa->bitmaps[x].val;
			}

			assert(st < pmax[v]);

			pa->remove(mhead, false)->expand(v, st);

			if (pa->isZero())
				delete pa;
			else
				pa->insert_tail(ahead + st);
		}

		for (hpNsat_val_t y = 0; y != pmax[v]; y++)
			hpNsat_sort_or(ahead + y);

		TAILQ_INIT(&thead);
		TAILQ_INIT(&uhead);

		if (pmax[v] >= 2) {
			hpNsat_binary_conflict(ahead + 0, ahead + 1, &thead);
			for (hpNsat_val_t x = 2; x != pmax[v]; x++) {
				hpNsat_binary_conflict(&thead, ahead + x, &uhead);
				hpNsat_free(&thead);
				TAILQ_CONCAT(&thead, &uhead, entry);
			}
		}

		hpNsat_sort_or(&thead);

		for (pa = TAILQ_FIRST(&thead); pa != 0; pa = pn) {
			pn = pa->next();
			pa->remove(&thead)->insert_tail(mhead, false);
		}

		(new ANDMAP())->insert_head(pderiv).setZero();

		for (hpNsat_val_t y = 0; y != pmax[v]; y++) {
			for (pa = TAILQ_FIRST(ahead + y); pa; pa = pn) {
				pn = pa->next();
				*pa &= ANDMAP(v, y);
				pa->remove(ahead + y)->insert_head(pderiv);
			}
		}
		(new ANDMAP(v,0))->insert_head(pderiv);
	}

	/* cleanup */
	for (hpNsat_var_t x = 0; x != vm; x++) {
		for (pa = TAILQ_FIRST(mhead + x); pa != 0; pa = pn) {
			pn = pa->next();

			pa->remove(mhead, false)->insert_tail(phead);
		}
	}

	delete [] mhead;

	return (TAILQ_FIRST(phead) != 0);
}

bool
hpNsat_solve_callback(ANDMAP *xm, hpNsat_val_t *psol, const hpNsat_val_t *pmax, hpNsat_solve_callback_t *cb, void *arg)
{
	if (xm == NULL)
		return (cb(arg, psol));

	const hpNsat_var_t v = xm->bitmaps[0].var;
	const hpNsat_val_t max = pmax[v];

	for (psol[v] = 0; psol[v] != max; psol[v]++) {
		ANDMAP *pa;

		for (pa = xm->next(); !pa->isZero(); pa = pa->next()) {
			if (pa->expand_all(psol))
				goto next;
		}

		if (hpNsat_solve_callback(pa->next(), psol, pmax, cb, arg))
			return (true);
	next:;
	}
	return (false);
}

static bool
hpNsat_solve_do_count(void *arg, hpNsat_val_t *)
{
	(*(size_t *)arg)++;
	return (false);
}

size_t
hpNsat_solve_count(ANDMAP_HEAD_t *phead, hpNsat_val_t *psol, const hpNsat_val_t *pmax)
{
	size_t retval = 0;
	hpNsat_solve_callback(TAILQ_FIRST(phead), psol, pmax, hpNsat_solve_do_count, &retval);
	return (retval);
}
