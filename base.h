#pragma once

#include "asm.h"
#include <cstring>

//	_Size1 >= _Size2
int add(uint64_t* _Dst, const uint64_t* _Src1, uint64_t _Size1, const uint64_t* _Src2, uint64_t _Size2) {
	int cf = asmAdd0(_Size2, _Dst, _Src1, _Src2);
	return asmAdd1(_Size1 - _Size2, _Src1 + _Size2, _Dst + _Size2, cf);
}

//	_Src1 >= _Src2
void sub(uint64_t* _Dst, const uint64_t* _Src1, uint64_t _Size1, const uint64_t* _Src2, uint64_t _Size2) {
	int cf = asmSub0(_Size2, _Dst, _Src1, _Src2);
	asmSub1(_Size1 - _Size2, _Src1 + _Size2, _Dst + _Size2, cf);
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
	else if (_Size2 < toom22mul_threshold)
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