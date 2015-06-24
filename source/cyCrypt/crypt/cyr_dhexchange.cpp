/*
Copyright(C) thecodeway.com
*/
#include <cy_crypt.h>
#include "cyr_dhexchange.h"

namespace cyclone
{

/*
copy from skynet @cloudwu
https://github.com/cloudwu/skynet
https://github.com/thejinchao/dhexchange
*/

/* P =  2^128-159 = 0xffffffffffffffffffffffffffffff61 (The biggest 64bit prime) */
static const dhkey_t P = { { 0xffffffffffffff61ULL, 0xffffffffffffffffULL } };
static const dhkey_t INVERT_P = { { 159, 0 } };
static const dhkey_t G = { { 5, 0 } };

/*--------------------------------------------------------------------------*/
static int INLINE
_u128_is_zero(const dhkey_t& key) {
	return (key.dq.low == 0 && key.dq.high == 0);
}

/*--------------------------------------------------------------------------*/
static int INLINE
_u128_is_odd(const dhkey_t& key) {
	return (key.dq.low & 1);
}

/*--------------------------------------------------------------------------*/
static void INLINE 
_u128_lshift(dhkey_t* key) {
	uint64_t t = (key->dq.low >> 63) & 1;
	key->dq.high = (key->dq.high << 1) | t;
	key->dq.low = key->dq.low << 1;
}

/*--------------------------------------------------------------------------*/
static void INLINE
_u128_rshift(dhkey_t* key) {
	uint64_t t = (key->dq.high & 1) << 63;
	key->dq.high = key->dq.high >> 1;
	key->dq.low = (key->dq.low >> 1) | t;
}

/*--------------------------------------------------------------------------*/
static int INLINE
_u128_compare(const dhkey_t& a, const dhkey_t& b) {
	if (a.dq.high > b.dq.high) return 1;
	else if (a.dq.high == b.dq.high) {
		if (a.dq.low > b.dq.low) return 1;
		else if (a.dq.low == b.dq.low) return 0;
		else return -1;
	} else 
		return -1;
}

/*--------------------------------------------------------------------------*/
static void INLINE
_u128_add(dhkey_t* r, const dhkey_t& a, const dhkey_t& b) {
	uint64_t overflow = 0;
	uint64_t low = a.dq.low + b.dq.low;
	if (low < a.dq.low || low < b.dq.low) {
		overflow = 1;
	}

	r->dq.low = low;
	r->dq.high = a.dq.high + b.dq.high + overflow;
}

/*--------------------------------------------------------------------------*/
static void INLINE
_u128_add_i(dhkey_t* r, const dhkey_t& a, const uint64_t& b) {
	uint64_t overflow = 0;
	uint64_t low = a.dq.low + b;
	if (low < a.dq.low || low < b) {
		overflow = 1;
	}

	r->dq.low = low;
	r->dq.high = a.dq.high + overflow;
}

/*--------------------------------------------------------------------------*/
static void INLINE
_u128_sub(dhkey_t* r, const dhkey_t& a, const dhkey_t& b) {
	dhkey_t invert_b;
	invert_b.dq.low = ~b.dq.low;
	invert_b.dq.high = ~b.dq.high;
	_u128_add_i(&invert_b, invert_b, 1);
	_u128_add(r, a, invert_b);
}

/*--------------------------------------------------------------------------*/
/* r = a*b mod P */
static void
_mulmodp(dhkey_t* r, dhkey_t a, dhkey_t b)
{
	dhkey_t t;
	dhkey_t double_a;
	dhkey_t P_a;

	r->dq.low = r->dq.high = 0;
	while (!_u128_is_zero(b)) {
		if (_u128_is_odd(b)) {
			_u128_sub(&t, P, a);

			if (_u128_compare(*r, t) >= 0) {
				_u128_sub(r, *r, t);
			}
			else {
				_u128_add(r, *r, a);
			}
		}
		double_a = a;
		_u128_lshift(&double_a);

		_u128_sub(&P_a, P, a);

		if (_u128_compare(a, P_a) >= 0) {
			_u128_add(&a, double_a, INVERT_P);
		}
		else {
			a = double_a;
		}
		_u128_rshift(&b);
	}
}

/*--------------------------------------------------------------------------*/
/* r = a^b mod P (reduce) */
static void
_powmodp_r(dhkey_t* r, const dhkey_t& a, const dhkey_t& b)
{
	dhkey_t t;
	dhkey_t half_b = b;

	if (b.dq.high == 0 && b.dq.low == 1) {
		*r = a;
		return;
	}

	_u128_rshift(&half_b);

	_powmodp_r(&t, a, half_b);
	_mulmodp(&t, t, t);

	if (_u128_is_odd(b)) {
		_mulmodp(&t, t, a);
	}
	*r = t;
}

/*--------------------------------------------------------------------------*/
/* r = a^b mod P */
static void 
_powmodp(dhkey_t* r, dhkey_t a, dhkey_t b)
{
	if (_u128_compare(a, P)>0)
		_u128_sub(&a, a, P);

	_powmodp_r(r, a, b);
}

/*--------------------------------------------------------------------------*/
void DH_generate_key_pair(dhkey_t& public_key, dhkey_t& private_key)
{
	/* generate random private key */
	int i;
	for (i = 0; i < DH_KEY_LENGTH; i++) {
		private_key.bytes[i] = (uint8_t)(rand() & 0xFF);
	}

	/* pub_key = G^prv_key mod P*/
	_powmodp(&public_key, G, private_key);
}

/*--------------------------------------------------------------------------*/
void
DH_generate_key_secret(dhkey_t& secret_key, const dhkey_t& my_private, const dhkey_t& another_public)
{
	/* secret_key = other_key^prv_key mod P*/
	_powmodp(&secret_key, another_public, my_private);
}

}

