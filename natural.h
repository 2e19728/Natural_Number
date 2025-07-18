#ifndef _2E19728TH_NATURAL_H
#define _2E19728TH_NATURAL_H

#include "basic_natural.h"
#include <cctype>
#include <iostream>
#include <string>
#include <algorithm>
#include <intrin.h>

extern "C" {

	//	_Dst = _Src1 + _Src2; return CF;
	int asmAdd0(uint64_t _Size, uint64_t* _Dst, const uint64_t* _Src1, const uint64_t* _Src2);

	//	_Dst = _Src + 1; return CF; 
	int asmAdd1(uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst, const void* _Memcpy);

	//	_Dst = _Src1 - _Src2; return CF;
	int asmSub0(uint64_t _Size, uint64_t* _Dst, const uint64_t* _Src1, const uint64_t* _Src2);

	//	_Dst = _Src - 1;
	void asmSub1(uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst, const void* _Memcpy);

	//	_Dst = _Src << _CL;
	void asmShl(uint64_t _CL, uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst);

	//	_Dst = _Src >> _CL;
	void asmShr(uint64_t _CL, uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst);

	//	_Dst = _Src1 * _N
	void asmMul0(uint64_t _Size, uint64_t _N, const uint64_t* _Src, uint64_t* _Dst);

	//	_Size1 <= _Size2; _Dst = _Src1 * _Src2
	void asmMul1(uint64_t _Size1, uint64_t _Size2, const uint64_t* _Src1, const uint64_t* _Src2, uint64_t* _Dst);

	//	_Dst = _Src * _Src
	void asmSqr(uint64_t _Size, uint64_t* _Dst, const uint64_t* _Src);

	void asmNtt(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmNtt2(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmINtt(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmINtt2(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmINttShr(int _Scale, uint64_t _Mod, uint64_t* _Begin);

	//	return _A * _B % _Mod
	uint64_t asmMod(uint64_t _A, uint64_t _B, uint64_t _Mod);

	uint64_t asmSave(const uint64_t* _info_0, const uint64_t* _info_1, const uint64_t* _info_2, uint64_t _Size, uint64_t* _Dst);

}

//	_Size1 >= _Size2
int add(uint64_t* _Dst, const uint64_t* _Src1, uint64_t _Size1, const uint64_t* _Src2, uint64_t _Size2) {
	int cf = asmAdd0(_Size2, _Dst, _Src1, _Src2);
	if (cf)
		return asmAdd1(_Size1 - _Size2, _Src1 + _Size2, _Dst + _Size2, memcpy);
	else {
		memcpy(_Dst + _Size2, _Src1 + _Size2, (_Size1 - _Size2) * sizeof(uint64_t));
		return 0;
	}
}

//	_Src1 >= _Src2
void sub(uint64_t* _Dst, const uint64_t* _Src1, uint64_t _Size1, const uint64_t* _Src2, uint64_t _Size2) {
	int cf = asmSub0(_Size2, _Dst, _Src1, _Src2);
	if (cf)
		asmSub1(_Size1 - _Size2, _Src1 + _Size2, _Dst + _Size2, memcpy);
	else
		memcpy(_Dst + _Size2, _Src1 + _Size2, (_Size1 - _Size2) * sizeof(uint64_t));
}

const uint64_t toom22mul_threshold = 32;

//	d = _Dst; a = _Src1; b = _Src2; s = _Size
void toom22mul(uint64_t* d, const uint64_t* a, const uint64_t* b, uint64_t s) {
	if (s < toom22mul_threshold)
		return asmMul1(s, s, a, b, d);
	uint64_t s0 = s + 1 >> 1, s1 = s - s0;
	toom22mul(d, a, b, s0);
	toom22mul(d + (s0 << 1), a + s0, b + s0, s1);
	uint64_t* a1 = new uint64_t[s0 + 1], * b1 = new uint64_t[s0 + 1], * d1 = new uint64_t[s0 + 1 << 1];
	a1[s0] = add(a1, a, s0, a + s0, s1);
	b1[s0] = add(b1, b, s0, b + s0, s1);
	toom22mul(d1, a1, b1, s0 + 1);
	sub(d1, d1, s0 + 1 << 1, d, s0 << 1);
	sub(d1, d1, s0 + 1 << 1, d + (s0 << 1), s1 << 1);
	add(d + s0, d + s0, s + s1, d1, s0 + 1 << 1);
	delete[] a1, b1, d1;
}

const uint64_t toom22sqr_threshold = 48;

//	d = _Dst; a = _Src; s = _Size
void toom22sqr(uint64_t* d, const uint64_t* a, uint64_t s) {
	if (s < toom22sqr_threshold)
		return asmSqr(s, d, a);
	uint64_t s0 = s + 1 >> 1, s1 = s - s0;
	toom22sqr(d, a, s0);
	toom22sqr(d + (s0 << 1), a + s0, s1);
	uint64_t* a1 = new uint64_t[s0 + 1], * d1 = new uint64_t[s0 + 1 << 1];
	a1[s0] = add(a1, a, s0, a + s0, s1);
	toom22sqr(d1, a1, s0 + 1);
	sub(d1, d1, s0 + 1 << 1, d, s0 << 1);
	sub(d1, d1, s0 + 1 << 1, d + (s0 << 1), s1 << 1);
	add(d + s0, d + s0, s + s1, d1, s0 + 1 << 1);
	delete[] a1, d1;
}

//	_Size1 >= _Size2; _Dst = _Src1 * _Src2
void mul_base(uint64_t* _Dst, const uint64_t* _Src1, uint64_t _Size1, const uint64_t* _Src2, uint64_t _Size2) {
	if (_Size2 <= 1)
		asmMul0(_Size1, *_Src2, _Src1, _Dst);
	else if(_Size2 < toom22mul_threshold)
		asmMul1(_Size2, _Size1, _Src2, _Src1, _Dst);
	else {
		toom22mul(_Dst, _Src1, _Src2, _Size2);
		_Src1 += _Size2;
		_Dst += _Size2;
		_Size1 -= _Size2;
		memset(_Dst + _Size2, 0, sizeof(uint64_t) * (_Size1));
		uint64_t* d = new uint64_t[_Size2 << 1];
		while (_Size1 >= _Size2) {
			toom22mul(d, _Src1, _Src2, _Size2);
			add(_Dst, _Dst, _Size2 << 1, d, _Size2 << 1);
			_Src1 += _Size2;
			_Dst += _Size2;
			_Size1 -= _Size2;
		}
		if (_Size1 > 0) {
			mul_base(d, _Src2, _Size2, _Src1, _Size1);
			add(_Dst, _Dst, _Size1 + _Size2, d, _Size1 + _Size2);
		}
		delete[] d;
	}
}

//	_Dst = _Src * _Src
void sqr_base(uint64_t* _Dst, const uint64_t* _Src, uint64_t _Size) {
	if (_Size <= 1)
		asmMul0(_Size, *_Src, _Src, _Dst);
	else if (_Size < toom22mul_threshold)
		asmSqr(_Size, _Dst, _Src);
	else
		toom22sqr(_Dst, _Src, _Size);
}

struct natural :basic_natural {

	natural(uint64_t n = 0) {
		data[0] = n;
	}

	natural(const char* s) {
		*this = s;
	}

	natural(const natural& n) :basic_natural(n) {}

	natural(std::initializer_list<uint64_t> n) :basic_natural(n) {}

	natural(natural&& n) noexcept :basic_natural(std::move(n)) {}

	natural& std();

	natural& operator=(uint64_t);

	natural& operator=(const char*);

	natural& operator=(const natural&);

	natural& operator=(natural&&) noexcept;

	natural& hex(const char*);

	natural& dec(const char*, const char*, int);

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

	std::ostream& _print_dec(std::ostream& os, int base_n, bool is_first)const;

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

natural& natural::operator=(const char* s) {
	if (s[0] == '0')
		if (tolower(s[1]) == 'x')
			hex(s + 2);
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

natural& natural::hex(const char* s) {
	static const char hexStr[4] = { 0, 48, 55, 87 };
	uint64_t ss = strlen(s);
	if (s[0] == '0' && tolower(s[1]) == 'x')
		s += 2, ss -= 2;
	resize((ss + 15) >> 4);
	uint64_t i = -ss & 15, j = size;
	s -= i;
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
		*this = 0ull;
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

const uint64_t NTTmul_threshold = 768;
struct NTT_info {

	static const uint64_t mods[3];
	static const uint64_t root_base[3][56];
	static const uint64_t iroot_base[3][56];
	static basic_natural root[3];
	static basic_natural iroot[3];
	static int root_scale;
	basic_natural info[3];
	int NTT_scale;

	static void init(int _NTT_scale) {
		if (_NTT_scale <= root_scale)
			return;
		for (int i = 0; i < 3; i++) {
			root[i].resize(1ull << _NTT_scale);
			for (int j = root_scale; j < _NTT_scale; j++)
				for (uint64_t k = 0, n = 1ull << j; k < n; k += 2) {
					root[i][n + k] = asmMod(root[i][k], root_base[i][j], mods[i]);
					root[i][n + k + 1] = _udiv128(root[i][n + k], 0, mods[i], nullptr) + 1;
				}
			iroot[i].resize(1ull << _NTT_scale);
			for (int j = root_scale; j < _NTT_scale; j++)
				for (uint64_t k = 0, n = 1ull << j; k < n; k += 2) {
					iroot[i][n + k] = asmMod(iroot[i][k], iroot_base[i][j], mods[i]);
					iroot[i][n + k + 1] = _udiv128(iroot[i][n + k], 0, mods[i], nullptr) + 1;
				}
		}
		root_scale = _NTT_scale;
	}

	NTT_info() :NTT_scale(0) {}

	NTT_info(const natural& n, int _NTT_scale) {
		load(n, _NTT_scale);
	}

	void load(const uint64_t*, uint64_t, int);

	void load(const natural&, int);

	void NTT();

	void INTT();

	void mul(const NTT_info&, const NTT_info&);

	uint64_t save(uint64_t*, uint64_t)const;

	void save(natural&, uint64_t)const;

};

const uint64_t NTT_info::mods[3] = {
	27ull << 56 | 1,
	58ull << 56 | 1,
	87ull << 56 | 1
};

const uint64_t NTT_info::root_base[3][56]{
	{
		0x1b00000000000000, 0x0c55f5e087b66679, 0x05d00f46ebf68efc, 0x048864e8f79c7099, 0x121a7906088f35c9, 0x0a707c27703307a8, 0x1a72390d129c217e, 0x14794b5b0eff3c1d,
		0x150aba0a482a0f41, 0x06724e24293f5b86, 0x0e4944f5f7cf4097, 0x186725790196b6d7, 0x1282d5a8cd67201f, 0x12e9e778e733b261, 0x02b70f6b0d4a2680, 0x059df5b4354e9d7f,
		0x193aa62399495777, 0x11b8a30797545f26, 0x00014f5c30f8efbc, 0x05ed6f8b2ed1d536, 0x0906d3b9852ec38f, 0x10c45314cc77b053, 0x18275e18005bcf48, 0x0514d6456c5a8365,
		0x16a4e6b4538c15c6, 0x19dc8ebd3864bb9c, 0x1253c4291e14513f, 0x03390b0b8a8f39ef, 0x115460f2af355b5d, 0x03e7d4c6519e4fae, 0x1612bd0b0c807014, 0x19b48989659bbc1e,
		0x18c8ad8701dce021, 0x00656b06b1fef5e1, 0x192054d8e0d98368, 0x00f76bace7abd4cb, 0x07768878a81ffa6c, 0x028c219d139bbe02, 0x0959bbe081d127fb, 0x13bd1ce230a76d3f,
		0x144f394db1ebb988, 0x09a7965810a40b98, 0x07c62c5c31a01917, 0x198264d7b92a0ff5, 0x07f27550e89dfee7, 0x0aa7be203ac5edf6, 0x0b3299e03fa84a13, 0x0250dc45636976a8,
		0x18e5a8d2f0982456, 0x17c26444df30c6a7, 0x17fd02a7837348ff, 0x00a33f092e0b1ac1, 0x000000000cc6db61, 0x0000000000003931, 0x0000000000000079, 0x000000000000000b
	},
	{
		0x3a00000000000000, 0x0b5ea5c01435f877, 0x279e926c244e5b38, 0x120da1843a627638, 0x24a8f0822831956e, 0x2199348392e64bed, 0x1778454b4655688a, 0x29e345ba8d50ff33,
		0x28037c699a8409bf, 0x02946bccfb7875a5, 0x19a702af56d79371, 0x0c88fcc933e3ade0, 0x2d04aabf083e596d, 0x2c7c83ddc4e6045d, 0x1ed47d8d9fe4383e, 0x221350494f6cbd85,
		0x0f979b66cc0978e8, 0x19ad9482e86163cc, 0x01ed78e2f5f9a9dc, 0x237efa2c5e3e99ae, 0x2cf497d4f2918a3c, 0x0ebb70fd1d2ce835, 0x30019ed71781c420, 0x0977b9196e263958,
		0x2b20e0cfec908df3, 0x2356b574c520834d, 0x139fa4cbb31bcb02, 0x051b9372b32d4045, 0x0b8fd90431a61c6a, 0x32d6f90bf41b8a2a, 0x008393580a8f8d40, 0x12ab3e189876c2b6,
		0x1eff4afb813fb1e1, 0x1b76615f315173a1, 0x21ce3fe2322a21cc, 0x3995503c81e90438, 0x173348425a387d2c, 0x22781b63e914d58b, 0x1653f406f9de7aa8, 0x0398d957310a8395,
		0x098ac5d4b01acf8f, 0x0c4bccdfe12d0ff6, 0x32c488ca19c62e55, 0x201dc82ac3c4a9ad, 0x11f45747e6fd15d5, 0x14eeab6b3026e680, 0x11b95283b1adf675, 0x2b50093ca8d38830,
		0x1a11a7b9611a7baa, 0x17fffffffffffffc, 0x0000000100000000, 0x0000000000010000, 0x0000000000000100, 0x0000000000000010, 0x0000000000000004, 0x0000000000000002
	},
	{
		0x5700000000000000, 0x25eeaecc59203257, 0x0894a6a0d69ffa9f, 0x45efe465dd261a8c, 0x32004bea8d8c6b05, 0x51d1a4a056db8efb, 0x030e0e60a108c7d4, 0x1552e0364c6698c6,
		0x1fde67485112f101, 0x121d2babe5a17006, 0x40ae7411f8b2079c, 0x049bd54e09b7bcc0, 0x5618c746b86b3fff, 0x014f9a3b3c536562, 0x478b136dc0acb53c, 0x0cd928ca349150a7,
		0x2aac61c39caada38, 0x0a2f8dfe14850f1a, 0x251a8be905833450, 0x3c7dfa086e287454, 0x0a2700365877fc92, 0x1f874ec09c259bc9, 0x1d7a0653ae3ab538, 0x561da2e6aca4bb66,
		0x3745007b4c5f4e96, 0x52d8076b73305796, 0x5609b51e6262b905, 0x450fb776ccea8260, 0x1209fb38bbce4cbd, 0x3b91a96863b41e6d, 0x2a805f5540305fd1, 0x18d074f21d9bb9c7,
		0x27852f125ce038c5, 0x27a13c78a2cea5b1, 0x39bd7b08d38f1b5a, 0x28f42c1355f5a4d5, 0x562c74f137904ab4, 0x15c0b5221862604a, 0x3e45003fe6672245, 0x21a3aa11c512f4f4,
		0x1194f148ad0d95d0, 0x1b10f7a7211a7631, 0x12e9d86a51add86a, 0x0ebc8981589d2c1c, 0x29ebf23aba3a515a, 0x4088ee8ac148c260, 0x0665ef7befb47442, 0x03b70fe5e9d5ef2f,
		0x3d9941c499089e44, 0x00dea4a7c31535f8, 0x28a24eadb9b14771, 0x0b43389cfed1ac42, 0x349fc3f96684e0f3, 0x0000000226761f10, 0x0000000000017764, 0x0000000000000136
	}
};

const uint64_t NTT_info::iroot_base[3][56]{
	{
		0x1b00000000000000, 0x0eaa0a1f78499988, 0x181f53f62f0a3966, 0x11af8e382966df93, 0x061aa49bc8c68e7e, 0x0ee4b3f720abbc84, 0x188ae203558327dc, 0x01601d934ca27e0e,
		0x1a44188b632c0036, 0x1094e3ae85c8a861, 0x0a9690e4cfe7b4e1, 0x03f64170fcbd6640, 0x080062ecea3a9cae, 0x19194e8d40b9665e, 0x18978ad2c429fac8, 0x04359ca87bd86a30,
		0x0f9b726e21b3f742, 0x0f1a783886569e6d, 0x1210471efff7de08, 0x1a3522951787cc05, 0x1aaf36ec832c9fa0, 0x06352217d4cbdfc4, 0x0590d57bde4a8e03, 0x11b4d8443bc4f25f,
		0x118c9ac8cd5018b1, 0x0739f4d44ff6d1ed, 0x0bdff3199cff1ca3, 0x0f36409b45b6d00c, 0x007bc2bce66c1eff, 0x0807524f30ede847, 0x129bd036bc2e03c7, 0x0166d6b5e67524ab,
		0x0435db457f1262f1, 0x06cd41522df9f8df, 0x02d206ac543db691, 0x15c46158b49a22ff, 0x018a56a0143c8298, 0x03b8b55bd55f90ad, 0x0d612fc0ab420270, 0x107e4118998c1296,
		0x1103280e9f950ba2, 0x1295913d096a25a0, 0x0395629478b99b29, 0x1944918378622eef, 0x0f9982e637edfdb7, 0x02ce6b02d6a1872d, 0x18af972937bf8837, 0x0820108a39a973a0,
		0x10bde1b63a752e8c, 0x1033cd166401aab4, 0x1a6dfc09a947a753, 0x1444a755ef8bbf37, 0x0edbbe88bf7a288d, 0x027beaced9690884, 0x0391fbc4c2a50659, 0x0c45d1745d1745d2
	},
	{
		0x3a00000000000000, 0x2ea15a3febca078a, 0x0bdc45c04b8fea45, 0x276b8974dd6cdc4b, 0x13491ee45cc16371, 0x0a55f976dd0f93fa, 0x020e75f34ca83188, 0x2948df4f456f48ad,
		0x334163ca861755a4, 0x2414c7c748f3358a, 0x3680ff448a1a1d94, 0x0721b0eb7782c0f9, 0x32ab7d5a2d8c4bdb, 0x124b9d8dabda2670, 0x0ba2f77129c3f56e, 0x21e162fe7455d1d0,
		0x071e52b626ad5fe2, 0x3044afc1b0ff9f47, 0x194bec9b5c636af3, 0x0236f8c90a01c927, 0x1ce82a257e5c870f, 0x252a46b37cafb952, 0x029433e8898337bc, 0x212af2916b6d4731,
		0x0051236b350ba95f, 0x1581987447b66c83, 0x3991bb1a88d0bacd, 0x262618747086d2ea, 0x337c79fabeaabb7b, 0x08bb669a723c6e01, 0x0b7877d255733d49, 0x1bd9a18450e0ae83,
		0x139c359d42adfa0a, 0x0179518bbea5d58a, 0x147d37867c24583e, 0x060e1415f841adbb, 0x12efeb6ae120a57d, 0x06d0227483c4901c, 0x0aa13a79cf0f1c3b, 0x0e972fc06a84d391,
		0x0467aca2354d1bed, 0x2205a6ed9b1f231e, 0x34352dfbb1d2a12b, 0x09e127647f9576b0, 0x0ffe32bb93cbe3fe, 0x2c1d6a47fee6749c, 0x39ffe59c956f4f87, 0x39d8e0ca60000001,
		0x3705d80000000001, 0x0d24000000000000, 0x39ffffffc6000001, 0x39ffc60000000001, 0x39c6000000000001, 0x3660000000000001, 0x2b80000000000001, 0x1d00000000000001
	},
	{
		0x5700000000000000, 0x31115133a6dfcdaa, 0x264f7a44f2dc101c, 0x024934e3f187ae56, 0x110156e0bc017643, 0x124be95ccbce10eb, 0x04c014168266d94f, 0x25264b4c5bcefe32,
		0x29b20b4bf5e85061, 0x4fd88af336c94b67, 0x0f1e455645baef59, 0x28eebb29fa488818, 0x41a96e76e1fc459e, 0x4bbd62d0824d6881, 0x348ba85808b6eccd, 0x10b1be7add3a6105,
		0x4be34f24f3f83aaf, 0x32d6740121c1900f, 0x080d66b4fd153f16, 0x1760865185aaae99, 0x2cb22191d6e6d69f, 0x446433be011ffcdf, 0x34b6dfd3d404e4d3, 0x12706af4051187d3,
		0x371890c6baf59540, 0x1a1b1b90377e6f48, 0x541c2553b9aaeb8d, 0x0ea761b6d90072ec, 0x0b4412168e0d4285, 0x0dd0736bdb64b6ad, 0x5410c9b99b145158, 0x0a5cb629a7d0d0d0,
		0x405308b924442778, 0x0e545025d3f8f6ec, 0x22144b9c76de03e4, 0x00f34806cd3f5f27, 0x1255f46f8bf9890d, 0x328a388d106657d3, 0x0e23b6553922bb6c, 0x05d954963798ff25,
		0x02725ea9834db7dc, 0x27bff1eb8e072dd0, 0x16046df4722c4c07, 0x44f69e898dbd4049, 0x0c706c58aeed540f, 0x4ee91287e5b7aae9, 0x3d6e5b538638db46, 0x0d4dde910b2cc1b9,
		0x1fdc9191dfa0fa07, 0x320f3c38a06ef140, 0x491d76bf22140b04, 0x5070424c82c3ab2e, 0x40bf123e57353d3e, 0x413f4490baa97eb6, 0x4926c9f732d0ff91, 0x38f8915789157892
	}
};

basic_natural NTT_info::root[3] = {
	{ 1, _udiv128(1, 0, NTT_info::mods[0], nullptr) + 1 },
	{ 1, _udiv128(1, 0, NTT_info::mods[1], nullptr) + 1 },
	{ 1, _udiv128(1, 0, NTT_info::mods[2], nullptr) + 1 }
};

basic_natural NTT_info::iroot[3] = {
	{ 1, _udiv128(1, 0, NTT_info::mods[0], nullptr) + 1 },
	{ 1, _udiv128(1, 0, NTT_info::mods[1], nullptr) + 1 },
	{ 1, _udiv128(1, 0, NTT_info::mods[2], nullptr) + 1 }
};

int NTT_info::root_scale = 1;

void NTT_info::load(const uint64_t* _Src, uint64_t _Size, int _NTT_scale) {
	NTT_scale = _NTT_scale;
	uint64_t N = 1ull << _NTT_scale - 1;
	for (int i = 0; i < 3; i++)
		info[i].resize(N + N);
	if (_Size <= N) {
		for (uint64_t j = 0; j < _Size; j++) {
			uint64_t n = _Src[j];
			for (int i = 0; i < 3; i++)
				info[i][j] = info[i][j + N] = n % mods[i];
		}
		for (int i = 0; i < 3; i++) {
			memset(info[i].data + _Size, 0, sizeof(uint64_t) * (N - _Size));
			memset(info[i].data + N + _Size, 0, sizeof(uint64_t) * (N - _Size));
		}
	}
	else {
		uint64_t j = 0;
		for (; j < _Size - N; j++) {
			uint64_t n0 = _Src[j], n1 = _Src[j + N];
			for (int i = 0; i < 3; i++) {
				uint64_t n2 = n0 % mods[i], n3 = n1 % mods[i];
				int64_t n4 = n2 + n3, n5 = n2 - n3;
				info[i][j] = (n4 >= mods[i] ? n4 - mods[i] : n4);
				info[i][j + N] = (n5 < 0 ? n5 + mods[i] : n5);
			}
		}
		for (; j < N; j++) {
			uint64_t n = _Src[j];
			for (int i = 0; i < 3; i++)
				info[i][j] = info[i][j + N] = n % mods[i];
		}
	}
	init(NTT_scale);
}

void NTT_info::load(const natural& n, int _NTT_scale) {
	load(n.data, n.size, _NTT_scale);
}

const int NTT_scale_L3_threshold = 20;
void NTT_info::NTT() {
	if (NTT_scale <= NTT_scale_L3_threshold) {
		for (int i = 0; i < 3; i++)
			for (uint64_t j = 1ull << NTT_scale - 2; j > 0; j >>= 1)
				asmNtt(j, mods[i], info[i].data, root[i].data, info[i].data + info[i].size);
		return;
	}
	for (int i = 0; i < 3; i++) {
		for (uint64_t j = 1ull << NTT_scale - 2; j >= 1ull << NTT_scale_L3_threshold; j >>= 1)
			asmNtt(j, mods[i], info[i].data, root[i].data, info[i].data + info[i].size);
		for (int j = NTT_scale_L3_threshold - 1; j >= 0; j--)
			asmNtt(1ull << j, mods[i], info[i].data, root[i].data, info[i].data + (1 << NTT_scale_L3_threshold));
		for (uint64_t k = 1; k < info[i].size >> NTT_scale_L3_threshold; k++) {
			for (int j = NTT_scale_L3_threshold - 1; j >= 0; j--)
				asmNtt2(1ull << j, mods[i], info[i].data + (k << NTT_scale_L3_threshold),
					root[i].data + (k << NTT_scale_L3_threshold - j), info[i].data + (k + 1 << NTT_scale_L3_threshold));
		}
	}
}

void NTT_info::INTT() {
	if (NTT_scale <= NTT_scale_L3_threshold) {
		for (int i = 0; i < 3; i++) {
			for (uint64_t j = 1; j < 1ull << NTT_scale - 1; j <<= 1)
				asmINtt(j, mods[i], info[i].data, iroot[i].data, info[i].data + info[i].size);
			asmINttShr(NTT_scale, mods[i], info[i].data);
		}
		return;
	}
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < NTT_scale_L3_threshold; j++)
			asmINtt(1ull << j, mods[i], info[i].data, iroot[i].data, info[i].data + (1 << NTT_scale_L3_threshold));
		for (uint64_t k = 1; k < info[i].size >> NTT_scale_L3_threshold; k++) {
			for (int j = 0; j < NTT_scale_L3_threshold; j++)
				asmINtt2(1ull << j, mods[i], info[i].data + (k << NTT_scale_L3_threshold),
					iroot[i].data + (k << NTT_scale_L3_threshold - j), info[i].data + (k + 1 << NTT_scale_L3_threshold));
		}
		for (uint64_t j = 1ull << NTT_scale_L3_threshold; j < 1ull << NTT_scale - 1; j <<= 1)
			asmINtt(j, mods[i], info[i].data, iroot[i].data, info[i].data + info[i].size);
		asmINttShr(NTT_scale, mods[i], info[i].data);
	}
}

void NTT_info::mul(const NTT_info& ni_a, const NTT_info& ni_b) {
	NTT_scale = ni_a.NTT_scale;
	uint64_t NTT_Size = 1ull << NTT_scale;
	for (int i = 0; i < 3; i++) {
		info[i].resize(ni_a.info[i].size);
		for (uint64_t j = 0; j < NTT_Size; j++)
			info[i][j] = asmMod(ni_a.info[i][j], ni_b.info[i][j], mods[i]);
	}
}

uint64_t NTT_info::save(uint64_t* _Dst, uint64_t _Size)const {
	return asmSave(info[0].data, info[1].data, info[2].data, _Size, _Dst);
}

void NTT_info::save(natural& n, uint64_t _Size)const {
	if (_Size - 1 > 1ull << NTT_scale) {
		n.resize(1ull << NTT_scale);
		save(n.data, 1ull << NTT_scale);
		return;
	}
	n.resize(_Size);
	n[_Size - 1] = save(n.data, _Size - 1);
	n.std();
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
/*
	int NTT_scale = get_NTT_scale(ap->size, bp->size);
	NTT_info ni_a(*ap, NTT_scale), ni_b(*bp, NTT_scale);
	ni_a.NTT();
	ni_b.NTT();
	ni_a.mul(ni_a, ni_b);
	ni_a.INTT();
	ni_a.save(*this, ap->size + bp->size);
	return *this;
*/
}

natural& natural::_sqr_NTT(const natural* p) {
	int NTT_scale = get_NTT_scale(p->size, p->size);
	NTT_info ni(*p, NTT_scale);
	ni.NTT();
	ni.mul(ni, ni);
	ni.INTT();
	ni.save(*this, p->size * 2);
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
	if (ni_a.NTT_scale < 10) {
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
	}
}

void sqr_save_NTT_info(natural& d, const natural& a, NTT_info& ni_a) {
	if (ni_a.NTT_scale < 10) {
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

std::ostream& operator <<(std::ostream& os, const natural& n);

natural reciprocal(const natural& divs, uint64_t N) {
	natural ans = (1ull << 63 | divs[divs.size - 2] >> 1) + 1;
	if (ans[0] == 0)
		ans[0] = 1ull << 63;
	else
		ans[0] = _udiv128(1ull << 63, 0, ans[0], nullptr);
	natural tmp;
	uint64_t acc = N + 1 - divs.size;
	while (1) {
		uint64_t nxt_acc = 0, cur_acc = ans.size;
		if (cur_acc >= acc + 3) {
			ans >>= cur_acc - acc << 6;
			return ans;
		}

		if (divs.size > 2 * cur_acc + 1) {
			tmp.resize(2 * cur_acc + 1);
			asmAdd1(tmp.size, divs.data + divs.size - tmp.size, tmp.data, memcpy);
		}
		else
			tmp._shl(&divs, 2 * cur_acc + 1 - divs.size << 6);

		NTT_info ni_ans;
		ni_ans.NTT_scale = get_NTT_scale(tmp.size, 1);
		mul_save_NTT_info(tmp, ans, tmp, ni_ans);

		tmp.resize(tmp.size - cur_acc);
		for (uint64_t i = 0; i < tmp.size; i++)
			tmp[i] = ~tmp[i + cur_acc];
		tmp.std();
		nxt_acc = std::min(cur_acc, 2 * cur_acc - tmp.size + 1) << 1;
		if (nxt_acc > 1020 && nxt_acc <= 1024)
			nxt_acc = 1020;
		tmp >>= 2 * cur_acc - nxt_acc << 6;

		mul_load_NTT_info(tmp, ans, tmp, ni_ans);

		ans <<= nxt_acc - cur_acc << 6;
		add(ans.data, ans.data, ans.size, tmp.data + cur_acc, tmp.size - cur_acc);
	}
}

uint64_t div_base(const natural& a, uint64_t b, natural& q) {
	q.resize(a.size);
	uint64_t r = 0, i = a.size;
	while (i--)
		q[i] = _udiv128(r, a[i], b, &r);
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

natural& natural::_div(const natural* ap, const natural* bp, natural* r) {
	if (*bp == 0ull) {
		if (*ap == 0ull)
			*this = 1ull;
		else
			*this = 0ull;
		*r = 0ull;
		return *this;
	}
	if (*ap < *bp) {
		*this = 0ull;
		*r = *ap;
		return *this;
	}
	if (bp->size == 1) {
		*r = div_base(*ap, (*bp)[0], *this);
		return *this;
	}
	div_iterative(*ap, *bp, *this, *r);
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

#include <queue>
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
			natural n1 = std::move(pq.top());
			pq.pop();
			natural n2 = std::move(pq.top());
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

natural dec_base[64] = { 10000000000000000000ull }, dec_base_r[64];
NTT_info ni_dec_base[64], ni_dec_base_r[64];

natural& natural::dec(const char* begin, const char* end, int i = 0) {
	natural hi, lo;
	if (end != nullptr) {
		i = 63 - std::countl_zero(uint64_t((end - begin + 18) / 19 - 1));
		if (dec_base[i + 1] == 0ull) {
			int j = 0;
			do {
				if (dec_base[j + 1] == 0ull) {
					ni_dec_base[j].NTT_scale = get_NTT_scale(dec_base[j].size, dec_base[j].size);
					sqr_save_NTT_info(dec_base[j + 1], dec_base[j], ni_dec_base[j]);
					int n_shl = std::countl_zero(dec_base[j][dec_base[j].size - 1]) + 1 & 63;
					dec_base_r[j] = reciprocal(dec_base[j] << n_shl, dec_base[j].size * 2 + 1);
					ni_dec_base_r[j].NTT_scale = get_NTT_scale(dec_base_r[j].size, dec_base[j].size + 2);
					if (ni_dec_base_r[j].NTT_scale < 11)
						ni_dec_base_r[j].NTT_scale = 0;
					else {
						ni_dec_base_r[j].load(dec_base_r[j], ni_dec_base_r[j].NTT_scale);
						ni_dec_base_r[j].NTT();
					}
				}
			} while (++j <= i);
		}
		if (i == -1) {
			*this = 0ull;
			while (begin != end) {
				data[0] = data[0] * 10 + *begin - '0';
				begin++;
			}
			return *this;
		}
		hi.dec(begin, end - (19ull << i));
		lo.dec(end - (19ull << i), nullptr, i - 1);
	}
	else {
		if (i == -1) {
			*this = 0ull;
			for (int j = 0; j < 19; j++) {
				data[0] = data[0] * 10 + *begin - '0';
				begin++;
			}
			return *this;
		}
		hi.dec(begin, end, i - 1);
		lo.dec(begin + (19ull << i), end, i - 1);
	}
	*this = hi * dec_base[i] + lo;
	return *this;
}

std::istream& operator >>(std::istream& is, natural& n) {
	std::string s;
	is >> s;
	if (is.flags() & std::ios::hex)
		n.hex(s.c_str());
	else if (is.flags() & std::ios::dec)
		n.dec(s.c_str(), s.c_str() + s.size());
	return is;
}

std::ostream& natural::_print_dec(std::ostream& os, int i, bool is_first)const {
	if (i == -1) {
		if (is_first) {
			os.setf(std::ios_base::right);
			os << data[0];
			os.unsetf(std::ios_base::showbase);
			os.fill('0');
		}
		else {
			os.width(19);
			os << data[0];
		}
		return os;
	}
	natural hi, lo;
	if (i == 0)
		lo = div_base(*this, 10000000000000000000ull, hi);
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
	if (is_first && hi == 0ull) {
		lo._print_dec(os, i - 1, 1);
		return os;
	}
	hi._print_dec(os, i - 1, is_first);
	lo._print_dec(os, i - 1, 0);
	return os;
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
	else if (oldFlags & std::ios::dec) {
		int i = -1;
		do {
			if (dec_base[i + 1] == 0ull) {
				ni_dec_base[i].NTT_scale = get_NTT_scale(dec_base[i].size, dec_base[i].size);
				sqr_save_NTT_info(dec_base[i + 1], dec_base[i], ni_dec_base[i]);
				int n_shl = std::countl_zero(dec_base[i][dec_base[i].size - 1]) + 1 & 63;
				dec_base_r[i] = reciprocal(dec_base[i] << n_shl, dec_base[i].size * 2 + 1);
				ni_dec_base_r[i].NTT_scale = get_NTT_scale(dec_base_r[i].size, dec_base[i].size + 2);
				if (ni_dec_base_r[i].NTT_scale < 11)
					ni_dec_base_r[i].NTT_scale = 0;
				else {
					ni_dec_base_r[i].load(dec_base_r[i], ni_dec_base_r[i].NTT_scale);
					ni_dec_base_r[i].NTT();
				}
			}
		} while (n >= dec_base[++i]);
		n._print_dec(os, i - 1, 1);
	}
	os.fill(oldFill);
	os.flags(oldFlags);
	return os;
}

#endif