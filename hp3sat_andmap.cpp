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

void
hpsat_bitmap_to_andmap(const BITMAP_HEAD_t *bhead, ANDMAP_HEAD_t *ahead)
{
	for (BITMAP *pa = TAILQ_FIRST(bhead); pa; pa = pa->next()) {
		if (pa->isXorConst()) {
			(new ANDMAP(*pa))->insert_tail(ahead);
		} else {
			const size_t max = 1UL << pa->nvar;

			for (size_t x = 0; x != max; x++) {
				if (pa->peek(x) == 0)
					continue;
				ANDMAP *pb = new ANDMAP(true);
				for (size_t y = 0; y != pa->nvar; y++) {
					(new BITMAP(pa->pvar[y], ~(x >> y) & 1))->insert_tail(&pb->head);
				}
				pb->sort().insert_tail(ahead);
			}
		}
	}
}

hpsat_var_t
hpsat_maxvar(const ANDMAP_HEAD_t *phead, hpsat_var_t limit)
{
	hpsat_var_t max = HPSAT_VAR_MIN;

	for (ANDMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hpsat_var_t v = pa->maxVar(limit);
		if (max < v)
			max = v;
	}
	return (max);
}

hpsat_var_t
hpsat_minvar(const ANDMAP_HEAD_t *phead, hpsat_var_t limit)
{
	hpsat_var_t min = HPSAT_VAR_MIN;

	for (ANDMAP *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hpsat_var_t v = pa->minVar(limit);
		if (v > HPSAT_VAR_MIN) {
			if (min == HPSAT_VAR_MIN || min > v)
				min = v;
		}
	}
	return (min);
}

void
hpsat_free_andmaps(ANDMAP_HEAD_t *phead)
{
	ANDMAP *pa;

	while ((pa = TAILQ_FIRST(phead)))
		delete pa->remove(phead);
}

static int
hpsat_compare(const void *a, const void *b)
{
	return ((ANDMAP * const *)a)[0][0].compare(((ANDMAP * const *)b)[0][0]);
}

void
hpsat_merge(ANDMAP_HEAD_t *phead)
{
	ANDMAP *pa;
	ANDMAP *pb;
	ANDMAP *pn;
	bool found;
repeat:
	found = false;
	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		pn = pa->next();

		for (pb = TAILQ_FIRST(phead); pb; pb = pb->next()) {
			if (pa == pb)
				continue;
			if (pa->mergedTo(*pb)) {
				delete pa->remove(phead);
				found = true;
				break;
			}
		}
	}
	if (found)
		goto repeat;
}

bool
hpsat_sort_or(ANDMAP_HEAD_t *phead)
{
	ANDMAP *pa;
	ANDMAP **pp;
	size_t count = 0;
	bool did_sort = false;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		count++;

	if (count == 0)
		return (false);
	pp = new ANDMAP * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		pa->sort();

		if (pa->isOne()) {
			pa->remove(phead);
			hpsat_free_andmaps(phead);
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

bool
hpsat_sort_xor(ANDMAP_HEAD_t *phead)
{
	ANDMAP *pa;
	ANDMAP **pp;
	size_t count = 0;
	size_t x;
	size_t y;
	bool did_sort = false;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		count++;

	if (count == 0)
		return (false);

	pp = new ANDMAP * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		pa->sort();
		pp[count++] = pa;
	}

	for (x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hpsat_compare);

	TAILQ_INIT(phead);

	for (x = 0; x != count - 1; x++) {
		if (pp[x]->isXorConst() == false)
			break;
		if (pp[x + 1]->isXorConst() == false)
			break;
		/* accumulate all plain XORs */
		*pp[x + 1] ^= *pp[x];
		delete pp[x];
	}

	while (x != count) {
		for (y = x + 1; y != count; y++) {
			if (pp[x]->compare(*pp[y]))
				break;
		}
		/* XOR magic */
		if (pp[x]->isZero())
			delete pp[x];
		else if ((x ^ y) & 1)
			pp[x]->insert_tail(phead);
		else
			delete pp[x];

		while (++x != y)
			delete pp[x];
	}

	delete [] pp;

	return (did_sort);
}

ANDMAP &
ANDMAP :: xored(const ANDMAP &other, hpsat_var_t var)
{
	BITMAP *ba;
	BITMAP *bb;

	BITMAP *bm;
	BITMAP *bn;

	for (ba = first(); ba; ba = ba->next()) {
		if (ba->contains(var))
			break;
	}

	for (bb = other.first(); bb; bb = bb->next()) {
		if (bb->contains(var))
			break;
		else
			bb->dup()->insert_head(&head);
	}

	assert(ba != 0);
	assert(bb != 0);

	for (bm = ba->next(); bm; bm = bm->next()) {
		if (bm->contains(var))
			(*bm ^= *ba).toggleInverted();
	}

	for (bn = bb->next(); bn; bn = bn->next()) {
		if (bn->contains(var))
			(*bn ^ *bb).dup()->insert_head(&head).toggleInverted();
		else
			bn->dup()->insert_head(&head);
	}

	/* do the XOR */
	*ba ^= *bb;

	return (sort());
}

ANDMAP &
ANDMAP :: sort(void)
{
	do {
		hpsat_simplify_and(&head);
	} while (hpsat_sort_and(&head));
	return (*this);
}

bool
ANDMAP :: mergedTo(ANDMAP &other) const
{
	BITMAP *pa;
	BITMAP *pb;

	if ((*this & other) == *this)
		return (true);

	BITMAP *diffs[2];
	size_t ndiff = 0;

	pa = first();
	pb = other.first();

	while (pa && pb) {
		if (pa->compare(*pb, false))
			return (false);
		if (pa->isInverted() != pb->isInverted()) {
			if (ndiff == 2)
				return (false);
			diffs[ndiff++] = pb;
		}
		pa = pa->next();
		pb = pb->next();
	}

	if (pa != pb)
		return (false);

	switch (ndiff) {
	case 0:
		return (true);
	case 1:
		delete diffs[0]->remove(&other.head);
		return (true);
	case 2:
		*diffs[0] ^= *diffs[1];
		diffs[0]->toggleInverted();
		delete diffs[1]->remove(&other.head);
		other.sort();
		return (true);
	default:
		return (false);
	}
}

ANDMAP &
ANDMAP :: substitute(hpsat_var_t var, const BITMAP &expr)
{
	bool any = false;

#ifdef DEBUG
	assert(expr.contains(var));
#endif

	for (BITMAP *bm = first(); bm; bm = bm->next()) {
		if (bm->contains(var) == false)
			continue;
		assert(bm->isXorConst() == true);
		*bm ^= expr;
		any = true;
	}

	if (any)
		return (sort());
	else
		return (*this);
}

size_t
hpsat_numvar(const ANDMAP_HEAD_t *phead, hpsat_var_t v, BITMAP *presult)
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

ANDMAP &
ANDMAP :: xorify()
{
	BITMAP *ba;
	BITMAP *bn;

	for (ba = first(); ba; ba = bn) {
		bn = ba->next();

		if (ba->isXorConst())
			continue;

		const size_t max = 1UL << ba->nvar;
		size_t num = 0;
		size_t value = 0;

		for (size_t x = 0; x != max; x++) {
			if (ba->peek(x)) {
				num++;
				value = x;
			}
		}

		if (num != 1)
			continue;

		for (size_t y = 0; y != ba->nvar; y++)
			(new BITMAP(ba->pvar[y], ~(value >> y) & 1))->insert_head(&head);

		delete ba->remove(&head);
	}
	return (sort());
}
