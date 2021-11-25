/*-
 * Copyright (c) 2023 Hans Petter Selasky. All rights reserved.
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

#include "hpMsat.h"

bool
hpMsat_solve(EQ_HEAD_t *phead, int8_t **ppvar, hpMsat_var_t &vmin)
{
	hpMsat_var_t vmax = hpMsat_max_var(phead) + 1;
	bool failure = false;
	int8_t *pvar;
	EQ **phash;
	EQ *pa;
	EQ *pb;

	if (vmin < 1)
		vmin = 1;
	if (vmax < vmin)
		vmax = vmin;

	vmin = vmax;

	phash = new EQ * [vmax];
	pvar = new int8_t [vmax];

	memset(phash, 0, sizeof(phash[0]) * vmax);
	memset(pvar, 0, sizeof(pvar[0]) * vmax);
	pvar[0] = 1;

	/* quickly dispatch EQ solving via a hash table */

	while ((pa = TAILQ_FIRST(phead))) {
		pa->remove(phead)->gcd();

		for (;;) {
			hpMsat_var_t v = pa->maxVar();
			if (v == 0) {
				failure |= (pa->isZero() == false);
				delete pa;
				break;
			}
			if (phash[v] == 0) {
				phash[v] = pa;
				break;
			} else {
				EQ temp;
				hpMsat_eq_add(*phash[v], *pa, temp,
				    - pa->pvar[pa->index_of_var(v)].val,
				    phash[v]->pvar[phash[v]->index_of_var(v)].val);
				*pa = temp.gcd();
			}
		}
	}

	for (hpMsat_var_t i = 1; i != vmax; i++) {
		pa = phash[i];
		if (pa == 0)
			continue;

		for (size_t j = 0; j != pa->nvar; ) {
			hpMsat_var_t v = pa->pvar[j].var;
			if (v == 0 || v == i || phash[v] == 0) {
				j++;
				continue;
			}

			EQ temp;
			hpMsat_eq_add(*phash[v], *pa, temp,
			    pa->pvar[pa->index_of_var(v)].val,
			    - phash[v]->pvar[phash[v]->index_of_var(v)].val);
			*pa = temp.gcd();
			j = pa->index_of_var(v + 1, false);
		}
	}

	for (hpMsat_var_t i = 1; i != vmax; i++) {
		pa = phash[i];
		if (pa == 0)
			continue;

		for (pvar[i] = -1; pvar[i] != 2; pvar[i]++) {
			if (pa->expand_all(pvar) == 0)
				break;
		}
		pa->insert_tail(phead);
	}

	delete [] phash;

	if (failure == false && ppvar != 0)
		*ppvar = pvar;
	else
		delete [] pvar;

	return (failure);
}
