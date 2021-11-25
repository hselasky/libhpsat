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

size_t
hpsat_find_longest_path(XORMAP_HEAD_t *xhead, hpsat_var_t v, hpsat_var_t vm, hpsat_var_t *longest)
{
	XORMAP *xa[vm];
	hpsat_var_t next[vm];
	hpsat_var_t order[vm];
	hpsat_var_t w[vm];
	size_t sp = 0;

	memset(next, 255, sizeof(next));
	memset(order, 255, sizeof(order));
	w[sp] = v;
push:
	order[w[sp]] = 0;
	sp++;

	for (xa[sp] = TAILQ_FIRST(xhead); xa[sp]; xa[sp] = xa[sp]->next()) {
		if (xa[sp]->contains(w[sp - 1]) == false)
			continue;

		w[sp] = HPSAT_VAR_MIN;
		while (1) {
			w[sp] = xa[sp]->minVar(w[sp]);
			if (w[sp] == HPSAT_VAR_MIN)
				break;
			else if (order[w[sp]] != -1)
				continue;
			else
				goto push;    /* need to explore */
		pop:
			if (order[w[sp - 1]] < order[w[sp]] + 1) {
				order[w[sp - 1]] = order[w[sp]] + 1;
				next[w[sp - 1]] = w[sp];
			} else if (order[w[sp - 1]] == order[w[sp]] + 1 &&
			    next[w[sp - 1]] < w[sp]) {
				next[w[sp - 1]] = w[sp];
			}
		}
	}
	if (sp-- != 1)
		goto pop;

	for (sp = 0; v != -1; sp++) {
		longest[sp] = v;
		v = next[v];
	}
	return (sp);
}

size_t
hpsat_find_shortest_path(XORMAP_HEAD_t *xhead, hpsat_var_t v, hpsat_var_t vm, hpsat_var_t *shortest)
{
	XORMAP *xa[vm];
	hpsat_var_t next[vm];
	hpsat_var_t order[vm];
	hpsat_var_t w[vm];
	size_t sp = 0;

	memset(next, 255, sizeof(next));
	memset(order, 255, sizeof(order));
	w[sp] = v;
push:
	order[w[sp]] = HPSAT_VAR_MAX;
	sp++;

	for (xa[sp] = TAILQ_FIRST(xhead); xa[sp]; xa[sp] = xa[sp]->next()) {
		if (xa[sp]->contains(w[sp - 1]) == false)
			continue;

		w[sp] = HPSAT_VAR_MIN;
		while (1) {
			w[sp] = xa[sp]->minVar(w[sp]);
			if (w[sp] == HPSAT_VAR_MIN)
				break;
			else if (order[w[sp]] != -1)
				continue;
			else
				goto push;    /* need to explore */
		pop:
			if (order[w[sp - 1]] == -1 || order[w[sp - 1]] > order[w[sp]] + 1) {
				order[w[sp - 1]] = order[w[sp]] + 1;
				next[w[sp - 1]] = w[sp];
			} else if (order[w[sp - 1]] == order[w[sp]] + 1 &&
			    next[w[sp - 1]] < w[sp]) {
				next[w[sp - 1]] = w[sp];
			}
		}
	}
	if (sp-- != 1)
		goto pop;

	for (sp = 0; v != -1; sp++) {
		shortest[sp] = v;
		v = next[v];
	}
	return (sp);
}
