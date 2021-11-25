/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
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
hpsat_demux(BITMAP_HEAD_t *phead, hpsat_var_t *pvar)
{
	BITMAP *output[2] = {};
	BITMAP *pa;
	BITMAP *pn;
	hpsat_var_t v;
	uint8_t n;
	bool any;
	BITMAP_HEAD_t added;
	TAILQ_INIT(&added);
top:
	any = false;

	hpsat_sort_or(phead);

	for (pa = TAILQ_FIRST(phead); pa; pa = pn) {
		pn = pa->next();

		if (pa->isZero()) {
			delete pa->remove(phead);
			continue;
		} else if (pa->isXorConst())
			continue;

		v = pa->findAndVar();
		if (v == -1)
			continue;

		any = true;

		n = 0;

		BITMAP one(*pa);

		if (one.expand(v, 1).sort().isZero()) {
			/* do nothing */
		} else if (one.isOne()) {
			output[n++] = new BITMAP(v, false);
		} else {
			const size_t w = (*pvar)++;

			one ^= BITMAP(w, false);

                        (new BITMAP(BITMAP(w, false) & BITMAP(v, false)))->insert_tail(&added);
                        output[n++] = new BITMAP(one);
		}

                BITMAP zero(*pa);

                if (zero.expand(v, 0).sort().isZero()) {
                        /* do nothing */
                } else if (zero.isOne()) {
			output[n++] = new BITMAP(v, true);
                } else {
			const size_t w = (*pvar)++;

                        zero ^= BITMAP(w, false);

                        (new BITMAP(BITMAP(w, false) & BITMAP(v, true)))->insert_tail(&added);
                        output[n++] = new BITMAP(zero);
                }

                assert(n != 0);
		assert(n <= 2);

                delete pa->remove(phead);

                if (pn == 0)
                        pn = output[0];

                for (uint8_t y = 0; y != n; y++)
			output[y]->sort().insert_tail(phead);
	}

	if (any)
		goto top;

	TAILQ_CONCAT(phead, &added, entry);

	do {
		hpsat_simplify_or(phead);
	} while (hpsat_squash_or(phead));
}

void
hpsat_demux(XORMAP_HEAD_t *xhead, hpsat_var_t *pvar)
{
	XORMAP *output[2] = {};
	XORMAP *xa;
	XORMAP *xn;
	hpsat_var_t v;
	uint8_t n;
	bool any;
	XORMAP_HEAD_t added;
	TAILQ_INIT(&added);
top:
	any = false;

	hpsat_sort_or(xhead);

	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();

		if (xa->isZero()) {
			delete xa->remove(xhead);
			continue;
		} else if (xa->isXorConst())
			continue;

		v = xa->findAndVar();
		if (v == HPSAT_VAR_MIN)
			continue;

		any = true;

		n = 0;

		XORMAP one(*xa);

		if (one.expand(v, 1).isZero()) {
			/* do nothing */
		} else if (one.isOne()) {
			output[n++] = new XORMAP(BITMAP(v, false));
		} else {
			const size_t w = (*pvar)++;

			one ^= XORMAP(BITMAP(w, false));

                        (new XORMAP(ANDMAP(w, false) & ANDMAP(v, false)))->insert_tail(&added);
                        output[n++] = new XORMAP(one);
		}

                XORMAP zero(*xa);

                if (zero.expand(v, 0).isZero()) {
                        /* do nothing */
                } else if (zero.isOne()) {
			output[n++] = new XORMAP(BITMAP(v, true));
                } else {
			const size_t w = (*pvar)++;

                        zero ^= XORMAP(BITMAP(w, false));

                        (new XORMAP(ANDMAP(w, false) & ANDMAP(v, true)))->insert_tail(&added);
                        output[n++] = new XORMAP(zero);
                }

                assert(n != 0);
		assert(n <= 2);

                delete xa->remove(xhead);

                if (xn == 0)
                        xn = output[0];

                for (uint8_t y = 0; y != n; y++)
			output[y]->sort().insert_tail(xhead);
	}

	if (any)
		goto top;

	TAILQ_CONCAT(xhead, &added, entry);

	hpsat_sort_or(xhead);
}
