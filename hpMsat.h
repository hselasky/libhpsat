/*-
 * Copyright (c) 2020-2023 Hans Petter Selasky. All rights reserved.
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

#ifndef _HPMSAT_H_
#define	_HPMSAT_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/queue.h>

#include <iostream>

#define	HPMSAT_SWAP(a,b) do {	       	\
	typeof(a) __tmp = (a);	       	\
	(a) = (b);		       	\
	(b) = __tmp;		       	\
} while (0)

typedef ssize_t hpMsat_val_t;
typedef size_t hpMsat_var_t;

constexpr hpMsat_var_t HPMSAT_VAR_MAX = SIZE_MAX / 2; /* exclusive */
constexpr hpMsat_var_t HPMSAT_VAR_MIN = 1; /* inclusive */
constexpr hpMsat_var_t HPMSAT_VAR_ONE = 0;

class VAR {
public:
	hpMsat_var_t var;
	hpMsat_val_t val;

	VAR(hpMsat_val_t _val = 0) {
		var = HPMSAT_VAR_ONE;
		val = _val;
	};
	VAR(hpMsat_var_t _var, hpMsat_val_t _val) {
		var = _var;
		val = _val;
	};
	VAR(const VAR &other) {
		*this = other;
	};
	VAR & operator =(const VAR &other) {
		if (this == &other)
			return (*this);
		var = other.var;
		val = other.val;
		return (*this);
	};
	int compare(const VAR & other) const {
		if (var > other.var)
			return (1);
		else if (var < other.var)
			return (-1);
		else
			return (0);
	};
	bool contains(hpMsat_var_t _var) const {
		return (var == _var);
	};
	hpMsat_val_t expand_all(const int8_t *pval) const {
		return (val * pval[var]);
	};
	VAR & expand(hpMsat_var_t _var, int8_t value) {
		if (var == _var) {
			var = HPMSAT_VAR_ONE;
			val *= value;
		}
		return (*this);
	};
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
	void print(std::ostream &out = std::cout) const {
		if (var == HPMSAT_VAR_ONE) {
			out << val;
		} else {
			if (val != 1) {
				if (val == -1)
					out << "-";
				else
					out << val << "*";
			}
			out << "v[" << var << "]";
		}
	};
};

class EQ;
typedef TAILQ_CLASS_HEAD(EQ_HEAD, EQ) EQ_HEAD_t;
typedef TAILQ_CLASS_ENTRY(EQ) EQ_ENTRY_t;

extern void hpMsat_eq_add(const EQ &, const EQ &, EQ &, hpMsat_val_t, hpMsat_val_t);

class EQ {
public:
	EQ_ENTRY_t entry;
	VAR *pvar;
	size_t nvar;

	EQ() {
		/* defaults to logical false */
		memset(this, 0, sizeof(*this));
	};
	EQ(const EQ &other) {
		memset(this, 0, sizeof(*this));
		*this = other;
	};
	EQ(const VAR &other) {
		memset(this, 0, sizeof(*this));
		if (other.val != 0) {
			pvar = new VAR [1];
			nvar = 1;
			pvar[0] = other;
		}
	};
	~EQ() {
		delete [] pvar;
	};
	EQ *dup(void) const {
		return (new EQ(*this));
	};
	bool isZero() const {
		return (nvar == 0);
	};
	bool contains(hpMsat_var_t var) const {
		for (size_t x = 0; x != nvar; x++) {
			if (pvar[x].contains(var))
				return (true);
		}
		return (false);
	};
	EQ & operator =(const EQ &other) {
		if (this == &other)
			return (*this);
		nvar = other.nvar;
		delete [] pvar;
		if (nvar != 0) {
			pvar = new VAR [nvar];
			for (size_t x = 0; x != nvar; x++)
				pvar[x] = other.pvar[x];
		} else {
			pvar = 0;
		}
		return (*this);
	};
	EQ & operator +=(const EQ &other) {
		EQ temp;
		hpMsat_eq_add(*this, other, temp, 1, 1);
		*this = temp;
		return (*this);
	};
	EQ operator +(const EQ &other) const {
		EQ temp;
		hpMsat_eq_add(*this, other, temp, 1, 1);
		return (temp);
	};
	EQ & operator -=(const EQ &other) {
		EQ temp;
		hpMsat_eq_add(*this, other, temp, 1, -1);
		*this = temp;
		return (*this);
	};
	EQ operator -(const EQ &other) const {
		EQ temp;
		hpMsat_eq_add(*this, other, temp, 1, -1);
		return (temp);
	};
	EQ & expand(hpMsat_var_t var, int8_t value);

	hpMsat_var_t maxVar() const {
		if (nvar == 0)
			return (0);
		else
			return (pvar[nvar - 1].var);
	};

	size_t index_of_var(hpMsat_var_t, bool = true) const;

	hpMsat_val_t value_of_var(hpMsat_var_t var) const {
		size_t index = index_of_var(var, false);

		if (index >= nvar)
			return (0);
		else if (pvar[index].var == var)
			return (pvar[index].val);
		else
			return (0);
	};

	void get_range(hpMsat_val_t &min, hpMsat_val_t &max) const;

	EQ & mod(hpMsat_val_t mod);
	EQ & gcd();

	hpMsat_val_t expand_all(const int8_t *pval) const {
		hpMsat_val_t temp = 0;

		for (size_t x = 0; x != nvar; x++)
			temp += pvar[x].expand_all(pval);
		return (temp);
	};

	void print(std::ostream &out = std::cout) const {
		for (size_t x = 0; x != nvar; x++) {
			pvar[x].print(out);
			if (x + 1 != nvar && pvar[x + 1].val >= 0)
				out << "+";
		}
	};
	hpMsat_var_t queue() const {
		return (nvar == 0 ? 0 : pvar[nvar - 1].var);
	};
	EQ & insert_tail(EQ_HEAD_t *phead, bool single = true) {
		if (nvar == 0 || single) {
			TAILQ_INSERT_TAIL(phead, this, entry);
		} else {
			hpMsat_var_t v = queue();
			TAILQ_INSERT_TAIL(phead + v, this, entry);
		}
		return (*this);
	};
	EQ & insert_head(EQ_HEAD_t *phead, bool single = true) {
		if (nvar == 0 || single) {
			TAILQ_INSERT_HEAD(phead, this, entry);
		} else {
			hpMsat_var_t v = queue();
			TAILQ_INSERT_HEAD(phead + v, this, entry);
		}
		return (*this);
	};
	EQ *remove(EQ_HEAD_t *phead, bool single = true) {
		if (nvar == 0 || single) {
#ifdef DEBUG
			for (EQ *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
				assert(pa);
#endif
			TAILQ_REMOVE(phead, this, entry);
		} else {
			hpMsat_var_t v = queue();
#ifdef DEBUG
			/* check that element is on the list before removing */
			for (EQ *pq = TAILQ_FIRST(phead + v); pq != this; pq = pq->next())
				assert(pq != 0);
#endif
			TAILQ_REMOVE(phead + v, this, entry);
		}
		return (this);
	};
	EQ *prev() const {
		return (TAILQ_PREV(this, EQ_HEAD, entry));
	};
	EQ *next() const {
		return (TAILQ_NEXT(this, entry));
	};
};

/* generic functions */

extern void hpMsat_eq_sort(EQ &, hpMsat_val_t = 0);
extern void hpMsat_eq_gcd(EQ &);
extern hpMsat_var_t hpMsat_max_var(const EQ_HEAD_t *);
extern bool hpMsat_solve(EQ_HEAD_t *, int8_t **, hpMsat_var_t &);
extern void hpMsat_free(EQ_HEAD_t *);

/* logic related functions */

class V;
extern void hpMsat_logic_init(hpMsat_var_t = 1);
extern void hpMsat_logic_uninit();
extern void hpMsat_logic_concat(EQ_HEAD_t *);
extern void hpMsat_logic_build(const V &, const V &, V &, V &);
extern void hpMsat_logic_adder(const V &, const V &, const V &, V &, V &);
extern void hpMsat_logic_get_var(V *, hpMsat_var_t = 1);

class V {
public:
	hpMsat_var_t var;
	bool neg;

	V() {
		var = 0;
		neg = false;
	};
	V(const V &other) {
		*this = other;
	};
	V(hpMsat_var_t _var, bool _neg) {
		var = _var;
		neg = _neg;
	};
	V *dup(void) const {
		return (new V(*this));
	};
	V & operator =(const V &other) {
		if (this == &other)
			return (*this);
		var = other.var;
		neg = other.neg;
		return (*this);
	};
	V & operator ^=(const V &other) {
		V t[2];
		hpMsat_logic_build(*this, other, t[0], t[1]);
		*this = t[0];
		return (*this);
	};
	V operator ^(const V &other) const {
		V t[2];
		hpMsat_logic_build(*this, other, t[0], t[1]);
		return (t[0]);
	};
	V & operator &=(const V &other) {
		V t[2];
		hpMsat_logic_build(*this, other, t[0], t[1]);
		*this = t[1];
		return (*this);
	};
	V operator &(const V &other) const {
		V t[2];
		hpMsat_logic_build(*this, other, t[0], t[1]);
		return (t[1]);
	};
	V & operator |=(const V &other) {
		V t[4];
		hpMsat_logic_build(*this, other, t[0], t[1]);
		hpMsat_logic_build(t[0], t[1], t[2], t[3]);
		t[3].set_equal_to(0);
		*this = t[2];
		return (*this);
	};
	V operator |(const V &other) const {
		V t[4];
		hpMsat_logic_build(*this, other, t[0], t[1]);
		hpMsat_logic_build(t[0], t[1], t[2], t[3]);
		t[3].set_equal_to(0);
		return (t[2]);
	};

	V & set_equal_to(const V &);
	V & set_equal_to(hpMsat_val_t);

	void print(std::ostream &out = std::cout) const {
		if (neg)
			out << "-";
		out << "v" << var;
	};

	int compare(const V & other) const {
		if (var > other.var)
			return (1);
		else if (var < other.var)
			return (-1);
		else if (neg > other.neg)
			return (1);
		else if (neg < other.neg)
			return (-1);
		else
			return (0);
	};
	bool operator >(const V & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const V & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const V & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const V & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const V & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const V & other) const {
		return (compare(other) != 0);
	};
};

#endif					/* _HPMSAT_H_ */
