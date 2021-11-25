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

#include "hpsat.h"

bool
EQ :: solve(uint8_t *pvar, hpsat_var_t vmax, eq_solve_cb_t *cb, void *arg)
{
	if (vmax < HPSAT_VAR_MIN)
		return (false);

	bool retval;

	EQ *pone = new EQ [vmax];
	EQ *pzero = new EQ [vmax];

	memset(pvar, 0, sizeof(pvar[0]) * vmax);

	for (hpsat_var_t v = HPSAT_VAR_MIN; v != vmax; v++) {
		EQ *pe;
		EQ *pn;

		if (var == HPSAT_VAR_ORED) {
			for (pe = first(); pe; pe = pn) {
				pn = pe->next();

				if (pe->contains(v)) {
					pe->remove(&head);
					pe->dup()->expand(v, false).insert_tail(&pzero[v].head);
					pe->expand(v, true).insert_tail(&pone[v].head);
				}
			}
			pzero[v].var = var;
			pone[v].var = var;
		} else {
			pzero[v] = EQ(*this).expand(v, false);
			pone[v] = EQ(*this).expand(v, true);
			*this = EQ();
		}

		pzero[v].sort();
		pone[v].sort();

		/* add remaining conflicts */
		*this |= (pzero[v] & pone[v]);
	}

	/* check if there is no solution */
	if (var != HPSAT_VAR_ZERO) {
		retval = false;
		goto done;
	}
top:
	for (hpsat_var_t v = vmax; v-- != HPSAT_VAR_MIN; ) {
		if (pvar[v] == 0) {
			if (pzero[v].expand_all(pvar))
				pvar[v] = 1;
		}
		if (pvar[v] == 1) {
			if (pone[v].expand_all(pvar))
				pvar[v] = 2;
		}
	}

	for (hpsat_var_t v = HPSAT_VAR_MIN; v != vmax; v++) {
		if (pvar[v] == 2) {
			retval = false;
			goto done;
		}
	}

	if (cb) {
		if (cb(pvar, arg)) {
			retval = true;
			goto done;
		}

		for (hpsat_var_t v = HPSAT_VAR_MIN; v != vmax; v++) {
			if (pvar[v] == 0) {
				pvar[v] = 1;

				if (!pone[v].expand_all(pvar))
					goto top;
			}
			pvar[v] = 0;
		}
		retval = false;
	} else {
		retval = true;
	}
done:
	delete [] pzero;
	delete [] pone;

	return (retval);
}
