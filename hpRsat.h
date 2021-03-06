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

#ifndef _HPRSAT_H_
#define	_HPRSAT_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/queue.h>

#include <iostream>

#define	HPRSAT_SWAP(a,b) do {			\
	__typeof(a) __tmp = (a);		\
	(a) = (b);				\
	(b) = __tmp;				\
} while (0)

#define	HPRSAT_PWR_UNIT 2
#define	HPRSAT_PWR_SQRT (HPRSAT_PWR_UNIT / 2)

typedef ssize_t hprsat_val_t;
typedef ssize_t hprsat_pwr_t;

typedef ssize_t hprsat_var_t;
constexpr hprsat_var_t HPRSAT_VAR_MAX = SSIZE_MAX;	/* exclusive */
constexpr hprsat_var_t HPRSAT_VAR_MIN = -1;		/* exclusive */

extern hprsat_val_t hprsat_global_mod;
extern hprsat_val_t *hprsat_global_sqrt;

class VAR;
typedef TAILQ_CLASS_HEAD(VAR_HEAD, VAR) VAR_HEAD_t;
typedef TAILQ_CLASS_ENTRY(VAR) VAR_ENTRY_t;

class MUL;
typedef TAILQ_CLASS_HEAD(MUL_HEAD, MUL) MUL_HEAD_t;
typedef TAILQ_CLASS_ENTRY(MUL) MUL_ENTRY_t;

class ADD;
typedef TAILQ_CLASS_HEAD(ADD_HEAD, ADD) ADD_HEAD_t;
typedef TAILQ_CLASS_ENTRY(ADD) ADD_ENTRY_t;

class VAR {
public:
	VAR_ENTRY_t entry;
	hprsat_var_t var;
	hprsat_pwr_t pwr;
	ADD *add;

	VAR(const VAR &other) {
		add = 0;
		*this = other;
	};
	VAR(hprsat_var_t _var,
	    hprsat_pwr_t _pwr = (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT) {
		var = _var;
		pwr = _pwr;
		add = 0;
	};

	~VAR();

	VAR *dup(void) const {
		return (new VAR(*this));
	};
	VAR & addVar(hprsat_var_t);
	VAR & mulVar(hprsat_var_t);
	bool isVariable() const;
	hprsat_val_t getConst(bool &) const;
	bool contains(hprsat_var_t) const;
	VAR & operator =(const VAR &other);
	VAR & operator *=(const VAR &other) {
		pwr += other.pwr;
		return (*this);
	};
	VAR & sort();

	void print(std::ostream &out = std::cout) const;

	int compare(const VAR & other, bool exprOnly = false) const;

	bool operator >(const VAR & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const VAR & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const VAR & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const VAR & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const VAR & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const VAR & other) const {
		return (compare(other) != 0);
	};
	VAR *sort_insert_tail(VAR_HEAD_t *, hprsat_val_t &);
	VAR & insert_tail(VAR_HEAD_t *phead) {
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	VAR & insert_head(VAR_HEAD_t *phead) {
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	VAR *remove(VAR_HEAD_t *phead) {
#ifdef DEBUG
		for (VAR *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
			assert(pa);
#endif
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	};
	VAR *next() const {
		return (TAILQ_NEXT(this, entry));
	};
};

extern hprsat_var_t hprsat_maxvar(const VAR_HEAD_t *, hprsat_var_t = HPRSAT_VAR_MAX);
extern hprsat_var_t hprsat_minvar(const VAR_HEAD_t *, hprsat_var_t = HPRSAT_VAR_MIN);
extern void hprsat_free(VAR_HEAD_t *);
extern bool hprsat_sort_var(VAR_HEAD_t *, hprsat_val_t &);

class MUL {
public:
	MUL_ENTRY_t entry;
	VAR_HEAD_t vhead;	/* and list */
	hprsat_val_t factor_lin;

	MUL() {
		TAILQ_INIT(&vhead);
		factor_lin = 1;
	};
	MUL(const MUL &other) {
		TAILQ_INIT(&vhead);
		*this = other;
	};

	MUL(hprsat_val_t _factor,
	    hprsat_var_t _var = HPRSAT_VAR_MIN,
	    hprsat_pwr_t _pwr = (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT) {
		TAILQ_INIT(&vhead);
		factor_lin = _factor;
		if (_var > HPRSAT_VAR_MIN)
			(new VAR(_var, _pwr))->insert_tail(&vhead);
	};

	~MUL();

	MUL *dup(void) const {
		return (new MUL(*this));
	};
	bool isNegative() const {
		return (factor_lin < 0);
	};
	MUL & negate() {
		factor_lin = -factor_lin;
		return (*this);
	};
	MUL & resetFactor() {
		factor_lin = 1;
		return (*this);
	};
	hprsat_var_t maxVar(hprsat_var_t limit = HPRSAT_VAR_MAX) const;
	hprsat_var_t minVar(hprsat_var_t limit = HPRSAT_VAR_MIN) const;
	MUL & addVar(hprsat_var_t delta);
	MUL & mulVar(hprsat_var_t factor);
	bool isVariable() const;
	hprsat_val_t getConst(bool &) const;
	bool contains(hprsat_var_t var) const;
	MUL & operator =(const MUL &other);
	MUL & operator +=(const MUL &other);
	MUL & operator *=(const MUL &other);
	MUL & operator /=(const MUL &other);
	MUL & zero();

	MUL operator /(const MUL &other) const {
		MUL temp(*this);
		temp /= other;
		return (temp);
	};

	MUL operator *(const MUL &other) const {
		MUL temp(*this);
		temp *= other;
		return (temp);
	};

	void print(std::ostream &out = std::cout) const;

	MUL & insert_tail(MUL_HEAD_t *phead){
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	MUL & insert_head(MUL_HEAD_t *phead){
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	MUL & insert_before(MUL *pother) {
		TAILQ_INSERT_BEFORE(pother, this, entry);
		return (*this);
	};
	MUL *remove(MUL_HEAD_t *phead){
#ifdef DEBUG
		for (MUL *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
			assert(pa);
#endif
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	};
	MUL *prev() const {
		return (TAILQ_PREV(this, MUL_HEAD, entry));
	};
	MUL *next() const {
		return (TAILQ_NEXT(this, entry));
	};
	VAR *vfirst(void) const {
		return (TAILQ_FIRST(&vhead));
	};
	VAR *vlast(void) const {
		return (TAILQ_LAST(&vhead, VAR_HEAD));
	};
	int compare(const MUL & other, bool exprOnly = false) const;

	bool operator >(const MUL & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const MUL & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const MUL & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const MUL & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const MUL & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const MUL & other) const {
		return (compare(other) != 0);
	};
	size_t count() const;
	MUL & sort();
	MUL & expand(hprsat_var_t, hprsat_val_t);
	MUL & expand_all(const hprsat_val_t *);
};

extern hprsat_var_t hprsat_maxvar(const MUL_HEAD_t *, hprsat_var_t = HPRSAT_VAR_MAX);
extern hprsat_var_t hprsat_minvar(const MUL_HEAD_t *, hprsat_var_t = HPRSAT_VAR_MIN);
extern void hprsat_free(MUL_HEAD_t *);
extern bool hprsat_sort_add(MUL_HEAD_t *);

class ADD {
public:
	ADD_ENTRY_t entry;
	MUL_HEAD_t head;

	ADD() {
		TAILQ_INIT(&head);
	};
	ADD(const ADD &other) {
		TAILQ_INIT(&head);
		*this = other;
	};
	ADD(const MUL &other) {
		TAILQ_INIT(&head);
		other.dup()->insert_tail(&head);
	};
	ADD(hprsat_val_t _factor,
	    hprsat_var_t _var = HPRSAT_VAR_MIN,
	    hprsat_pwr_t _pwr = (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT) {
		TAILQ_INIT(&head);
		if (_factor != 0 || _var > HPRSAT_VAR_MIN)
			(new MUL(_factor, _var, _pwr))->insert_tail(&head);
	};
	~ADD() {
		hprsat_free(&head);
	};
	ADD *dup(void) const {
		return (new ADD(*this));
	};
	hprsat_var_t maxVar(hprsat_var_t limit = HPRSAT_VAR_MAX) const {
		return (hprsat_maxvar(&head, limit));
	};
	hprsat_var_t minVar(hprsat_var_t limit = HPRSAT_VAR_MIN) const {
		return (hprsat_minvar(&head, limit));
	};
	ADD & addVar(hprsat_var_t delta) {
		for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->addVar(delta);
		return (*this);
	};
	ADD & mulVar(hprsat_var_t factor) {
		for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->mulVar(factor);
		return (*this);
	};
	bool isVariable() const {
		for (MUL *pa = first(); pa; pa = pa->next()) {
			if (pa->isVariable())
				return (true);
		}
		return (false);
	};
	hprsat_val_t getConst(bool &) const;
	ADD & operator +=(const ADD &other) {
		for (MUL *pa = other.first(); pa; pa = pa->next())
			(new MUL(*pa))->insert_tail(&head);
		return (sort());
	};
	ADD operator +(const ADD &other) const {
		ADD temp(*this);
		temp += other;
		return (temp);
	};
	ADD & operator -=(const ADD &other) {
		for (MUL *pa = other.first(); pa; pa = pa->next())
			(new MUL(*pa))->insert_tail(&head).negate();
		return (sort());
	};
	ADD operator -(const ADD &other) const {
		ADD temp(*this);
		temp -= other;
		return (temp);
	};
	ADD & operator *=(const ADD &other) {
		*this = *this * other;
		return (*this);
	};
	ADD operator *(const ADD &other) const;

	/*
	 * LOGIC operators, AND, OR, XOR and NOT.
	 */
	ADD & operator &=(const ADD &other) {
		*this = *this & other;
		return (*this);
	};
	ADD operator &(const ADD &other) const {
		return (*this * other);
	};

	ADD & operator |=(const ADD &other) {
		*this = *this | other;
		return (*this);
	};
	ADD operator |(const ADD &other) const {
		return (*this + other - *this * other);
	};

	ADD & operator ^=(const ADD &other) {
		*this = *this ^ other;
		return (*this);
	};
	ADD operator ^(const ADD &other) const {
		return (*this + other - (ADD(2) * *this * other));
	};

	ADD operator !() const {
		return (ADD(1) - *this);
	};

	bool contains(hprsat_var_t var) const {
		for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			if (pa->contains(var))
				return (true);
		}
		return (false);
	};
	bool contains(const MUL &var) const {
		for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next()) {
			if (*pa == var)
				return (true);
		}
		return (false);
	};
	ADD & operator =(const ADD &other) {
		if (this == &other)
			return (*this);
		hprsat_free(&head);
		for (MUL *pa = TAILQ_FIRST(&other.head); pa; pa = pa->next())
			pa->dup()->insert_tail(&head);
		return (*this);
	};
	ADD & sort();

	ADD & negate() {
		for (MUL *pa = first(); pa; pa = pa->next())
			pa->negate();
		return (*this);
	};

	void print(std::ostream &out = std::cout) const;

	ADD & insert_before(ADD *pother) {
		TAILQ_INSERT_BEFORE(pother, this, entry);
		return (*this);
	};
	ADD & insert_tail(ADD_HEAD_t *phead) {
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	ADD *sort_insert_tail(ADD_HEAD_t *, hprsat_val_t &);
	ADD & insert_head(ADD_HEAD_t *phead) {
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	ADD *remove(ADD_HEAD_t *phead){
#ifdef DEBUG
		for (ADD *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
			assert(pa);
#endif
		TAILQ_REMOVE(phead, this, entry);
		return (this);
	};
	ADD *prev() const {
		return (TAILQ_PREV(this, ADD_HEAD, entry));
	};
	ADD *next() const {
		return (TAILQ_NEXT(this, entry));
	};
	MUL *first(void) const {
		return (TAILQ_FIRST(&head));
	};
	MUL *last(void) const {
		return (TAILQ_LAST(&head, MUL_HEAD));
	};
	int compare(const ADD &other) const {
		const size_t na = count();
		const size_t nb = other.count();

		if (na > nb)
			return (1);
		else if (na < nb)
			return (-1);

		const MUL *pa = first();
		const MUL *pb = other.first();

		while (pa && pb) {
			const int ret = pa->compare(*pb);
			if (ret)
				return (ret);
			pa = pa->next();
			pb = pb->next();
		}
		return ((pa != 0) - (pb != 0));
	};
	bool operator >(const ADD & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const ADD & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const ADD & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const ADD & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const ADD & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const ADD & other) const {
		return (compare(other) != 0);
	};
	size_t count() const {
		size_t retval = 0;
		for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			retval++;
		return (retval);
	};
	bool isNonZeroVariable() const {
		bool isNaN;
		const hprsat_val_t value = getConst(isNaN);
		if (isNaN || value != 0)
			return (true);
		else
			return (false);
	};
	bool isNonZeroConst() const {
		bool isNaN;
		const hprsat_val_t value = getConst(isNaN);
		if (isNaN || value == 0)
			return (false);
		else
			return (true);
	};
	ADD & expand(hprsat_var_t var, hprsat_val_t value) {
		for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->expand(var, value);
		return (sort());
	};
	ADD & expand_all(const hprsat_val_t *pvar) {
		for (MUL *pa = TAILQ_FIRST(&head); pa; pa = pa->next())
			pa->expand_all(pvar);
		return (sort());
	};
	ADD & align();
	ADD & xform_fwd(hprsat_var_t);
	ADD & xform_inv(hprsat_var_t);
	ADD expandPower(hprsat_pwr_t = (hprsat_global_mod - 1) * HPRSAT_PWR_UNIT);
	ADD raisePower(hprsat_pwr_t);
	ADD & defactor();
};

extern hprsat_var_t hprsat_maxvar(const ADD_HEAD_t *, hprsat_var_t = HPRSAT_VAR_MAX);
extern hprsat_var_t hprsat_minvar(const ADD_HEAD_t *, hprsat_var_t = HPRSAT_VAR_MIN);
extern void hprsat_free(ADD_HEAD_t *);

/* Complex version of ADD class. */

class CADD {
public:
	ADD x;
	ADD y;

	CADD() {};

	CADD(const CADD &other) {
		*this = other;
	};
	CADD(const ADD &other) {
		x = other;
	};
	CADD *dup(void) const {
		return (new CADD(*this));
	};
	CADD & operator +=(const CADD &other) {
		x += other.x;
		y += other.y;
		return (*this);
	};
	CADD operator +(const CADD &other) const {
		CADD temp(*this);
		temp += other;
		return (temp);
	};
	CADD & operator -=(const CADD &other) {
		x -= other.x;
		y -= other.y;
		return (*this);
	};
	CADD operator -(const CADD &other) const {
		CADD temp(*this);
		temp -= other;
		return (temp);
	};
	CADD & operator *=(const CADD &other) {
		*this = *this * other;
		return (*this);
	};
	CADD operator *(const CADD &other) const {
		CADD temp;
		temp.x = x * other.x - y * other.y;
		temp.y = x * other.y + y * other.x;
		return (temp);
	};
	bool contains(hprsat_var_t var) const {
		return (x.contains(var) || y.contains(var));
	};
	CADD & operator =(const CADD &other) {
		if (this == &other)
			return (*this);
		x = other.x;
		y = other.y;
		return (*this);
	};
	CADD & negate() {
		x.negate();
		y.negate();
		return (*this);
	};

	void print(std::ostream &out = std::cout) const {
		out << "{";
		x.print(out);
		out << ",";
		y.print(out);
		out << "}";
	};
};

/* solve functions */

typedef bool (hprsat_solve_callback_t)(void *, hprsat_val_t *);

extern bool hprsat_solve(ADD_HEAD_t *, ADD_HEAD_t *, hprsat_var_t *, bool = false, bool = false);
extern bool hprsat_solve_first(ADD_HEAD_t *, hprsat_val_t *, bool = false);
extern size_t hprsat_solve_count(ADD_HEAD_t *, hprsat_val_t *);
extern bool hprsat_solve_callback(ADD *, hprsat_val_t *, hprsat_solve_callback_t *, void *);
extern void hprsat_underiv(ADD_HEAD_t *, ADD_HEAD_t *);

/* CNF functions */

extern int hprsat_loadcnf(std::istream &, ADD_HEAD_t *, hprsat_var_t *, hprsat_var_t *);

/* digraph functions */

extern void hprsat_print_digraph(std::ostream &, ADD_HEAD_t *);

/* elevate functions */

extern bool hprsat_elevate_add(ADD_HEAD_t *, size_t = 64);

/* fast Fourier transforms */

extern void hprsat_fft_fwd(CADD *, uint8_t, bool = true);
extern void hprsat_fft_inv(CADD *, uint8_t, bool = true);
extern void hprsat_fft_mul(const CADD *, const CADD *, CADD *, uint8_t);

/* parsing functions */

extern int hprsat_parse(std::istream &, ADD_HEAD_t *, hprsat_var_t *, bool = false);

/* prime functions */

extern void hprsat_set_global_modulus(hprsat_val_t);
extern void hprsat_do_inverse(hprsat_val_t, hprsat_val_t, hprsat_val_t &);

/* simplify functions */

extern bool hprsat_simplify_add(ADD_HEAD_t *, bool = false);

/* trigometry functions */

extern ADD hprsat_sin_32(uint32_t angle, uint32_t mask = -1U, hprsat_var_t var = 0);
extern ADD hprsat_cos_32(uint32_t angle, uint32_t mask = -1U, hprsat_var_t var = 0);
extern CADD hprsat_cosinus_32(uint32_t angle, uint32_t mask = -1U, hprsat_var_t var = 0);

#endif					/* _HPRSAT_H_ */
