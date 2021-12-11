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

#include <stdlib.h>

static int
hpsat_compare_var_t(const void *pa, const void *pb)
{
	const hpsat_var_t a = *(const hpsat_var_t *)pa;
	const hpsat_var_t b = *(const hpsat_var_t *)pb;

	if (a > b)
		return (1);
	else if (a < b)
		return (-1);
	else
		return (0);
}

static hpsat_var_t
hpsat_maxvar_dual(BITMAP_HEAD_t *phead, BITMAP_HEAD_t *psub)
{
	if (psub == 0) {
		return (hpsat_maxvar(phead) + 1);
	} else {
		const hpsat_var_t a = hpsat_maxvar(phead);
		const hpsat_var_t b = hpsat_maxvar(psub);

		return ((a > b ? a : b) + 1);
	}
}

static hpsat_var_t
hpsat_maxvar_dual(ANDMAP_HEAD_t *phead, ANDMAP_HEAD_t *psub)
{
	if (psub == 0) {
		return (hpsat_maxvar(phead) + 1);
	} else {
		const hpsat_var_t a = hpsat_maxvar(phead);
		const hpsat_var_t b = hpsat_maxvar(psub);

		return ((a > b ? a : b) + 1);
	}
}

void
hpsat_simplify_or(BITMAP_HEAD_t *phead, BITMAP_HEAD_t *psub)
{
	const hpsat_var_t vmax = hpsat_maxvar_dual(phead, psub);
	BITMAP *pa;
	BITMAP *pn;

	BITMAP *plast[vmax];
	hpsat_var_t used[vmax];
	size_t index = 0;

	/* only zero the entries used */
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		for (size_t x = 0; x != pa->nvar; x++)
			plast[pa->pvar[x]] = 0;
	}

	if (psub != 0) {
		for (pa = TAILQ_FIRST(psub); pa; pa = pa->next()) {
			for (size_t x = 0; x != pa->nvar; x++)
				plast[pa->pvar[x]] = 0;
		}
	}

	/* optimise */
	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		pn = pa->next();
		if (pa->isXorConst() == false)
			continue;

		while (pa->nvar != 0) {
			const hpsat_var_t v = pa->pvar[pa->nvar - 1];
			if (plast[v] != 0) {
				*pa ^= *plast[v];
			} else {
				plast[v] = pa;
				used[index++] = v;
				break;
			}
		}

		/* sanity checks */
		if (pa->isZero())
			delete pa->remove(phead);
		else if (pa->isOne())
			goto err_one;
	}

	mergesort(used, index, sizeof(used[0]), &hpsat_compare_var_t);

	while (index--) {
		hpsat_var_t v = used[index];
		pa = plast[v];

		while (1) {
			v = pa->maxVar(v);
			if (v == HPSAT_VAR_MIN)
				break;
			if (plast[v] != 0)
				*pa ^= *plast[v];
		}
	}

	if (psub != 0) {
		/* optimise */
		for (pa = TAILQ_FIRST(psub); pa; pa = pa->next()) {
			if (pa->isXorConst() == false)
				continue;
			for (hpsat_var_t v = HPSAT_VAR_MAX;; ) {
				v = pa->maxVar(v);
				if (v == HPSAT_VAR_MIN)
					break;
				if (plast[v] != 0)
					*pa ^= *plast[v];
			}
		}
	}
	return;
err_one:
	pa->remove(phead);
	hpsat_free(phead);
	pa->insert_tail(phead);
}

void
hpsat_simplify_and(BITMAP_HEAD_t *phead, BITMAP_HEAD_t *psub)
{
	const hpsat_var_t vmax = hpsat_maxvar_dual(phead, psub);
	BITMAP *pa;
	BITMAP *pn;

	BITMAP *plast[vmax];
	hpsat_var_t used[vmax];
	size_t index = 0;

	/* only zero the entries used */
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		for (size_t x = 0; x != pa->nvar; x++)
			plast[pa->pvar[x]] = 0;
	}

	if (psub != 0) {
		for (pa = TAILQ_FIRST(psub); pa; pa = pa->next()) {
			for (size_t x = 0; x != pa->nvar; x++)
				plast[pa->pvar[x]] = 0;
		}
	}

	/* optimise */
	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		pn = pa->next();
		if (pa->isXorConst() == false)
			continue;

		while (pa->nvar != 0) {
			const hpsat_var_t v = pa->pvar[pa->nvar - 1];
			if (plast[v] != 0) {
				*pa ^= *plast[v];
				pa->toggleInverted();
			} else {
				plast[v] = pa;
				used[index++] = v;
				break;
			}
		}

		/* sanity checks */
		if (pa->isOne())
			delete pa->remove(phead);
		else if (pa->isZero())
			goto err_zero;
	}

	mergesort(used, index, sizeof(used[0]), &hpsat_compare_var_t);

	while (index--) {
		hpsat_var_t v = used[index];
		pa = plast[v];

		while (1) {
			v = pa->maxVar(v);
			if (v == HPSAT_VAR_MIN)
				break;
			if (plast[v] != 0) {
				*pa ^= *plast[v];
				pa->toggleInverted();
			}
		}
	}

	if (psub != 0) {
		/* optimise */
		for (pa = TAILQ_FIRST(psub); pa; pa = pa->next()) {
			if (pa->isXorConst() == false)
				continue;
			for (hpsat_var_t v = HPSAT_VAR_MAX;; ) {
				v = pa->maxVar(v);
				if (v == HPSAT_VAR_MIN)
					break;
				if (plast[v] != 0) {
					*pa ^= *plast[v];
					pa->toggleInverted();
				}
			}
		}
	}
	return;
err_zero:
	pa->remove(phead);
	hpsat_free(phead);
	pa->insert_tail(phead);
}

void
hpsat_simplify_or(ANDMAP_HEAD_t *phead, ANDMAP_HEAD_t *psub)
{
	const hpsat_var_t vmax = hpsat_maxvar_dual(phead, psub);
	ANDMAP *pa;
	ANDMAP *pn;

	ANDMAP *plast[vmax];
	hpsat_var_t used[vmax];
	size_t index = 0;

	/* only zero the entries used */
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
			for (size_t x = 0; x != bm->nvar; x++)
				plast[bm->pvar[x]] = 0;
		}
	}
	if (psub) {
		for (pa = TAILQ_FIRST(psub); pa; pa = pa->next()) {
			for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
				for (size_t x = 0; x != bm->nvar; x++)
					plast[bm->pvar[x]] = 0;
			}
		}
	}

	/* optimise */
	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		pn = pa->next();

		while (1) {
			hpsat_var_t v = pa->maxVar();
			if (v == HPSAT_VAR_MIN)
				break;
			if (plast[v] != 0) {
				pa->xored(*plast[v], v);
			} else {
				plast[v] = pa;
				used[index++] = v;
				break;
			}
		}

		/* sanity checks */
		if (pa->isZero())
			delete pa->remove(phead);
		else if (pa->isOne())
			goto err_one;
	}

	mergesort(used, index, sizeof(used[0]), &hpsat_compare_var_t);

	while (index--) {
		hpsat_var_t v = used[index];
		pa = plast[v];

		while (1) {
			v = pa->maxVar(v);
			if (v == HPSAT_VAR_MIN)
				break;
			if (plast[v] != 0)
				pa->xored(*plast[v], v);
		}
	}

	if (psub) {
		/* optimise */
		for (pa = TAILQ_FIRST(psub); pa; pa = pa->next()) {
			for (hpsat_var_t v = HPSAT_VAR_MAX;; ) {
				v = pa->maxVar(v);
				if (v == HPSAT_VAR_MIN)
					break;
				if (plast[v] != 0)
					pa->xored(*plast[v], v);
			}
		}
	}
	return;
err_one:
	pa->remove(phead);
	hpsat_free(phead);
	pa->insert_tail(phead);
}

static ANDMAP
hpsat_simplify_or_get_last(XORMAP *xa)
{
	if (xa->isXorConst()) {
		hpsat_var_t v = xa->maxVar();
		if (v == HPSAT_VAR_MIN)
			return (ANDMAP(xa->isOne()));
		else
			return (ANDMAP(v, false));
	} else {
		return (*xa->last());
	}
}

void
hpsat_simplify_or(XORMAP_HEAD_t *xhead)
{
	XORMAP *xa;
	XORMAP *xb;
	XORMAP *xn;

	XORMAP_HEAD_t last;

	TAILQ_INIT(&last);

	/* optimise */
	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xa->sort();

		xn = xa->next();
	top:
		const ANDMAP v = hpsat_simplify_or_get_last(xa);
		if (v.isZero()) {
			delete xa->remove(xhead);
			continue;
		} else if (v.isOne()) {
			goto err_one;
		}

		for (xb = TAILQ_FIRST(&last); xb; xb = xb->next()) {
			const ANDMAP t = hpsat_simplify_or_get_last(xb);
			int ret = v.compare(t);
			if (ret < 0) {
				break;
			} else if (ret > 0) {
				continue;
			} else {
				*xa ^= *xb;
				goto top;
			}
		}

		if (xb != 0) {
			xa->remove(xhead)->insert_before(xb);
		} else {
			xa->remove(xhead)->insert_tail(&last);
		}
	}

	assert(TAILQ_FIRST(xhead) == 0);

	for (xa = TAILQ_LAST(&last, XORMAP_HEAD); xa; xa = xa->prev()) {
		for (xb = xa->prev(); xb; xb = xb->prev()) {
			const ANDMAP v = hpsat_simplify_or_get_last(xb);
			if (v.isXorConst()) {
				if (xa->contains(v.maxVar()))
					*xa ^= *xb;
			} else if (xa->contains(v)) {
				*xa ^= *xb;
			}
		}
	}

	TAILQ_CONCAT(xhead, &last, entry);
	return;
err_one:
	xa->remove(xhead);
	hpsat_free(xhead);
	hpsat_free(&last);
	xa->insert_tail(xhead);
}

bool
hpsat_simplify_defactor(ANDMAP_HEAD_t *phead)
{
	ANDMAP *pa;
	BITMAP *ba;
	BITMAP *bn;

	bool any = false;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->isXorConst())
			continue;
		for (ba = pa->first(); ba; ba = bn) {
			bn = ba->next();

			ba->remove(&pa->head);

			if (ba->isInverted()) {
				ba->toggleInverted();
				(new ANDMAP(*pa))->insert_tail(phead);
				any = true;
			}

			while (ba->nvar > 1) {
				BITMAP var(ba->maxVar(), false);
				*ba ^= var;
				(new ANDMAP(*pa & ANDMAP(var)))->insert_tail(phead);
				any = true;
			}

			ba->insert_head(&pa->head);
		}
	}
	return (any);
}

void
hpsat_simplify_defactor(XORMAP_HEAD_t *xhead)
{
	XORMAP *xa;
	XORMAP *xn;

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		if (hpsat_simplify_defactor(&xa->head)) {
			if (xa->sort().isZero())
				delete xa->remove(xhead);
			else if (xa->isOne())
				goto err_one;
		}
	}
	return;
err_one:
	xa->remove(xhead);
	hpsat_free(xhead);
	xa->insert_tail(xhead);
}

bool
hpsat_simplify_factor(XORMAP_HEAD_t *xhead, ANDMAP_HEAD_t *phead)
{
	XORMAP *xa;

	ANDMAP *pa;

	BITMAP *ba;
	BITMAP *bb;
	BITMAP *bc;

	bool any = false;

	ANDMAP_HEAD_t copy;
	TAILQ_INIT(&copy);

	BITMAP af;
	BITMAP bf;

	XORMAP test;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->isXorConst() || pa->count() == 2) {
			(new ANDMAP(*pa))->insert_tail(&copy);
		} else {
			goto done;
		}
	}

	for (pa = TAILQ_FIRST(&copy); pa; pa = pa->next()) {
		if (pa->isXorConst())
			continue;
		ba = pa->first();
		if (ba == 0)
			continue;
		bb = ba->next();
		if (bb == 0 || bb->next() != 0)
			continue;
		break;
	}

	if (pa == 0)
		goto done;

	for (pa = TAILQ_FIRST(&copy); pa; pa = pa->next()) {
		for (bc = TAILQ_FIRST(&pa->head); bc; bc = bc->next()) {
			if (*bc != *ba)
				continue;
			bc->remove(&pa->head);
			if (pa->first())
				af ^= *pa->first();
			else
				af.toggleInverted();
			bc->insert_tail(&pa->head);
			break;
		}
	}

	for (pa = TAILQ_FIRST(&copy); pa; pa = pa->next()) {
		for (bc = TAILQ_FIRST(&pa->head); bc; bc = bc->next()) {
			if (*bc != *bb)
				continue;
			bc->remove(&pa->head);
			if (pa->first())
				bf ^= *pa->first();
			else
				bf.toggleInverted();
			bc->insert_tail(&pa->head);
			break;
		}
	}

	(pa = new ANDMAP(true))->insert_tail(&test.head);
	(new BITMAP(af))->insert_tail(&pa->head);
	(new BITMAP(bf))->insert_tail(&pa->head);

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		(new ANDMAP(*pa))->insert_tail(&test.head);

	hpsat_simplify_defactor(&test.head);

	if (test.sort().isXorConst()) {
		xa = new XORMAP(false);
		if (test.first())
			(new ANDMAP(*test.first()))->insert_tail(&xa->head);

		(pa = new ANDMAP(true))->insert_tail(&xa->head);
		(new BITMAP(af))->insert_tail(&pa->head);
		(new BITMAP(bf))->insert_tail(&pa->head);
		if (xa->sort().isZero())
			delete xa;
		else
			xa->insert_tail(xhead);
		any = true;
	}
done:
	hpsat_free(&copy);
	return (any);
}

void
hpsat_simplify_factor(XORMAP_HEAD_t *xhead)
{
	XORMAP *xa;
	XORMAP *xn;

	XORMAP_HEAD_t added;
	TAILQ_INIT(&added);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();
		if (hpsat_simplify_factor(&added, &xa->head))
			delete xa->remove(xhead);
	}
	for (xa = TAILQ_FIRST(&added); xa; xa = xn) {
		xn = xa->next();
		if (xa->isOne()) {
			goto err_one;
		} else if (xa->isZero()) {
			delete xa->remove(&added);
		} else {
			xa->remove(&added)->insert_tail(xhead);
		}
	}
	return;
err_one:
	xa->remove(&added);
	hpsat_free(&added);
	hpsat_free(xhead);
	xa->insert_tail(xhead);
}

static bool
hpsat_substitute(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv, hpsat_var_t v, const BITMAP &expr)
{
	XORMAP *xa;
	XORMAP *xn;
	bool any;

	if (expr.isZero()) {
		return (false);
	} else if (expr.isOne()) {
		hpsat_free(xhead);
		(new XORMAP(expr))->insert_head(xhead);
		return (false);
	}

	printf("c SUBSTITUTED "); expr.print(); printf(" # v%zd\n", v);

	assert(expr.contains(v));

	any = false;

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		if (xa->contains(v) == false)
			continue;
		any = true;

		if (xa->substitute(v, expr).isZero())
			delete xa->remove(xhead);
		else if (xa->isOne())
			goto err_one;
	}

	(new XORMAP(false))->insert_head(pderiv);
	(new XORMAP(expr))->insert_head(pderiv);
	(new XORMAP(v,false))->insert_head(pderiv);
	return (any);
err_one:
	xa->remove(xhead);
	hpsat_free(xhead);
	xa->insert_tail(xhead);
	return (any);
}

bool
hpsat_simplify_deriv(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv)
{
	const hpsat_var_t vmax = hpsat_maxvar(xhead) + 1;

	XORMAP *xa;
	XORMAP *xn;

	XORMAP *xb;
	XORMAP *xm;

	BITMAP *ba;

	hpsat_var_t v;

	XORMAP *plast[vmax];
	hpsat_var_t used[vmax];
	size_t index = 0;
	bool retval;

	BITMAP sub;

	/* only zero entries used */
	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		for (ANDMAP *pa = TAILQ_FIRST(&xa->head); pa; pa = pa->next()) {
			if (pa->isXorConst() == false)
				continue;
			for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
				for (size_t x = 0; x != bm->nvar; x++)
					plast[bm->pvar[x]] = 0;
			}
		}
	}

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		v = HPSAT_VAR_MAX;
		while (1) {
			v = xa->maxVar(v, true);
			if (v == HPSAT_VAR_MIN)
				break;
			if (plast[v] != 0) {
				*xa ^= *plast[v];
			} else {
				plast[v] = xa;
				used[index++] = v;
				break;
			}
		}

		if (xa->isZero())
			delete xa->remove(xhead);
		else if (xa->isOne())
			goto err_one;
	}

	mergesort(used, index, sizeof(used[0]), &hpsat_compare_var_t);

	for (size_t i = index; i--; ) {
		v = used[i];
		xa = plast[v];

		while (1) {
			v = xa->maxVar(v, true);
			if (v == HPSAT_VAR_MIN)
				break;
			if (plast[v] != 0)
				*xa ^= *plast[v];
		}
	}

	for (size_t i = 0; i != index; i++) {
		xa = plast[used[i]];
		if (xa->first()->first()->nvar < 3)
			continue;
		for (size_t j = i + 1; j != index; j++) {
			xn = plast[used[j]];
			if (xa->first()->first()->nvar != xn->first()->first()->nvar)
				continue;

			XORMAP temp(*xa ^ *xn);

			if (temp.first()->first()->nvar == 2) {
				*xa = temp;
				break;
			}
		}
	}

	if (pderiv == 0)
		return (index != 0);

	retval = false;

	/* Remove XORs with one or two variables from the equation. */

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();
		if (xa->isZero() || xa->isOne() || xa->isXorConst() == false)
			continue;

		ba = xa->first()->first();

		switch (ba->nvar) {
		case 1:
			v = ba->maxVar();

			(new XORMAP(false))->insert_head(pderiv);
			xa->remove(xhead)->insert_head(pderiv);
			(new XORMAP(v, false))->insert_head(pderiv);

			for (xb = TAILQ_FIRST(xhead); xb; xb = xm) {
				xm = xb->next();
				if (xb->contains(v) == false)
					continue;
				xb->expand(v, ba->isInverted());
				if (xb->isZero()) {
					delete xb->remove(xhead);
					if (xb == xn)
						xn = xm;
				} else {
					retval = true;
				}
			}
			break;
		case 2:
			v = ba->maxVar();

			sub = *ba;
			sub.expand(v, 0).sort();

			(new XORMAP(false))->insert_head(pderiv);
			xa->remove(xhead)->insert_head(pderiv);
			(new XORMAP(v, false))->insert_head(pderiv);

			for (xb = TAILQ_FIRST(xhead); xb; xb = xm) {
				xm = xb->next();
				if (xb->contains(v) == false)
					continue;
				if (xb->isXorConst()) {
					*xb ^= *xa;
				} else {
					BITMAP zero;
					BITMAP one;

					zero = one = xb->toBitMap();

					zero.expand(v, 0).sort();
					one.expand(v, 1).sort();

					zero = ((one ^ zero) & sub) ^ zero;

					*xb = XORMAP(ANDMAP(zero));
					xb->sort();
				}

				if (xb->isZero()) {
					delete xb->remove(xhead);
					if (xb == xn)
						xn = xm;
				}
			}
			break;
		default:
			break;
		}
	}
	return (retval);

err_one:
	xa->remove(xhead);
	hpsat_free(xhead);
	xa->insert_tail(xhead);
	return (false);
}

void
hpsat_simplify_conflicts(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv, hpsat_var_t v)
{
	XORMAP_HEAD_t prod_head[2];
	size_t count[2] = {};

	XORMAP *xa;
	XORMAP *xb;
	XORMAP *xn;

	TAILQ_INIT(prod_head + 0);
	TAILQ_INIT(prod_head + 1);

	(new XORMAP(false))->insert_head(pderiv);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();
		if (xa->contains(v) == false)
			continue;

		XORMAP zero(*xa);
		XORMAP one(*xa);

		xa->remove(xhead)->insert_head(pderiv);

		zero.expand(v, false);
		one.expand(v, true);

		if (zero.isZero())
			one.dup()->insert_tail(prod_head + 1);
		else
			zero.dup()->insert_tail(prod_head + 0);
	}

	hpsat_sort_or(prod_head + 0);
	hpsat_sort_or(prod_head + 1);

	for (xa = TAILQ_FIRST(prod_head + 0); xa; xa = xa->next())
		count[0]++;
	for (xb = TAILQ_FIRST(prod_head + 1); xb; xb = xb->next())
		count[1]++;

	if (count[0] < count[1]) {
		for (xa = TAILQ_FIRST(prod_head + 0); xa; xa = xa->next()) {
			for (xb = TAILQ_FIRST(prod_head + 1); xb; xb = xb->next()) {
				xn = new XORMAP(*xa & *xb);
				if (xn->sort().isZero())
					delete xn;
				else
					xn->insert_tail(xhead);
			}
			hpsat_sort_or(xhead);
		}
	} else {
		for (xb = TAILQ_FIRST(prod_head + 1); xb; xb = xb->next()) {
			for (xa = TAILQ_FIRST(prod_head + 0); xa; xa = xa->next()) {
				xn = new XORMAP(*xa & *xb);
				if (xn->sort().isZero())
					delete xn;
				else
					xn->insert_tail(xhead);
			}
			hpsat_sort_or(xhead);
		}
	}

	hpsat_free(prod_head + 0);
	hpsat_free(prod_head + 1);

	(new XORMAP(v, false))->insert_head(pderiv);
}

bool
hpsat_simplify_mixed(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv,
    BITMAP_HEAD_t *prod_head, hpsat_var_t v, hpsat_var_t vmax)
{
	XORMAP *xa;
	XORMAP *xn;

	BITMAP *ba;
	BITMAP *bb;
	BITMAP *bn;
	BITMAP *bm;

	bool retval = false;

	(new XORMAP(false))->insert_head(pderiv);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();
		if (xa->isXorConst() == false || xa->contains(v) == false)
			continue;

		if (xa->first()->first()->nvar <= 2)
			retval = true;

		xa->remove(xhead)->insert_head(pderiv);
	}

	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		if (xa->contains(v) == false)
			continue;

		BITMAP zero;
		BITMAP one;

		zero = one = xa->remove(xhead)->toBitMap();

		delete xa;

		zero.expand(v, false).sort();
		one.expand(v, true).sort();

		if (zero.isZero())
			(new BITMAP(one))->insert_tail(prod_head + 2 * v + 1);
		else
			(new BITMAP(zero))->insert_tail(prod_head + 2 * v + 0);
	}

	for (hpsat_var_t x = 0; x != vmax; x++) {
		if (x == v)
			continue;

		hpsat_var_t index;
		bool any = false;

		for (ba = TAILQ_FIRST(prod_head + 2 * x); ba; ba = bn) {
			bn = ba->next();

			assert(ba->nvar <= 1);

			if (ba->contains(v) == false)
				continue;

			index = 2 * v + (ba->isInverted() ? 0 : 1);
			delete ba->remove(prod_head + 2 * x);

			for (ba = TAILQ_FIRST(prod_head + 2 * x + 1); ba; ba = ba->next()) {
				if (ba->contains(v))
					continue;
				(new BITMAP(*ba))->insert_tail(prod_head + index);
				any = true;
			}
		}

		for (ba = TAILQ_FIRST(prod_head + 2 * x + 1); ba; ba = bn) {
			bn = ba->next();

			assert(ba->nvar <= 1);

			if (ba->contains(v) == false)
				continue;

			index = 2 * v + (ba->isInverted() ? 0 : 1);
			delete ba->remove(prod_head + 2 * x + 1);

			for (ba = TAILQ_FIRST(prod_head + 2 * x); ba; ba = ba->next()) {
				if (ba->contains(v))
					continue;
				(new BITMAP(*ba))->insert_tail(prod_head + index);
				any = true;
			}
		}

		if (any) {
			hpsat_sort_or(prod_head + 2 * v + 0);
			hpsat_sort_or(prod_head + 2 * v + 1);
		}
	}

	hpsat_check_inversion(prod_head + 2 * v + 0);
	hpsat_check_inversion(prod_head + 2 * v + 1);

	ba = TAILQ_FIRST(prod_head + 2 * v + 0);
	bb = TAILQ_FIRST(prod_head + 2 * v + 1);

	while (ba && bb) {
		switch (ba->compare(*bb)) {
		case -1:
			bb = bb->next();
			break;
		case 1:
			ba = ba->next();
			break;
		default:
			bn = ba->next();
			bm = bb->next();

			(new XORMAP(ANDMAP(*ba)))->sort().insert_tail(xhead);

			delete ba->remove(prod_head + 2 * v + 0);
			delete bb->remove(prod_head + 2 * v + 1);

			retval = true;

			ba = bn;
			bb = bm;
			break;
		}
	}

	for (ba = TAILQ_FIRST(prod_head + 2 * v + 0); ba; ba = ba->next()) {
		xa = (new XORMAP(ANDMAP(*ba & BITMAP(v, true))));
		if (xa->sort().isZero())
			delete xa;
		else
			xa->insert_head(pderiv);
	}

	for (ba = TAILQ_FIRST(prod_head + 2 * v + 1); ba; ba = ba->next()) {
		xa = (new XORMAP(ANDMAP(*ba & BITMAP(v, false))));
		if (xa->sort().isZero())
			delete xa;
		else
			xa->insert_head(pderiv);
	}

	(new XORMAP(v, false))->insert_head(pderiv);

	return (retval);
}

bool
hpsat_simplify_all(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv, hpsat_var_t vmax)
{
	XORMAP *xa;

	hpsat_var_t v;

	bool retval = false;

	BITMAP_HEAD_t prod_head[2 * vmax];

	uint8_t skip[vmax];

	for (v = 0; v != 2 * vmax; v++)
		TAILQ_INIT(prod_head + v);

	/* zero all entries */
	memset(skip, 0, sizeof(skip));

	/* remove taken variables */
	for (xa = TAILQ_FIRST(pderiv); xa; xa = xa->next()) {
		skip[xa->maxVar()] = 1;

		for (xa = xa->next(); !xa->isZero(); xa = xa->next())
			;
	}

	for (v = vmax; v--; ) {
		if (skip[v] != 0)
			continue;
		if (hpsat_simplify_mixed(xhead, pderiv, prod_head, v, vmax))
			retval = true;
	}

	for (v = 0; v != 2 * vmax; v++)
		hpsat_free(prod_head + v);

	return (retval);
}

/* Split a BITMAP by common and not common variables */
void
hpsat_simplify_split(BITMAP &a, BITMAP &b, BITMAP &c)
{
	hpsat_var_t v = HPSAT_VAR_MAX;

	while (1) {
		const hpsat_var_t m = a.maxVar(v);
		const hpsat_var_t n = b.maxVar(v);

		if (m > n) {
			v = m;
		} else if (m < n) {
			v = n;
		} else if (n == HPSAT_VAR_MIN) {
			break;
		} else {
			BITMAP bit(m, false);
			a ^= bit;
			b ^= bit;
			c ^= bit;
		}
	}
}

/* Split a ANDMAP by common and not common variables */
void
hpsat_simplify_split(ANDMAP &a, ANDMAP &b, ANDMAP &c)
{
	BITMAP *pa = a.first();
	BITMAP *pb = b.first();
	BITMAP *pc;
	BITMAP *pd;

	while (pa && pb) {
		pc = pa->next();
		pd = pb->next();

		switch (pa->compare(*pb)) {
		case 0:
			delete pa->remove(&a.head);
			pb->remove(&b.head)->insert_tail(&c.head);

			pa = pc;
			pb = pd;
			break;
		case 1:
			pb = pd;
			break;
		default:
			pa = pc;
			break;
		}
	}
}

static int
hpsat_compare_value(const void *a, const void *b)
{
	return ((ANDMAP * const *)a)[0][0].compare(((ANDMAP * const *)b)[0][0]);
}

static ssize_t
hpsat_lookup_value(ANDMAP **base, size_t num, ANDMAP **key)
{
	ANDMAP **result;

	result = (ANDMAP **)
	    bsearch(key, base, num, sizeof(base[0]), &hpsat_compare_value);

	if (result == 0)
		return (-1);
	else
		return (result - base);
}

static void
hpsat_merge_xor(XORMAP &from, XORMAP &to, bool doFreeFrom)
{
	ANDMAP_HEAD_t temp;
	ANDMAP *xa;
	ANDMAP *xb;
	ANDMAP *xp;

	TAILQ_INIT(&temp);
	TAILQ_CONCAT(&temp, &to.head, entry);

	xa = from.first();
	xb = TAILQ_FIRST(&temp);

	while (xa && xb) {
		switch (xa->compare(*xb)) {
		case 0:
			/* same value on both cancels */
			xp = xa;
			xa = xa->next();
			if (doFreeFrom)
				delete xp->remove(&from.head);
			xp = xb;
			xb = xb->next();
			delete xp->remove(&temp);
			break;
		case 1:
			xp = xb;
			xb = xb->next();
			xp->remove(&temp)->insert_tail(&to.head);
			break;
		default:
			xp = xa;
			xa = xa->next();
			if (doFreeFrom)
				xp->remove(&from.head)->insert_tail(&to.head);
			else
				xp->dup()->insert_tail(&to.head);
			break;
		}
	}
	if (doFreeFrom) {
		TAILQ_CONCAT(&to.head, &from.head, entry);
	} else {
		while (xa) {
			xp = xa;
			xa = xa->next();
			xp->dup()->insert_tail(&to.head);
		}
	}
	TAILQ_CONCAT(&to.head, &temp, entry);
}

bool
hpsat_simplify_xormap(XORMAP_HEAD_t *xhead)
{
	ANDMAP_HEAD_t andmap;

	ANDMAP **phash;
	XORMAP **plast;
	size_t nhash;

	XORMAP *xa;
	XORMAP *xb;
	XORMAP *xn;

	BITMAP *bm;

	ANDMAP *pa;
	ANDMAP *pb;
	ANDMAP *pc;

	ssize_t index;

	size_t x;
	size_t y;
	size_t z;

	bool any;

	/* collect all different kinds of expressions first */
	TAILQ_INIT(&andmap);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		xa->defactor();
		hpsat_sort_xor_no_accumulate(&xa->head);
		xn = xa->dup();
		TAILQ_CONCAT(&andmap, &xn->head, entry);
		delete xn;
	}

	nhash = 0;
	for (pa = TAILQ_FIRST(&andmap); pa; pa = pa->next())
		nhash++;

	/* check if there are no entries */
	if (nhash == 0)
		return (false);

	phash = new ANDMAP * [nhash];

	nhash = 0;
	for (pa = TAILQ_FIRST(&andmap); pa; pa = pa->next())
		phash[nhash++] = pa;

	mergesort(phash, nhash, sizeof(phash[0]), &hpsat_compare_value);

	for (x = z = 0; x != nhash; ) {
		for (y = x + 1; y != nhash; y++) {
			if (hpsat_compare_value(phash + x, phash + y))
				break;
			delete phash[y]->remove(&andmap);
		}
		phash[z++] = phash[x];
		x = y;
	}
	nhash = z;

	plast = new XORMAP * [nhash];
	memset(plast, 0, sizeof(plast[0]) * nhash);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		while (1) {
			if (xa->isZero()) {
				delete xa->remove(xhead);
				break;
			} else if (xa->isOne()) {
				goto err_one;
			}

			pa = xa->last();

			index = hpsat_lookup_value(phash, nhash, &pa);
			assert(index != -1);

			if (plast[index] != 0) {
				hpsat_merge_xor(*plast[index], *xa, false);
			} else {
				plast[index] = xa;
				break;
			}
		}
	}

repeat_0:
	any = false;

	for (x = nhash; x--; ) {
		xa = plast[x];
		if (xa == 0)
			continue;
	repeat_1:
		for (pa = xa->last()->prev(); pa; pa = pa->prev()) {
			if (pa->isOne())
				break;
			index = hpsat_lookup_value(phash, nhash, &pa);
			if (index != -1 && plast[index] != 0) {
				hpsat_merge_xor(*plast[index], *xa, false);
				goto repeat_1;
			}
		}
	}

	/* insertion step */

	printf("c NHASH=%zd\n", nhash);

	for (x = 0; x != nhash; x++) {
		xa = plast[x];
		if (xa == 0)
			continue;
		pa = xa->last();

		for (y = x + 1; y != nhash; y++) {
			xb = plast[y];
			if (xb == 0)
				continue;

			XORMAP xored;

			for (pb = xb->first(); pb; pb = pb->next()) {
				ANDMAP t[3] = { *pa, *pb, ANDMAP(true) };

				hpsat_simplify_split(t[0], t[1], t[2]);

				if (t[0].isOne() == false)
					continue;
				for (pc = xa->first(); pc; pc = pc->next())
					(new ANDMAP(t[1] & *pc))->insert_tail(&xored.head);
			}

			hpsat_sort_xor_no_accumulate(&xored.head);

			if (xored.isZero())
				continue;

			hpsat_merge_xor(xored, *xb, true);

			any = true;

			pb = xb->last();

			/* check if the last ANDMAP changed */
			if (pb == 0 || hpsat_compare_value(&pb, phash + y) != 0) {
				plast[y] = 0;

				while (1) {
					if (xb->isZero()) {
						delete xb->remove(xhead);
						break;
					} else if (xb->isOne()) {
						xa = xb;
						goto err_one;
					}

					pb = xb->last();

					index = hpsat_lookup_value(phash, nhash, &pb);
					if (index == -1) {
						/* the entry doesn't exist, so we need to re-hash */
						*phash[y] = *pb;
						plast[y] = xb;

						/* insertion sort */
						for (z = y; z != 0; z--) {
							if (hpsat_compare_value(phash + z - 1, phash + z) > 0) {
								HPSAT_SWAP(phash[z - 1], phash[z]);
								HPSAT_SWAP(plast[z - 1], plast[z]);
								/* update "x" if needed */
								if (x == z - 1)
									x = z;
							} else {
								break;
							}
						}
						break;
					} else if (plast[index] != 0) {
						hpsat_merge_xor(*plast[index], *xb, false);
					} else {
						plast[index] = xb;
						break;
					}
				}
			}
		}
	}

	if (any)
		goto repeat_0;

	hpsat_free(&andmap);

	delete [] plast;
	delete [] phash;

	any = false;

	/* get the sorting back to normal */
	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next())
		xa->sort();

	while (1) {
		xa = TAILQ_FIRST(xhead);
		if (xa == 0 || xa->isXorConst())
			break;

		pa = xa->last();
		pb = xa->first();

		if (pb->isOne() == false || pb->next() != pa) {
			/* put the easy targets first */
			for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
				xn = xa->next();

				if (xa->isXorConst())
					continue;

				pa = xa->last();
				pb = xa->first();

				if (pb->isOne() && pb->next() == pa)
					xa->remove(xhead)->insert_head(xhead);
			}
		}

		xa = TAILQ_FIRST(xhead);

		pa = xa->last();
		pb = xa->first();

		if (pb->isOne() && pb->next() == pa) {
			xa->remove(xhead);
			for (bm = pa->first(); bm; bm = bm->next()) {
				bm->toggleInverted();
				(new XORMAP(*bm))->insert_tail(xhead);
				any = true;
			}
			delete xa;
			continue;
		}
		break;
	}
	return (any);

err_one:
	xa->remove(xhead);
	hpsat_free(xhead);
	xa->insert_tail(xhead);

	hpsat_free(&andmap);

	delete [] plast;
	delete [] phash;

	return (false);
}

bool
hpsat_simplify_join(XORMAP_HEAD_t *xhead)
{
	XORMAP *xa;
	XORMAP *xb;
	XORMAP *xn;

	XORMAP_HEAD_t added;
	TAILQ_INIT(&added);

	bool any = false;

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		for (xb = xa->next(); xb; xb = xb->next()) {
			XORMAP temp(*xa & *xb);
			if (temp.isZero()) {
				(new XORMAP(*xa ^ *xb))->insert_tail(&added);
				if (xn == xb)
					xn = xn->next();
				delete xa->remove(xhead);
				delete xb->remove(xhead);
				any = true;
				break;
			}
		}
	}

	if (any) {
		TAILQ_CONCAT(xhead, &added, entry);
		hpsat_sort_or(xhead);
	}

	return (any);
}

bool
hpsat_simplify_find_zeros(XORMAP_HEAD_t *xhead)
{
	BITMAP_HEAD_t xmap;
	TAILQ_INIT(&xmap);

	if (hpsat_simplify_variable_map(xhead, &xmap))
		return (false);

	bool any = false;

	hpsat_var_t count = 0;

	for (BITMAP *bm = TAILQ_FIRST(&xmap); bm; bm = bm->next())
		count++;

	XORMAP *psubst[count];

	memset(psubst, 0, sizeof(psubst));

	for (XORMAP *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		ANDMAP *pa = xa->first();
		if (pa == 0 || pa->isZero() || pa->isOne() ||
		    pa->isXorConst() == false)
			continue;
		hpsat_var_t i = 0;
		for (BITMAP *bm = TAILQ_FIRST(&xmap); bm; bm = bm->next(), i++) {
			if (bm->compare(*pa->first(), false) == 0) {
				psubst[i] = xa;
				break;
			}
		}
	}

	XORMAP *xn;

	for (XORMAP *xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		ANDMAP *pn;
		bool found = false;

		for (ANDMAP *pa = xa->first(); pa; pa = pn) {
			pn = pa->next();

			if (pa->isXorConst())
				continue;

			XORMAP test(true);

			for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
				for (hpsat_var_t i = 0;; i++) {
					if (i == count) {
						test &= XORMAP(*bm);
						break;
					}
					if (psubst[i] == 0)
						continue;
					if (psubst[i]->first()->first()->compare(*bm, false))
						continue;
					XORMAP build(false);
					if (psubst[i]->first()->first()->isInverted() ^ bm->isInverted())
						(new ANDMAP(true))->insert_tail(&build.head);
					for (ANDMAP *pb = psubst[i]->first()->next(); pb; pb = pb->next())
						(new ANDMAP(*pb))->insert_tail(&build.head);
					test &= build;
					break;
				}
			}

			hpsat_simplify_defactor(&test.head);

			if (test.sort().isXorConst()) {
				printf("c REPLACING WITH "); test.print(); printf("\n");
				delete pa->remove(&xa->head);
				TAILQ_CONCAT(&xa->head, &test.head, entry);
				found = true;
			}
		}
		if (found) {
			any = true;
			if (xa->sort().isZero())
				delete xa->remove(xhead);
		}
	}

	hpsat_free(&xmap);
	return (any);
}

bool
hpsat_simplify_assignments(XORMAP_HEAD_t *xhead, BITMAP_HEAD_t *psel, const BITMAP &var, bool insert_all)
{
	hpsat_var_t v = HPSAT_VAR_MAX;
	bool any = false;

	while (1) {
		const hpsat_var_t m = hpsat_maxvar(psel + 0, v);
		const hpsat_var_t n = hpsat_maxvar(psel + 1, v);

		if (m > n) {
			v = m;
		} else if (m < n) {
			v = n;
		} else if (n == HPSAT_VAR_MIN) {
			break;
		} else {
			BITMAP *pfound[2] = {};

			v = n;
			for (size_t i = 0; i != 2; i++) {
				for (BITMAP *pa = TAILQ_FIRST(psel + i); pa; pa = pa->next()) {
					if (pa->contains(v) == false)
						continue;
					if (pfound[i] != 0) {
						*pa ^= *pfound[i];
					} else {
						pfound[i] = pa;
					}
				}

				pfound[i]->remove(psel + i);

				do {
					hpsat_simplify_or(psel + i);
				} while (hpsat_sort_or(psel + i));
			}

			hpsat_var_t w = HPSAT_VAR_MAX;

			while (1) {
				const hpsat_var_t k = pfound[0]->maxVar(w);
				const hpsat_var_t l = pfound[1]->maxVar(w);

				if (k > l) {
					for (BITMAP *pa = TAILQ_FIRST(psel + 0); pa; pa = pa->next()) {
						if (pa->maxVar() == k)
							*pfound[0] ^= *pa;
					}
					w = k;
				} else if (k < l) {
					for (BITMAP *pa = TAILQ_FIRST(psel + 1); pa; pa = pa->next()) {
						if (pa->maxVar() == l)
							*pfound[1] ^= *pa;
					}
					w = l;
				} else if (k == HPSAT_VAR_MIN) {
					break;
				} else {
					w = k;
				}
			}

			BITMAP assign(false);

			hpsat_simplify_split(*pfound[0], *pfound[1], assign);

			if (pfound[0]->isInverted()) {
				pfound[0]->toggleInverted();
				assign ^= var;
			}

			if (pfound[1]->isInverted()) {
				pfound[1]->toggleInverted();
				assign ^= var;
				assign.toggleInverted();
			}

			XORMAP *xa = new XORMAP();

			(new ANDMAP(assign ^ *pfound[1]))->insert_tail(&xa->head);
			(new ANDMAP(ANDMAP(*pfound[0] ^ *pfound[1]) & ANDMAP(var)))->insert_tail(&xa->head);

			assert(var.isInverted() == false);

			if (xa->sort().isZero()) {
				delete xa;

				delete pfound[0];
				delete pfound[1];
			} else if (insert_all || xa->isXorConst()) {
				printf("c INSERTED :: "); xa->print(); printf("\n");
				xa->insert_tail(xhead);
				any = true;

				delete pfound[0];
				delete pfound[1];
			} else {
				*pfound[0] ^= assign;
				*pfound[1] ^= assign;

				pfound[0]->insert_tail(psel + 0);
				pfound[1]->insert_tail(psel + 1);

				delete xa;
			}
		}
	}
	return (any);
}

bool
hpsat_simplify_variable_map(XORMAP_HEAD_t *xhead, BITMAP_HEAD_t *pvar)
{
	XORMAP *xa;
	ANDMAP *pa;
	BITMAP *bm;
	BITMAP *bn;

	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		if (xa->isOne()) {
			hpsat_free(pvar);
			return (true);
		}

		for (pa = xa->first(); pa; pa = pa->next()) {
			for (bm = pa->first(); bm; bm = bm->next()) {
				bn = bm->dup();
				if (bn->isInverted())
					bn->toggleInverted();
				bn->insert_tail(pvar);
			}
		}
	}
	hpsat_sort_or(pvar);
	return (false);
}

bool
hpsat_simplify_symmetry(XORMAP_HEAD_t *xhead, XORMAP_HEAD_t *pderiv, bool insert_all)
{
	BITMAP_HEAD_t sel[2];
	BITMAP_HEAD_t vmap;

	ANDMAP_HEAD_t cross;

	ANDMAP *pa;
	ANDMAP *pn;

	BITMAP *ba;
	BITMAP *bb;
	BITMAP *bm;

	XORMAP *xa;
	XORMAP *xn;

	bool changed = false;

	TAILQ_INIT(&vmap);
	TAILQ_INIT(&cross);

	for (uint8_t x = 0; x != 2; x++)
		TAILQ_INIT(&sel[x]);

	if (hpsat_simplify_variable_map(xhead, &vmap))
		return (false);

	for (bm = TAILQ_FIRST(&vmap); bm; bm = bm->next()) {

		/* extract matching 2-SAT clauses */
		for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
			xn = xa->next();

			if (xa->isZero() || xa->isAtomic() == false)
				continue;

			pa = TAILQ_FIRST(&xa->head);
			if (pa == 0)
				continue;

			for (ba = pa->first(); ba; ba = ba->next()) {
				if (ba->compare(*bm, false))
					continue;
				for (bb = pa->first(); bb; bb = bb->next()) {
					if (bb == ba)
						continue;
					bb->dup()->insert_tail(sel + ba->isInverted());
				}
				delete xa->remove(xhead);
				break;
			}
		}

		(new BITMAP(*bm))->insert_tail(sel + 0).toggleInverted();
		(new BITMAP(*bm))->insert_tail(sel + 1);

		/* Try to figure out assignments */
		if (hpsat_simplify_assignments(xhead, sel, *bm, insert_all))
			changed = true;

		/* Simplify expressions */
		for (uint8_t i = 0; i != 2; i++) {
			do {
				hpsat_simplify_or(sel + i);
			} while (hpsat_sort_or(sel + i));
		}

		/* Output expressions, if any */
		for (ba = TAILQ_FIRST(sel + 0); ba; ba = ba->next()) {
			pn = new ANDMAP(true);
			ba->dup()->insert_tail(&pn->head);
			bm->dup()->insert_tail(&pn->head);

			changed |= ba->isOne();

			if (pn->sort().isZero())
				delete pn;
			else {
				xa = new XORMAP(false);
				pn->insert_tail(&xa->head);
				xa->insert_tail(xhead);
			}
		}

		/* Output expressions, if any */
		for (ba = TAILQ_FIRST(sel + 1); ba; ba = ba->next()) {
			pn = new ANDMAP(true);

			ba->dup()->insert_tail(&pn->head);
			bm->dup()->toggleInverted().insert_tail(&pn->head);

			changed |= ba->isOne();

			if (pn->sort().isZero())
				delete pn;
			else {
				xa = new XORMAP(false);
				pn->insert_tail(&xa->head);
				xa->insert_tail(xhead);
			}
		}

		/* Output cross-expressions */
		if (insert_all) {
			for (ba = TAILQ_FIRST(sel + 0); ba; ba = ba->next()) {
				for (bb = TAILQ_FIRST(sel + 1); bb; bb = bb->next()) {
					(new XORMAP(ANDMAP(*ba) & ANDMAP(*bb)))->insert_tail(xhead);
				}
			}
		}

		for (uint8_t x = 0; x != 2; x++)
			hpsat_free(sel + x);
	}
	if (insert_all)
		hpsat_sort_or(xhead);

	hpsat_free(&vmap);
	return (changed);
}

bool
hpsat_simplify_insert(XORMAP_HEAD_t *phead, XORMAP_HEAD_t *qhead)
{
	XORMAP *xa;
	XORMAP *xb;
	XORMAP *xn;

	ANDMAP *pa;
	ANDMAP *pb;
	ANDMAP *pn;

	XORMAP xored;
	ANDMAP sel;

	bool any = false;

	size_t count;

	for (xa = TAILQ_FIRST(phead); xa; xa = xa->next())
		xa->defactor();

	if (qhead != 0) {
		for (xa = TAILQ_FIRST(qhead); xa; xa = xa->next())
			xa->defactor();
	} else {
		qhead = phead;
	}

	for (xa = TAILQ_FIRST(phead); xa; xa = xa->next()) {
		pb = xa->last();
		if (pb == 0)
			continue;

		if (pb->isZero() || pb->isOne())
			continue;

		if (pb->isXorConst()) {
			sel = ANDMAP(pb->maxVar(), false);
			pb = &sel;
		}

		count = pb->count();

		for (xb = TAILQ_FIRST(qhead); xb; xb = xn) {

			xn = xb->next();
			if (xb == xa)
				continue;

			pa = xb->last();
			if (pa == 0 || pa->count() < count)
				continue;

			for (pa = xb->first(); pa; pa = pn) {
				pn = pa->next();

				if (pb->isXorConst() && pa->isXorConst()) {
					hpsat_var_t v = pb->maxVar();

					/* handle special case */
					if (pa->contains(v))
						(new ANDMAP(true))->insert_tail(&xored.head);
				} else {
					ANDMAP t[3] = { *pb, *pa, ANDMAP(true) };
					hpsat_simplify_split(t[0], t[1], t[2]);

					if (t[0].isOne())
						(new ANDMAP(t[1]))->insert_tail(&xored.head);
				}
			}

			if (xored.sort().isZero())
				continue;

			xored &= *xa;
			if (xored.defactor().isZero())
				continue;

			TAILQ_CONCAT(&xb->head, &xored.head, entry);

			if (xb->sort().isZero())
				delete xb->remove(qhead);
			else
				any = true;
		}
	}
	return (any);
}

bool
hpsat_simplify_redundant(XORMAP_HEAD_t *phead)
{
	XORMAP *xa;
	XORMAP *xn;
	bool any = false;

	for (xa = TAILQ_LAST(phead, XORMAP_HEAD); xa; xa = xn) {
		xn = xa->prev();

		BITMAP test;

		for (XORMAP *xb = TAILQ_FIRST(phead); xb; xb = xb->next()) {
			if (xb == xa)
				continue;
			test |= xb->toBitMap();
		}

		/* check if equation pointed to by "xa" is redundant */
		if ((test | xa->toBitMap()) == test) {
			delete xa->remove(phead);
			any = true;
		}
	}
	return (any);
}
