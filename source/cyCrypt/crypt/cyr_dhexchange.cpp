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
*/

//-------------------------------------------------------------------------------------
uint128_t DHExchange::P = uint128_t(0xffffffffffffffffull, 0xffffffffffffff61ull);
uint128_t DHExchange::INVERT_P = uint128_t(159);
uint128_t DHExchange::G = uint128_t(5);

//-------------------------------------------------------------------------------------
DHExchange::DHExchange()
	: m_privateKey(0)
	, m_publicKey(0)
	, m_pairKey(0)
{
	//create random private key(a)
	m_privateKey.make_rand();

	//public key = G^a mod P
	_powmodp(m_publicKey, G, m_privateKey);
}

//-------------------------------------------------------------------------------------
DHExchange::~DHExchange()
{

}

//-------------------------------------------------------------------------------------
void DHExchange::_powmodp(uint128_t& r, uint128_t a, uint128_t b)
{
	if (a > P)
		a = a - P;

	_powmodp_r(r, a, b);
}

//-------------------------------------------------------------------------------------
void DHExchange::_powmodp_r(uint128_t& r, const uint128_t& a, const uint128_t& b)
{
	if (b == uint128_t(1)) {
		r = a;
		return;
	}

	uint128_t t;
	uint128_t half_b = b;
	half_b.shift_r();

	_powmodp_r(t, a, half_b);

	_mulmodp(t, t, t);
	if (b.is_odd()) {
		_mulmodp(t, t, a);
	}
	r = t;
}

//-------------------------------------------------------------------------------------
void DHExchange::_mulmodp(uint128_t& r, uint128_t a, uint128_t b)
{
	r = 0;
	while (!b.is_zero()) {
		if (b.is_odd()) {
			uint128_t t = P - a;
			if (r >= t) {
				r = r - t;
			}
			else {
				r = r + a;
			}
		}
		uint128_t double_a = a;
		double_a.shift_l();
		if (a >= P - a) {
			a = double_a + INVERT_P;
		}
		else {
			a = double_a;
		}
		b.shift_r();
	}
}

//-------------------------------------------------------------------------------------
uint128_t DHExchange::getPairKey(const uint128_t& anotherKey)
{
	//public key = G^a mod P
	_powmodp(m_pairKey, anotherKey, m_privateKey);
	return m_pairKey;
}


}

