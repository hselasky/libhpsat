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

#define	MAXVAR 8

int main()
{
	size_t zc = 0;
	size_t zr = 0;
	XORMAP_HEAD_t eq[2 * MAXVAR];

	for (size_t x = 0; x != 2 * MAXVAR; x++)
		TAILQ_INIT(&eq[x]);

#if 1
	for (size_t x = 0; x != MAXVAR; x++) {
		for (size_t y = 0; y != MAXVAR; y++) {
			(new XORMAP(XORMAP(x,false) & XORMAP(y,false)))->insert_tail(&eq[x + y]);
		}
	}
#else
	for (size_t x = 0; x != MAXVAR; x++) {
		(new XORMAP(XORMAP(x,false)))->insert_tail(&eq[x]);
		(new XORMAP(XORMAP((x + MAXVAR - 1) % MAXVAR,false)))->insert_tail(&eq[x]);
	}
#endif

	bool any;
repeat:
	any = false;
	for (size_t x = 0; x != 2 * MAXVAR; x++) {
		XORMAP *xn;
		XORMAP *xm;

		for (XORMAP *xa = TAILQ_FIRST(&eq[x]); xa; xa = xn) {
			xn = xa->next();

			for (XORMAP *xb = xa->next(); xb; xb = xm) {
				xm = xb->next();

				any = true;

				XORMAP *xc = new XORMAP(*xa & *xb);
				if (xc->defactor().isZero()) {
					printf("ZERO CARRY\n");
					zc++;
					delete xc;
				} else {
					if (x + 1 >= 2 * MAXVAR)
						delete xc;
					else
						xc->insert_tail(&eq[x + 1]).defactor();
				}
				*xa ^= *xb;
				delete xb->remove(&eq[x]);
				if (xb == xn)
					xn = xm;
				if (xa->isZero()) {
					printf("ZERO REMAINDER\n");
					zr++;
					delete xa->remove(&eq[x]);
					goto next;
				}
			}
		}
	next:;
	}

	if (any)
		goto repeat;

	for (size_t x = 0; x != 2 * MAXVAR; x++) {
		XORMAP *xa = TAILQ_FIRST(&eq[x]);
		if (xa == 0)
			continue;
		if (((127*127) >> x) & 1)
			*xa ^= XORMAP(true);
	}

	printf("ZERO CARRY: %zd\n", zc);
	printf("ZERO REMAINDER: %zd\n", zr);

	for (size_t x = 0; x != 2 * MAXVAR; x++) {
		hpsat_find_ored(&eq[x]);
		hpsat_find_anded(&eq[x]);
	}

	for (size_t x = 0; x != 2 * MAXVAR; x++) {
		printf("VAR[%zd] = \n", x);
		for (XORMAP *xa = TAILQ_FIRST(&eq[x]); xa; xa = xa->next()) {
			xa->print(); printf(" || \n");
		}
		hpsat_free(&eq[x]);
	}
	return (0);
}
