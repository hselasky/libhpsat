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
hprsat_maxvar(const ADD_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t max = HPRSAT_VAR_MIN;

	for (ADD *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hprsat_var_t v = pa->maxVar(limit);
		if (max < v)
			max = v;
	}
	return (max);
}

hprsat_var_t
hprsat_minvar(const ADD_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t min = HPRSAT_VAR_MIN;

	for (ADD *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hprsat_var_t v = pa->minVar(limit);
		if (v > HPRSAT_VAR_MIN) {
			if (min == HPRSAT_VAR_MIN || min > v)
				min = v;
		}
	}
	return (min);
}

void
hprsat_free(ADD_HEAD_t *phead)
{
	ADD *px;

	while ((px = TAILQ_FIRST(phead)))
		delete px->remove(phead);
}

ADD &
ADD :: sort()
{
	hprsat_sort_add(&head);
	return (*this);
}

ADD
ADD :: operator *(const ADD &other) const
{
#if 0
	/*
	 * Speedup multiplication by use of transform.
	 */
	ADD ta(*this);
	ADD tb(other);
	ADD tc;

	for (hprsat_var_t v = HPRSAT_VAR_MAX;; ) {
		hprsat_var_t va = maxVar(v);
		hprsat_var_t vb = other.maxVar(v);

		v = (va > vb) ? va : vb;
		if (v == HPRSAT_VAR_MIN)
			break;

		ta.xform_fwd(v);
		tb.xform_fwd(v);
	}

	MUL *pa = ta.first();
	MUL *pb = tb.first();

	while (pa && pb) {
		int ret = pa->compare(*pb, true);
		if (ret == 0) {
			(new MUL(*pa * *pb))->insert_tail(&tc.head);
			pa = pa->next();
			pb = pb->next();
		} else if (ret > 0) {
			pb = pb->next();
		} else {
			pa = pa->next();
		}
	}

	for (hprsat_var_t v = HPRSAT_VAR_MAX;; ) {
		hprsat_var_t va = maxVar(v);
		hprsat_var_t vb = other.maxVar(v);

		v = (va > vb) ? va : vb;
		if (v == HPRSAT_VAR_MIN)
			break;

		tc.xform_inv(v);
	}
	return (tc);
#else
	ADD temp;
	MUL *paa = first();
	MUL *pbb = other.first();

	/* Standard multiplication. */
	for (MUL *pa = paa; pa; pa = pa->next()) {
		for (MUL *pb = pbb; pb; pb = pb->next())
			(new MUL(pa[0] * pb[0]))->insert_tail(&temp.head);
		temp.sort();
	}
	return (temp);
#endif
}

ADD *
ADD :: sort_insert_tail(ADD_HEAD_t *phead, hprsat_val_t &factor)
{
	bool isNaN;
	const hprsat_val_t value = getConst(isNaN);
	ADD *pn = next();
	if (isNaN) {
		TAILQ_INSERT_TAIL(phead, this, entry);
	} else {
		delete this;
		factor *= value;
		factor %= hprsat_global_mod;
	}
	return (pn);
}

hprsat_val_t
ADD :: getConst(bool &isNaN) const
{
	hprsat_val_t value = 0;
	for (MUL *pa = first(); pa; pa = pa->next()) {
		const hprsat_val_t temp = pa->getConst(isNaN);
		if (isNaN) {
			return (temp);
		} else {
			value += temp;
		}
	}
	isNaN = false;
	value %= hprsat_global_mod;
	if (value < 0)
		value += hprsat_global_mod;
	return (value);
}

ADD &
ADD :: align()
{
	hprsat_val_t value;

	if (last() == 0)
		goto done;

	hprsat_do_inverse(last()->factor_lin, hprsat_global_mod, value);

	if (value == 0)
		goto done;

	for (MUL *pa = first(); pa; pa = pa->next()) {
		pa->factor_lin *= value;
		pa->factor_lin %= hprsat_global_mod;
		if (pa->factor_lin < 0)
			pa->factor_lin += hprsat_global_mod;
	}
done:
	return (*this);
}

ADD
ADD :: expandPower(hprsat_pwr_t exp)
{
	MUL one;
	ADD temp(one);
	ADD base(*this);

	assert(exp >= 0);
	assert((exp % HPRSAT_PWR_UNIT) == 0);

	while (exp >= HPRSAT_PWR_UNIT) {
		if ((exp & HPRSAT_PWR_UNIT) != 0)
			temp *= base;
		base *= base;
		exp /= 2;
	}
	return (temp);
}

ADD &
ADD :: xform_fwd(hprsat_var_t v)
{
	MUL_HEAD_t temp;
	TAILQ_INIT(&temp);

	MUL mv(1, v);

	for (MUL *pa = first(); pa; pa = pa->next()) {
		if (!pa->contains(v))
			(new MUL(*pa * mv))->insert_tail(&temp);
	}
	TAILQ_CONCAT(&head, &temp, entry);
	return (sort());
}

ADD &
ADD :: xform_inv(hprsat_var_t v)
{
	MUL_HEAD_t temp;
	TAILQ_INIT(&temp);

	MUL mv(-1, v);

	for (MUL *pa = first(); pa; pa = pa->next()) {
		if (!pa->contains(v))
			(new MUL(*pa * mv))->insert_tail(&temp);
	}
	TAILQ_CONCAT(&head, &temp, entry);
	return (sort());
}

ADD
ADD :: raisePower(hprsat_pwr_t pwr)
{
	ADD ret;

	VAR *pv = new VAR(0, pwr);
	pv->add = new ADD(*this);
	MUL *pm = new MUL();

	pv->insert_tail(&pm->vhead);
	pm->insert_tail(&ret.head);

	return (ret.sort());
}

void
ADD :: print(std::ostream &out) const
{
	for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
		if (pa->vfirst())
			out << "(";
		pa->print(out);
		if (pa->vfirst())
			out << ")";
		if (pa->next())
			out << "+";
	}
}

ADD &
ADD :: defactor()
{
	VAR *pv;

	MUL *pa;
	MUL *pb;
	MUL *pn;

	bool any;
top:
	any = false;

	for (pa = first(); pa; pa = pn) {
		pn = pa->next();

		for (pv = pa->vfirst(); pv; pv = pv->next()) {
			if (pv->add == 0)
				continue;
			if ((pv->pwr % HPRSAT_PWR_UNIT) != 0)
				continue;
			ADD temp(pv->add->expandPower(pv->pwr));
			delete pv->remove(&pa->vhead);
			while ((pb = temp.first())) {
				pb->remove(&temp.head)->insert_tail(&head);
				*pb *= *pa;
			}
			delete pa->remove(&head);
			any = true;
			break;
		}
	}

	if (any) {
		sort();
		goto top;
	}
	return (*this);
}

ADD &
ADD :: defactor_all()
{
	ADD square;
	ADD linear;

	VAR *pv;

	MUL *pa;
	MUL *pn;

top:
	defactor();

	for (pa = first(); pa; pa = pn) {
		pn = pa->next();

		for (pv = pa->vfirst();; pv = pv->next()) {
			if (pv == 0) {
				pa->remove(&head)->insert_tail(&linear.head);
				break;
			} else if ((pv->pwr % HPRSAT_PWR_UNIT) != 0) {
				pa->remove(&head)->negate().insert_tail(&square.head);
				break;
			}
		}
	}

	if (square.first() != 0) {
		square *= square;
		linear *= linear;
		*this = linear - square;
		linear = ADD();
		square = ADD();
		goto top;
	}

	*this = linear;

	return (*this);
}
