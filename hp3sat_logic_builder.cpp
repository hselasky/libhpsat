/*-
 * Copyright (c) 2021 Hans Petter Selasky. All rights reserved.
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

#include <sys/cdefs.h>

#include "hp3sat.h"

struct hpsat_lb {
	BITMAP_HEAD_t head;
	hpsat_var_t varnum;
	size_t maxvar;
};

static __thread struct hpsat_lb hpsat_lb;

void
hpsat_logic_builder_init(size_t maxvar)
{
	TAILQ_INIT(&hpsat_lb.head);
	hpsat_lb.varnum = 0;
	hpsat_lb.maxvar = maxvar;
}

void
hpsat_logic_builder_finish(void)
{
	hpsat_free(&hpsat_lb.head);
}

variable_t
hpsat_logic_builder_new_variable(void)
{
	return (variable_t(BITMAP(hpsat_lb.varnum++, false)));
}

BITMAP_HEAD_t *
hpsat_logic_builder_equation(void)
{
	return (&hpsat_lb.head);
}

size_t
hpsat_logic_builder_maxvar(void)
{
	return (hpsat_lb.maxvar);
}

variable_t &
variable_t :: operator =(const variable_t &other)
{
	if (&other != this)
		v = other.v;
	return (*this);
}

bool
variable_t :: isInverted(void) const
{
	return (v.isInverted());
}

void
variable_t :: toggleInverted(void)
{
	v.toggleInverted();
}

void
variable_t :: equal_to_const(bool value) const
{
	(new BITMAP(v ^ BITMAP(value)))->sort().insert_tail(&hpsat_lb.head);
}

void
variable_t :: equal_to_var(const variable_t &other) const
{
	(new BITMAP(v ^ other.v))->sort().insert_tail(&hpsat_lb.head);
}

variable_t
variable_t :: operator ~(void) const
{
	variable_t temp = *this;
	temp.toggleInverted();
	return (temp);
}

variable_t
variable_t :: operator &(const variable_t &other) const
{
	const variable_t retval = hpsat_logic_builder_new_variable();

	(new BITMAP(retval.v ^ (v & other.v)))->sort().insert_tail(&hpsat_lb.head);

	return (retval);
}

variable_t &
variable_t :: operator &=(const variable_t &other)
{
	*this = *this & other;
	return (*this);
}

variable_t
variable_t :: operator ^(const variable_t &other) const
{
	const variable_t retval = hpsat_logic_builder_new_variable();

	(new BITMAP(retval.v ^ (v ^ other.v)))->sort().insert_tail(&hpsat_lb.head);

	return (retval);
}

variable_t &
variable_t :: operator ^=(const variable_t &other)
{
	*this = *this ^ other;
	return (*this);
}

variable_t
variable_t :: operator |(const variable_t &other) const
{
	const variable_t retval = hpsat_logic_builder_new_variable();

	(new BITMAP(retval.v ^ (v | other.v)))->sort().insert_tail(&hpsat_lb.head);

	return (retval);
}

variable_t &
variable_t :: operator |=(const variable_t &other)
{
	*this = *this | other;
	return (*this);
}

var_t :: var_t(void)
{
	z = new variable_t [hpsat_lb.maxvar];
}

var_t :: var_t(const var_t &other)
{
	z = new variable_t [hpsat_lb.maxvar];

	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		z[x] = other.z[x];
}

var_t :: ~var_t(void)
{
	delete [] z;
}

var_t &
var_t :: operator =(const var_t &other)
{
	if (&other != this) {
		for (size_t x = 0; x != hpsat_lb.maxvar; x++)
			z[x] = other.z[x];
	}
	return (*this);
}

var_t &
var_t :: alloc(size_t max, bool is_signed)
{
	const variable_t zero;

	if (max == 0)
		max = hpsat_lb.maxvar;
	for (size_t x = 0; x != max; x++)
		z[x] = hpsat_logic_builder_new_variable();
	for (size_t x = max; x != hpsat_lb.maxvar; x++)
		z[x] = is_signed ? z[max - 1] : zero;
	return (*this);
}

var_t &
var_t :: from_const(uint64_t var)
{
	for (size_t x = 0; x != hpsat_lb.maxvar; x++) {
		if (x >= 64)
			z[x].v = BITMAP(false);
		else
			z[x].v = BITMAP((var >> x) & 1);
	}
	return (*this);
}

var_t &
var_t :: equal_to_const(bool other)
{
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		z[x].equal_to_const(other);
	return (*this);
}

var_t &
var_t :: equal_to_var(const var_t &other)
{
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		z[x].equal_to_var(other.z[x]);
	return (*this);
}

var_t
var_t :: operator ~(void) const
{
	var_t r = *this;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		r.z[x].toggleInverted();
	return (r);
}

var_t
var_t :: operator ^(const var_t &other) const
{
	var_t c;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		c.z[x] = z[x] ^ other.z[x];
	return (c);
}

var_t &
var_t :: operator ^=(const var_t &other)
{
	*this = *this ^ other;
	return (*this);
};

var_t
var_t :: operator ^(const variable_t &other) const
{
	var_t c;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		c.z[x] = z[x] ^ other;
	return (c);
}

var_t
var_t :: operator &(const var_t &other) const
{
	var_t c;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		c.z[x] = z[x] & other.z[x];
	return (c);
}

var_t &
var_t :: operator &=(const var_t &other)
{
	*this = *this & other;
	return (*this);
}

var_t
var_t :: operator &(const variable_t &other) const
{
	var_t c;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		c.z[x] = z[x] & other;
	return (c);
}

var_t
var_t :: operator |(const var_t &other) const
{
	var_t c;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		c.z[x] = z[x] | other.z[x];
	return (c);
}

var_t &
var_t :: operator |=(const var_t &other)
{
	*this = *this | other;
	return (*this);
}

var_t
var_t :: operator |(const variable_t &other) const
{
	var_t c;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		c.z[x] = z[x] | other;
	return (c);
}

var_t
var_t :: operator <<(size_t shift) const
{
	var_t c;
	if (shift < hpsat_lb.maxvar) {
		for (size_t x = 0; x != hpsat_lb.maxvar - shift; x++)
			c.z[x + shift] = z[x];
	}
	return (c);
}

var_t &
var_t :: operator <<=(size_t shift)
{
	*this = *this << shift;
	return (*this);
}

var_t
var_t :: operator >>(size_t shift) const
{
	var_t c;
	if (shift < hpsat_lb.maxvar) {
		for (size_t x = shift; x != hpsat_lb.maxvar; x++)
			c.z[x - shift] = z[x];
	}
	return (c);
}

var_t &
var_t :: operator >>=(size_t shift)
{
	*this = *this >> shift;
	return (*this);
}

var_t
var_t :: operator +(const var_t &other) const
{
	const var_t &a = *this;
	const var_t &b = other;
	var_t c;

	c.alloc();

	/*
	 * Build equation for addition after HP Selasky 2021:
	 * a + b = c
	 */
	const var_t t = (a ^ b);
	const var_t u = (a | b);

	(t ^ c ^ (u << 1) ^ ((t & c) << 1)).equal_to_const(false);

	return (c);
}

var_t &
var_t :: operator +=(const var_t &other)
{
	*this = *this + other;
	return (*this);
}

var_t
var_t :: operator -(const var_t &other) const
{
	var_t a;
	const var_t &b = other;
	const var_t &c = *this;

	a.alloc();

	/*
	 * Build equation for addition after HP Selasky 2021:
	 * a = c - b
	 */
	const var_t t = (a ^ b);
	const var_t u = (a | b);

	(t ^ c ^ (u << 1) ^ ((t & c) << 1)).equal_to_const(false);

	return (a);
}

var_t &
var_t :: operator -=(const var_t &other)
{
	*this = *this - other;
	return (*this);
}

var_t
var_t :: operator *(const var_t &other) const
{
	var_t r;
	for (size_t x = 0; x != hpsat_lb.maxvar; x++)
		r = r + ((*this & other.z[x]) << x);
	return (r);
}

var_t &
var_t :: operator *=(const var_t &other)
{
	*this = *this * other;
	return (*this);
}

var_t
var_t :: operator %(const var_t &other) const
{
	var_t r = *this;

	for (size_t max = hpsat_lb.maxvar; max--; ) {
		if (!other.z[max].v.isZero()) {
			for (size_t x = (hpsat_lb.maxvar - max); x--; ) {
				var_t temp = (other << x);
				r -= (temp & (temp >= r));
			}
			break;
		}
	}
	return (r);
}

var_t &
var_t :: operator %=(const var_t &other)
{
	*this = *this % other;
	return (*this);
}

variable_t
var_t :: operator >(const var_t &other) const
{
	return (other - *this).z[hpsat_lb.maxvar - 1];
}

variable_t
var_t :: operator >=(const var_t &other) const
{
	return ~(*this - other).z[hpsat_lb.maxvar - 1];
}

variable_t
var_t :: operator <(const var_t &other) const
{
	return (*this - other).z[hpsat_lb.maxvar - 1];
}

variable_t
var_t :: operator <=(const var_t &other) const
{
	return ~(other - *this).z[hpsat_lb.maxvar - 1];
}
