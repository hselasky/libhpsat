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

hpsat_var_t
hpsat_maxvar(const XORMAP_HEAD_t *phead, hpsat_var_t limit)
{
	hpsat_var_t max = HPSAT_VAR_MIN;

	for (XORMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hpsat_var_t v = pa->maxVar(limit);
		if (max < v)
			max = v;
	}
	return (max);
}

hpsat_var_t
hpsat_minvar(const XORMAP_HEAD_t *phead, hpsat_var_t limit)
{
	hpsat_var_t min = HPSAT_VAR_MIN;

	for (XORMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hpsat_var_t v = pa->minVar(limit);
		if (v > HPSAT_VAR_MIN) {
			if (min == HPSAT_VAR_MIN || min > v)
				min = v;
		}
	}
	return (min);
}

void
hpsat_free(XORMAP_HEAD_t *phead)
{
	XORMAP *px;

	while ((px = TAILQ_FIRST(phead)))
		delete px->remove(phead);
}

XORMAP &
XORMAP :: sort(void)
{
	hpsat_sort_xor(&head);
	return (*this);
}

static int
hpsat_compare(const void *a, const void *b)
{
	return ((XORMAP * const *)a)[0][0].compare(((XORMAP * const *)b)[0][0]);
}

bool
hpsat_sort_or(XORMAP_HEAD_t *phead)
{
	XORMAP *pa;
	XORMAP **pp;
	size_t count = 0;
	bool did_sort = false;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		count++;

	if (count == 0)
		return (false);
	pp = new XORMAP * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		pa->sort();

		if (pa->isOne()) {
			pa->remove(phead);
			hpsat_free(phead);
			pa->insert_tail(phead);
			goto done;
		}
		pp[count++] = pa;
	}

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hpsat_compare);

	TAILQ_INIT(phead);

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->isZero() ||
		    pp[x - 1]->compare(*pp[x]) == 0) {
			delete pp[x - 1];
		} else {
			pp[x - 1]->insert_tail(phead);
		}
	}

	if (pp[count - 1]->isZero())
		delete pp[count - 1];
	else
		pp[count - 1]->insert_tail(phead);
done:
	delete [] pp;

	return (did_sort);
}

XORMAP &
XORMAP :: substitute(hpsat_var_t var, const BITMAP &expr)
{
	bool any = false;

#ifdef DEBUG
	assert(expr.contains(var));
#endif
	for (ANDMAP *pa = first(); pa; pa = pa->next()) {
		for (BITMAP *bm = pa->first(); bm; bm = bm->next()) {
			if (bm->contains(var) == false)
				continue;
			assert(bm->isXorConst() == true);
			*bm ^= expr;
			any = true;
		}
	}

	if (any)
		return (sort());
	else
		return (*this);
}

XORMAP &
XORMAP :: substitute(hpsat_var_t var, const XORMAP &expr)
{
#ifdef DEBUG
	assert(expr.contains(var));
#endif
	if (contains(var)) {
		BITMAP set(var, true);
		BITMAP clr(var, false);

		*this ^= (*this ^ XORMAP(*this).substitute(var, set)) & expr;

		substitute(var, clr);
	}
	return (*this);
}

XORMAP &
XORMAP :: xorify()
{
	for (ANDMAP *pa = first(); pa; pa = pa->next())
		pa->xorify();
	return (*this);
}

void
hpsat_bitmap_to_xormap(const BITMAP_HEAD_t *bhead, XORMAP_HEAD_t *xhead)
{
	for (BITMAP *pa = TAILQ_FIRST(bhead); pa; pa = pa->next())
		(new XORMAP(pa->toXorMap()))->insert_tail(xhead);
}

size_t
hpsat_numvar(const XORMAP_HEAD_t *phead, hpsat_var_t v, BITMAP *presult)
{
	size_t retval = 0;

	while (1) {
		const hpsat_var_t n = hpsat_maxvar(phead, v);
		if (n == HPSAT_VAR_MIN)
			break;
		if (presult)
			*presult ^= BITMAP(n, false);
		retval++;
		v = n;
	}
	return (retval);
}

hpsat_var_t
XORMAP :: findAndVar() const
{
	hpsat_var_t retval = HPSAT_VAR_MIN;

	for (ANDMAP *pa = first(); pa; pa = pa->next()) {
		if (pa->isXorConst())
			continue;
		retval = pa->maxVar();
		if (retval != HPSAT_VAR_MIN)
			break;
	}
	return (retval);
}

bool
XORMAP :: isXorAble(hpsat_var_t var) const
{
	XORMAP temp[2] = { *this, *this };

	temp[0].expand(var, false);
	temp[1].expand(var, true);

	temp[0] &= temp[1];

	if (hpsat_simplify_defactor(&temp[0].head))
		temp[0].sort();

	return (temp[0].isZero());
}

XORMAP &
XORMAP :: defactor()
{
	hpsat_simplify_defactor(&head);
	return (sort());
}

const XORMAP &
XORMAP :: xored(const XORMAP &other, hpsat_var_t var, XORMAP_HEAD_t *xhead) const
{
	XORMAP temp[4] = { *this, *this, other, other };
	XORMAP *xn;

	temp[0].expand(var, false);
	temp[1].expand(var, true);
	temp[2].expand(var, false);
	temp[3].expand(var, true);

	xn = new XORMAP(temp[0] & temp[3]);
	if (xn->defactor().isZero())
		delete xn;
	else
		xn->insert_tail(xhead);

	xn = new XORMAP(temp[1] & temp[2]);
	if (xn->defactor().isZero())
		delete xn;
	else
		xn->insert_tail(xhead);

	xn = new XORMAP(temp[0] & temp[1]);
	if (xn->defactor().isZero())
		delete xn;
	else
		xn->insert_tail(xhead);

	xn = new XORMAP(temp[2] & temp[3]);
	if (xn->defactor().isZero())
		delete xn;
	else
		xn->insert_tail(xhead);

	return (*this);
}

void
hpsat_merge(XORMAP_HEAD_t *phead)
{
	XORMAP *pa;
	XORMAP *pb;
	XORMAP *pn;
	XORMAP *pm;

	bool found;

repeat:
	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		pn = pa->next();
		for (pb = pn; pb; pb = pm) {
			pm = pb->next();

			XORMAP temp(*pa & *pb);
			if (temp == *pa) {
				delete pa->remove(phead);
				break;
			} else if (temp == *pb) {
				delete pb->remove(phead);
				if (pn == pb)
					pn = pm;
			}
		}
	}

	found = false;
	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		pn = pa->next();
		if (pa->first() == 0 || pa->first()->next() != 0)
			continue;
		for (pb = TAILQ_FIRST(phead); pb; pb = pb->next()) {
			if (pa == pb ||
			    pb->first() == 0 ||
			    pb->first()->next() != 0)
				continue;
			if (pa->first()->mergedTo(*pb->first())) {
				delete pa->remove(phead);
				found = true;
				break;
			}
		}
	}
	if (found)
		goto repeat;
}
