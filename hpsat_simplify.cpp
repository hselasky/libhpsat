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

static bool
hpsat_simplify(const EQP entry, EQP *ppeq, bool doInsert)
{
	bool any = false;

	for (;;) {
		hpsat_var_t v = entry.p->maxVar();

		if (v >= HPSAT_VAR_MIN) {
			if (ppeq[v].p == 0) {
				if (doInsert)
					ppeq[v] = entry;
				break;
			} else if (ppeq[v].s) {
				*entry.p ^= *ppeq[v].p;
				*entry.p ^= EQ(HPSAT_VAR_ONE);
				any = true;
			} else {
				*entry.p ^= *ppeq[v].p;
				any = true;
			}
		} else {
			break;
		}
	}
	return (any);
}

static void
hpsat_remove(const EQP entry, EQP *ppeq)
{
	hpsat_var_t v = entry.p->maxVar();

	if (ppeq[v].p == entry.p)
		ppeq[v].p = 0;
}

bool
EQ :: simplify(EQP *ppeq)
{
	bool any = false;

	switch (var) {
	case HPSAT_VAR_ORED:
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == true) {
				const EQP entry = {peq,false};
				any |= hpsat_simplify(entry, ppeq, true);
			}
		}
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == false)
				any |= peq->simplify(ppeq);
		}
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == true) {
				const EQP entry = {peq,false};
				hpsat_remove(entry, ppeq);
			}
		}
		break;
	case HPSAT_VAR_ANDED:
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == true) {
				const EQP entry = {peq,true};
				any |= hpsat_simplify(entry, ppeq, true);
			}
		}
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == false)
				any |= peq->simplify(ppeq);
		}
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == true) {
				const EQP entry = {peq,true};
				hpsat_remove(entry, ppeq);
			}
		}
		break;
	case HPSAT_VAR_XORED:
		for (EQ *peq = first(); peq; peq = peq->next()) {
			if (peq->isXOR() == true) {
				const EQP entry = {peq,false};
				any |= hpsat_simplify(entry, ppeq, false);
			} else {
				any |= peq->simplify(ppeq);
			}
		}
		break;
	default:
		break;
	}
	return (any);
}

bool
hpsat_simplify(EQ &eq)
{
	const hpsat_var_t vm = eq.maxVar() + 1;
	EQP *ppeq = new EQP [vm];
	bool any = false;

	memset(ppeq, 0, sizeof(ppeq[0]) * vm);

	while (eq.simplify(ppeq)) {
		eq.sort();
		any = true;
	}

	delete [] ppeq;

	return (any);
}
