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
	return ((MUL * const *)a)[0][0].compare(((MUL * const *)b)[0][0]);
}

static int
hprsat_compare_value_no_factor(const void *a, const void *b)
{
	return ((MUL * const *)a)[0][0].compare(((MUL * const *)b)[0][0], false);
}

static ssize_t
hprsat_lookup_value_no_factor(MUL **base, size_t num, MUL **key)
{
	MUL **result;

	result = (MUL **)
	    bsearch(key, base, num, sizeof(base[0]), &hprsat_compare_value_no_factor);

	if (result == 0)
		return (-1);
	else {
		/* Make sure we return the first occurrence. */
		while (result != base &&
		       hprsat_compare_value_no_factor(result, result - 1) == 0)
			result--;
		return (result - base);
	}
}

static bool
hprsat_simplify_subtract_gcd(const ADD &src, ADD &dst, const MUL &which)
{
	/* Try to divide. */
	MUL mul(which / *src.last());

	/* Check if division was exact. */
	if ((mul * *src.last()) != which)
		return (false);

	/* Do subtraction. */
	for (MUL *pa = src.first(); pa; pa = pa->next())
		(new MUL(*pa * mul))->negate().insert_tail(&dst.head);
	dst.sort().doGCD();
	return (true);
}

static ssize_t
hprsat_simplify_add_insert(ADD_HEAD_t *xhead, ADD_HEAD_t *pleft,
    MUL **phash, ADD **plast, size_t nhash, ADD *xa,
    ssize_t newindex, bool ignoreNonZero)
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

	/* Top-most value should be positive. */
	if (pa->isNegative())
		xa->negate();

	index = hprsat_lookup_value_no_factor(phash, nhash, &pa);

	/* Try GCD first. */
	if (index != -1) {
		for (ssize_t match = index; match != nhash; match++) {
			if (match != index &&
			    hprsat_compare_value_no_factor(phash + index, phash + match) != 0)
				break;

			if (plast[match] == 0) {
				continue;
			} else if (hprsat_simplify_subtract_gcd(*plast[match], *xa, *pa)) {
				goto top;
			} else if (hprsat_simplify_subtract_gcd(*xa, *plast[match], *phash[match])) {
				HPRSAT_SWAP(plast[match], xa);
				goto top;
			}
		}
		if (newindex == -1) {
			/* Find a free slot, if any. */
			for (ssize_t match = index; match != nhash; match++) {
				if (match != index &&
				    hprsat_compare_value_no_factor(phash + index, phash + match) != 0)
					break;
				if (plast[match] == 0) {
					newindex = match;
					break;
				}
			}
		}
	}

	index = newindex;
	if (index == -1) {
		xa->remove(xhead)->insert_tail(pleft);
		return (-1);	/* No free index */
	}

	/* the entry doesn't exist, so we need to re-hash */
	*phash[index] = *pa;
	plast[index] = xa;

	/* insertion sort - move down */
	for (; index != 0; index--) {
		if (hprsat_compare_value(phash + index - 1, phash + index) > 0) {
			HPRSAT_SWAP(phash[index - 1], phash[index]);
			HPRSAT_SWAP(plast[index - 1], plast[index]);
		} else {
			break;
		}
	}

	/* insertion sort - move up */
	for (; index != nhash - 1; index++) {
		if (hprsat_compare_value(phash + index, phash + index + 1) > 0) {
			HPRSAT_SWAP(phash[index], phash[index + 1]);
			HPRSAT_SWAP(plast[index], plast[index + 1]);
		} else {
			break;
		}
	}
	return (index);
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
	MUL *pb;

	ssize_t index;

	size_t x;
	size_t y;
	size_t z;

	bool any;

	/* Collect all different kinds of expressions first. */
	TAILQ_INIT(&mulmap);
	TAILQ_INIT(&leftover);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		xa->sort().doGCD();

		for (pa = xa->first(); pa; pa = pa->next()) {
			if (pa->isNegative())
				pa->dup()->negate().insert_tail(&mulmap);
			else
				pa->dup()->insert_tail(&mulmap);
		}
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

		switch (hprsat_simplify_add_insert(xhead, &leftover, phash,
		    plast, nhash, xa, -1, ignoreNonZero)) {
		case -1:
			break;
		case -2:
			goto err_non_zero;
		default:
			break;
		}
	}

	printf("# NHASH=%zd\n", nhash);

	any = false;

	/* Try to defactor statements. */
	for (x = 0; x != nhash; x++) {
repeat_0:
		xa = plast[x];
		if (xa == 0)
			continue;

		index = hprsat_lookup_value_no_factor(phash, nhash, phash + x);
		if (index < 0)
			continue;

		for (y = index; y != nhash; y++) {
			if (y == x)
				continue;
		repeat_1:
			xb = plast[y];
			if (xb == 0)
				continue;

			/* Scan elements in reverse order. */
			for (pb = xb->last(); pb; pb = pb->prev()) {

				if (hprsat_simplify_subtract_gcd(*xa, *xb, *pb) == false)
					continue;

				/* Need to re-hash. */
				plast[y] = 0;

				index = hprsat_simplify_add_insert(xhead, &leftover, phash,
				    plast, nhash, xb, y, ignoreNonZero);
				switch (index) {
				case -1:
					break;
				case -2:
					if (ignoreNonZero)
						break;
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
