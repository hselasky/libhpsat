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

#include <math.h>

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
		pa->sort(phead);
		count++;
	}

	if (count == 0)
		return (false);

	pp = new MUL * [count];

	count = 0;
	for (pa = TAILQ_FIRST(phead); pa; pa = pa->next())
		pp[count++] = pa;

	for (x = 1; x != count; x++) {
		if ((pp + x - 1)[0][0].compare((pp + x)[0][0], false) > 0) {
			did_sort = true;
			break;
		}
	}
	if (did_sort)
		mergesort(pp, count, sizeof(pp[0]), &hprsat_compare);

	TAILQ_INIT(phead);

	for (x = 0; x != count; ) {
		for (y = x + 1; y != count; y++) {
			if ((pp + x)[0][0].compare((pp + y)[0][0], false) != 0)
				break;
			pp[x]->factor_lin += pp[y]->factor_lin;
		}

		if (pp[x]->getConst() == 0.0) {
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
MUL :: sort(MUL_HEAD_t *phead)
{
	VAR *pa;
	VAR *pn;

	MUL *ra;

	if (factor_lin == 0.0 || factor_sqrt == 0.0)
		return (zero());

	for (pa = vfirst(); pa; pa = pn) {
		const double value = pa->getConst();
		pn = pa->next();
		if (hprsat_is_nan(value) == false) {
			factor_lin *= value;
			delete pa->remove(&vhead);
		}
	}

	if (factor_lin == 0.0)
		return (zero());

	ADD defactor(1.0);

	hprsat_sort_mul(&ahead, defactor, factor_sqrt);

	if (factor_sqrt == 0)
		return (zero());

	if (defactor != ADD(1.0)) {
		while (1) {
			ra = defactor.first();
			if (ra == 0)
				break;
			*ra *= *this;
			ra->remove(&defactor.head)->insert_tail(phead);
		}
		return (zero());
	}
	return (*this);
}

MUL :: MUL(const ADD &other)
{
	TAILQ_INIT(&vhead);
	TAILQ_INIT(&ahead);
	factor_lin = 1.0;
	factor_sqrt = 1.0;
	other.dup()->insert_tail(&ahead);
}

MUL :: ~MUL()
{
	hprsat_free(&vhead);
	hprsat_free(&ahead);
}

hprsat_var_t
MUL :: maxVar(hprsat_var_t limit) const
{
	hprsat_var_t vmax = hprsat_maxvar(&vhead, limit);
	hprsat_var_t xmax = hprsat_maxvar(&ahead, limit);
	return (vmax > xmax ? vmax : xmax);
}

hprsat_var_t
MUL :: minVar(hprsat_var_t limit) const
{
	hprsat_var_t vmin = hprsat_minvar(&vhead, limit);
	hprsat_var_t xmin = hprsat_minvar(&ahead, limit);
	if (vmin == HPRSAT_VAR_MIN)
		return (xmin);
	else if (xmin == HPRSAT_VAR_MIN)
		return (vmin);
	else
		return (vmin < xmin ? vmin : xmin);
}

MUL &
MUL :: addVar(hprsat_var_t delta)
{
	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next())
		pa->addVar(delta);
	for (ADD *pa = TAILQ_FIRST(&ahead); pa; pa = pa->next())
			pa->addVar(delta);
	return (*this);
}

MUL &
MUL :: mulVar(hprsat_var_t factor)
{
	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next())
		pa->mulVar(factor);
	for (ADD *pa = TAILQ_FIRST(&ahead); pa; pa = pa->next())
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
	for (ADD *pa = afirst(); pa; pa = pa->next()) {
		if (pa->isVariable())
			return (true);
	}
	return (false);
}

double
MUL :: getConst(bool doSqrt) const
{
	double value_lin = factor_lin;
	double value_sqrt = factor_sqrt;
	bool undefined = false;

	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next()) {
		const double temp = pa->getConst();
		if (hprsat_is_nan(temp)) {
			undefined = true;
			continue;
		}
		value_lin *= temp;
	}

	for (ADD *pa = TAILQ_FIRST(&ahead); pa; pa = pa->next()) {
		const double temp = pa->getConst(doSqrt);
		if (hprsat_is_nan(temp)) {
			undefined = true;
			continue;
		}
		value_sqrt *= temp;
	}

	/* Zero is stronger than undefined. */
	if (value_lin == 0.0 || value_sqrt == 0.0)
		return (0.0);

	if (undefined)
		return (hprsat_nan);
	else if (hprsat_try_sqrt(value_sqrt, doSqrt))
		return (value_sqrt * value_lin);
	else
		return (hprsat_nan);
}

bool
MUL :: contains(hprsat_var_t var) const
{
	for (VAR *pa = TAILQ_FIRST(&vhead); pa; pa = pa->next()) {
		if (pa->contains(var))
			return (true);
	}
	for (ADD *pa = TAILQ_FIRST(&ahead); pa; pa = pa->next()) {
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
	hprsat_free(&ahead);
	factor_lin = other.factor_lin;
	factor_sqrt = other.factor_sqrt;
	for (VAR *pa = TAILQ_FIRST(&other.vhead); pa; pa = pa->next())
		pa->dup()->insert_tail(&vhead);
	for (ADD *pa = TAILQ_FIRST(&other.ahead); pa; pa = pa->next())
		pa->dup()->insert_tail(&ahead);
	return (*this);
}

MUL &
MUL :: operator *=(const MUL &other)
{
	VAR *p0 = TAILQ_FIRST(&vhead);
	const VAR *p1 = TAILQ_FIRST(&other.vhead);

	ADD *q0 = TAILQ_FIRST(&ahead);
	const ADD *q1 = TAILQ_FIRST(&other.ahead);

	factor_lin *= other.factor_lin;
	factor_sqrt *= other.factor_sqrt;

	TAILQ_INIT(&vhead);
	TAILQ_INIT(&ahead);

	while (p0 && p1) {
		if (p0->var == p1->var) {
			p1 = p1->next();
		} else if (p0->var > p1->var) {
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

	while (q0 && q1) {
		int ret = q0->compare(*q1);
		if (ret == 0) {
			q0 = q0->sort_insert_tail(&ahead, factor_sqrt);
			(new ADD(*q1))->sort_insert_tail(&ahead, factor_sqrt);
			q1 = q1->next();
		} else if (ret > 0) {
			(new ADD(*q1))->sort_insert_tail(&ahead, factor_sqrt);
			q1 = q1->next();
		} else {
			q0 = q0->sort_insert_tail(&ahead, factor_sqrt);
		}
	}
	while (q0) {
		q0 = q0->sort_insert_tail(&ahead, factor_sqrt);
	}
	while (q1) {
		(new ADD(*q1))->sort_insert_tail(&ahead, factor_sqrt);
		q1 = q1->next();
	}

	if (getConst() == 0)
		return (zero());

	return (*this);
}

size_t
MUL :: count() const
{
	size_t retval = 0;
	for (VAR *pa = vfirst(); pa; pa = pa->next())
		retval++;
	for (ADD *pa = afirst(); pa; pa = pa->next())
		retval++;
	return (retval);
}

static void
hprsat_out_value(std::ostream &out, double value)
{
	if (floor(value) == value && fabs(value) < 10000.0) {
		out << value;
	} else {
		out << std::fixed << value << std::defaultfloat;
	}
}

void
MUL :: print(std::ostream &out) const
{
	const double value = getConst();

	if (hprsat_is_nan(value)) {
		if (factor_lin != 1.0) {
			if (factor_lin != -1.0) {
				hprsat_out_value(out, factor_lin);
				if (factor_sqrt != 1.0 || vfirst() || afirst())
					out << "*";
			} else {
				out << "-";
			}
		}
		if (factor_sqrt != 1.0) {
			out << "sqrt(";
			hprsat_out_value(out, factor_sqrt);
			out << ")";
			if (vfirst() || afirst())
				out << "*";
		}
		for (VAR *pa = vfirst(); pa; pa = pa->next()) {
			pa->print(out);
			if (pa->next() || afirst())
				out << "*";
		}
		for (ADD *pa = afirst(); pa; pa = pa->next()) {
			out << "sqrt(";
			pa->print(out);
			out << ")";
			if (pa->next())
				out << "*";
		}
	} else {
		hprsat_out_value(out, value);
	}
}

int
MUL :: compare(const MUL & other, bool compareFactor) const
{
	const size_t na = count();
	const size_t nb = other.count();

	if (na > nb)
		return (1);
	else if (na < nb)
		return (-1);

	const VAR *pa = vfirst();
	const VAR *pb = other.vfirst();

	while (pa && pb) {
		const int ret = pa->compare(*pb);
		if (ret)
			return (ret);
		pa = pa->next();
		pb = pb->next();
	}
	int ret = (pa != 0) - (pb != 0);
	if (ret)
		return (ret);

	const ADD *qa = afirst();
	const ADD *qb = other.afirst();

	while (qa && qb) {
		const int ret = qa->compare(*qb);
		if (ret)
			return (ret);
		qa = qa->next();
		qb = qb->next();
	}
	ret = (qa != 0) - (qb != 0);

	if (ret)
		return (ret);
	else if (factor_sqrt > other.factor_sqrt)
		return (1);
	else if (factor_sqrt < other.factor_sqrt)
		return (-1);
	else if (compareFactor == false)
		return (0);
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
				factor_lin = 0.0;
			delete pa->remove(&vhead);
		}
	}

	for (ADD *pa = afirst(); pa; pa = pa->next())
		pa->expand(var, value);

	return (*this);
}

MUL &
MUL :: expand_all(const uint8_t *pvar)
{
	for (VAR *pa = vfirst(); pa; pa = pa->next()) {
		if (pvar[pa->var] == 0)
			factor_lin = 0.0;
	}
	hprsat_free(&vhead);

	for (ADD *pa = afirst(); pa; pa = pa->next())
		pa->expand_all(pvar);

	return (*this);
}

MUL &
MUL :: zero()
{
	factor_lin = 0.0;
	factor_sqrt = 1.0;
	hprsat_free(&vhead);
	hprsat_free(&ahead);
	return (*this);
}

/* greatest common divisor, Euclid equation */

static double
hprsat_gcd(double a, double b)
{
	double an;
	double bn;

	if (a < 0.0)
		a = -a;
	if (b < 0.0)
		b = -b;

	while (b != 0.0) {
		an = b;
		bn = fmod(a, b);
		a = an;
		b = bn;
	}
	return (a);
}

MUL &
MUL :: doGCD(const MUL &other)
{
	VAR *va;
	VAR *vb;
	VAR *vn;

	ADD *aa;
	ADD *ab;
	ADD *an;

	factor_lin = hprsat_gcd(factor_lin, other.factor_lin);
	factor_sqrt = hprsat_gcd(factor_sqrt, other.factor_sqrt);

	va = vfirst();
	vb = other.vfirst();

	while (va && vb) {
		int ret = va->compare(*vb);
		if (ret < 0) {
			vn = va->next();
			delete va->remove(&vhead);
			va = vn;
		} else if (ret == 0) {
			va = va->next();
			vb = vb->next();
		} else {
			vb = vb->next();
		}
	}

	while (va) {
		vn = va->next();
		delete va->remove(&vhead);
		va = vn;
	}

	aa = afirst();
	ab = other.afirst();

	while (aa && ab) {
		int ret = aa->compare(*ab);
		if (ret < 0) {
			an = aa->next();
			delete aa->remove(&ahead);
			aa = an;
		} else if (ret == 0) {
			aa = aa->next();
			ab = ab->next();
		} else {
			ab = ab->next();
		}
	}

	while (aa) {
		an = aa->next();
		delete aa->remove(&ahead);
		aa = an;
	}
	return (*this);
}

MUL &
MUL :: operator /=(const MUL &other)
{
	VAR *va;
	VAR *vb;
	VAR *vn;

	ADD *aa;
	ADD *ab;
	ADD *an;

	factor_lin /= other.factor_lin;
	factor_sqrt /= other.factor_sqrt;

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

	aa = afirst();
	ab = other.afirst();

	while (aa && ab) {
		int ret = aa->compare(*ab);
		if (ret == 0) {
			an = aa->next();
			delete aa->remove(&ahead);
			aa = an;
		} else if (ret > 0) {
			ab = ab->next();
		} else {
			aa = aa->next();
		}
	}
	return (*this);
}
