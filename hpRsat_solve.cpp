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

static void
hprsat_solve_simplify(ADD_HEAD_t *xhead, bool useProbability = false)
{
	while (hprsat_simplify_add(xhead, useProbability) /* ||
	     hprsat_elevate_add(xhead) */)
		;
}

static void
hprsat_sort_head(ADD_HEAD_t *ahead, ADD_HEAD_t *bhead)
{
	ADD *pa = TAILQ_FIRST(ahead);
	ADD *pb = TAILQ_FIRST(bhead);

	while (pa && pb) {
		pa = pa->next();
		pb = pb->next();
	}

	if (pa != 0)
		TAILQ_SWAP(ahead, bhead, ADD, entry);
}

bool
hprsat_solve(ADD_HEAD_t *xhead, ADD_HEAD_t *pderiv, hprsat_var_t *pvmax, bool useProbability, bool useFullRange)
{
	const hprsat_var_t vm = *pvmax;

	ADD *xa;
	ADD *xb;
	ADD *xn;

	/* Make sure all expressions are defactored. */
	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();
		if (xa->defactor().first() == 0)
			delete xa->remove(xhead);
	}

	printf("INPUT\n");
	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();
		xa->print(); std::cout << "\n";
	}

	hprsat_solve_simplify(xhead, useProbability);

	printf("OUTPUT\n");
	for (xa = TAILQ_FIRST(xhead); xa; xa = xn) {
		xn = xa->next();
		xa->print(); std::cout << "\n";
	}

	ADD_HEAD_t bhead[hprsat_global_mod];

	hprsat_val_t maxValue[vm];

	for (hprsat_val_t z = 0; z != hprsat_global_mod; z++)
		TAILQ_INIT(&bhead[z]);

	if (useFullRange) {
		for (hprsat_var_t v = 0; v != vm; v++)
			maxValue[v] = hprsat_global_mod;
	} else {
		for (hprsat_var_t v = 0; v != vm; v++) {
			maxValue[v] = 1;

			for (xa = TAILQ_FIRST(xhead); xa != 0; xa = xa->next()) {
				if (xa->contains(v)) {
					ADD *aptr = 0;
					for (hprsat_val_t z = maxValue[v] - 1; z != hprsat_global_mod; z++) {
						(xb = new ADD(*xa))->expand(v, z).sort();
						if (aptr != 0 && maxValue[v] < z + 1 && *aptr != *xb)
							maxValue[v] = z + 1;
						delete aptr;
						aptr = xb;
					}
					delete aptr;
				}
			}
		}
	}

	for (hprsat_var_t v = 0; v != vm; v++) {

		printf("# PROGRESS v%zd of %zd\n", v, vm);

		ADD_HEAD_t ahead;
		ADD_HEAD_t thead;
		ADD_HEAD_t uhead;

		TAILQ_INIT(&ahead);
		TAILQ_INIT(&thead);
		TAILQ_INIT(&uhead);

		for (xa = TAILQ_FIRST(xhead); xa != 0; xa = xn) {
			xn = xa->next();
			if (xa->contains(v)) {
				/* Expand variable. */
				for (hprsat_val_t z = 0; z != maxValue[v]; z++)
					(new ADD(*xa))->insert_tail(&bhead[z]).expand(v, z).sort();

				xa->remove(xhead)->align().insert_tail(&ahead);
			}
		}

		for (hprsat_val_t z = 0; z != maxValue[v]; z++)
			hprsat_solve_simplify(&bhead[z]);

		for (hprsat_val_t z = 0; z != maxValue[v]; z++) {
			for (hprsat_val_t t = z + 1; t != maxValue[v]; t++) {
				hprsat_sort_head(&bhead[z], &bhead[t]);
			}
		}

		(new ADD(1))->insert_tail(&thead);

		/* Compute the variable conflict. */
		for (hprsat_val_t z = 0; z != maxValue[v]; z++) {
			for (xa = TAILQ_FIRST(&bhead[z]); xa != 0; xa = xa->next()) {
				for (xb = TAILQ_FIRST(&thead); xb != 0; xb = xb->next()) {
					xn = new ADD(xa[0] * xb[0]);
					if (xn->isNonZeroVariable())
						xn->defactor().insert_tail(&uhead);
					else
						delete xn;
				}
			}
			hprsat_free(&thead);
			hprsat_free(&bhead[z]);

			TAILQ_CONCAT(&thead, &uhead, entry);

			hprsat_solve_simplify(&thead);
		}

		if (TAILQ_FIRST(&thead)) {
			TAILQ_CONCAT(xhead, &thead, entry);
			hprsat_solve_simplify(xhead, useProbability);
		}

		(new ADD(0))->insert_tail(&ahead);
		(new ADD(maxValue[v], v))->insert_head(&ahead);

		TAILQ_CONCAT(&ahead, pderiv, entry);
		TAILQ_CONCAT(pderiv, &ahead, entry);
	}

	printf("# REMAINDER\n");
	for (xa = TAILQ_FIRST(xhead); xa != 0; xa = xn) {
		xn = xa->next();
		printf("# "); xa->print(); printf("\n");
	}
	return (TAILQ_FIRST(xhead) != 0);
}

bool
hprsat_solve_callback(ADD *xm, hprsat_val_t *psol, hprsat_solve_callback_t *cb, void *arg)
{
	ADD *xa;

	if (xm == NULL)
		return (cb(arg, psol));

	const hprsat_var_t v = xm->first()->vfirst()->var;
	const hprsat_val_t m = xm->first()->factor_lin;

	for (psol[v] = 0; psol[v] != m; psol[v]++) {
		for (xa = xm->next(); xa->first(); xa = xa->next()) {
			if (ADD(*xa).expand_all(psol).isNonZeroConst())
				goto next;
		}
		if (hprsat_solve_callback(xa->next(), psol, cb, arg) == false)
			return (false);
	next:;
	}
	return (true);
}

static bool
hprsat_solve_do_count(void *arg, hprsat_val_t *)
{
	(*(size_t *)arg)++;
	return (true);
}

size_t
hprsat_solve_count(ADD_HEAD_t *xhead, hprsat_val_t *psol)
{
	size_t retval = 0;
	hprsat_solve_callback(TAILQ_FIRST(xhead), psol, hprsat_solve_do_count, &retval);
	return (retval);
}

bool
hprsat_solve_first(ADD_HEAD_t *xhead, hprsat_val_t *psol, bool useProbability)
{
	bool retval = true;

	/* compute the derivated XORs in the right order */
	for (ADD *xa = TAILQ_FIRST(xhead); xa; xa = xa->next()) {
		const hprsat_var_t v = xa->first()->vfirst()->var;
		const hprsat_val_t m = xa->first()->factor_lin;

		hprsat_val_t score[m];
		memset(score, 0, sizeof(score));

		printf("# VAR %zd\n", v);
		if (useProbability) {
			for (hprsat_val_t x = 0; x != m; x++) {
				psol[v] = x;

				for (ADD *xb = xa->next(); xb->first(); xb = xb->next()) {
					bool isNaN;
					const hprsat_val_t test = ADD(*xb).expand_all(psol).getConst(isNaN);
					if (isNaN == false)
						score[x] += abs(test);
				}
			}
			psol[v] = 0;
			for (hprsat_val_t x = 1; x != m; x++) {
				if (score[x] < score[psol[v]])
					psol[v] = x;
			}
		}
		for (xa = xa->next(); xa->first(); xa = xa->next()) {
			printf("# "); xa->print();
			while (psol[v] != m) {
				if (ADD(*xa).expand_all(psol).isNonZeroConst() == false)
					break;
				if (useProbability) {
					std::cout << " ERROR=" << score[0] << "/" << score[1];
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
			if (ADD(*xa).expand_all(psol).isNonZeroConst())
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
