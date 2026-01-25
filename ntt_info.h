#pragma once

#include "asm.h"
#include "basic_natural.h"

const uint64_t NTTmul_threshold = 384;
const uint64_t NTTdiv_threshold = 256;
const int NTTscale_threshold = 10;

struct NTT_info {

	static const uint64_t mods[3];
	static const uint64_t mods_124[3];
	static const uint64_t root_base[3][56];
	static const uint64_t iroot_base[3][56];
	static basic_natural root[3];
	static basic_natural iroot[3];
	static int root_scale;
	basic_natural info[3];
	int NTT_scale;

	static void init(int _NTT_scale) {
		_NTT_scale -= 1;
		if (_NTT_scale <= root_scale)
			return;
		for (int i = 0; i < 3; i++) {
			root[i].resize(1ull << _NTT_scale);
			for (int j = root_scale; j < _NTT_scale; j++)
				for (uint64_t k = 0, n = 1ull << j; k < n; k += 2) {
					root[i][n + k] = asmMulMod(root[i][k], root_base[i][j], mods[i], mods_124[i]);
					root[i][n + k + 1] = asmDiv(0, root[i][n + k], mods[i]) + 1;
				}
			iroot[i].resize(1ull << _NTT_scale);
			for (int j = root_scale; j < _NTT_scale; j++)
				for (uint64_t k = 0, n = 1ull << j; k < n; k += 2) {
					iroot[i][n + k] = asmMulMod(iroot[i][k], iroot_base[i][j], mods[i], mods_124[i]);
					iroot[i][n + k + 1] = asmDiv(0, iroot[i][n + k], mods[i]) + 1;
				}
		}
		root_scale = _NTT_scale;
	}

	NTT_info() :NTT_scale(0) {}

	NTT_info(const basic_natural& n, int _NTT_scale) {
		load(n, _NTT_scale);
	}

	void load(const uint64_t*, uint64_t, int);

	void load(const basic_natural&, int);

	void NTT();

	void INTT();

	void mul(const NTT_info&, const NTT_info&);

	uint64_t CRT(uint64_t);

	void save(basic_natural&, uint64_t);

};

const uint64_t NTT_info::mods[3] = {
	27ull << 56 | 1,
	58ull << 56 | 1,
	87ull << 56 | 1
};

const uint64_t NTT_info::mods_124[3] = {
	0x97b425ed097b425a,
	0x469ee58469ee5846,
	0x2f149902f149902f
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

basic_natural NTT_info::root[3] = { { 1, 10 }, { 1, 5 }, { 1, 3 } };

basic_natural NTT_info::iroot[3] = { { 1, 10 }, { 1, 5 }, { 1, 3 } };

int NTT_info::root_scale = 1;

void NTT_info::load(const uint64_t* _Src, uint64_t _Size, int _NTT_scale) {
	static const uint64_t mods_64[3] = { 10, 5, 3 };
	NTT_scale = _NTT_scale;
	uint64_t N = 1ull << _NTT_scale - 1;
	for (int i = 0; i < 3; i++)
		info[i].resize(N + N);
	if (_Size <= N) {
		for (int i = 0; i < 3; i++) {
			asmLoad(_Size, mods_64[i], info[i].data, info[i].data + N, _Src, mods[i]);
			memset(info[i].data + _Size, 0, sizeof(uint64_t) * (N - _Size));
			memset(info[i].data + N + _Size, 0, sizeof(uint64_t) * (N - _Size));
		}
	}
	else {
		for (int i = 0; i < 3; i++) {
			uint64_t j = 0;
			for (; j < _Size - N; j++) {
				uint64_t n0 = asmMod(_Src[j], mods_64[i], mods[i]), n1 = asmMod(_Src[j + N], mods_64[i], mods[i]);
				info[i][j] = n0 + n1;
				info[i][j + N] = n0 - n1 + mods[i];
			}
			asmLoad(N + N - _Size, mods_64[i], info[i].data + j, info[i].data + N + j, _Src + j, mods[i]);
		}
	}
	init(NTT_scale);
}

void NTT_info::load(const basic_natural& n, int _NTT_scale) {
	load(n.data, n.size, _NTT_scale);
}

const int NTT_scale_L3_threshold = 20;
void NTT_info::NTT() {
	if (NTT_scale <= NTT_scale_L3_threshold) {
		for (int i = 0; i < 3; i++)
			for (uint64_t j = 1ull << NTT_scale - 2; j > 1; j >>= 1)
				asmNtt(j, mods[i], info[i].data, root[i].data, info[i].data + info[i].size);
		return;
	}
	for (int i = 0; i < 3; i++) {
		for (uint64_t j = 1ull << NTT_scale - 2; j >= 1ull << NTT_scale_L3_threshold; j >>= 1)
			asmNtt(j, mods[i], info[i].data, root[i].data, info[i].data + info[i].size);

		for (int j = NTT_scale_L3_threshold - 1; j > 0; j--)
			asmNtt(1ull << j, mods[i], info[i].data, root[i].data, info[i].data + (1 << NTT_scale_L3_threshold));
		for (uint64_t k = 1; k < info[i].size >> NTT_scale_L3_threshold; k++)
			for (int j = NTT_scale_L3_threshold - 1; j > 0; j--)
				asmNtt2(1ull << j, mods[i], info[i].data + (k << NTT_scale_L3_threshold),
					root[i].data + (k << NTT_scale_L3_threshold - j), info[i].data + (k + 1 << NTT_scale_L3_threshold));
	}
}

void NTT_info::INTT() {
	if (NTT_scale <= NTT_scale_L3_threshold) {
		for (int i = 0; i < 3; i++) {
			for (uint64_t j = 2; j < 1ull << NTT_scale - 1; j <<= 1)
				asmINtt(j, mods[i], info[i].data, iroot[i].data, info[i].data + info[i].size);
			asmINttShr(NTT_scale, mods[i], info[i].data);
		}
		return;
	}
	for (int i = 0; i < 3; i++) {
		for (int j = 1; j < NTT_scale_L3_threshold; j++)
			asmINtt(1ull << j, mods[i], info[i].data, iroot[i].data, info[i].data + (1 << NTT_scale_L3_threshold));
		for (uint64_t k = 1; k < info[i].size >> NTT_scale_L3_threshold; k++)
			for (int j = 1; j < NTT_scale_L3_threshold; j++)
				asmINtt2(1ull << j, mods[i], info[i].data + (k << NTT_scale_L3_threshold),
					iroot[i].data + (k << NTT_scale_L3_threshold - j), info[i].data + (k + 1 << NTT_scale_L3_threshold));

		for (uint64_t j = 1ull << NTT_scale_L3_threshold; j < 1ull << NTT_scale - 1; j <<= 1)
			asmINtt(j, mods[i], info[i].data, iroot[i].data, info[i].data + info[i].size);
		asmINttShr(NTT_scale, mods[i], info[i].data);
	}
}

void (* const asmNttMul[3])(uint64_t, uint64_t*, const uint64_t*, const uint64_t*, const uint64_t*) = { asmNttMul0, asmNttMul1, asmNttMul2 };
void NTT_info::mul(const NTT_info& ni_a, const NTT_info& ni_b) {
	NTT_scale = ni_a.NTT_scale;
	uint64_t NTT_Size = 1ull << NTT_scale;
	for (int i = 0; i < 3; i++) {
		info[i].resize(ni_a.info[i].size);
		asmNttMul[i](NTT_Size, info[i].data, ni_a.info[i].data, ni_b.info[i].data, root[i].data);
	}
}

uint64_t NTT_info::CRT(uint64_t _Size) {
	return asmCRT(info[0].data, info[1].data, info[2].data, _Size);
}

void NTT_info::save(basic_natural& n, uint64_t _Size) {
	if (_Size - 1 > 1ull << NTT_scale)
		CRT(1ull << NTT_scale);
	else {
		info[0].resize(_Size);
		info[0][_Size - 1] = CRT(_Size - 1);
	}
	n = std::move(info[0]);
}