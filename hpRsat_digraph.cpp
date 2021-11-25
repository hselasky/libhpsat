/*-
 * Copyright (c) 2021-2022 Hans Petter Selasky. All rights reserved.
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

void
hprsat_print_digraph(std::ostream &out, ADD_HEAD_t *pderiv)
{
	ADD_HEAD_t temp;
	TAILQ_INIT(&temp);

	out <<  "digraph {\n"
		"	graph [];\n"
		"	node [];\n"
		"	edge [];\n";

	for (hprsat_var_t w = HPRSAT_VAR_MAX;
	     (w = hprsat_maxvar(pderiv, w)) != HPRSAT_VAR_MIN; ) {
		out << "n" << w << " [label=\"v" << w << "\"];\n";
	}

	for (ADD *xa = TAILQ_FIRST(pderiv); xa; xa = xa->next()) {
		const hprsat_var_t v = xa->first()->vfirst()->var;

		for (xa = xa->next(); xa->first(); xa = xa->next()) {
			(new ADD(*xa))->insert_tail(&temp);
		}

		for (hprsat_var_t w = HPRSAT_VAR_MAX;
		     (w = hprsat_maxvar(&temp, w)) != HPRSAT_VAR_MIN; ) {
			if (w == v)
				continue;
			out << "n" << w << " -> n" << v << "\n";
		}

		hprsat_free(&temp);
	}

	out << "}\n";
}
