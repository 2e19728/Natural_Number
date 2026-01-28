#pragma once

#include "asm.h"
#include "base.h"
#include "ntt_info.h"
#include "basic_natural.h"

#include <queue>
#include <cctype>
#include <format>
#include <string>
#include <iostream>
#include <algorithm>
#include <string_view>

struct natural :basic_natural {

	natural(uint64_t n = 0) {
		data[0] = n;
	}

	natural(std::string_view sv) {
		*this = sv;
	}

	natural(const natural& n) :basic_natural(n) {}

	natural(std::initializer_list<uint64_t> n) :basic_natural(n) {}

	natural(natural&& n) noexcept :basic_natural(std::move(n)) {}

	natural& std();

	natural& operator=(uint64_t);

	natural& operator=(std::string_view);

	natural& operator=(const natural&);

	natural& operator=(natural&&) noexcept;

	natural& hex(std::string_view);

	natural& dec(std::string_view);

	natural& _or(const natural*, const natural*);

	natural& _xor(const natural*, const natural*);

	natural& _and(const natural*, const natural*);

	natural& _shl(const natural*, uint64_t);

	natural& _shr(const natural*, uint64_t);

	natural& _add(const natural*, const natural*);

	natural& _sub(const natural*, const natural*);

	natural& _mul_base(const natural*, const natural*);

	natural& _sqr_base(const natural*);

	natural& _mul_NTT(const natural*, const natural*);

	natural& _sqr_NTT(const natural*);

	natural& _mul(const natural*, const natural*);

	natural& _sqr(const natural*);

	natural& _div(const natural*, const natural*, natural*);

	void _dec_split(int, natural&, natural&)const;
	
	void _print_dec(std::ostream&, int, bool)const;

	void _format_dec(std::format_context&, int, bool)const;

};

natural& natural::std() {
	if (!size) {
		size = 1;
		data[0] = 0;
		return *this;
	}
	while (--size)
		if (data[size])
			break;
	size++;
	return *this;
}

natural& natural::operator=(uint64_t n) {
	size = 1;
	data[0] = n;
	return *this;
}

natural& natural::operator=(std::string_view sv) {
	if (sv.size() > 1 && sv[0] == '0' && tolower(sv[1]) == 'x') {
		sv.remove_prefix(2);
		hex(sv);
	}
	else
		dec(sv);
	return *this;
}

natural& natural::operator=(const natural& n) {
	basic_natural::operator=(n);
	return *this;
}

natural& natural::operator=(natural&& n) noexcept {
	basic_natural::operator=(std::move(n));
	return *this;
}

natural& natural::hex(std::string_view sv) {
	static const char hexStr[8] = { 0, 48, 55, 87 };
	if (sv.empty()) {
		*this = 0;
		return *this;
	}
	resize(sv.size() + 15 >> 4);
	uint64_t i = -sv.size() & 15, j = size;
	const char* s = sv.data() - i;
	while (j--) {
		uint64_t x = 0;
		for (; i < 16; i++) {
			char c = s[i];
			c -= hexStr[c >> 5 & 3];
			x = x << 4 | c;
		}
		data[j] = x;
		s += 16;
		i = 0;
	}
	std();
	return *this;
}

bool operator<(const natural& a, const natural& b) {
	if (a.size != b.size)
		return a.size < b.size;
	uint64_t i = a.size;
	while (i--)
		if (a[i] != b[i])
			return a[i] < b[i];
	return 0;
}

bool operator>(const natural& a, const natural& b) {
	return b < a;
}

bool operator<=(const natural& a, const natural& b) {
	return !(b < a);
}

bool operator>=(const natural& a, const natural& b) {
	return !(a < b);
}

bool operator==(const natural& a, const natural& b) {
	if (a.size != b.size)
		return 0;
	uint64_t i = a.size;
	while (i--)
		if (a[i] != b[i])
			return 0;
	return 1;
}

bool operator!=(const natural& a, const natural& b) {
	return !(a == b);
}

natural& natural::_or(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	resize(ap->size);
	for (uint64_t i = 0; i < bp->size; i++)
		data[i] = ap->data[i] | bp->data[i];
	memcpy(data + bp->size, ap->data + bp->size, (ap->size - bp->size) * sizeof(uint64_t));
	return *this;
}

natural& operator|=(natural& a, const natural& b) {
	a._or(&a, &b);
	return a;
}

natural operator|(const natural& a, const natural& b) {
	natural d;
	d._or(&a, &b);
	return d;
}

natural& natural::_xor(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	resize(ap->size);
	for (uint64_t i = 0; i < bp->size; i++)
		data[i] = ap->data[i] ^ bp->data[i];
	memcpy(data + bp->size, ap->data + bp->size, (ap->size - bp->size) * sizeof(uint64_t));
	std();
	return *this;
}

natural& operator^=(natural& a, const natural& b) {
	a._xor(&a, &b);
	return a;
}

natural operator^(const natural& a, const natural& b) {
	natural d;
	d._xor(&a, &b);
	return d;
}

natural& natural::_and(const natural* ap, const natural* bp) {
	uint64_t s = std::min(ap->size, bp->size);
	resize(s);
	for (uint64_t i = 0; i < s; i++)
		data[i] = ap->data[i] & bp->data[i];
	std();
	return *this;
}

natural& operator&=(natural& a, const natural& b) {
	a._and(&a, &b);
	return a;
}

natural operator&(const natural& a, const natural& b) {
	natural d;
	d._and(&a, &b);
	return d;
}

natural& natural::_shl(const natural* ap, uint64_t b) {
	if (*ap == 0) {
		*this = 0;
		return *this;
	}
	uint64_t as = ap->size, n = b >> 6;
	b &= 63;
	if (b == 0) {
		resize(as + n);
		memmove(data + n, ap->data, as * sizeof(uint64_t));
		memset(data, 0, n * sizeof(uint64_t));
		return *this;
	}
	resize(as + n + 1);
	asmShl(b, as, ap->data, data + n);
	memset(data, 0, n * sizeof(uint64_t));
	std();
	return *this;
}

natural& operator<<=(natural& a, uint64_t b) {
	a._shl(&a, b);
	return a;
}

natural operator<<(const natural& a, uint64_t b) {
	natural d;
	d._shl(&a, b);
	return d;
}

natural& natural::_shr(const natural* ap, uint64_t b) {
	uint64_t as = ap->size, n = b >> 6;
	if (as <= n) {
		*this = 0;
		return *this;
	}
	b &= 63;
	resize(as - n);
	if (b == 0) {
		memmove(data, ap->data + n, (as - n) * sizeof(uint64_t));
		return *this;
	}
	asmShr(b, as - n, ap->data + n, data);
	std();
	return *this;
}

natural& operator>>=(natural& a, uint64_t b) {
	a._shr(&a, b);
	return a;
}

natural operator>>(const natural& a, uint64_t b) {
	natural d;
	d._shr(&a, b);
	return d;
}

natural& natural::_add(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	uint64_t as = ap->size, bs = bp->size;
	resize(as + 1);
	data[as] = add(data, ap->data, as, bp->data, bs);
	std();
	return *this;
}

natural& operator+=(natural& a, const natural& b) {
	a._add(&a, &b);
	return a;
}

natural operator+(const natural& a, const natural& b) {
	natural d;
	d._add(&a, &b);
	return d;
}

natural& natural::_sub(const natural* ap, const natural* bp) {
	if (ap->size < bp->size)
		std::swap(ap, bp);
	uint64_t as = ap->size, bs = bp->size;
	if (as == bs) {
		while (--bs)
			if ((*ap)[bs] != (*bp)[bs])
				break;
		if ((*ap)[bs] < (*bp)[bs])
			std::swap(ap, bp);
		resize(++bs);
		asmSub0(bs, data, ap->data, bp->data);
	}
	else {
		resize(as);
		sub(data, ap->data, as, bp->data, bs);
	}
	std();
	return *this;
}

natural& operator-=(natural& a, const natural& b) {
	a._sub(&a, &b);
	return a;
}

natural operator-(const natural& a, const natural& b) {
	natural d;
	d._sub(&a, &b);
	return d;
}

natural& natural::_mul_base(const natural* ap, const natural* bp) {
	if (this == ap || this == bp) {
		natural ans;
		ans._mul_base(ap, bp);
		*this = std::move(ans);
		return *this;
	}
	if (ap->size < bp->size)
		std::swap(ap, bp);
	resize(ap->size + bp->size);
	mul_base(data, ap->data, ap->size, bp->data, bp->size);
	std();
	return *this;
}

natural& natural::_sqr_base(const natural* p) {
	if (this == p) {
		natural ans;
		ans._sqr_base(p);
		*this = std::move(ans);
		return *this;
	}
	resize(p->size << 1);
	sqr_base(data, p->data, p->size);
	std();
	return *this;
}

int get_NTT_scale(uint64_t _Size1, uint64_t _Size2) {
	return 64 - std::countl_zero(_Size1 + _Size2 - 2);
}

void mul_save_NTT_info(natural&, const natural&, const natural&, NTT_info&);
void mul_load_NTT_info(natural&, const natural&, const natural&, const NTT_info&);

natural& natural::_mul_NTT(const natural* ap, const natural* bp) {
//*
	if (ap->size < bp->size)
		std::swap(ap, bp);
	uint64_t a0_size = ap->size + 1 >> 1;
	int NTT_scale = get_NTT_scale(ap->size, bp->size);
	int NTT_scale_opt = get_NTT_scale(a0_size, bp->size);
	if (NTT_scale == NTT_scale_opt) {
		NTT_info ni_a(*ap, NTT_scale), ni_b(*bp, NTT_scale);
		ni_a.NTT();
		ni_b.NTT();
		ni_a.mul(ni_a, ni_b);
		ni_a.INTT();
		ni_a.save(*this, ap->size + bp->size);
		std();
		return *this;
	}
	natural a0, a1;
	a0.resize(a0_size);
	a1.resize(ap->size - a0_size);
	memcpy(a0.data, ap->data, sizeof(uint64_t) * a0_size);
	memcpy(a1.data, ap->data + a0_size, sizeof(uint64_t) * a1.size);
	NTT_info ni_b;
	ni_b.NTT_scale = NTT_scale_opt;
	mul_save_NTT_info(a0, *bp, a0, ni_b);
	mul_load_NTT_info(a1, *bp, a1, ni_b);
	*this = (a1 << (a0_size << 6)) + a0;
	return *this;
//*/
/*
	int NTT_scale = get_NTT_scale(ap->size, bp->size);
	NTT_info ni_a(*ap, NTT_scale), ni_b(*bp, NTT_scale);
	ni_a.NTT();
	ni_b.NTT();
	ni_a.mul(ni_a, ni_b);
	ni_a.INTT();
	ni_a.save(*this, ap->size + bp->size);
	std();
	return *this;
//*/
}

natural& natural::_sqr_NTT(const natural* p) {
	int NTT_scale = get_NTT_scale(p->size, p->size);
	NTT_info ni(*p, NTT_scale);
	ni.NTT();
	ni.mul(ni, ni);
	ni.INTT();
	ni.save(*this, p->size * 2);
	std();
	return *this;
}

natural& natural::_mul(const natural* ap, const natural* bp) {
	if (std::min(ap->size, bp->size) < NTTmul_threshold)
		_mul_base(ap, bp);
	else
		_mul_NTT(ap, bp);
	return *this;
}

void mul_save_NTT_info(natural& d, const natural& a, const natural& b, NTT_info& ni_a) {
	if (ni_a.NTT_scale < NTTscale_threshold) {
		d._mul_base(&a, &b);
		ni_a.NTT_scale = 0;
	}
	else {
		ni_a.load(a, ni_a.NTT_scale);
		NTT_info ni_b(b, ni_a.NTT_scale);
		ni_a.NTT();
		ni_b.NTT();
		ni_b.mul(ni_a, ni_b);
		ni_b.INTT();
		ni_b.save(d, a.size + b.size);
		d.std();
	}
}

void mul_save_NTT_info(natural& d, const natural& a, const natural& b, NTT_info& ni_a, NTT_info& ni_b) {
	if (ni_a.NTT_scale < NTTscale_threshold) {
		d._mul_base(&a, &b);
		ni_a.NTT_scale = 0;
		ni_b.NTT_scale = 0;
	}
	else {
		ni_a.load(a, ni_a.NTT_scale);
		ni_b.load(b, ni_a.NTT_scale);
		ni_a.NTT();
		ni_b.NTT();
		NTT_info ni;
		ni.mul(ni_a, ni_b);
		ni.INTT();
		ni.save(d, a.size + b.size);
		d.std();
	}
}

void sqr_save_NTT_info(natural& d, const natural& a, NTT_info& ni_a) {
	if (ni_a.NTT_scale < NTTscale_threshold) {
		d._sqr_base(&a);
		ni_a.NTT_scale = 0;
	}
	else {
		ni_a.load(a, ni_a.NTT_scale);
		NTT_info ni_b;
		ni_a.NTT();
		ni_b.mul(ni_a, ni_a);
		ni_b.INTT();
		ni_b.save(d, a.size * 2);
		d.std();
	}
}

void mul_load_NTT_info(natural& d, const natural& a, const natural& b, const NTT_info& ni_a) {
	if (ni_a.NTT_scale == 0)
		d._mul_base(&a, &b);
	else {
		NTT_info ni_b(b, ni_a.NTT_scale);
		ni_b.NTT();
		ni_b.mul(ni_a, ni_b);
		ni_b.INTT();
		ni_b.save(d, a.size + b.size);
		d.std();
	}
}

void mul_load_NTT_info(natural& d, const natural& a, const natural& b, const NTT_info& ni_a, const NTT_info& ni_b) {
	if (ni_a.NTT_scale == 0)
		d._mul_base(&a, &b);
	else {
		NTT_info ni;
		ni.mul(ni_a, ni_b);
		ni.INTT();
		ni.save(d, a.size + b.size);
		d.std();
	}
}

natural& natural::_sqr(const natural* p) {
	if (p->size < NTTmul_threshold)
		_sqr_base(p);
	else
		_sqr_NTT(p);
	return *this;
}

natural& operator*=(natural& a, const natural& b) {
	a._mul(&a, &b);
	return a;
}

natural operator*(const natural& a, const natural& b) {
	natural d;
	d._mul(&a, &b);
	return d;
}

natural sqr(const natural& a) {
	natural d;
	d._sqr(&a);
	return d;
}

natural pow(const natural& b, uint64_t e) {
	if (e == 0)
		return 1;
	if (e == 1 || b < 2)
		return b;
	natural p = pow(b, e >> 1);
	p._sqr(&p);
	if (e & 1)
		p *= b;
	return p;
}

//	divs[divs.size - 1] == 1;
natural reciprocal(const natural& divs, uint64_t divd_size) {
	uint64_t expected_acc = divd_size + 1 - divs.size;
	natural ans = (1ull << 63 | divs[divs.size - 2] >> 1) + 1;
	if (ans[0] == 0)
		ans[0] = 1ull << 63;
	else
		ans[0] = asmDiv(0, 1ull << 63, ans[0]);

	while (1) {
		uint64_t nxt_acc = 0, cur_acc = ans.size;
		if (cur_acc >= expected_acc + 1) {
			ans >>= cur_acc - expected_acc << 6;
			return ans;
		}

		natural tmp;
		if (divs.size > 2 * cur_acc + 1)
			tmp._shr(&divs, divs.size - 2 * cur_acc - 1 << 6);
		else
			tmp._shl(&divs, 2 * cur_acc + 1 - divs.size << 6);

		NTT_info ni_ans(get_NTT_scale(tmp.size, 1));
		mul_save_NTT_info(tmp, ans, tmp, ni_ans);

		tmp.resize(tmp.size - cur_acc);
		for (uint64_t i = 0; i < tmp.size; i++)
			tmp[i] = ~tmp[i + cur_acc];
		tmp.std();

		int l_zero = std::countl_zero(tmp[tmp.size - 1]);
		nxt_acc = std::min(2 * cur_acc, (2 * cur_acc - tmp.size) * 2 + (l_zero + 31 >> 5));
		if (nxt_acc == NTTdiv_threshold)
			nxt_acc = NTTdiv_threshold - 1;
		tmp >>= 2 * cur_acc - nxt_acc << 6;
		if (tmp == 0)
			tmp = 1;

		mul_load_NTT_info(tmp, ans, tmp, ni_ans);
		ans <<= nxt_acc - cur_acc << 6;
		add(ans.data, ans.data, ans.size, tmp.data + cur_acc, tmp.size - cur_acc);
	}
}

uint64_t div_base(const natural& a, uint64_t b, natural& q) {
	q.resize(a.size);
	uint64_t r = 0, i = a.size;
	while (i--)
		q[i] = asmDivMod(a[i], r, b, &r);
	q.std();
	return r;
}

void div_iterative(const natural& a, const natural& b, natural& q, natural& r) {
	int n_shl = std::countl_zero(b[b.size - 1]) + 1 & 63;
	natural _r = reciprocal(b << n_shl, a.size + 1);
	natural _q = (a >> (b.size - 1 << 6)) * _r >> (a.size - b.size + 2 << 6) - n_shl;
	_r = a - _q * b;
	while (_r >= b) {
		_q += 1;
		_r -= b;
	}
	q = std::move(_q);
	r = std::move(_r);
}

natural reciprocal(const natural& divisor, uint64_t dividend_size, NTT_info& ni_divisor) {
	uint64_t expected_acc = dividend_size + 1 - divisor.size;
	natural ans = (1ull << 63 | divisor[divisor.size - 2] >> 1) + 1;
	ans[0] = ans[0] ? asmDiv(0, 1ull << 63, ans[0]) : 1ull << 63;
	
	for (uint64_t i = 1; i < divisor.size; i *= 2) {
		uint64_t prev_acc = ans.size, divisor_seg_size = 2 * prev_acc + 1;
		if (2 * i >= divisor.size)
			divisor_seg_size = divisor.size;
		uint64_t *divisor_seg = divisor.data + divisor.size - divisor_seg_size;

		natural tmp;
		NTT_info ni_ans;
		if (2 * i <= NTTdiv_threshold) {
			tmp.resize(divisor_seg_size + prev_acc);
			mul_base(tmp.data, divisor_seg, divisor_seg_size, ans.data, prev_acc);
			tmp.std();
		}
		else {
			int NTT_scale = get_NTT_scale(i, i);
			ni_ans.load(ans, NTT_scale);
			ni_divisor.load(divisor_seg, divisor_seg_size, NTT_scale);
			ni_ans.NTT();
			ni_divisor.NTT();
			NTT_info ni_tmp;
			ni_tmp.mul(ni_ans, ni_divisor);
			ni_tmp.INTT();
			ni_tmp.save(tmp, divisor_seg_size + prev_acc);
			tmp.std();
		}

		uint64_t n_shl = divisor_seg_size - 1 - prev_acc;
		while (tmp.size > n_shl + 1 && ~tmp[tmp.size - 1] == 0)
			tmp.size -= 1;
		int l_zero = std::countl_zero(~tmp[tmp.size - 1]);
		uint64_t cur_acc = std::min(2 * prev_acc, (2 * prev_acc - tmp.size + n_shl) * 2 + (l_zero + 31 >> 5));
		if (cur_acc == NTTdiv_threshold)
			cur_acc -= 1;
		n_shl += 2 * prev_acc - cur_acc;
		if (tmp.size <= n_shl)
			tmp = 1;
		else {
			tmp.resize(tmp.size - n_shl);
			for (uint64_t i = 0; i < tmp.size; i++)
				tmp[i] = ~tmp[i + n_shl];
			tmp.std();
		}

		mul_load_NTT_info(tmp, ans, tmp, ni_ans);

		tmp >>= prev_acc << 6;
		uint64_t inc_acc = cur_acc - prev_acc;
		if (inc_acc >= tmp.size) {
			memset(tmp.data + tmp.size, 0, sizeof(uint64_t) * (inc_acc - tmp.size));
			memcpy(tmp.data + inc_acc, ans.data, sizeof(uint64_t) * prev_acc);
		}
		else
			add(tmp.data + inc_acc, ans.data, prev_acc, tmp.data + inc_acc, tmp.size - inc_acc);
		tmp.resize(cur_acc);
		ans = std::move(tmp);

		if (cur_acc >= expected_acc + 1) {
			ans >>= cur_acc - expected_acc << 6;
			return ans;
		}
	}
	return ans;
}

void div_iterative2(const natural& divd, const natural& divs, natural& q, natural& r) {
	int n_shl = std::countl_zero(divs[divs.size - 1]) + 1 & 63;
	natural b = divs << n_shl;
	r = divd << n_shl;
	
	NTT_info ni_b;
	natural rec = reciprocal(b, r.size, ni_b);
	NTT_info ni_rec(get_NTT_scale(rec.size, rec.size));
	int NTT_scale_b = get_NTT_scale(b.size, 1);
	uint64_t NTT_size_b = 1ull << NTT_scale_b;
	if (ni_b.NTT_scale != NTT_scale_b && NTT_scale_b >= NTTscale_threshold - 1) {
		ni_b.load(b, get_NTT_scale(b.size, 1));
		ni_b.NTT();
	}
	
	q.resize(r.size - b.size + 1);
	memset(q.data, 0, sizeof(uint64_t) * q.size);

	natural tmp;
	int first = 1;
	if (ni_b.NTT_scale == 0 || 0) {
		while (r.size > rec.size + b.size - 1) {
			if (first)
				mul_save_NTT_info(tmp, rec, r >> (r.size - rec.size << 6), ni_rec);
			else
				mul_load_NTT_info(tmp, rec, r >> (r.size - rec.size << 6), ni_rec);
			
			first = 0;
			tmp >>= rec.size << 6;
			uint64_t idx = r.size - rec.size - b.size + 1;
			add(q.data + idx, q.data + idx, q.size - idx, tmp.data, tmp.size);

			tmp *= b;
			sub(r.data + idx, r.data + idx, r.size - idx, tmp.data, tmp.size);
			r.std();
		}

		if (first)
			mul_save_NTT_info(tmp, rec, r >> (b.size - 1 << 6), ni_rec);
		else
			mul_load_NTT_info(tmp, rec, r >> (b.size - 1 << 6), ni_rec);

		tmp >>= rec.size << 6;
		q += tmp;
		tmp *= b;
		r -= tmp;
	}
	else {
		while (r.size > rec.size + b.size - 1) {
			if (first)
				mul_save_NTT_info(tmp, rec, r >> (r.size - rec.size << 6), ni_rec);
			else
				mul_load_NTT_info(tmp, rec, r >> (r.size - rec.size << 6), ni_rec);
			
			first = 0;
			tmp >>= rec.size << 6;
			uint64_t idx = r.size - rec.size - b.size + 1;
			add(q.data + idx, q.data + idx, q.size - idx, tmp.data, tmp.size);

			mul_load_NTT_info(tmp, b, tmp, ni_b);
			sub(r.data + idx, r.data + idx, r.size - idx, tmp.data, tmp.size);
			r.std();
			
			if (r.size - idx > NTT_size_b) {
				int cf = add(r.data + idx, r.data + idx, NTT_size_b, r.data + idx + NTT_size_b, r.size - idx - NTT_size_b);
				asmAdd1(NTT_size_b, r.data + idx, r.data + idx, cf);
				r.resize(idx + NTT_size_b);
			}
			if (r.size == idx + NTT_size_b && r[r.size - 1] >> 63)
				r.resize(idx);
			r.std();
		}

		if (first)
			mul_save_NTT_info(tmp, rec, r >> (b.size - 1 << 6), ni_rec);
		else
			mul_load_NTT_info(tmp, rec, r >> (b.size - 1 << 6), ni_rec);

		tmp >>= rec.size << 6;
		q += tmp;
		mul_load_NTT_info(tmp, b, tmp, ni_b);
		r -= tmp;

		if (r.size > NTT_size_b) {
			int cf = add(r.data, r.data, NTT_size_b, r.data + NTT_size_b, r.size - NTT_size_b);
			asmAdd1(NTT_size_b, r.data, r.data, cf);
			r.resize(NTT_size_b);
		}
		if (r.size == NTT_size_b && r[r.size - 1] >> 63)
			r = 0;
		r.std();
	}

	while (r >= b) {
		q += 1;
		r -= b;
	}
	r >>= n_shl;
}

natural& natural::_div(const natural* ap, const natural* bp, natural* r) {
	if (*bp == 0) {
		if (*ap == 0)
			*this = 1ull;
		else
			*this = 0;
		*r = 0;
		return *this;
	}
	if (*ap < *bp) {
		*this = 0;
		*r = *ap;
		return *this;
	}
	if (bp->size == 1) {
		*r = div_base(*ap, (*bp)[0], *this);
		return *this;
	}
	div_iterative2(*ap, *bp, *this, *r);
	return *this;
}

natural& operator/=(natural& a, const natural& b) {
	natural r;
	a._div(&a, &b, &r);
	return a;
}

natural operator/(const natural& a, const natural& b) {
	natural q, r;
	q._div(&a, &b, &r);
	return q;
}

natural& operator%=(natural& a, const natural& b) {
	natural q;
	q._div(&a, &b, &a);
	return a;
}

natural operator%(const natural& a, const natural& b) {
	natural q, r;
	q._div(&a, &b, &r);
	return r;
}

natural factorial(uint64_t n) {
	if (n <= 1)
		return 1;
	if (n == 2)
		return 2;
	bool* is_prime = new bool[n + 1];
	uint64_t* prime = new uint64_t[n], * prime_exp = new uint64_t[n], prime_cnt = 0;
	memset(is_prime, 1, (n + 1) * sizeof(bool));
	for (uint64_t i = 2; i <= n; i++) {
		if (is_prime[i]) {
			prime[prime_cnt] = i;
			prime_exp[prime_cnt] = 0;
			uint64_t _n = n;
			while (_n)
				prime_exp[prime_cnt] += _n /= i;
			prime_cnt++;
		}
		for (uint64_t j = 0; j < prime_cnt && i * prime[j] <= n; j++) {
			is_prime[i * prime[j]] = 0;
			if (i % prime[j] == 0)
				break;
		}
	}
	natural ans = 1;
	for (int i = 63 - std::countl_zero(prime_exp[1]); i >= 0; i--) {
		std::priority_queue<natural, std::vector<natural>, std::greater<natural> > pq;
		for (uint64_t j = 1; j < prime_cnt; j++) {
			if (prime_exp[j] < 1ull << i)
				break;
			if (prime_exp[j] & 1ull << i)
				pq.push(prime[j]);
		}
		while (pq.size() > 1) {
			natural n1 = pq.top();
			pq.pop();
			natural n2 = pq.top();
			pq.pop();
			pq.push(n1 * n2);
		}
		ans = sqr(ans) * pq.top();
	}
	ans <<= prime_exp[0];
	delete[] is_prime, prime, prime_exp;
	return ans;
}

void calc_e(natural& num, natural& den, uint64_t begin, uint64_t end) {
	if (end - begin == 2) {
		num = end;
		den = begin + 1;
		if (begin)
			den *= begin;
		return;
	}
	natural num1, den1;
	calc_e(num, den, begin, begin + end >> 1);
	calc_e(num1, den1, begin + end >> 1, end);
	NTT_info ni_den1;
	ni_den1.NTT_scale = get_NTT_scale(std::max(den.size, num.size), den1.size);
	mul_save_NTT_info(num, den1, num, ni_den1);
	mul_load_NTT_info(den, den1, den, ni_den1);
	num += num1;
}

void sqrt_10005(natural& num, natural& den, uint64_t acc) {
	num = 4001;
	den = 40;
	natural cache1, cache2;
	while (num.size + den.size - 2 < acc) {
		NTT_info ni_num, ni_den;
		ni_num.NTT_scale = ni_den.NTT_scale = get_NTT_scale(num.size, num.size);
		sqr_save_NTT_info(cache1, den, ni_den);
		sqr_save_NTT_info(cache2, num, ni_num);
		mul_load_NTT_info(den, num, den, ni_num, ni_den);
		den <<= 1;
		num = cache1 * 10005 + cache2;
	}
}

void calc_pi(natural& num, natural& den, natural& numc, uint64_t begin, uint64_t end) {
	if (end - begin == 1) {
		den = begin;
		den = den * den * den * 0x26dd041d878000ull;
		numc = 2 * begin - 1;
		numc *= 6 * begin - 1;
		numc *= 6 * begin - 5;
		num = begin;
		num = (0x207e2da6ull * num + 0xcf6371) * numc;
		return;
	}
	natural num1, den1, numc1;
	calc_pi(num, den, numc, begin, begin + end >> 1);
	calc_pi(num1, den1, numc1, begin + end >> 1, end);
	if (end - begin == 2) {
		num = numc * num1 - num * den1;
		den = den * den1;
		numc = numc * numc1;
		return;
	}
	NTT_info ni_den1, ni_numc;
	ni_den1.NTT_scale = get_NTT_scale(std::max(den.size, num.size), den1.size);
	mul_save_NTT_info(num, den1, num, ni_den1);
	mul_load_NTT_info(den, den1, den, ni_den1);
	ni_numc.NTT_scale = get_NTT_scale(std::max(numc1.size, num1.size), numc.size);
	mul_save_NTT_info(num1, numc, num1, ni_numc);
	mul_load_NTT_info(numc, numc, numc1, ni_numc);
	num = num + num1;
}

const uint64_t dec_length = 18, dec_base_0 = 1'000'000'000'000'000'000ull;
natural dec_base[64] = { dec_base_0 }, dec_base_r[64];
NTT_info ni_dec_base[64], ni_dec_base_r[64];

//	Calculate dec_base[i + 1] dec_base_r[i]
void calc_dec_base(int i) {
	if (dec_base[i + 1] == 0) {
		ni_dec_base[i].NTT_scale = get_NTT_scale(dec_base[i].size, dec_base[i].size);
		sqr_save_NTT_info(dec_base[i + 1], dec_base[i], ni_dec_base[i]);
		int n_shl = std::countl_zero(dec_base[i][dec_base[i].size - 1]) + 1 & 63;
		dec_base_r[i] = reciprocal(dec_base[i] << n_shl, dec_base[i].size * 2 + 1);
		ni_dec_base_r[i].NTT_scale = get_NTT_scale(dec_base_r[i].size, dec_base[i].size + 2);
		if (ni_dec_base_r[i].NTT_scale < NTTscale_threshold)
			ni_dec_base_r[i].NTT_scale = 0;
		else {
			ni_dec_base_r[i].load(dec_base_r[i], ni_dec_base_r[i].NTT_scale);
			ni_dec_base_r[i].NTT();
		}
	}
}

natural& natural::dec(std::string_view sv) {
	int i = 63 - std::countl_zero(uint64_t((sv.size() + dec_length - 1) / dec_length - 1));
	if (i == -1) {
		*this = 0;
		for (int i = 0; i < sv.size(); i++)
			data[0] = data[0] * 10 + sv[i] - '0';
		return *this;
	}
	if (dec_base[i] == 0)
		for (int j = 0; j < i; j++)
			calc_dec_base(j);
	natural hi, lo;
	hi.dec(sv.substr(0, sv.size() - (dec_length << i)));
	lo.dec(sv.substr(sv.size() - (dec_length << i)));
	*this = hi * dec_base[i] + lo;
	return *this;
}

std::istream& operator >>(std::istream& is, natural& n) {
	std::string s;
	is >> s;
	if (is.flags() & std::ios::hex)
		n.hex(s);
	else if (is.flags() & std::ios::dec)
		n.dec(s);
	return is;
}

void natural::_dec_split(int i, natural& hi, natural& lo)const {
	if (i == 0)
		lo = div_base(*this, dec_base_0, hi);
	else {
		int n_shl = std::countl_zero(dec_base[i][dec_base[i].size - 1]) + 1 & 63;
		mul_load_NTT_info(hi, dec_base_r[i], *this >> (dec_base[i].size - 1 << 6), ni_dec_base_r[i]);
		hi >>= (dec_base[i].size + 2 << 6) - n_shl;
		mul_load_NTT_info(lo, dec_base[i], hi, ni_dec_base[i]);
		lo = *this - lo;
		while (lo >= dec_base[i]) {
			hi += 1;
			lo -= dec_base[i];
		}
	}
}

void natural::_print_dec(std::ostream& os, int i, bool is_first)const {
	if (i == -1) {
		if (is_first) {
			os.setf(std::ios_base::right);
			os << data[0];
			os.unsetf(std::ios_base::showbase);
			os.fill('0');
		}
		else {
			os.width(dec_length);
			os << data[0];
		}
		return;
	}
	natural hi, lo;
	_dec_split(i, hi, lo);
	if (is_first && hi == 0) {
		lo._print_dec(os, i - 1, true);
		return;
	}
	hi._print_dec(os, i - 1, is_first);
	lo._print_dec(os, i - 1, false);
	return;
}

void natural::_format_dec(std::format_context& ctx, int i, bool is_first)const {
	if (i == -1) {
		if (is_first)
			std::format_to(ctx.out(), "{}", data[0]);
		else
			std::format_to(ctx.out(), "{:0{}}", data[0], dec_length);
		return;
	}
	natural hi, lo;
	_dec_split(i, hi, lo);
	if (is_first && hi == 0) {
		lo._format_dec(ctx, i - 1, true);
		return;
	}
	hi._format_dec(ctx, i - 1, is_first);
	lo._format_dec(ctx, i - 1, false);
	return;
}

int dec_init(const natural& n) {
	int i = 0;
	for (; n >= dec_base[i]; i++)
		calc_dec_base(i);
	return i - 1;
}

std::ostream& operator <<(std::ostream& os, const natural& n) {
	std::ios_base::fmtflags oldFlags = os.flags();
	char oldFill = os.fill();
	if (oldFlags & std::ios::hex) {
		uint64_t i = n.size - 1;
		os.setf(std::ios_base::right);
		os << n[i];
		os.unsetf(std::ios_base::showbase);
		os.fill('0');
		while (i--) {
			os.width(16);
			os << n[i];
		}
	}
	else if (oldFlags & std::ios::dec)
		n._print_dec(os, dec_init(n), true);
	os.fill(oldFill);
	os.flags(oldFlags);
	return os;
}

template<>
struct std::formatter<natural> {
	bool prefix = false;
	bool upper = false;
	char base = 'd';

	constexpr auto parse(std::format_parse_context& ctx) {
		auto it = ctx.begin();
		if (it == ctx.end() || *it == '}')
			return it;
		if (*it == '#') {
			prefix = true;
			++it;
		}
		if (*it == '}')
			return it;
		upper = isupper(*it);
		base = tolower(*it);
		if (base != 'd' && base != 'x')
			throw std::format_error("Invalid format specifier for natural.");
		++it;
		if (*it != '}')
			throw std::format_error("Invalid format specifier for natural.");
		return it;
	}

	auto format(const natural& n, std::format_context& ctx) const {
		if (base == 'x') {
			auto out = ctx.out();
			if (prefix) {
				if (upper)
					out = std::format_to(out, "0X");
				else
					out = std::format_to(out, "0x");
			}
			uint64_t i = n.size - 1;
			if (upper) {
				out = std::format_to(out, "{:X}", n[i]);
				while (i--)
					out = std::format_to(out, "{:016X}", n[i]);
			}
			else {
				out = std::format_to(out, "{:x}", n[i]);
				while (i--)
					out = std::format_to(out, "{:016x}", n[i]);
			}
			return out;
		}
		else if (base == 'd') {
			n._format_dec(ctx, dec_init(n), true);
			return ctx.out();
		}
	}
};