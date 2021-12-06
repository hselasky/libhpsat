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

/* padd[..] = 0 */

void
hpsat_build_single_adder_logic(const XORMAP_HEAD_t *padd, size_t num, XORMAP_HEAD_t *phead)
{
	XORMAP *xa;
	XORMAP *xb;

	XORMAP last;

	for (size_t x = 0; x != num; x++) {
		for (xa = TAILQ_FIRST(&padd[x]); xa; xa = xa->next()) {
			XORMAP temp(*xa);
			TAILQ_CONCAT(&last.head, &temp.head, entry);
		}
		last.sort();
		if (last.isZero() == false)
			(new XORMAP(last))->insert_tail(phead);
		last = XORMAP();

		for (xa = TAILQ_FIRST(&padd[x]); xa; xa = xa->next()) {
			for (xb = xa->next(); xb; xb = xb->next()) {
				XORMAP temp(*xa & *xb);
				TAILQ_CONCAT(&last.head, &temp.head, entry);
			}
		}

		for (xa = TAILQ_FIRST(&padd[x]); xa; xa = xa->next()) {
			XORMAP temp(*xa);
			TAILQ_CONCAT(&last.head, &temp.head, entry);
		}
		last.sort();
	}
}

void
hpsat_build_multi_adder_logic(XORMAP_HEAD_t *padd, size_t num)
{
	XORMAP *xa;
	XORMAP *xb;
	XORMAP *xn;
	XORMAP *xm;

	for (size_t x = 0; x != num; x++) {
		for (xa = TAILQ_FIRST(&padd[x]); xa; xa = xn) {
			xn = xa->next();

			size_t count = 1;

			for (xb = xa->next(); xb; xb = xm) {
				xm = xb->next();

				if (*xa == *xb) {
					if (xn == xb)
						xn = xm;
					count++;
					delete xb->remove(&padd[x]);
				}
			}

			for (size_t y = x; y != num; y++) {
				if (count & 1)
					xa->dup()->insert_head(&padd[y]);
				count /= 2;
			}
			delete xa->remove(&padd[x]);
		}

		while ((xa = TAILQ_FIRST(&padd[x])) != 0) {
			xb = xa->next();
			if (xb == 0) {
				if (xa->isZero())
					delete xa->remove(&padd[x]);
				break;
			}
			xn = new XORMAP(*xa & *xb);

			if (xn->isZero() || x + 1 == num)
				delete xn;
			else
				xn->insert_tail(&padd[x + 1]);

			TAILQ_CONCAT(&xa->head, &xb->head, entry);
			xa->sort();
			delete xb->remove(&padd[x]);
		}
	}
}
