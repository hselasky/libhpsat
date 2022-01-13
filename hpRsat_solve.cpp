/*-
 * Copyright (c) 2020-2022 Hans Petter Selasky. All rights reserved.
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

bool
hprsat_solve(ADD_HEAD_t *xhead, ADD_HEAD_t *pderiv, hprsat_var_t *pvmax)
{
	const hprsat_var_t vm = *pvmax;

	ADD *xa;
	ADD *xb;
	ADD *xn;

	for (xa = TAILQ_FIRST(xhead); xa != 0; xa = xa->next())
		xa->doGCD();

	for (hprsat_var_t v = 0; v != vm; v++) {

		printf("# PROGRESS v%zd of %zd\n", v, vm);

		ADD_HEAD_t ahead;
		ADD_HEAD_t bhead[2];
		ADD_HEAD_t thead;

		TAILQ_INIT(&ahead);
		TAILQ_INIT(&bhead[0]);
		TAILQ_INIT(&bhead[1]);
		TAILQ_INIT(&thead);

		for (xa = TAILQ_FIRST(xhead); xa != 0; xa = xn) {
			xn = xa->next();
			if (xa->contains(v)) {
				/* Expand variable. */
				(new ADD(*xa))->insert_tail(&bhead[0]).expand(v, false).sort();
				(new ADD(*xa))->insert_tail(&bhead[1]).expand(v, true).sort();
				xa->remove(xhead)->insert_tail(&ahead);
			}
		}

		hprsat_sort_or(&bhead[0]);
		hprsat_sort_or(&bhead[1]);

		/* Compute the variable conflict. */
		for (xa = TAILQ_FIRST(&bhead[0]); xa != 0; xa = xa->next()) {
			for (xb = TAILQ_FIRST(&bhead[1]); xb != 0; xb = xb->next()) {
				xn = new ADD(xa[0] * xb[0]);
				const double test = xn->doGCD().getConst();
				if (hprsat_is_nan(test) == false && test == 0.0)
					delete xn;
				else
					xn->insert_tail(&thead);
			}
		}

		hprsat_free(&bhead[0]);
		hprsat_free(&bhead[1]);

		if (TAILQ_FIRST(&thead)) {
			TAILQ_CONCAT(xhead, &thead, entry);
			hprsat_sort_or(xhead);
		}

		(new ADD(0.0))->insert_tail(&ahead);
		(new ADD(1.0,v))->insert_head(&ahead);

		TAILQ_CONCAT(&ahead, pderiv, entry);
		TAILQ_CONCAT(pderiv, &ahead, entry);
	}
	return (TAILQ_FIRST(xhead) != 0);
}

bool
hprsat_solve_callback(ADD *xm, uint8_t *psol, hprsat_solve_callback_t *cb, void *arg)
{
	ADD *xa;

	if (xm == NULL)
		return (cb(arg, psol));

	const hprsat_var_t v = xm->first()->vfirst()->var;

	for (psol[v] = 0; psol[v] != 2; psol[v]++) {
		for (xa = xm->next(); xa->first(); xa = xa->next()) {
			const double test = ADD(*xa).expand_all(psol).getConst();
			if (hprsat_is_nan(test) || test != 0.0)
				goto next;
		}
		if (hprsat_solve_callback(xa->next(), psol, cb, arg) == false)
			return (false);
	next:;
	}
	return (true);
}

static bool
hprsat_solve_do_count(void *arg, uint8_t *)
{
	(*(size_t *)arg)++;
	return (true);
}

size_t
hprsat_solve_count(ADD_HEAD_t *xhead, uint8_t *psol)
{
	size_t retval = 0;
	hprsat_solve_callback(TAILQ_FIRST(xhead), psol, hprsat_solve_do_count, &retval);
	return (retval);
}

bool
hprsat_solve_first(ADD_HEAD_t *xhead, uint8_t *psol, bool useProbability)
{
	bool retval = true;

	/* compute the derivated XORs in the right order */
	for (ADD *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		double score[2] = {};
		const hprsat_var_t v = xa->first()->vfirst()->var;
		printf("# VAR %zd\n", v);
		if (useProbability) {
			for (uint8_t x = 0; x != 2; x++) {
				psol[v] = x;

				for (ADD *xb = xa->next(); xb->first(); xb = xb->next()) {
					const double test = ADD(*xb).expand_all(psol).getConst();
					if (hprsat_is_nan(test) == false) {
						score[x] += fabs(test);
					}
				}
			}
			psol[v] = (score[0] > score[1]);
		}
		for (xa = xa->next(); xa->first(); xa = xa->next()) {
			printf("# "); xa->print();
			while (psol[v] != 2) {
				const double test = ADD(*xa).expand_all(psol).getConst();
				if (hprsat_is_nan(test) == false && test == 0.0)
					break;
				if (useProbability) {
					printf(" ERROR=%f/%f", score[0], score[1]);
					break;
				}
				psol[v]++;
			}
			printf("\n");
		}
	}

	if (useProbability)
		return (retval);

	/* double check solution */
	for (ADD *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		for (xa = xa->next(); xa->first(); xa = xa->next()) {
			double test = ADD(*xa).expand_all(psol).getConst();
			if (hprsat_is_nan(test) || test != 0.0)
				retval = false;
		}
	}
	return (retval);
}

void
hprsat_underiv(ADD_HEAD_t *xhead, ADD_HEAD_t *pderiv)
{
	ADD *xa;
	ADD *xn;

	for (xa = TAILQ_FIRST(pderiv); xa != 0; ) {
		xn = xa->next();
		delete xa->remove(pderiv);
		xa = xn;

		while (xa->first()) {
			xn = xa->next();
			xa->remove(pderiv)->insert_head(xhead);
			xa = xn;
		}
		xn = xa->next();
		delete xa->remove(pderiv);
		xa = xn;
	}
}