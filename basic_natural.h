#pragma once

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

struct basic_natural {

	uint64_t* data;
	uint64_t size;
	uint64_t capacity;

	basic_natural() {
		data = (uint64_t*)malloc(sizeof(uint64_t));
		if (data == nullptr)
			exit(0xc0000005);
		size = capacity = 1;
	}

	basic_natural(std::initializer_list<uint64_t> n) {
		size = n.size();
		capacity = std::bit_ceil(size);
		data = (uint64_t*)malloc(capacity * sizeof(uint64_t));
		if (data == nullptr)
			exit(0xc0000005);
		memcpy(data, n.begin(), size * sizeof(uint64_t));
	}

	basic_natural(const basic_natural& n) {
		size = n.size;
		capacity = std::bit_ceil(size);
		data = (uint64_t*)malloc(capacity * sizeof(uint64_t));
		if (data == nullptr)
			exit(0xc0000005);
		memcpy(data, n.data, size * sizeof(uint64_t));
	}

	basic_natural(basic_natural&& n) noexcept {
		size = n.size;
		capacity = n.capacity;
		data = n.data;
		n.data = nullptr;
	}

	~basic_natural() {
		free(data);
		data = nullptr;
	}

	basic_natural& operator=(const basic_natural&);

	basic_natural& operator=(basic_natural&&) noexcept;

	void resize(uint64_t);

	uint64_t& operator[](uint64_t);

	const uint64_t& operator[](uint64_t)const;

};

basic_natural& basic_natural::operator=(const basic_natural& n) {
	resize(n.size);
	memcpy(data, n.data, size * sizeof(uint64_t));
	return *this;
}

basic_natural& basic_natural::operator=(basic_natural&& n) noexcept{
	if (this != &n) {
		free(data);
		size = n.size;
		capacity = n.capacity;
		data = n.data;
		n.data = nullptr;
	}
	return *this;
}

void basic_natural::resize(uint64_t s) {
	size = s;
	if (capacity < s) {
		capacity = std::bit_ceil(s);
		data = (uint64_t*)realloc(data, capacity * sizeof(uint64_t));
		if (data == nullptr)
			exit(0xc0000005);
	}
}

uint64_t& basic_natural::operator[](uint64_t idx) {
	return data[idx];
}

const uint64_t& basic_natural::operator[](uint64_t idx)const {
	return data[idx];
}