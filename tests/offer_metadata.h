#pragma once

#include <atomic>
#include <cstdint>
#include <sstream>

#include "trie/metadata.h"

#include "tests/xdr/test_types.h"

namespace trie {
namespace test {

struct AtomicOrderbookMetadata;


/*! Duplication of SizeMixin.

Stores an offer's available sell amount (it's "endowment").
*/
struct OrderbookMetadata {

	using AtomicT = AtomicOrderbookMetadata;
	int64_t endow;

	OrderbookMetadata(const Offer& offer) : endow(offer.amount) {}

	OrderbookMetadata() : endow(0) {}

	OrderbookMetadata(const OrderbookMetadata& v) : endow(v.endow) {}


	OrderbookMetadata& operator+=(const OrderbookMetadata& other) {
		endow += other.endow;
		return *this;
	}
	OrderbookMetadata& operator-=(const OrderbookMetadata& other) {
		endow -= other.endow;
		return *this;
	}

	bool operator==(const OrderbookMetadata& other) {
		return endow == other.endow;
	}

	std::string to_string() const {
		std::stringstream s;
		s << "endow:"<<endow<<" ";
		return s.str();
	}

	template<typename AtomicType>
	void unsafe_load_from(const AtomicType& s) {
		endow = s.endow.load(load_order);
	}
};



struct AtomicOrderbookMetadata {

	using BaseT = OrderbookMetadata;

	std::atomic_int64_t endow;

	AtomicOrderbookMetadata(const Offer& offer) : endow(offer.amount) {}

	AtomicOrderbookMetadata() : endow(0) {}

	AtomicOrderbookMetadata(const OrderbookMetadata& v) : endow(v.endow) {}

	void operator+= (const OrderbookMetadata& other) {
		endow.fetch_add(other.endow, store_order);
	}

	void operator-= (const OrderbookMetadata& other) {
		endow.fetch_sub(other.endow, store_order);
	}

	void clear() {
		endow = 0;
	}

	void unsafe_store(const OrderbookMetadata& other) {
		endow.store(other.endow, store_order);
	}

	std::string to_string() const {
		std::stringstream s;
		s << "endow:"<<endow<<" ";
		return s.str();
	}

};

} /* test */
} /* trie */
