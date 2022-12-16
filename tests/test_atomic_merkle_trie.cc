#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "mtt/trie/debug_macros.h"
#include "mtt/trie/atomic_merkle_trie.h"
#include "mtt/trie/utils.h"
#include "mtt/trie/types.h"

#include <utils/serialize_endian.h>
#include <utils/time.h>
#include <utils/threadlocal_cache.h>

#include <tbb/parallel_for.h>

namespace trie
{


TEST_CASE("basic get subnode ref and normalize", "[amt]")
{

	using mt = AtomicMerkleTrie<UInt64Prefix, EmptyValue, 256>;

	mt m;

	REQUIRE(m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0x0000'0000'0000'0000), PrefixLenBits(32)) != nullptr);
	REQUIRE(m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0xFFFF'FFFF'0000'0000), PrefixLenBits(32)) != nullptr);
	auto* p0 = m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0xFFFF'0000'0000'0000), PrefixLenBits(16));

	auto* p1 = m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0xFFFF'AAAA'0000'0000), PrefixLenBits(32));
	auto* p2 = m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0xFFFF'AAAA'0000'0000), PrefixLenBits(32));

	REQUIRE(p1 == p2);

	REQUIRE(p0 -> get_child(0xA) == p1);
}

TEST_CASE("normalize cleans up in case of no inserts", "[amt]")
{
	using mt = AtomicMerkleTrie<UInt64Prefix, EmptyValue, 256>;

	mt m;

	REQUIRE(m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0x0000'0000'0000'0000), PrefixLenBits(32)) != nullptr);
	REQUIRE(m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0xFFFF'FFFF'0000'0000), PrefixLenBits(32)) != nullptr);
	REQUIRE(m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0xFFFF'0000'0000'0000), PrefixLenBits(24)) != nullptr);
	REQUIRE(m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0xFF00'FF00'0000'0000), PrefixLenBits(16)) != nullptr);

	auto h1 = m.hash_and_normalize();

	mt m2;
	auto h2 = m2.hash_and_normalize();

	REQUIRE(h1 == h2);
}

TEST_CASE("check inserts", "[amt]")
{
	using mt = AtomicMerkleTrie<UInt64Prefix, EmptyValue, 256>;

	mt m;

	auto* root = m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(0));

	for (uint64_t i = 0; i < 1000; i++)
	{
		uint64_t query = (i * 17) % 6701;  //6701 is prime

		root -> template insert<OverwriteInsertFn<EmptyValue>, EmptyValue>(UInt64Prefix(query), EmptyValue{}, m.get_gc());
	}

	auto h1 = m.hash_and_normalize();

	for (uint64_t i = 0; i < 1000; i++)
	{
		uint64_t query = ((i + 1000) * 17) % 6701;  //6701 is prime

		m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(query), PrefixLenBits(56));
	}

	auto h2 = m.hash_and_normalize();

	REQUIRE(h1 == h2);

}
TEST_CASE("force recompute", "[amt]")
{
	using mt = AtomicMerkleTrie<UInt64Prefix, EmptyValue, 256>;

	mt m;

	auto h1 = m.hash_and_normalize();

	auto* base = m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(60));

	base -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0), EmptyValue{}, m.get_gc());

	auto h2 = m.hash_and_normalize();

	REQUIRE(h1 != h2);

	base -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0), EmptyValue{}, m.get_gc());

	REQUIRE(h2 == m.hash_and_normalize());
}

TEST_CASE("deletions", "[amt]")
{
	using mt = AtomicMerkleTrie<UInt64Prefix, EmptyValue, 256>;

	mt m;

	auto h = m.hash_and_normalize();

	auto* root = m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(32));

	SECTION("single elt")
	{

		root -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0000'0000'0000'0000), EmptyValue{}, m.get_gc());

		root -> delete_value(UInt64Prefix(0x0000'0000'0000'0000), m.get_gc());

		REQUIRE(m.hash_and_normalize() == h);
	}

	SECTION("several elt")
	{
		root -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0000'0000'0000'0000), EmptyValue{}, m.get_gc());
		root -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0000'0000'0000'1111), EmptyValue{}, m.get_gc());

		h = m.hash_and_normalize();
		root = m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(32));

		root -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0000'0000'0000'2222), EmptyValue{}, m.get_gc());

		root -> delete_value(UInt64Prefix(0x0000'0000'0000'2222), m.get_gc());

		REQUIRE(m.hash_and_normalize() == h);
	}

	SECTION("cleanup chain")
	{
		m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(32));
		m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(40));
		m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(44));
		m.get_subnode_ref_and_invalidate_hash(UInt64Prefix(0), PrefixLenBits(48));

		root -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0000'0000'0000'2222), EmptyValue{}, m.get_gc());
		root -> template insert<OverwriteInsertFn<EmptyValue>>(UInt64Prefix(0x0000'0000'0000'1111), EmptyValue{}, m.get_gc());

		root -> delete_value(UInt64Prefix(0x0000'0000'0000'2222), m.get_gc());
		root -> delete_value(UInt64Prefix(0x0000'0000'0000'1111), m.get_gc());

		REQUIRE(m.hash_and_normalize() == h);
	}
}




} // namespace trie
