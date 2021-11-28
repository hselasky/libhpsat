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

static void
generate_function(XORMAP_HEAD_t &eq)
{
	hpsat_logic_builder_init(MAXVAR);

	var_t a;
	var_t b;
	var_t f;
	var_t g;

	a.alloc();
	b.alloc();
	f.alloc();
	g.alloc();

	var_t an;
	var_t bn;

	an = a ^ (a << 2) ^ b;
	bn = (((a ^ b) & (a << 2)) | (a & b)) << 1;

	an.equal_to_var(f);
	bn.equal_to_var(g);

	XORMAP_HEAD_t temp;
	TAILQ_INIT(&temp);

	XORMAP_HEAD_t solution;
	TAILQ_INIT(&solution);

	BITMAP_HEAD_t *peq = hpsat_logic_builder_equation();

	hpsat_var_t vm = hpsat_maxvar(peq) + 1;

	hpsat_sort_or(peq);
	hpsat_bitmap_to_xormap(peq, &temp);
	hpsat_logic_builder_finish();

	hpsat_demux(&temp, &vm);
	hpsat_solve(&temp, &solution, vm);
	hpsat_solve_strip(&temp, &solution, 4 * MAXVAR, vm);
	hpsat_underiv(&eq, &solution);
	TAILQ_CONCAT(&eq, &temp, entry);
	hpsat_sort_or(&eq);
}

static void
square_equation(XORMAP_HEAD_t *peq)
{
	XORMAP_HEAD_t temp;
	XORMAP_HEAD_t solution;

	XORMAP *xa;

	TAILQ_INIT(&temp);
	TAILQ_INIT(&solution);

	hpsat_var_t mv = hpsat_maxvar(peq) + 1;

	if (mv < 4 * MAXVAR)
		mv = 4 * MAXVAR;

	while ((xa = TAILQ_FIRST(peq))) {
		(new XORMAP(*xa))->shiftVar(MAXVAR * 4 + mv).insert_tail(&temp);
		xa->remove(peq)->shiftVar(MAXVAR * 4).insert_tail(&temp);
	}

	for (hpsat_var_t v = 0; v != 2 * MAXVAR; v++) {
		/* input to input */
		(new XORMAP(BITMAP(v, false) ^ BITMAP(v + 4 * MAXVAR, false)))->insert_tail(&temp);
		/* output to input */
		(new XORMAP(BITMAP(v + 6 * MAXVAR, false) ^ BITMAP(v + 4 * MAXVAR + mv, false)))->insert_tail(&temp);
		/* output to output */
		(new XORMAP(BITMAP(v + 2 * MAXVAR, false) ^ BITMAP(v + 6 * MAXVAR + mv, false)))->insert_tail(&temp);
	}

	mv = 2 * mv + 4 * MAXVAR;

	hpsat_demux(&temp, &mv);
	hpsat_solve(&temp, &solution, mv);
	hpsat_solve_strip(&temp, &solution, 4 * MAXVAR, mv);
	hpsat_underiv(peq, &solution);
	TAILQ_CONCAT(peq, &temp, entry);
	hpsat_sort_or(peq);
}

int main()
{
	XORMAP_HEAD_t eq;
	TAILQ_INIT(&eq);

	generate_function(eq);

	for (uint32_t x = 0; x != MAXVAR; x++) {
		printf("c ROUND %d\n", x);
		for (XORMAP *xa = TAILQ_FIRST(&eq); xa; xa = xa->next()) {
			printf("c "); xa->print(); printf(" # R\n");
		}
		square_equation(&eq);
	}

	hpsat_free(&eq);
	return (0);
}
