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

#ifndef _HPNSAT_H_
#define	_HPNSAT_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/queue.h>

#include <iostream>

#define	HPNSAT_SWAP(a,b) do {			\
	__typeof(a) __tmp = (a);		\
	(a) = (b);				\
	(b) = __tmp;				\
} while (0)

typedef ssize_t hpNsat_var_t;
typedef ssize_t hpNsat_val_t;
constexpr hpNsat_var_t HPNSAT_VAR_MAX = SSIZE_MAX / 2; /* exclusive */
constexpr hpNsat_var_t HPNSAT_VAR_MIN = -HPNSAT_VAR_MAX;		/* exclusive */

class BITMAP {
public:
	BITMAP() {
		var = HPNSAT_VAR_MIN;
		val = 0;
	};
	BITMAP(const BITMAP &other) {
		var = other.var;
		val = other.val;
	};
	BITMAP(hpNsat_var_t _var, hpNsat_val_t _val) {
		var = _var;
		val = _val;
	};
	BITMAP *dup(void) const {
		return (new BITMAP(*this));
	};

	hpNsat_var_t var;
	hpNsat_val_t val;

	bool contains(hpNsat_var_t _var) const {
		return (var == _var);
	};
	BITMAP & operator =(const BITMAP &other) {
		if (this == &other)
			return (*this);
		var = other.var;
		val = other.val;
		return (*this);
	};
	bool expand_all(const hpNsat_val_t *pval) const {
		return (pval[var] == val);
	};
	void stats_all(hpNsat_val_t *pval) const {
		if (pval[var] < val)
			pval[var] = val;
	};
	void print(std::ostream &out = std::cout) const {
		out << "v" << var << "=" << val;
	};

	int compare(const BITMAP & other) const {
		if (var > other.var)
			return (1);
		else if (var < other.var)
			return (-1);
		else if (val > other.val)
			return (1);
		else if (val < other.val)
			return (-1);
		else
			return (0);
	};

	bool operator >(const BITMAP & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const BITMAP & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const BITMAP & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const BITMAP & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const BITMAP & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const BITMAP & other) const {
		return (compare(other) != 0);
	};
};

class ANDMAP;
typedef TAILQ_CLASS_HEAD(ANDMAP_HEAD, ANDMAP) ANDMAP_HEAD_t;
typedef TAILQ_CLASS_ENTRY(ANDMAP) ANDMAP_ENTRY_t;

class ANDMAP {
public:
	ANDMAP_ENTRY_t entry;
	BITMAP *bitmaps;
	size_t nummaps;

	ANDMAP() {
		/* defaults to logical true */
		memset(this, 0, sizeof(*this));
	};
	ANDMAP(const ANDMAP &other) {
		memset(this, 0, sizeof(*this));
		*this = other;
	};
	ANDMAP(const BITMAP &other) {
		memset(this, 0, sizeof(*this));
		nummaps = 1;
		bitmaps = new BITMAP [nummaps];
		bitmaps[0] = other;
	};
	ANDMAP(hpNsat_var_t var, hpNsat_val_t val) {
		memset(this, 0, sizeof(*this));
		nummaps = 1;
		bitmaps = new BITMAP [nummaps];
		bitmaps[0] = BITMAP(var, val);
	};
	~ANDMAP() {
		delete [] bitmaps;
	};
	ANDMAP *dup(void) const {
		return (new ANDMAP(*this));
	};
	bool isOne() const {
		return (nummaps == 0);
	};
	bool isZero() const {
		if (nummaps == 0)
			return (false);
		for (size_t x = 0; x != nummaps - 1; x++) {
			if (bitmaps[x].var != bitmaps[x + 1].var)
				continue;
			if (bitmaps[x].val != bitmaps[x + 1].val)
				return (true);
		}
		return (false);
	};
	ANDMAP & setZero() {
		nummaps = 2;
		delete [] bitmaps;
		bitmaps = new BITMAP [nummaps];
		bitmaps[0] = BITMAP(0,0);
		bitmaps[1] = BITMAP(0,1);
		return (*this);
	};
	hpNsat_var_t maxVar(hpNsat_var_t limit = HPNSAT_VAR_MAX) const {
		for (size_t x = nummaps; x--; ) {
			if (bitmaps[x].var < limit)
				return (bitmaps[x].var);
		}
		return (HPNSAT_VAR_MIN);
	};
	hpNsat_var_t minVar(hpNsat_var_t limit = HPNSAT_VAR_MIN) const {
		for (size_t x = 0; x != nummaps; x++) {
			if (bitmaps[x].var > limit)
				return (bitmaps[x].var);
		}
		return (HPNSAT_VAR_MIN);
	};
	bool contains(hpNsat_var_t var) const {
		for (size_t x = 0; x != nummaps; x++) {
			if (bitmaps[x].contains(var))
				return (true);
		}
		return (false);
	};
	ANDMAP & operator =(const ANDMAP &other) {
		if (this == &other)
			return (*this);
		nummaps = other.nummaps;
		delete [] bitmaps;
		bitmaps = new BITMAP [nummaps];
		for (size_t x = 0; x != nummaps; x++)
			bitmaps[x] = other.bitmaps[x];
		return (*this);
	};
	ANDMAP & operator &=(const ANDMAP &other) {
		ANDMAP temp;
		size_t x = 0;
		size_t y = 0;

		temp.bitmaps = new BITMAP [nummaps + other.nummaps];

		/* keep it sorted */
		while (x != nummaps && y != other.nummaps) {
			if (bitmaps[x] > other.bitmaps[y]) {
				temp.bitmaps[temp.nummaps++] = other.bitmaps[y++];
			} else if (bitmaps[x] < other.bitmaps[y]) {
				temp.bitmaps[temp.nummaps++] = bitmaps[x++];
			} else {
				temp.bitmaps[temp.nummaps++] = bitmaps[x];
				x++;
				y++;
			}
		}
		while (x != nummaps) {
			temp.bitmaps[temp.nummaps++] = bitmaps[x++];
		}
		while (y != other.nummaps) {
			temp.bitmaps[temp.nummaps++] = other.bitmaps[y++];
		}

		*this = temp;

		return (*this);
	};
	ANDMAP operator &(const ANDMAP &other) const {
		ANDMAP temp(*this);
		temp &= other;
		return (temp);
	};
	ANDMAP & expand(hpNsat_var_t var, hpNsat_val_t value) {
		for (size_t x = 0; x != nummaps; ) {
			if (bitmaps[x].contains(var) == false) {
				x++;
				continue;
			}
			if (bitmaps[x].val != value)
				return (setZero());
			for (size_t y = x + 1; y != nummaps; y++)
				bitmaps[y - 1] = bitmaps[y];
			nummaps--;
		}
		return (*this);
	};
	bool expand_all(const hpNsat_val_t *pval) const {
		bool retval = 1;
		for (size_t x = 0; x != nummaps; x++)
			retval &= bitmaps[x].expand_all(pval);
		return (retval);
	};
	void stats_all(hpNsat_val_t *pval) const {
		for (size_t x = 0; x != nummaps; x++)
			bitmaps[x].stats_all(pval);
	};
	void print(std::ostream &out = std::cout) const {
		if (isOne()) {
			out << "true";
		} else if (isZero()) {
			out << "false";
		} else {
			for (size_t x = 0; x != nummaps; x++) {
				out << "(";
				bitmaps[x].print(out);
				out << ")";
				if (x + 1 != nummaps)
					out << "&";
			}
		}
	};
	ANDMAP & insert_tail(ANDMAP_HEAD_t *phead, bool single = true) {
		if (nummaps == 0 || single) {
			TAILQ_INSERT_TAIL(phead, this, entry);
		} else {
			hpNsat_var_t v = bitmaps[0].var;
			TAILQ_INSERT_TAIL(phead + v, this, entry);
		}
		return (*this);
	};
	ANDMAP & insert_head(ANDMAP_HEAD_t *phead, bool single = true) {
		if (nummaps == 0 || single) {
			TAILQ_INSERT_HEAD(phead, this, entry);
		} else {
			hpNsat_var_t v = bitmaps[0].var;
			TAILQ_INSERT_HEAD(phead + v, this, entry);
		}
		return (*this);
	};
	ANDMAP *remove(ANDMAP_HEAD_t *phead, bool single = true) {
		if (nummaps == 0 || single) {
#ifdef DEBUG
			for (ANDMAP *pa = TAILQ_FIRST(phead); pa != this; pa = pa->next())
				assert(pa);
#endif
			TAILQ_REMOVE(phead, this, entry);
		} else {
			hpNsat_var_t v = bitmaps[0].var;
#ifdef DEBUG
			/* check that element is on the list before removing */
			for (ANDMAP *pq = TAILQ_FIRST(phead + v); pq != this; pq = pq->next())
				assert(pq != 0);
#endif
			TAILQ_REMOVE(phead + v, this, entry);
		}
		return (this);
	};
	ANDMAP *prev() const {
		return (TAILQ_PREV(this, ANDMAP_HEAD, entry));
	};
	ANDMAP *next() const {
		return (TAILQ_NEXT(this, entry));
	};
	int compare(const ANDMAP & other) const {
		size_t x;

		if (nummaps > other.nummaps)
			return (1);
		else if (nummaps < other.nummaps)
			return (-1);

		for (x = 0; x != nummaps; x++) {
			const int ret = bitmaps[x].compare(other.bitmaps[x]);
			if (ret)
				return (ret);
		}
		return (0);
	};
	bool operator >(const ANDMAP & other) const {
		return (compare(other) > 0);
	};
	bool operator <(const ANDMAP & other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const ANDMAP & other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const ANDMAP & other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const ANDMAP & other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const ANDMAP & other) const {
		return (compare(other) != 0);
	};
};

extern hpNsat_var_t hpNsat_maxvar(const ANDMAP_HEAD_t *, hpNsat_var_t = HPNSAT_VAR_MAX);
extern hpNsat_var_t hpNsat_minvar(const ANDMAP_HEAD_t *, hpNsat_var_t = HPNSAT_VAR_MIN);
extern size_t hpNsat_numvar(const ANDMAP_HEAD_t *, hpNsat_var_t = HPNSAT_VAR_MAX, BITMAP * = 0);
extern bool hpNsat_sort_or(ANDMAP_HEAD_t *);
extern void hpNsat_free(ANDMAP_HEAD_t *);

/* CNF functions */

extern int hpNsat_loadcnf(std::istream &, ANDMAP_HEAD_t *, hpNsat_var_t *, hpNsat_var_t *, hpNsat_val_t **);

/* solve functions */

typedef bool (hpNsat_solve_callback_t)(void *, hpNsat_val_t *);

extern bool hpNsat_solve(ANDMAP_HEAD_t *, ANDMAP_HEAD_t *, hpNsat_var_t, const hpNsat_val_t *);
extern size_t hpNsat_solve_count(ANDMAP_HEAD_t *, hpNsat_val_t *, const hpNsat_val_t *);
extern bool hpNsat_solve_callback(ANDMAP *, hpNsat_val_t *, const hpNsat_val_t *, hpNsat_solve_callback_t *, void *);

#endif					/* _HPNSAT_H_ */
