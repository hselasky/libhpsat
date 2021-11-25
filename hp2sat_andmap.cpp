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

#include "hp2sat.h"

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

size_t
hpsat_numvar(const ANDMAP_HEAD_t *phead, hpsat_var_t v)
{
	size_t retval = 0;

	while (1) {
		const hpsat_var_t n = hpsat_maxvar(phead, v);
		if (n == HPSAT_VAR_MIN)
			break;
		retval++;
		v = n;
	}
	return (retval);
}

void
hpsat_free(ANDMAP_HEAD_t *phead)
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
