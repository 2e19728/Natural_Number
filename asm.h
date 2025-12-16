#pragma once

#include <stdint.h>

extern "C" {

	//	_Dst = _Src1 + _Src2; return CF;
	uint8_t asmAdd0(uint64_t _Size, uint64_t* _Dst, const uint64_t* _Src1, const uint64_t* _Src2);

	//	_Dst = _Src + CF; return CF; 
	uint8_t asmAdd1(uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst, uint8_t CF);

	//	_Dst = _Src1 - _Src2; return CF;
	uint8_t asmSub0(uint64_t _Size, uint64_t* _Dst, const uint64_t* _Src1, const uint64_t* _Src2);

	//	_Dst = _Src - CF;
	void asmSub1(uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst, uint8_t CF);

	//	_Dst = _Src << _CL;
	void asmShl(uint64_t _CL, uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst);

	//	_Dst = _Src >> _CL;
	void asmShr(uint64_t _CL, uint64_t _Size, const uint64_t* _Src, uint64_t* _Dst);

	//	_Dst = _Src1 * _N;
	void asmMul0(uint64_t _Size, uint64_t _N, const uint64_t* _Src, uint64_t* _Dst);

	//	_Size1 <= _Size2; _Dst = _Src1 * _Src2;
	void asmMul1(uint64_t _Size1, uint64_t _Size2, const uint64_t* _Src1, const uint64_t* _Src2, uint64_t* _Dst);

	//	_Dst = _Src * _Src;
	void asmSqr(uint64_t _Size, uint64_t* _Dst, const uint64_t* _Src);

	void asmLoad(uint64_t _Size, uint64_t _Mod_64, uint64_t* _Dst1, uint64_t* _Dst2, const uint64_t* _Src, uint64_t _Mod);

	void asmNtt(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmNtt2(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmNtt3(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmNttMul(uint64_t _N, uint64_t* _Dst, const uint64_t* _Src1, const uint64_t* _Src2, uint64_t _Mod, uint64_t _Mod_124);

	void asmINtt(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmINtt2(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmINtt3(uint64_t _N, uint64_t _Mod, uint64_t* _Begin, const uint64_t* _Root, uint64_t* _End);

	void asmINttShr(int _Scale, uint64_t _Mod, uint64_t* _Begin);

	uint64_t asmDiv(uint64_t _Low, uint64_t _High, uint64_t _Mod);

	//	_R = ceil(pow(2, 64) / _Mod); return _A % _Mod;
	uint64_t asmMod(uint64_t _A, uint64_t _R, uint64_t _Mod);

	//	_R = ceil(pow(2, 124) / _Mod); return _A * _B % _Mod;
	uint64_t asmMulMod(uint64_t _A, uint64_t _B, uint64_t _Mod, uint64_t _R);

	uint64_t asmSave(const uint64_t* _info_0, const uint64_t* _info_1, const uint64_t* _info_2, uint64_t _Size, uint64_t* _Dst);

}