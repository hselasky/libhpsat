/*-
 * Copyright (c) 2022 Hans Petter Selasky. All rights reserved.
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

#include <stdlib.h>

#include "hpRsat.h"

class hprsat_range {
public:
	hprsat_range() {
		step = 0;
		combo = 0;
		num_pos = 0;
		num_neg = 0;
		num_res = 0;
		variable = 0;
	};
	hprsat_val_t step;
	hprsat_val_t combo;
	hprsat_val_t num_pos;
	hprsat_val_t num_neg;
	hprsat_val_t num_res;
	bool variable;
};

static int
hprsat_compare_range(const void *_a, const void *_b)
{
	const hprsat_range *a = *(const hprsat_range * const *)_a;
	const hprsat_range *b = *(const hprsat_range * const *)_b;

	hprsat_val_t va = a->num_pos + a->num_neg;
	hprsat_val_t vb = b->num_pos + b->num_neg;

	if (va > vb)
		return (1);
	else if (va < vb)
		return (-1);
	else
		return (0);
}

static void
hprsat_clip(hprsat_val_t &value)
{
	if (value > hprsat_global_modulus)
		value = hprsat_global_modulus;
}

/*
 * Returns true if ranges were merged.
 */
static bool
hprsat_merge_range(const hprsat_range &from, hprsat_range &to)
{
	hprsat_val_t d;

	/* d = (from.step / to.step) */
	hprsat_do_global_inverse(to.step, d);
	d *= from.step;
	hprsat_do_global_modulus(d);

	/* Check for intersection. */
	if (to.num_pos >= d || to.num_neg >= d) {
		/* Extend range. */
		to.num_pos += from.num_pos * d;
		to.num_neg += from.num_neg * d;
		hprsat_clip(to.num_pos);
		hprsat_clip(to.num_neg);
		return (true);
	}

	/* d = -d */
	d = hprsat_global_modulus - d;

	/* Check for intersection. */
	if (to.num_pos >= d || to.num_neg >= d) {
		/* Extend range. */
		to.num_pos += from.num_neg * d;
		to.num_neg += from.num_pos * d;
		hprsat_clip(to.num_pos);
		hprsat_clip(to.num_neg);
		return (true);
	}
	return (false);
}

/*
 * Returns true if ranges were merged.
 */
static bool
hprsat_within_range(const hprsat_val_t step, hprsat_range &range)
{
	hprsat_val_t d;

	/* d = (from.step / to.step) */
	hprsat_do_global_inverse(range.step, d);
	d *= step;
	hprsat_do_global_modulus(d);

	/* Store combination. */
	range.combo = d;

	return (d <= range.num_pos || (hprsat_global_modulus - d) <= range.num_neg);
}

static bool
hprsat_intersects(const MUL &mul, hprsat_range &to)
{
	hprsat_range from;

	from.step = mul.factor_lin;

	return (hprsat_merge_range(from, to));
}

static bool
hprsat_elevate_add(ADD *xa, ADD_HEAD_t *phead, size_t searchLimit)
{
	hprsat_range *prange;
	hprsat_range **pprange;
	hprsat_val_t bias = 0;
	size_t count = 0;
	size_t x,y;
	bool ret;

	MUL *pa;
	MUL *pn;

	ADD *xn;

	for (pa = xa->first(); pa; pa = pa->next()) {
		/* No square roots are supported. */
		if (pa->factor_sqrt != 1 || pa->afirst() != 0)
			return (false);
		/* Skip constants. */
		if (pa->vfirst() == 0 || pa->factor_lin == 0)
			continue;
		count++;
	}

	/* No variables, nothing to do! */
	if (count == 0)
		return (false);

	/* Reserve one extra element for a constant, if any. */
	prange = new hprsat_range [count];
	pprange = new hprsat_range * [count];

	count = 0;

	for (pa = xa->first(); pa; pa = pa->next()) {
		/* Accumulate constants. */
		if (pa->vfirst() == 0 || pa->factor_lin == 0) {
			bias += pa->factor_lin;
			continue;
		}

		/* Setup range element. */
		prange[count].step = pa->factor_lin;
		prange[count].num_pos = 1;
		pprange[count] = prange + count;
		count++;
	}

	/* Try to merge the range elements. */
	while (1) {
		bool found = false;

		for (x = 0; x != count; x++) {
			if (pprange[x] == 0)
				continue;
			for (y = 0; y != count; y++) {
				if (x == y || pprange[y] == 0)
					continue;
				if (hprsat_merge_range(*pprange[y], *pprange[x])) {
					pprange[y] = 0;
					found = true;
				}
			}
		}
		if (found == false)
			break;
	}

	/* Remove all unused entries. */
	for (x = y = 0; x != count; x++) {
		if (pprange[x] == 0)
			continue;
		pprange[y++] = pprange[x];
	}
	count = y;

	/* If there is only one range, nothing to do. */
	if (count == 1) {
		delete [] prange;
		delete [] pprange;
		return (false);
	}

	/* Sort the elements by range to optimize searching. */
	mergesort(pprange, count, sizeof(pprange[0]), &hprsat_compare_range);

	/*
	 * Reset combination and compute number
	 * of total combinations.
	 */
	hprsat_val_t ncombo = 1;

	for (x = 0; x != count - 1; x++) {
		hprsat_range &r = *pprange[x];
		r.combo = -r.num_neg;
		ncombo *= r.num_pos + r.num_neg + 1;
	}

	/* Check if searching is out of bounds. */
	if (ncombo > searchLimit) {
		delete [] prange;
		delete [] pprange;
		return (false);
	}

	size_t nsol = 0;

	while (true) {
		hprsat_val_t sum = 0;

		/* Sum up what we've got. */
		for (x = 0; x != count - 1; x++) {
			hprsat_range &r = *pprange[x];
			sum += r.combo * r.step;
		}

		sum = - sum - bias;

		/* Check if we found a valid combination. */
		if (hprsat_within_range(sum, *pprange[count - 1])) {
			if (nsol++ == 0) {
				for (x = 0; x != count; x++) {
					hprsat_range &r = *pprange[x];
					r.num_res = r.combo;
				}
			} else {
				for (x = 0; x != count; x++) {
					hprsat_range &r = *pprange[x];
					r.variable |= (r.num_res != r.combo);
				}
			}
		}

		/* Increment the state. */
		for (x = 0; x != count - 1; x++) {
			hprsat_range &r = *pprange[x];
			if (r.combo == r.num_pos) {
				r.combo = -r.num_neg;
			} else {
				r.combo++;
				break;
			}
		}
		if (x == count - 1)
			break;
	}

	if (nsol == 0) {
		std::cout << "# NO SOLUTIONS\n";
		*xa = ADD(1);
		delete [] prange;
		delete [] pprange;
		return (true);
	}

	ret = false;

	/* Split the equations. */
	for (x = 0; x != count; x++) {
		hprsat_range &r = *pprange[x];

		if (r.variable)
			continue;

		ret = true;
		xn = new ADD();
		for (pa = xa->first(); pa; pa = pn) {
			pn = pa->next();
			/* Skip constants. */
			if (pa->vfirst() == 0 || pa->factor_lin == 0)
				  continue;
			/* Check if element belongs to the given range. */
			if (hprsat_intersects(*pa, r))
				pa->remove(&xa->head)->insert_tail(&xn->head);
		}

		/* Add "bias", if any. */
		if (r.num_res != 0) {
			bias = - r.num_res * r.step;
			hprsat_do_global_modulus(bias);
			(new MUL(bias))->insert_head(&xn->head);
			/* Subract bias from remaining equation. */
			(new MUL(bias))->negate().insert_head(&xa->head);
		}
		xn->sort().insert_tail(phead);
	}

	if (xa->sort().first() == 0)
		delete xa->remove(phead);

	delete [] prange;
	delete [] pprange;
	return (ret);
}

bool
hprsat_elevate_add(ADD_HEAD_t *phead, size_t searchLimit)
{
	ADD_HEAD_t output;
	ADD *xa;
	ADD *xn;
	bool any = false;

	TAILQ_INIT(&output);

	for (xa = TAILQ_FIRST(phead); xa; xa = xn) {
		xn = xa->next();
		xa->remove(phead)->insert_tail(&output);
		any |= hprsat_elevate_add(xa, &output, searchLimit);
	}

	TAILQ_CONCAT(phead, &output, entry);

	return (any);
}
