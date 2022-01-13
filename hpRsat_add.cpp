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

#include <math.h>

#include "hpRsat.h"

hprsat_var_t
hprsat_maxvar(const ADD_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t max = HPRSAT_VAR_MIN;

	for (ADD *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hprsat_var_t v = pa->maxVar(limit);
		if (max < v)
			max = v;
	}
	return (max);
}

hprsat_var_t
hprsat_minvar(const ADD_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t min = HPRSAT_VAR_MIN;

	for (ADD *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hprsat_var_t v = pa->minVar(limit);
		if (v > HPRSAT_VAR_MIN) {
			if (min == HPRSAT_VAR_MIN || min > v)
				min = v;
		}
	}
	return (min);
}

void
hprsat_free(ADD_HEAD_t *phead)
{
	ADD *px;

	while ((px = TAILQ_FIRST(phead)))
		delete px->remove(phead);
}

ADD &
ADD :: sort()
{
	hprsat_sort_add(&head);
	return (*this);
}

static int
hprsat_compare(const void *a, const void *b)
{
	return ((ADD * const *)a)[0][0].compare(((ADD * const *)b)[0][0]);
}

bool
hprsat_sort_or(ADD_HEAD_t *phead)
{
	ADD *pa;
	ADD *pn;
	ADD **pp;
	size_t count = 0;
	bool did_sort;

	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		const double value = pa->sort().getConst();
		pn = pa->next();

		if (hprsat_is_nan(value) == false &&
		    value == 0) {
			delete pa->remove(phead);
		} else if (!pa->isVariable()) {
			pa->remove(phead);
			hprsat_free(phead);
			pa->insert_tail(phead);
			return (false);
		} else {
			count++;
		}
	}

	if (count == 0)
		return (false);
	pp = new ADD * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		pp[count++] = pa;

	did_sort = false;

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hprsat_compare);

	TAILQ_INIT(phead);

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) == 0) {
			did_sort = true;
			delete pp[x - 1];
		} else {
			pp[x - 1]->insert_tail(phead);
		}
	}
	pp[count - 1]->insert_tail(phead);

	delete [] pp;

	return (did_sort);
}

bool
hprsat_sort_mul(ADD_HEAD_t *phead, ADD &defactor, double &factor)
{
	ADD *pa;
	ADD *pn;
	ADD **pp;
	size_t count = 0;
	bool did_sort;

	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		const double value = pa->sort().getConst();
		pn = pa->next();

		if (hprsat_is_nan(value) == false) {
			factor *= value;
			delete pa->remove(phead);
		} else {
			count++;
		}
	}

	if (factor == 0.0) {
		hprsat_free(phead);
		return (false);
	}

	if (count == 0)
		return (false);
	pp = new ADD * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		pp[count++] = pa;

	did_sort = false;
	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hprsat_compare);

	TAILQ_INIT(phead);

	for (size_t x = 1; x != count; x++) {
		if (pp[x - 1]->compare(*pp[x]) == 0) {
			/* Accumulate power. */
			did_sort = true;
			defactor *= *pp[x - 1];

			delete pp[x - 1];
			delete pp[x];
			x++;
			if (x == count)
				break;
		} else {
			pp[x - 1]->insert_tail(phead);
		}
	}

	pp[count - 1]->insert_tail(phead);

	delete [] pp;

	return (did_sort);
}

ADD
ADD :: operator *(const ADD &other) const
{
	ADD temp;

	/* Standard multiplication. */
	for (MUL *pa = first(); pa; pa = pa->next()) {
		for (MUL *pb = other.first(); pb; pb = pb->next())
			(new MUL(pa[0] * pb[0]))->insert_tail(&temp.head);
	}
	return (temp);
}

double hprsat_nan;

static void __attribute__((__constructor__))
hprsat_nan_init(void)
{
	hprsat_nan = nan("");
}

bool
hprsat_is_nan(double value)
{
	return (isnan(value));
}

bool
hprsat_try_sqrt(double &value)
{
	if (isnan(value) || value < 0.0 ||
	    floor(value) != value)
		return (false);
	/* Check fast path first. */
	if (value == 0.0 || value == 1.0)
		return (true);
	double test = sqrt(value);
	if ((test * test) == value) {
		value = test;
		return (true);
	}
	return (false);
}

/* greatest common divisor, Euclid equation */
static double
hprsat_gcd(double a, double b)
{
	double an;
	double bn;

	if (a < 0.0)
		a = -a;
	if (b < 0.0)
		b = -b;

	while (b != 0.0) {
		an = b;
		bn = fmod(a, b);
		a = an;
		b = bn;
	}
	return (a);
}

ADD &
ADD :: doGCD()
{
	MUL *pa = first();
	if (pa == 0)
		return (*this);

	double factor = pa->factor_lin;

	while ((pa = pa->next()) != 0)
		factor = hprsat_gcd(factor, pa->factor_lin);

	if (factor != 0.0 && factor != 1.0) {
		for (pa = first(); pa; pa = pa->next()) {
			pa->factor_lin /= factor;
		}
	}
	return (*this);
}
