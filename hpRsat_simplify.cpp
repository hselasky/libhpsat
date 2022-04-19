/*-
 * Copyright (c) 2021-2022 Hans Petter Selasky. All rights reserved.
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

#include "hpRsat.h"

#include <stdlib.h>

static int
hprsat_compare_value(const void *a, const void *b)
{
	return ((MUL * const *)a)[0][0].compare(((MUL * const *)b)[0][0], true);
}

static ssize_t
hprsat_lookup_value(MUL **base, size_t num, MUL **key)
{
	MUL **result;

	result = (MUL **)
	    bsearch(key, base, num, sizeof(base[0]), &hprsat_compare_value);

	if (result == 0)
		return (-1);
	else
		return (result - base);
}

static bool
hprsat_simplify_subtract_sort(const ADD &src, ADD &dst, const MUL &dst_which)
{
	const MUL &src_which = *src.last();

	/* Try to divide. */
	MUL mul;

	if (dst_which.compare(src_which, true) == 0) {
		mul = MUL(1);
	} else {
		mul = MUL(dst_which).resetFactor() /
		      MUL(src_which).resetFactor();

		/* Check if division was not exact. */
		if ((mul * src_which).compare(dst_which, true) != 0)
			return (false);
	}

	/* Update factors. */
	mul.factor_lin = dst_which.factor_lin;

	for (MUL *pa = dst.first(); pa; pa = pa->next()) {
		pa->factor_lin *= src_which.factor_lin;
		pa->factor_lin %= hprsat_global_mod;
	}

	/* Do subtraction. */
	for (MUL *pa = src.first(); pa; pa = pa->next())
		(new MUL(*pa * mul))->negate().insert_tail(&dst.head);

	dst.sort();

	return (true);
}

static bool
hprsat_simplify_subtract_all(const ADD &src, ADD &dst)
{
	bool any = false;
top:
	/* Scan elements in reverse order. */
	for (MUL *pa = dst.last(); pa; pa = pa->prev()) {
		if (hprsat_simplify_subtract_sort(src, dst, *pa)) {
			any = true;
			goto top;
		}
	}
	return (any);
}

static ssize_t
hprsat_simplify_add_insert(ADD_HEAD_t *xhead, ADD_HEAD_t *pleft,
    MUL **phash, ADD **plast, size_t nhash, ADD *xa,
    ssize_t nindex, bool ignoreNonZero)
{
	ssize_t index;
	bool isNaN;
	hprsat_val_t value;
	MUL *pa;

 top:
	value = xa->getConst(isNaN);

	if (isNaN == false) {
		if (value == 0 || ignoreNonZero) {
			delete xa->remove(xhead);
			return (-1);	/* zero */
		} else if (value != 0) {
			return (-2);	/* no solution */
		}
	}

	pa = xa->last();
	assert(pa != 0);

	/* Try to lookup current value. */
	index = hprsat_lookup_value(phash, nhash, &pa);
	if (index != -1) {
		if (plast[index] != 0) {
			hprsat_simplify_subtract_sort(*plast[index], *xa, *pa);
			goto top;
		} else {
			plast[index] = xa;
			return (index);
		}
	} else if (nindex == -1) {
		xa->remove(xhead)->insert_tail(pleft);
		return (-1);	/* No free index */
	}

	/* The entry doesn't exist, so we need to re-hash. */
	*phash[nindex] = *pa;
	plast[nindex] = xa;

	/* Insertion sort - move down. */
	for (; nindex != 0; nindex--) {
		if (hprsat_compare_value(phash + nindex - 1, phash + nindex) > 0) {
			HPRSAT_SWAP(phash[nindex - 1], phash[nindex]);
			HPRSAT_SWAP(plast[nindex - 1], plast[nindex]);
		} else {
			break;
		}
	}

	/* Insertion sort - move up. */
	for (; nindex != (ssize_t)(nhash - 1); nindex++) {
		if (hprsat_compare_value(phash + nindex, phash + nindex + 1) > 0) {
			HPRSAT_SWAP(phash[nindex], phash[nindex + 1]);
			HPRSAT_SWAP(plast[nindex], plast[nindex + 1]);
		} else {
			break;
		}
	}
	return (nindex);
}

bool
hprsat_simplify_add(ADD_HEAD_t *xhead, bool ignoreNonZero)
{
	MUL_HEAD_t mulmap;
	ADD_HEAD_t leftover;

	MUL **phash;
	ADD **plast;
	size_t nhash;

	ADD *xa;
	ADD *xb;
	ADD *xn;

	MUL *pa;

	ssize_t index;

	size_t x;
	size_t y;
	size_t z;

	bool any;

	/* Collect all different kinds of expressions first. */
	TAILQ_INIT(&mulmap);
	TAILQ_INIT(&leftover);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		xa->sort();

		for (pa = xa->first(); pa; pa = pa->next())
			pa->dup()->insert_tail(&mulmap);
	}

	/* Build a hashmap. */
	nhash = 0;
	for (pa = TAILQ_FIRST(&mulmap); pa; pa = pa->next())
		nhash++;

	/* Check if there are no entries. */
	if (nhash == 0)
		return (false);

	phash = new MUL * [nhash];

	nhash = 0;
	for (pa = TAILQ_FIRST(&mulmap); pa; pa = pa->next())
		phash[nhash++] = pa;

	mergesort(phash, nhash, sizeof(phash[0]), &hprsat_compare_value);

	/* Remove equal entries. */
	for (x = z = 0; x != nhash; ) {
		for (y = x + 1; y != nhash; y++) {
			if (hprsat_compare_value(phash + x, phash + y))
				break;
			delete phash[y]->remove(&mulmap);
		}
		phash[z++] = phash[x];
		x = y;
	}
	nhash = z;

	plast = new ADD * [nhash];
	memset(plast, 0, sizeof(plast[0]) * nhash);

	/* Quickly remove duplicated statements. */
	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		if (hprsat_simplify_add_insert(xhead, &leftover, phash,
		    plast, nhash, xa, -1, ignoreNonZero) == -2)
			goto err_non_zero;
	}

	printf("# NHASH=%zd\n", nhash);

	any = false;

	/* Try to defactor statements. */
	for (x = 0; x != nhash; x++) {
repeat_0:
		xa = plast[x];
		if (xa == 0)
			continue;

		for (y = x + 1; y != nhash; y++) {
		repeat_1:
			xb = plast[y];
			if (xb == 0)
				continue;

			if (hprsat_simplify_subtract_all(*xa, *xb)) {

				/* Need to re-hash. */
				plast[y] = 0;

				index = hprsat_simplify_add_insert(xhead, &leftover, phash,
				    plast, nhash, xb, y, ignoreNonZero);

				switch (index) {
				case -1:
					break;
				case -2:
					goto err_non_zero;
				default:
					any = true;
					if (index >= (ssize_t)y) {
						goto repeat_1;
					} else {
						if (index < (ssize_t)x)
							x = index;
						goto repeat_0;
					}
				}
				break;
			}
		}
	}

	hprsat_free(&mulmap);

	delete [] plast;
	delete [] phash;

	if (TAILQ_FIRST(&leftover)) {
		TAILQ_CONCAT(xhead, &leftover, entry);
		return (true);
	}
#if 0
	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {

		/* Ignore all single variable statements. */
		if (xa->maxVar() == xa->minVar())
			continue;
		/* check if any bits must be constant */
		for (hprsat_var_t v = HPRSAT_VAR_MAX; (v = xa->maxVar(v)) != HPRSAT_VAR_MIN; ) {
			if (ADD(*xa).expand(v, false).isNonZeroConst()) {
				(new ADD(ADD(1, v) - ADD(1)))->insert_head(xhead);
				any = true;
			} else if (ADD(*xa).expand(v, true).isNonZeroConst()) {
				(new ADD(1, v))->insert_head(xhead);
				any = true;
			}
		}
	}
#endif
	return (any);

err_non_zero:
	hprsat_free(xhead);
	hprsat_free(&leftover);

	(new ADD(1))->insert_tail(xhead);

	hprsat_free(&mulmap);

	delete [] plast;
	delete [] phash;

	return (false);
}
