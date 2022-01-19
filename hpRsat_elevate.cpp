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

class hprsat_level {
public:
	hprsat_level() {
		value = 0;
		num_rem = 0;
		num_pos = 0;
		num_neg = 0;
	};
	hprsat_val_t value;
	hprsat_val_t num_rem;
	ssize_t num_pos;
	ssize_t num_neg;
};

static int
hprsat_compare_value(const void *_a, const void *_b)
{
	const hprsat_level *a = *(const hprsat_level * const *)_a;
	const hprsat_level *b = *(const hprsat_level * const *)_b;

	if (a->value > b->value)
		return (1);
	else if (a->value < b->value)
		return (-1);
	else
		return (0);
}

static void
hprsat_elevate_add(ADD *xa, ADD_HEAD_t *phead, bool ignoreNonZero, bool &any)
{
	hprsat_level *plevel;
	hprsat_level **pplevel;
	hprsat_val_t bias = 0;
	hprsat_val_t total_min = 0;
	hprsat_val_t total_max = 0;
	size_t count = 0;
	size_t x,y,z;

	MUL *pa;
	MUL *pn;

	ADD *xn;

	for (pa = xa->first(); pa; pa = pa->next()) {
		/* No square roots are supported. */
		if (pa->factor_sqrt != 1 || pa->afirst() != 0)
			return;
		count++;
	}

	if (count == 0)
		return;

	plevel = new hprsat_level [count];
	pplevel = new hprsat_level * [count];

	count = 0;

	for (pa = xa->first(); pa; pa = pa->next()) {
		if (pa->vfirst() == 0 || pa->factor_lin == 0) {
			bias += pa->factor_lin;
		} else if (pa->factor_lin < 0) {
			total_min += pa->factor_lin;
			plevel[count].value = - pa->factor_lin;
			plevel[count].num_neg = -1;
			pplevel[count] = plevel + count;
			count++;
		} else {
			total_max += pa->factor_lin;
			plevel[count].value = pa->factor_lin;
			plevel[count].num_pos = 1;
			pplevel[count] = plevel + count;
			count++;
		}
	}

	/* Check for out of range. */
	if ((total_min + bias) > 0 || (total_max + bias) < 0) {
		delete [] plevel;
		delete [] pplevel;
		*xa = ADD(1);		/* no solution - out of range */
		return;
	}

	mergesort(pplevel, count, sizeof(pplevel[0]), &hprsat_compare_value);

	/* Accumulate equal entries. */
	for (x = z = 0; x != count; ) {
		for (y = x + 1; y != count; y++) {
			if (hprsat_compare_value(pplevel + x, pplevel + y))
				break;
			pplevel[x]->num_neg += pplevel[y]->num_neg;
			pplevel[x]->num_pos += pplevel[y]->num_pos;
		}
		pplevel[z++] = pplevel[x];
		x = y;
	}
	count = z;

	/* Check if part of equation can be elevated. */
	for (x = 0; x != count; x++) {
		hprsat_level &cur = *pplevel[x];
		hprsat_val_t cur_min = cur.value * cur.num_neg;
		hprsat_val_t cur_max = cur.value * cur.num_pos;

		total_min -= cur_min;
		total_max -= cur_max;

		/* Range check. */
		if ((total_min == 0 ||
		     total_min > -cur.value) &&
		    (total_max == 0 ||
		     total_max < cur.value)) {
			/* Don't remove the last one. */
			if (total_min == 0 &&
			    total_max == 0)
				break;

			xn = new ADD();
			for (pa = xa->first(); pa; pa = pn) {
				pn = pa->next();
				if (pa->vfirst() == 0 || pa->factor_lin == 0) {
					pa->factor_lin %= cur.value;
				} else if (pa->factor_lin == cur.value ||
					   pa->factor_lin == -cur.value) {
					pa->remove(&xa->head)->insert_tail(&xn->head);
				}
			}
			(new MUL(bias - (bias % cur.value)))->insert_head(&xn->head);

			bias %= cur.value;

			xn->sort().insert_tail(phead);
			any = true;
		} else {
			total_min += cur_min;
			total_max += cur_max;
		}
	}

	delete [] plevel;
	delete [] pplevel;
}

bool
hprsat_elevate_add(ADD_HEAD_t *phead, bool ignoreNonZero)
{
	ADD_HEAD_t output;
	ADD *xa;
	bool any = false;

	TAILQ_INIT(&output);

	for (xa = TAILQ_FIRST(phead); xa; xa = xa->next()) {
		hprsat_elevate_add(xa, &output, ignoreNonZero, any);
	}

	TAILQ_CONCAT(phead, &output, entry);

	return (any);
}
