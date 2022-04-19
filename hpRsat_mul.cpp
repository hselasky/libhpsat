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
hprsat_maxvar(const MUL_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t max = HPRSAT_VAR_MIN;

	for (MUL *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hprsat_var_t v = pa->maxVar(limit);
		if (max < v)
			max = v;
	}
	return (max);
}

hprsat_var_t
hprsat_minvar(const MUL_HEAD_t *phead, hprsat_var_t limit)
{
	hprsat_var_t min = HPRSAT_VAR_MIN;

	for (MUL *pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		const hprsat_var_t v = pa->minVar(limit);
		if (v > HPRSAT_VAR_MIN) {
			if (min == HPRSAT_VAR_MIN || min > v)
				min = v;
		}
	}
	return (min);
}

void
hprsat_free(MUL_HEAD_t *phead)
{
	MUL *pa;

	while ((pa = TAILQ_FIRST(phead)))
		delete pa->remove(phead);
}

static int
hprsat_compare(const void *a, const void *b)
{
	return ((MUL * const *)a)[0][0].compare(((MUL * const *)b)[0][0]);
}

bool
hprsat_sort_add(MUL_HEAD_t *phead)
{
	MUL *pa;
	MUL **pp;
	size_t count = 0;
	size_t x;
	size_t y;
	bool did_sort = false;

	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next()) {
		pa->sort();
		count++;
	}

	if (count == 0)
		return (false);

	pp = new MUL * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		pp[count++] = pa;

	for (x = 1; x != count; x++) {
		if ((pp + x - 1)[0][0].compare((pp + x)[0][0], true) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hprsat_compare);

	TAILQ_INIT(phead);

	for (x = 0; x != count; ) {
		bool isNaN;
		hprsat_val_t value;

		for (y = x + 1; y != count; y++) {
			if ((pp + x)[0][0].compare((pp + y)[0][0], true) != 0)
				break;
			*pp[x] += *pp[y];
		}

		if (y != x + 1)
			pp[x]->sort();

		value = pp[x]->getConst(isNaN);
		if (isNaN == false && value == 0) {
			did_sort = true;
			delete pp[x];
		} else {
			pp[x]->insert_tail(phead);
		}
		while (++x != y) {
			did_sort = true;
			delete pp[x];
		}
	}

	delete [] pp;

	return (did_sort);
}

MUL &
MUL :: sort()
{
	if (factor_lin == 0)
		return (zero());

	hprsat_sort_var(&vhead, factor_lin);

	factor_lin %= hprsat_global_mod;
	if (factor_lin < 0)
		factor_lin += hprsat_global_mod;

	return (factor_lin ? *this : zero());
}

MUL :: ~MUL()
{
	hprsat_free(&vhead);
}

hprsat_var_t
MUL :: maxVar(hprsat_var_t limit) const
{
	return (hprsat_maxvar(&vhead, limit));
}

hprsat_var_t
MUL :: minVar(hprsat_var_t limit) const
{
	return (hprsat_minvar(&vhead, limit));
}

MUL &
MUL :: addVar(hprsat_var_t delta)
{
	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next())
		pa->addVar(delta);
	return (*this);
}

MUL &
MUL :: mulVar(hprsat_var_t factor)
{
	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next())
		pa->mulVar(factor);
	return (*this);
}

bool
MUL :: isVariable() const
{
	for (VAR *pa = vfirst(); pa; pa = pa->next()) {
		if (pa->isVariable())
			return (true);
	}
	return (false);
}

hprsat_val_t
MUL :: getConst(bool &isNaN) const
{
	hprsat_val_t value_lin = factor_lin;
	bool undefined = false;

	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next()) {
		const hprsat_val_t temp = pa->getConst(isNaN);
		if (isNaN) {
			undefined = true;
		} else {
			value_lin *= temp;
		}
	}

	/* Zero is stronger than undefined. */
	if (value_lin == 0) {
		isNaN = false;
		return (0);
	} else if (undefined) {
		isNaN = true;
		return (0);
	} else {
		isNaN = false;
		return (value_lin);
	}
}

bool
MUL :: contains(hprsat_var_t var) const
{
	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next()) {
		if (pa->contains(var))
			return (true);
	}
	return (false);
}

MUL &
MUL :: operator =(const MUL &other)
{
	if (this == &other)
		return (*this);
	hprsat_free(&vhead);
	factor_lin = other.factor_lin;
	for (VAR *pa = TAILQ_FIRST(&other.vhead); pa; pa = pa->next())
		pa->dup()->insert_tail(&vhead);
	return (*this);
}

MUL &
MUL :: operator +=(const MUL &other)
{
	factor_lin += other.factor_lin;

	return (*this);
}

MUL &
MUL :: operator *=(const MUL &other)
{
	const hprsat_pwr_t m = (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT;
	hprsat_val_t value;
	bool isNaN;

	VAR *p0 = TAILQ_FIRST(&vhead);
	const VAR *p1 = TAILQ_FIRST(&other.vhead);

	factor_lin *= other.factor_lin;
	factor_lin %= hprsat_global_mod;

	TAILQ_INIT(&vhead);

	while (p0 && p1) {
		int cmp = p0->compare(*p1, true);
		if (cmp == 0) {
			if (p0->pwr != 0 || p1->pwr != 0) {
				p0->pwr += p1->pwr;
				p0->pwr %= m;
				if (p0->pwr < 0)
					p0->pwr += m;
				if (p0->pwr == 0)
					p0->pwr = m;
			}
			p1 = p1->next();
		} else if (cmp > 0) {
			(new VAR(*p1))->sort_insert_tail(&vhead, factor_lin);
			p1 = p1->next();
		} else {
			p0 = p0->sort_insert_tail(&vhead, factor_lin);
		}
	}
	while (p0) {
		p0 = p0->sort_insert_tail(&vhead, factor_lin);
	}
	while (p1) {
		(new VAR(*p1))->sort_insert_tail(&vhead, factor_lin);
		p1 = p1->next();
	}

	value = getConst(isNaN);
	if (isNaN == false && value == 0)
		return (zero());

	return (*this);
}

size_t
MUL :: count() const
{
	size_t retval = 0;
	for (VAR *pa = vfirst(); pa; pa = pa->next())
		retval++;
	return (retval);
}

void
MUL :: print(std::ostream &out) const
{
	if (factor_lin != 1 || vfirst() == 0) {
		out << factor_lin;
		if (vfirst())
			out << "*";
	}

	for (VAR *pa = vfirst(); pa; pa = pa->next()) {
		pa->print(out);
		if (pa->next())
			out << "*";
	}
}

int
MUL :: compare(const MUL & other, bool exprOnly) const
{
	const size_t na = count();
	const size_t nb = other.count();
	int ret;

	if (na > nb)
		return (1);
	else if (na < nb)
		return (-1);

	const VAR *pa = vfirst();
	const VAR *pb = other.vfirst();

	while (pa && pb) {
		ret = pa->compare(*pb);
		if (ret)
			return (ret);
		pa = pa->next();
		pb = pb->next();
	}
	ret = (pa != 0) - (pb != 0);
	if (ret || exprOnly)
		return (ret);
	else if (factor_lin > other.factor_lin)
		return (1);
	else if (factor_lin < other.factor_lin)
		return (-1);
	else
		return (0);
}

MUL &
MUL :: expand(hprsat_var_t var, bool value)
{
	for (VAR *pa = vfirst(), *pb; pa; pa = pb) {
		pb = pa->next();

		if (pa->contains(var)) {
			if (value == false)
				factor_lin = 0;
			delete pa->remove(&vhead);
		}
	}
	return (*this);
}

MUL &
MUL :: expand_all(const uint8_t *pvar)
{
	for (VAR *pa = vfirst(); pa; pa = pa->next()) {
		if (pvar[pa->var] == 0)
			factor_lin = 0;
	}
	hprsat_free(&vhead);
	return (*this);
}

MUL &
MUL :: zero()
{
	factor_lin = 0;
	hprsat_free(&vhead);
	return (*this);
}

MUL &
MUL :: operator /=(const MUL &other)
{
	VAR *va;
	VAR *vb;
	VAR *vn;

	factor_lin /= other.factor_lin;

	va = vfirst();
	vb = other.vfirst();

	while (va && vb) {
		int ret = va->compare(*vb);
		if (ret == 0) {
			vn = va->next();
			delete va->remove(&vhead);
			va = vn;
		} else if (ret > 0) {
			vb = vb->next();
		} else {
			va = va->next();
		}
	}
	return (*this);
}
