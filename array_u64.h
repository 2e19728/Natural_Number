// array_u64.h - drop-in style dynamic array of uint64_t limbs, a cleaned-up variant of
// basic_natural (ntt/original/basic_natural.h).
//
// Kept from basic_natural:
//   * raw members  uint64_t* data; uint64_t size; uint64_t capacity;
//   * resize() grows capacity to the next power of two (std::bit_ceil) and does NOT
//     value-initialize the fresh limbs (caller must fill before read); version_3 takes
//     the block from the thread-local cache below instead of realloc;
//   * operator[] unchecked; no push_back/reserve/iterators yet.
// Fixed:
//   1. default ctor leaves data = nullptr, size = capacity = 0 (no limb allocated);
//   2. moves leave the source in a valid empty state (data=null, size=capacity=0);
//   3. allocation failure throws std::bad_alloc instead of exit(0xc0000005).
//
// Changed in version_3:
//   4. large blocks come from a small thread-local cache instead of going straight back
//      to the kernel (see nat_block_pool below); `nat_pool_enable = false` restores the
//      plain malloc/free behaviour for A/B measurement.
//
// Requires C++20 (std::bit_ceil).

#pragma once

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>

// ---------------------------------------------------------------- large-block cache
// An NTT workspace holds three arrays of 2^scale limbs, i.e. tens to hundreds of
// megabytes, and they are created and destroyed once per multiply.  With plain
// malloc/free every one of those pages is handed back to the kernel and faulted in
// again on the next multiply: glibc serves blocks above its mmap threshold with mmap
// and unmaps them on free, and the dynamic threshold stops growing at 32 MiB on
// x86-64, i.e. below the size class that matters here.  Measured at scale 23:
// ~150k minor faults per multiply, of which >95% disappear once the blocks are
// recycled (verify/gmpbench.sh reports the counters; see also the notes in
// verify/n3notes.md).
//
// The cache is a per-thread LIFO of at most nat_block_pool::MAXSLOTS blocks holding at
// most MAXBYTES in total, so a program that stops doing bignum work still releases
// everything (it just has to allocate real memory once more to do so).
inline bool nat_pool_enable = true;                  // A/B switch (no effect on results)

struct nat_block_pool {
	static constexpr int MAXSLOTS = 8;
	static constexpr uint64_t MAXBYTES = 768ull << 20;
	static constexpr uint64_t MINBYTES = 1ull << 20;   // below 1 MiB: not worth caching
	void* ptr[MAXSLOTS];
	uint64_t elems[MAXSLOTS];                        // block capacity, in limbs
	int n = 0;
	uint64_t bytes = 0;
	// diagnostics (cheap enough to keep: three increments per array creation/destruction)
	uint64_t hits = 0, misses = 0, recycled = 0, dropped = 0;
};

inline nat_block_pool& nat_pool() {
	static thread_local nat_block_pool p;
	return p;
}

// a block of exactly `elems` limbs (power of two), or nullptr if malloc fails.
// Best fit: never hand a 134 MB block to a 2 MB request when a 2 MB one is cached.
inline uint64_t* nat_block_get(uint64_t elems) {
	nat_block_pool& p = nat_pool();
	if (nat_pool_enable) {
		int best = -1;
		for (int i = 0; i < p.n; i++)
			if (p.elems[i] >= elems && (best < 0 || p.elems[i] < p.elems[best]))
				best = i;
		if (best >= 0) {
			uint64_t* q = (uint64_t*)p.ptr[best];
			p.bytes -= p.elems[best] * sizeof(uint64_t);
			p.ptr[best] = p.ptr[--p.n];
			p.elems[best] = p.elems[p.n];
			p.hits++;
			return q;
		}
	}
	nat_pool().misses++;
	return (uint64_t*)malloc(elems * sizeof(uint64_t));
}

inline void nat_block_drop(int i) {                  // evict slot i, freeing the block
	nat_block_pool& p = nat_pool();
	free(p.ptr[i]);
	p.bytes -= p.elems[i] * sizeof(uint64_t);
	p.ptr[i] = p.ptr[--p.n];
	p.elems[i] = p.elems[p.n];
}

// Recycle a block.  Blocks under nat_block_pool::MINBYTES are given straight back: every
// bignum construction produces a handful of tiny arrays, and letting those pin the six
// slots would lock the large blocks out of the cache (that was a measured bug: the cache
// stayed full of 1-limb blocks and every transform kept re-faulting its 200 MB).  When
// the cache is full, a smaller cached block is evicted for a bigger newcomer.
inline void nat_block_put(uint64_t* q, uint64_t elems) {
	if (q == nullptr)
		return;
	const uint64_t bytes = elems * sizeof(uint64_t);
	nat_block_pool& p = nat_pool();
	if (!nat_pool_enable || bytes < nat_block_pool::MINBYTES) {
		nat_pool().dropped++;
		free(q);
		return;
	}
	for (;;) {
		if (p.bytes + bytes <= nat_block_pool::MAXBYTES && p.n < nat_block_pool::MAXSLOTS)
			break;
		int m = -1;                                  // smallest resident block
		for (int i = 0; i < p.n; i++)
			if (m < 0 || p.elems[i] < p.elems[m])
				m = i;
		if (m < 0 || p.elems[m] * sizeof(uint64_t) >= bytes) {
			nat_pool().dropped++;
			free(q);                                 // nothing smaller to evict
			return;
		}
		nat_block_drop(m);
	}
	p.ptr[p.n] = q;
	p.elems[p.n] = elems;
	p.n++;
	p.bytes += bytes;
	p.recycled++;
}

struct array_u64 {

	uint64_t* data;
	uint64_t size;
	uint64_t capacity;

	array_u64() : data(nullptr), size(0), capacity(0) {}

	array_u64(std::initializer_list<uint64_t> n) : data(nullptr), size(0), capacity(0) {
		size = n.size();
		if (size) {
			capacity = std::bit_ceil(size);
			data = nat_block_get(capacity);
			if (data == nullptr)
				throw std::bad_alloc();
			memcpy(data, n.begin(), size * sizeof(uint64_t));
		}
	}

	array_u64(const array_u64& n) : data(nullptr), size(0), capacity(0) {
		size = n.size;
		if (size) {
			capacity = std::bit_ceil(size);
			data = nat_block_get(capacity);
			if (data == nullptr)
				throw std::bad_alloc();
			memcpy(data, n.data, size * sizeof(uint64_t));
		}
	}

	array_u64(array_u64&& n) noexcept {
		data = n.data;
		size = n.size;
		capacity = n.capacity;
		n.data = nullptr;
		n.size = 0;
		n.capacity = 0;
	}

	~array_u64() {
		nat_block_put(data, capacity);
		data = nullptr;
	}

	array_u64& operator=(const array_u64& n) {
		if (this != &n) {
			resize(n.size);
			if (size)
				memcpy(data, n.data, size * sizeof(uint64_t));
		}
		return *this;
	}

	array_u64& operator=(array_u64&& n) noexcept {
		if (this != &n) {
			nat_block_put(data, capacity);
			data = n.data;
			size = n.size;
			capacity = n.capacity;
			n.data = nullptr;
			n.size = 0;
			n.capacity = 0;
		}
		return *this;
	}

	void resize(uint64_t s) {
		if (capacity < s) {
			uint64_t nc = std::bit_ceil(s);
			uint64_t* nd = nat_block_get(nc);
			if (nd == nullptr)
				throw std::bad_alloc();
			if (data != nullptr && size != 0)
				memcpy(nd, data, size * sizeof(uint64_t));
			nat_block_put(data, capacity);           // recycle the old, possibly smaller, block
			data = nd;
			capacity = nc;
		}
		size = s;
	}

	uint64_t& operator[](uint64_t idx) {
		return data[idx];
	}

	const uint64_t& operator[](uint64_t idx) const {
		return data[idx];
	}

};
