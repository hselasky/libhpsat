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

hprsat_var_t
hprsat_maxvar(const VAR_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t max = HPRSAT_VAR_MIN;

	for (VAR *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->var < limit && max < pa->var)
			max = pa->var;
	}
	return (max);
}

hprsat_var_t
hprsat_minvar(const VAR_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t min = HPRSAT_VAR_MIN;

	for (VAR *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		if (pa->var > limit &&
		    (min == HPRSAT_VAR_MIN || min > pa->var))
			min = pa->var;
	}
	return (min);
}

void
hprsat_free(VAR_HEAD_t *phead)
{
	VAR *pa;

	while ((pa = TAILQ_FIRST(phead)) != 0)
		delete pa->remove(phead);
}

VAR *
VAR :: sort_insert_tail(VAR_HEAD_t *phead, hprsat_val_t &factor)
{
	bool isNaN;
	const hprsat_val_t value = getConst(isNaN);
	VAR *pn = next();

	if (isNaN) {
		TAILQ_INSERT_TAIL(phead, this, entry);
	} else {
		delete this;
		factor *= value;
		hprsat_do_global_modulus(factor);
	}
	return (pn);
}
