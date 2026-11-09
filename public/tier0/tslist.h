// Copyright Valve Corporation, All rights reserved.
//
// LIFO from disassembly of Windows API and
// http://jim.afim-asso.org/jim2002/articles/L17_Fober.pdf
// FIFO from
// http://jim.afim-asso.org/jim2002/articles/L17_Fober.pdf

#ifndef SE_PUBLIC_TIER0_TSLIST_H_
#define SE_PUBLIC_TIER0_TSLIST_H_

#include <atomic>
#include <cstddef>      // offsetof
#include <cstdint>      // uintptr_t
#ifndef NDEBUG
#include <mutex>          // debug misuse checks (live-list registry)
#include <unordered_set>
#endif
#include <new>          // placement new
#include <type_traits>

#include "dbg.h"
#include "threadtools.h"
#include "memalloc.h"
#include "memdbgoff.h"


#if defined( PLATFORM_64BITS )

#if defined (PLATFORM_WINDOWS) && defined( PLATFORM_SSE_INTRINSICS )
#else  // PLATFORM_WINDOWS
using int128 = __int128_t;
#define int128_zero() 0
#endif// PLATFORM_WINDOWS

#define TSLIST_HEAD_ALIGNMENT 16
#define TSLIST_NODE_ALIGNMENT 16

static_assert( sizeof( int128 ) == 16, "int128 must be 16 bytes: it is the unit of the head CAS" );

// dimhotepus: Drop duplicated ThreadInterlockedAssignIf128.
namespace tslist_detail
{
#if defined( __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16 )
	inline bool AssignIf128( int128 volatile *pDest, const int128 &value, const int128 &comparand )
	{
		return __sync_bool_compare_and_swap( pDest, comparand, value );
	}
#else
	inline bool AssignIf128( int128 volatile *pDest, const int128 &value, const int128 &comparand )
	{
		return ThreadInterlockedAssignIf128( pDest, value, comparand );
	}
#endif
}  // namespace tslist_detail

inline bool ThreadInterlockedAssignIf64x128( volatile int128 *pDest, const int128 &value, const int128 &comperand )
	{ return tslist_detail::AssignIf128( pDest, value, comperand ); }
#else

#define TSLIST_HEAD_ALIGNMENT 8
#define TSLIST_NODE_ALIGNMENT 8

inline bool ThreadInterlockedAssignIf64x128( volatile int64 *pDest, const int64 value, const int64 comperand )
	{ return ThreadInterlockedAssignIf64( pDest, value, comperand ); }

#endif

// dimhotepus: Relaxed atomic access for fields that are read while other threads may write
// them (the speculative node->Next read in Pop, CTSQueue head/tail snapshots). The values
// are always validated by a following CAS, so a stale value is harmless - but a plain
// access is a data race (UB) in the C++ memory model and trips ThreadSanitizer. Aligned
// loads/stores of these sizes are single instructions on every target, so this is free.
namespace tslist_detail
{
	template <typename T>
	[[nodiscard]] inline T LoadRelaxed( const T *p )
	{
#if defined( __GNUC__ ) || defined( __clang__ )
		return __atomic_load_n( p, __ATOMIC_RELAXED );
#else
		return *static_cast<const volatile T *>( p );
#endif
	}

	template <typename T>
	[[nodiscard]] inline T LoadAcquire( const T *p )
	{
#if defined( __GNUC__ ) || defined( __clang__ )
		return __atomic_load_n( p, __ATOMIC_ACQUIRE );
#else
		const T v = *static_cast<const volatile T *>( p );
		ThreadMemoryBarrier();
		return v;
#endif
	}

	template <typename T>
	inline void StoreRelaxed( T *p, T value )
	{
#if defined( __GNUC__ ) || defined( __clang__ )
		__atomic_store_n( p, value, __ATOMIC_RELAXED );
#else
		*static_cast<volatile T *>( p ) = value;
#endif
	}
}  // namespace tslist_detail

#define TSLIST_HEAD_ALIGN alignas(TSLIST_HEAD_ALIGNMENT)
#define TSLIST_NODE_ALIGN alignas(TSLIST_NODE_ALIGNMENT)
#define TSLIST_HEAD_ALIGN_POST
#define TSLIST_NODE_ALIGN_POST

// Lock free stack node.
struct TSLIST_NODE_ALIGN TSLNodeBase_t : public CAlignedNewDelete<TSLIST_NODE_ALIGNMENT>
{
	TSLNodeBase_t *Next; // name to match Windows
};

// dimhotepus: Node invariants. Users (e.g. the small block heap) push raw, never-constructed
// memory blocks as nodes, which is only sound for a trivial type whose only state is Next
// at offset 0; nodes must also be at least TSLIST_NODE_ALIGNMENT aligned.
static_assert( std::is_trivial_v<TSLNodeBase_t>,
	"TSLNodeBase_t must be trivial: raw memory blocks are used as nodes without construction" );
static_assert( std::is_standard_layout_v<TSLNodeBase_t>,
	"TSLNodeBase_t must be standard-layout (Next at a fixed, known offset)" );
static_assert( offsetof( TSLNodeBase_t, Next ) == 0, "Next must be the first member (Windows SLIST layout)" );
static_assert( sizeof( TSLNodeBase_t ) == TSLIST_NODE_ALIGNMENT,
	"A node is exactly one aligned pointer slot" );
static_assert( alignof( TSLNodeBase_t ) == TSLIST_NODE_ALIGNMENT, "Node alignment must be TSLIST_NODE_ALIGNMENT" );

namespace tslist_detail
{
	// T can be linked into the lists directly (CTSSimpleList, CTSListBase::Push).
	template <typename T>
	inline constexpr bool is_tslist_node_v = std::is_base_of_v<TSLNodeBase_t, T>;

	// Element types are stored inside nodes allocated by CAlignedNewDelete<TSLIST_NODE_ALIGNMENT>;
	// a more strictly aligned T would end up misaligned.
	template <typename T>
	inline constexpr bool fits_node_alignment_v = alignof( T ) <= TSLIST_NODE_ALIGNMENT;
}  // namespace tslist_detail

// Exported trait for users.
template <typename T>
inline constexpr bool is_tslist_node_v = tslist_detail::is_tslist_node_v<T>;

// dimhotepus: Ownership tags for misuse checks. A node handed out to a caller is in no
// list, so its Next field is free: it then holds the owning container's address with
// bit 0 set. Real links never have bit 0 set (nodes are TSLIST_NODE_ALIGNMENT aligned),
// so tagged, linked and fresh (nullptr) nodes are distinguishable.
namespace tslist_detail
{
	static_assert( TSLIST_NODE_ALIGNMENT >= 2 && TSLIST_HEAD_ALIGNMENT >= 2, "bit 0 of node/container addresses must be free for tags" );

	[[nodiscard]] inline TSLNodeBase_t *MakeOwnerTag( const void *pOwner )
	{
		return reinterpret_cast<TSLNodeBase_t *>( reinterpret_cast<uintptr_t>( pOwner ) | uintptr_t{ 1 } );
	}

	[[nodiscard]] inline bool IsOwnerTag( const TSLNodeBase_t *pNext )
	{
		return ( reinterpret_cast<uintptr_t>( pNext ) & uintptr_t{ 1 } ) != 0;
	}

	[[nodiscard]] inline const void *OwnerFromTag( const TSLNodeBase_t *pTag )
	{
		return reinterpret_cast<const void *>( reinterpret_cast<uintptr_t>( pTag ) & ~uintptr_t{ 1 } );
	}

#ifndef NDEBUG
	// Live CTSList objects, so a debug check never reads a list that was already
	// destroyed. Per module (each DLL has its own): across modules a check is skipped,
	// never wrong. Intentionally leaked: it must outlive static lists at exit.
	struct CDebugListRegistry
	{
		std::mutex mutex;
		std::unordered_set<const void *> alive;
	};

	[[nodiscard]] inline CDebugListRegistry &DebugListRegistry()
	{
		static auto *s_pRegistry = new CDebugListRegistry;
		return *s_pRegistry;
	}
#endif
}  // namespace tslist_detail

// Lock free stack head.
union TSLIST_HEAD_ALIGN TSLHead_t
{
	struct Value_t
	{
		TSLNodeBase_t *Next;

#ifdef PLATFORM_64BITS
		// dimhotepus: Depth was uint16 (+ uint16 padding), so Count() wrapped at 65536
		// (a 4 MiB small-block pool alone holds up to 262144 free 16-byte blocks).
		// The padding now widens it to 32 bits; the head stays 16 bytes.
		uint32	Depth;
		// dimhotepus: 64 bit has better ABA prevention.
		uint32	Sequence;
#else
		// 32-bit: the whole head must fit an 8-byte CAS, so these stay 16-bit.
		uint16   Depth;
		uint16	Sequence;
#endif
	} value;

	struct Value32_t
	{
		TSLNodeBase_t *Next_do_not_use_me;
#ifdef PLATFORM_64BITS
		uint64   DepthAndSequence;
#else
		uint32   DepthAndSequence;
#endif
	} value32;

#ifdef PLATFORM_64BITS
	int128 value64x128;
#else
	int64 value64x128;
#endif
};

// dimhotepus: Head invariants. The whole head is swapped with ONE CAS of value64x128 and
// compared byte-wise (std::atomic::compare_exchange and __sync both compare object
// representations), so every view must cover it exactly and the struct view must have no
// padding: indeterminate padding bytes would make a CAS fail forever.
static_assert( sizeof( TSLHead_t ) == 2 * sizeof( void * ), "Head is a pointer plus a pointer-sized tag" );
static_assert( alignof( TSLHead_t ) == TSLIST_HEAD_ALIGNMENT, "Head alignment must be TSLIST_HEAD_ALIGNMENT" );
static_assert( sizeof( TSLHead_t::Value_t ) == sizeof( TSLHead_t ), "value must cover the whole head" );
static_assert( sizeof( TSLHead_t::Value32_t ) == sizeof( TSLHead_t ), "value32 must cover the whole head" );
static_assert( sizeof( TSLHead_t{}.value64x128 ) == sizeof( TSLHead_t ), "value64x128 must cover the whole head (CAS unit)" );
static_assert( std::has_unique_object_representations_v<TSLHead_t::Value_t>,
	"TSLHead_t::Value_t must have no padding bits: the CAS compares raw bytes" );
static_assert( std::is_trivially_copyable_v<TSLHead_t>, "TSLHead_t must be trivially copyable (std::atomic, byte-wise CAS)" );
static_assert( offsetof( TSLHead_t::Value_t, Next ) == 0, "Next must be the first word of the head" );
static_assert( sizeof( TSLHead_t::Value_t::Depth ) == sizeof( TSLHead_t::Value_t::Sequence ),
	"Depth and Sequence share the tag word" );
static_assert( sizeof( TSLHead_t::Value_t::Depth ) <= sizeof( int ), "Count() returns Depth as int" );
#ifdef PLATFORM_64BITS
static_assert( sizeof( TSLHead_t::Value_t::Depth ) == 4, "64-bit: 32-bit Depth so Count() does not wrap at 65536" );
static_assert( sizeof( TSLHead_t::Value_t::Sequence ) == 4, "64-bit: 32-bit ABA sequence" );
#endif

// dimhotepus: Fast path for GCC/Clang when the target guarantees a native CAS of the whole
// head (cmpxchg16b with -mcx16 on x86-64, casp/ldxp on ARM64, cmpxchg8b on 32-bit).
// std::atomic<TSLHead_t> is not lock-free on GCC (is_lock_free() == 0): every operation
// was an out-of-line libatomic call, and its 16-byte *load* is itself a locked
// read-modify-write. Here the initial head read is plain field loads - a torn or stale
// snapshot simply fails the CAS, which then returns the current value - and the CAS is
// inline. Same algorithm and ABA protection; measured ~11% faster per push/pop (x86-64).
// The storage stays std::atomic<TSLHead_t> (same layout, AccessUnprotected() unchanged).
// MSVC keeps the std::atomic path.
#if ( defined( __GNUC__ ) || defined( __clang__ ) ) && \
	( ( defined( PLATFORM_64BITS ) && defined( __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16 ) ) || \
	  ( !defined( PLATFORM_64BITS ) && defined( __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 ) ) )
#define TSLIST_INLINE_HEAD_CAS 1
#endif

// Lock-free only if std::atomic<TSLHead_t> is lock-free on the target platform
// (always the case with TSLIST_INLINE_HEAD_CAS).
//
// NOTE: Nodes must not be freed or reused while any concurrent operation could
// still access them.  Use hazard pointers, epoch-based reclamation, or an
// equivalent scheme if nodes need to be reclaimed concurrently.
//
// dimhotepus: The sequence counter wrapping around is normal and harmless: ABA protection
// only needs it to differ between one thread's read and its CAS, i.e. 2^32 (2^16 on
// 32-bit) operations would have to happen in between. The old asserts on wrap fired after
// 2^32 operations on 64-bit and every 65536 operations on 32-bit debug builds.
class TSLIST_HEAD_ALIGN CTSListBase
    : public CAlignedNewDelete<TSLIST_HEAD_ALIGNMENT> {
 public:
  CTSListBase() : head_{TSLHead_t{{nullptr, 0, 0}}} {
    if ((reinterpret_cast<size_t>(&head_)) % TSLIST_HEAD_ALIGNMENT != 0) {
      Error("CTSListBase: Misaligned list head.\n");
    }
  }

  ~CTSListBase() { Detach(); }

  TSLNodeBase_t *Push(TSLNodeBase_t *node) {
    TSLHead_t orig = LoadHead();

    for (;;) {
      tslist_detail::StoreRelaxed( &node->Next, orig.value.Next );

      TSLHead_t next = orig;
      next.value.Next = node;
      next.value.Depth = orig.value.Depth + 1;
      next.value.Sequence = orig.value.Sequence + 1;

      if (CompareExchangeHead(orig, next)) {
        return orig.value.Next;
      }
    }
  }

  TSLNodeBase_t *Pop() {
    TSLHead_t orig = LoadHead();

    for (;;) {
      TSLNodeBase_t* node = orig.value.Next;
      if (node == nullptr) {
        return nullptr;
      }

      // dimhotepus: This dereference is safe only if node reclamation is appropriately
      // deferred while concurrent operations may still access it. The value may be stale
      // (node popped and reused meanwhile); the CAS below then fails because the sequence
      // changed. Relaxed atomic load: the race is intentional.
      TSLHead_t next = orig;
      next.value.Next = tslist_detail::LoadRelaxed( &node->Next );
      next.value.Depth = orig.value.Depth - 1;
      next.value.Sequence = orig.value.Sequence + 1;

      if (CompareExchangeHead(orig, next)) {
        return node;
      }
    }
  }

  TSLNodeBase_t *Detach() {
    TSLHead_t orig = LoadHead();

    for (;;) {
      TSLHead_t next = orig;
      next.value.Next = nullptr;
      next.value.Depth = 0;
      next.value.Sequence = orig.value.Sequence + 1;

      if (CompareExchangeHead(orig, next)) {
        return orig.value.Next;
      }
    }
  }

  [[nodiscard]] [[deprecated("Unprotected access is unsafe and should not be used")]]
  std::atomic<TSLHead_t> *AccessUnprotected() {
    return &head_;
  }

  // Approximate when other threads push/pop concurrently. 32-bit builds: wraps at 65536.
  [[nodiscard]] int Count() const {
#ifdef TSLIST_INLINE_HEAD_CAS
    return static_cast<int>(tslist_detail::LoadRelaxed(&RawHead()->value.Depth));
#else
    TSLHead_t orig = head_.load(std::memory_order_relaxed);
    return orig.value.Depth;
#endif
  }

 private:
#ifdef TSLIST_INLINE_HEAD_CAS
  static_assert(sizeof(std::atomic<TSLHead_t>) == sizeof(TSLHead_t),
                "Inline head CAS needs std::atomic<TSLHead_t> to be just the value");

  TSLHead_t *RawHead() { return reinterpret_cast<TSLHead_t *>(&head_); }
  const TSLHead_t *RawHead() const { return reinterpret_cast<const TSLHead_t *>(&head_); }

  // Field-wise snapshot; may be torn, the CAS validates it.
  TSLHead_t LoadHead() const {
    const TSLHead_t *raw = RawHead();
    TSLHead_t h;
    h.value.Sequence = tslist_detail::LoadAcquire(&raw->value.Sequence);
    h.value.Depth = tslist_detail::LoadRelaxed(&raw->value.Depth);
    h.value.Next = tslist_detail::LoadAcquire(&raw->value.Next);
    return h;
  }

  // Full barrier (acq_rel) like the std::atomic version. On failure, expected receives
  // the current head.
  bool CompareExchangeHead(TSLHead_t &expected, const TSLHead_t &desired) {
    const auto old = __sync_val_compare_and_swap(&RawHead()->value64x128,
                                                 expected.value64x128,
                                                 desired.value64x128);
    if (old == expected.value64x128) {
      return true;
    }
    expected.value64x128 = old;
    return false;
  }
#else
  TSLHead_t LoadHead() const {
    return head_.load(std::memory_order_acquire);
  }

  bool CompareExchangeHead(TSLHead_t &expected, const TSLHead_t &desired) {
    return head_.compare_exchange_weak(expected, desired,
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire);
  }
#endif

  std::atomic<TSLHead_t> head_;
};

static_assert( alignof( CTSListBase ) >= TSLIST_HEAD_ALIGNMENT, "List object must keep its head CAS-aligned" );
static_assert( sizeof( std::atomic<TSLHead_t> ) >= sizeof( TSLHead_t ), "std::atomic must store the whole head" );

// Lock free simple stack.
template <typename T>
class TSLIST_HEAD_ALIGN CTSSimpleList : public CTSListBase
{
	// dimhotepus: Was an unused enable_if_t alias (obscure error, easy to lose). T is linked
	// directly, so it must derive from TSLNodeBase_t.
	static_assert( tslist_detail::is_tslist_node_v<T>, "CTSSimpleList<T>: T must derive from TSLNodeBase_t" );
	static_assert( alignof( T ) >= TSLIST_NODE_ALIGNMENT, "CTSSimpleList<T>: T must keep the node alignment" );

public:
	void Push( T *pNode )
	{
		static_assert( sizeof(T) >= sizeof(TSLNodeBase_t) );
		CTSListBase::Push( pNode );
	}

	T *Pop()
	{
		return static_cast<T *>(CTSListBase::Pop());
	}
};

// Replacement for CTSList<> and CObjectPool<> that does not have a per-item,
// per-alloc new/delete overhead similar to CTSSimpleList except that it
// allocates it's own pool objects and frees them on destruct.  Also it does not
// overlay the TSLNodeBase_t memory on T's memory.
template< class T > 
class TSLIST_HEAD_ALIGN CTSPool : public CTSListBase
{
	// packs the node and the item (T) into a single struct and pools those
	struct TSLIST_NODE_ALIGN TSPoolNode_t :
		public CAlignedNewDelete<TSLIST_NODE_ALIGNMENT, TSLNodeBase_t>
	{
		T elem;
	};

	static_assert( tslist_detail::fits_node_alignment_v<T>,
		"CTSPool<T>: alignof(T) exceeds TSLIST_NODE_ALIGNMENT, pool nodes would misalign T" );
	static_assert( std::is_default_constructible_v<T>, "CTSPool<T>: GetObject() default-constructs T" );
	static_assert( std::is_destructible_v<T>, "CTSPool<T>: Purge() destroys T" );
	static_assert( alignof( TSPoolNode_t ) == TSLIST_NODE_ALIGNMENT, "CTSPool<T>: node alignment" );

public:
	~CTSPool()
	{
		Purge();
	}

	void Purge()
	{
		while ( true )
		{
			auto *pNode = static_cast<TSPoolNode_t *>(CTSListBase::Pop());
			if ( !pNode )
				break;

			delete pNode;
		}
	}

	// pInfo must come from GetObject()/Get() of this pool (as in Valve's original), and be
	// returned once.
	void PutObject( T *pInfo )
	{
#ifndef NDEBUG
		// Every pool node is TSLIST_NODE_ALIGNMENT aligned: if the node computed from pInfo is
		// not, pInfo cannot come from a pool - reject without touching that memory.
		if ( ( reinterpret_cast<uintptr_t>( pInfo ) - ElemOffset() ) % alignof( TSPoolNode_t ) != 0 )
		{
			AssertMsg( false, "CTSPool::PutObject( %p ): object was not obtained from GetObject() of pool %p (misaligned for a pool node).",
				static_cast<void *>( pInfo ), static_cast<void *>( this ) );
			return;
		}
#endif

		// dimhotepus: Return the object's own node to the pool. The previous version
		// allocated a new node and copied *pInfo into it, so the node handed out by
		// GetObject() leaked on every Get/Put cycle (and T had to be copyable).
		TSPoolNode_t *pNode = NodeFromElem( pInfo );

#ifndef NDEBUG
		// dimhotepus: Debug check of the contract. GetObject() tagged the node with this
		// pool; claim the tag atomically, so that even two threads returning the same
		// object at once are caught (exactly one wins). A rejected object is NOT pushed:
		// pushing a foreign or already-pooled node would corrupt the pool.
		// Note: for a foreign object this reads the word(s) just before it - harmless for
		// heap objects inside bigger allocations, reported by ASan for others; either way
		// the call is a bug.
		TSLNodeBase_t *const pTag = tslist_detail::MakeOwnerTag( this );
		auto *const pSeen = static_cast<TSLNodeBase_t *>( ThreadInterlockedCompareExchangePointer(
			reinterpret_cast<void * volatile *>( &pNode->Next ), nullptr, pTag ) );
		if ( pSeen != pTag )
		{
			if ( tslist_detail::IsOwnerTag( pSeen ) )
			{
				AssertMsg( false, "CTSPool::PutObject( %p ): object belongs to another pool (%p), not to %p.",
					static_cast<void *>( pInfo ), tslist_detail::OwnerFromTag( pSeen ), static_cast<void *>( this ) );
			}
			else
			{
				AssertMsg( false, "CTSPool::PutObject( %p ): object was not obtained from GetObject() of pool %p, or was already returned (double PutObject).",
					static_cast<void *>( pInfo ), static_cast<void *>( this ) );
			}
			return;
		}
#endif

		CTSListBase::Push( pNode );
	}

	T *GetObject()
	{
		auto *pNode = static_cast<TSPoolNode_t *>(CTSListBase::Pop());
		if ( !pNode )
		{
			pNode = new TSPoolNode_t;
		}
		// dimhotepus: Mark the node as handed out by this pool (checked by PutObject in debug
		// builds). Written in release too, so that debug and release code can share a pool.
		tslist_detail::StoreRelaxed( &pNode->Next, tslist_detail::MakeOwnerTag( this ) );
		return &pNode->elem;
	}

	// omg windows sdk - why do you #define GetObject()?
	FORCEINLINE T *Get()
	{
		return GetObject();
	}

private:
	// TSPoolNode_t derives from TSLNodeBase_t, so it is not standard-layout and offsetof
	// is "conditionally-supported". MSVC, GCC and Clang all support it for non-virtual
	// inheritance (the node has no virtual bases); GCC/Clang only warn.
	[[nodiscard]] static constexpr size_t ElemOffset()
	{
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
		return offsetof( TSPoolNode_t, elem );
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic pop
#endif
	}

	[[nodiscard]] static TSPoolNode_t *NodeFromElem( T *pElem )
	{
		constexpr size_t nElemOffset = ElemOffset();
		// Not >= sizeof(TSLNodeBase_t): the Itanium ABI (GCC/Clang) places elem in the base's
		// tail padding (offset 8 on 64-bit), MSVC after it (offset 16). Both are fine as long
		// as elem does not overlap Next.
		static_assert( nElemOffset >= offsetof( TSLNodeBase_t, Next ) + sizeof( TSLNodeBase_t * ),
			"CTSPool<T>: elem must not overlap the Next link" );
		static_assert( nElemOffset + sizeof( T ) <= sizeof( TSPoolNode_t ), "CTSPool<T>: elem must lie inside the node" );
		return reinterpret_cast<TSPoolNode_t *>( reinterpret_cast<char *>( pElem ) - nElemOffset );
	}
};

// Lock free typed list.
//
// dimhotepus: PopItem() used to delete the popped node while other threads could still be
// inside Pop() reading that node's Next (heap-use-after-free; with the small block heap the
// page can even be decommitted -> access violation). PopItem() now recycles nodes through
// an internal free list that PushItem() reuses; they are deleted by Purge()/the destructor,
// which must not run concurrently with other operations. T must be copy-assignable.
// If you Pop() nodes yourself with other threads still popping, do not delete them: give
// them back with FreeNode().
template <typename T>
class TSLIST_HEAD_ALIGN CTSList : public CTSListBase
{
public:
	struct TSLIST_NODE_ALIGN Node_t : public CAlignedNewDelete<TSLIST_NODE_ALIGNMENT, TSLNodeBase_t>
	{
		// dimhotepus: Next starts as nullptr (never "tagged"); elem() keeps the value
		// initialization the old defaulted constructor gave for new Node_t().
		Node_t() : elem() { Next = nullptr; }
		Node_t( const T &init ) : elem( init ) { Next = nullptr; }

		~Node_t()
		{
#ifndef NDEBUG
			CTSList::DebugCheckNodeDelete( this );
#endif
		}

		T elem;
	};

	static_assert( tslist_detail::fits_node_alignment_v<T>,
		"CTSList<T>: alignof(T) exceeds TSLIST_NODE_ALIGNMENT, list nodes would misalign T" );
	static_assert( alignof( Node_t ) == TSLIST_NODE_ALIGNMENT, "CTSList<T>: node alignment" );

	CTSList()
	{
#ifndef NDEBUG
		auto &registry = tslist_detail::DebugListRegistry();
		std::lock_guard<std::mutex> lock( registry.mutex );
		registry.alive.insert( this );
#endif
	}

	CTSList( const CTSList & ) = delete;
	CTSList &operator=( const CTSList & ) = delete;

	~CTSList()
	{
		Purge();

#ifndef NDEBUG
		auto &registry = tslist_detail::DebugListRegistry();
		std::lock_guard<std::mutex> lock( registry.mutex );
		registry.alive.erase( this );
#endif
	}

	// Not thread-safe.
	void Purge()
	{
		AssertMsg( m_nDebugActivePops.load( std::memory_order_acquire ) == 0,
			"CTSList::Purge(): called while another thread is inside Pop() on list %p (Purge is not thread-safe).",
			static_cast<void *>( this ) );

		DeleteChain( Detach() );
		DeleteChain( static_cast<Node_t *>( m_FreeNodes.Detach() ) );
	}

	// Not thread-safe.
	void RemoveAll()
	{
		Purge();
	}

	Node_t *Push( Node_t *pNode )
	{
		return static_cast<Node_t *>(CTSListBase::Push( pNode ));
	}

	// The returned node may still be read by other threads inside Pop(): do not delete it
	// while any thread may be popping - give it back with FreeNode() or Push() instead.
	// Debug builds assert on such a delete.
	Node_t *Pop()
	{
#ifndef NDEBUG
		m_nDebugActivePops.fetch_add( 1, std::memory_order_acq_rel );
		auto *pNode = static_cast<Node_t *>( CTSListBase::Pop() );
		if ( pNode )
		{
			// Tag the node as "handed out by this list" (see DebugCheckNodeDelete).
			tslist_detail::StoreRelaxed( &pNode->Next, tslist_detail::MakeOwnerTag( this ) );
		}
		m_nDebugActivePops.fetch_sub( 1, std::memory_order_acq_rel );
		return pNode;
#else
		return static_cast<Node_t *>(CTSListBase::Pop());
#endif
	}

	void PushItem( const T &init )
	{
		static_assert( std::is_copy_constructible_v<T>, "CTSList<T>::PushItem: T must be copy-constructible (new node)" );
		static_assert( std::is_copy_assignable_v<T>, "CTSList<T>::PushItem: T must be copy-assignable (recycled node)" );

		auto *pNode = static_cast<Node_t *>( m_FreeNodes.Pop() );
		if ( pNode )
		{
			pNode->elem = init;
		}
		else
		{
			pNode = new Node_t( init );
		}
		Push( pNode );
	}

	bool PopItem( T *pResult)
	{
		static_assert( std::is_copy_assignable_v<T>, "CTSList<T>::PopItem: T must be copy-assignable" );

		Node_t *pNode = Pop();
		if ( !pNode )
			return false;

		*pResult = pNode->elem;
		m_FreeNodes.Push( pNode );
		return true;
	}

	// Recycle a node obtained from Pop() of this list, or a fresh node (instead of
	// deleting it).
	void FreeNode( Node_t *pNode )
	{
#ifndef NDEBUG
		// A rejected node is NOT pushed: it still belongs to the other list, and keeping it in
		// two lists would delete it twice.
		const TSLNodeBase_t *const pNext = tslist_detail::LoadRelaxed( &pNode->Next );
		if ( tslist_detail::IsOwnerTag( pNext ) && tslist_detail::OwnerFromTag( pNext ) != this )
		{
			AssertMsg( false, "CTSList::FreeNode( %p ): node was popped from another list (%p), not from %p.",
				static_cast<void *>( pNode ), tslist_detail::OwnerFromTag( pNext ), static_cast<void *>( this ) );
			return;
		}
#endif
		m_FreeNodes.Push( pNode );
	}

	Node_t *Detach()
	{
		return static_cast<Node_t *>(CTSListBase::Detach());
	}

private:
	static void DeleteChain( Node_t *pCurrent )
	{
		while ( pCurrent )
		{
			auto *pNext = static_cast<Node_t *>( pCurrent->Next );
			delete pCurrent;
			pCurrent = pNext;
		}
	}

#ifndef NDEBUG
	// dimhotepus: Debug check for deleting a node handed out by Pop() while another thread
	// is inside Pop() on the same list - that thread may read the node's Next after the
	// delete (use-after-free; an access violation if its page gets decommitted).
	// Deleting with no concurrent Pop() is fine and not reported.
	static void DebugCheckNodeDelete( Node_t *pNode )
	{
		const TSLNodeBase_t *const pNext = tslist_detail::LoadRelaxed( &pNode->Next );
		if ( !tslist_detail::IsOwnerTag( pNext ) )
			return;  // never handed out by Pop() (fresh, linked, or recycled node)

		const auto *pOwner = static_cast<const CTSList *>( tslist_detail::OwnerFromTag( pNext ) );

		// Only look at the owner if it is still alive (it may have been destroyed before
		// this node is deleted, which is legal). Holding the registry lock also keeps it
		// alive during the check: ~CTSList unregisters under the same lock.
		auto &registry = tslist_detail::DebugListRegistry();
		std::lock_guard<std::mutex> lock( registry.mutex );
		if ( registry.alive.count( pOwner ) &&
			 pOwner->m_nDebugActivePops.load( std::memory_order_acquire ) > 0 )
		{
			AssertMsg( false, "CTSList: node %p from Pop() deleted while another thread is inside Pop() on list %p. "
				"Give it back with FreeNode() (or Push()), or delete only when no thread can be popping.",
				static_cast<void *>( pNode ), static_cast<const void *>( pOwner ) );
		}
	}
#endif

	CTSListBase m_FreeNodes;

	// Threads currently inside Pop(); maintained in debug builds only. Present in release
	// too, so that debug and release code agree on the class layout.
	std::atomic<int> m_nDebugActivePops{ 0 };
};

// Lock free list with list for free nodes.
template <typename T>
class TSLIST_HEAD_ALIGN CTSListWithFreeList : public CTSListBase
{
public:
	struct TSLIST_NODE_ALIGN Node_t : public TSLNodeBase_t
	{
		Node_t() = default;
		Node_t( const T &init ) : elem( init ) {}

		T elem;
	};

	static_assert( tslist_detail::fits_node_alignment_v<T>,
		"CTSListWithFreeList<T>: alignof(T) exceeds TSLIST_NODE_ALIGNMENT, list nodes would misalign T" );
	static_assert( alignof( Node_t ) == TSLIST_NODE_ALIGNMENT, "CTSListWithFreeList<T>: node alignment" );

	~CTSListWithFreeList()
	{
		Purge();
	}

	void Purge()
	{
		Node_t *pCurrent = Detach();
		while ( pCurrent )
		{
			auto *pNext = static_cast<Node_t *>(pCurrent->Next);
			delete pCurrent;
			pCurrent = pNext;
		}
		pCurrent = static_cast<Node_t *>(m_FreeList.Detach());
		while ( pCurrent )
		{
			auto *pNext = static_cast<Node_t *>(pCurrent->Next);
			delete pCurrent;
			pCurrent = pNext;
		}
	}

	void RemoveAll()
	{
		Node_t *pCurrent = Detach();
		while ( pCurrent )
		{
			auto *pNext = static_cast<Node_t *>(pCurrent->Next);
			m_FreeList.Push( pCurrent );
			pCurrent = pNext;
		}
	}

	Node_t *Push( Node_t *pNode )
	{
		return static_cast<Node_t *>(CTSListBase::Push( pNode ));
	}

	Node_t *Pop()
	{
		return static_cast<Node_t *>(CTSListBase::Pop());
	}

	void PushItem( const T &init )
	{
		static_assert( std::is_default_constructible_v<T>, "CTSListWithFreeList<T>::PushItem: T must be default-constructible (new node)" );
		static_assert( std::is_copy_assignable_v<T>, "CTSListWithFreeList<T>::PushItem: T must be copy-assignable" );

		auto *pNode = static_cast<Node_t *>(m_FreeList.Pop());
		if ( !pNode )
		{
			pNode = new Node_t;
		}
		pNode->elem = init;
		Push( pNode );
	}

	bool PopItem( T *pResult)
	{
		static_assert( std::is_copy_assignable_v<T>, "CTSListWithFreeList<T>::PopItem: T must be copy-assignable" );

		Node_t *pNode = Pop();
		if ( !pNode )
			return false;

		*pResult = pNode->elem;
		m_FreeList.Push( pNode );
		return true;
	}

	Node_t *Detach()
	{
		return static_cast<Node_t *>(CTSListBase::Detach());
	}

	void FreeNode( Node_t *pNode )
	{
		m_FreeList.Push( pNode );
	}

private:
	CTSListBase m_FreeList;
};

// Lock free queue.
//
// A special consideration: the element type should be simple.  This code
// actually dereferences freed nodes as part of pop, but later detects that.
// If the item in the queue is a complex type, only bad things can come of that.
// 
// Also, therefore, if you're using Push/Pop instead of push item, be aware that
// the node memory cannot be freed until all threads that might have been
// popping have completed the pop.  The PushItem()/PopItem() for handles this by
// keeping a persistent free list.  Don't mix Push/PushItem.  Note also nodes
// will be freed at the end, and are expected to have been allocated with
// operator new.
template <typename T, bool bTestOptimizer = false>
class TSLIST_NODE_ALIGN CTSQueue : public CAlignedNewDelete<TSLIST_NODE_ALIGNMENT>
{
public:
	// dimhotepus: CTSQueue has strict requirements due to lock-free dereference of freed nodes
	static_assert(std::is_trivially_copyable_v<T>,
		"CTSQueue requires trivially copyable types. "
		"The queue dereferences freed nodes to read element values, which is only safe for simple types. "
		"Use: fundamental types, enums, pointers, trivial aggregates of these.");
	static_assert(std::is_trivially_destructible_v<T>,
		"CTSQueue requires trivially destructible types. "
		"Destructors are never called on queue elements. "
		"Use: types with no user-defined, deleted, or virtual destructors.");
	static_assert(std::is_standard_layout_v<T>,
		"CTSQueue requires standard-layout types. "
		"Memory layout assumptions depend on consistent field ordering.");
	static_assert(tslist_detail::fits_node_alignment_v<T>,
		"CTSQueue<T>: alignof(T) exceeds TSLIST_NODE_ALIGNMENT, queue nodes would misalign T");

	struct TSLIST_NODE_ALIGN Node_t : public TSLNodeBase_t
	{
		Node_t() = default;
		Node_t( const T &init ) : elem{init}
		{
			Next = nullptr;
		}

		T elem;

		// dimhotepus: Relaxed atomic access; Next is read by other threads while pushes link it.
		[[nodiscard]] Node_t *GetNextNode() { return static_cast<Node_t*>( tslist_detail::LoadRelaxed( &Next ) ); }
		[[nodiscard]] Node_t **GetNextNodePtr() { return reinterpret_cast<Node_t**>(&Next); }
		void SetNextNode(Node_t* next) { tslist_detail::StoreRelaxed( &Next, static_cast<TSLNodeBase_t *>( next ) ); }
	};

	union TSLIST_HEAD_ALIGN NodeLink_t
	{
		// override new/delete so we can guarantee {8,16}-byte aligned allocs
		static void * operator new( size_t size )
		{
			auto *link = static_cast<NodeLink_t *>(MemAlloc_AllocAligned( size, TSLIST_HEAD_ALIGNMENT, __FILE__, __LINE__ ));
			return link;
		}

		static void operator delete( void *p )
		{
			MemAlloc_FreeAligned( p );
		}

		struct Value_t
		{
			Node_t *pNode;
			intp	sequence;
		} value;

#ifdef PLATFORM_64BITS
		int128 value64x128;
#else
		int64 value64x128;
#endif
	};

	// dimhotepus: Same contract as TSLHead_t: one CAS of value64x128 swaps the whole link and
	// compares raw bytes, so the struct view must cover it exactly, without padding.
	static_assert( sizeof( NodeLink_t ) == 2 * sizeof( void * ), "CTSQueue: link is a pointer plus a sequence" );
	static_assert( alignof( NodeLink_t ) == TSLIST_HEAD_ALIGNMENT, "CTSQueue: link must be CAS-aligned" );
	static_assert( sizeof( typename NodeLink_t::Value_t ) == sizeof( NodeLink_t ), "CTSQueue: value must cover the link" );
	static_assert( sizeof( NodeLink_t{}.value64x128 ) == sizeof( NodeLink_t ), "CTSQueue: value64x128 must cover the link" );
	static_assert( std::has_unique_object_representations_v<typename NodeLink_t::Value_t>,
		"CTSQueue: link must have no padding bits (byte-wise CAS)" );
	static_assert( alignof( Node_t ) == TSLIST_NODE_ALIGNMENT, "CTSQueue: node alignment" );

	CTSQueue()
	{
		static_assert( sizeof(Node_t) >= sizeof(TSLNodeBase_t) );

		if ( reinterpret_cast<size_t>(&m_Head) % TSLIST_HEAD_ALIGNMENT != 0 )
		{
			Error( "CTSQueue: Misaligned queue head %p.\n", &m_Head );
		}
		if ( reinterpret_cast<size_t>(&m_Tail) % TSLIST_HEAD_ALIGNMENT != 0 )
		{
			Error( "CTSQueue: Misaligned queue tail %p.\n", &m_Tail );
		}

		m_Count.store(0, std::memory_order_relaxed );
		
		// list always contains a dummy node
		m_Head.value.pNode = m_Tail.value.pNode = new Node_t;
		m_Head.value.pNode->SetNextNode(End());
		m_Head.value.sequence = m_Tail.value.sequence = 0;
	}

	~CTSQueue()
	{
		Purge();

		Assert( m_Count.load( std::memory_order_relaxed ) == 0 );
		Assert( m_Head.value.pNode == m_Tail.value.pNode );
		Assert( m_Head.value.pNode->GetNextNode() == End() ); //-V2002

		delete m_Head.value.pNode;
	}

	// Note: Purge, RemoveAll, and Validate are *not* threadsafe
	void Purge()
	{
#ifndef NDEBUG
		[[maybe_unused]] const bool bValid{ ValidateQueue() };
		AssertMsg( bValid, "Corrupt CTSQueue detected." );
#endif

		Node_t *pNode;
		while ( ( pNode = Pop() ) != nullptr )
		{
			delete pNode;
		}

		while ( ( pNode = static_cast<Node_t *>(m_FreeNodes.Pop()) ) != nullptr )
		{
			delete pNode;
		}

		Assert( m_Count.load() == 0 );
		Assert( m_Head.value.pNode == m_Tail.value.pNode );
		Assert( m_Head.value.pNode->GetNextNode() == End() ); //-V2002

		m_Head.value.sequence = m_Tail.value.sequence = 0;
	}

	void RemoveAll()
	{
#ifndef NDEBUG
		ValidateQueue();
#endif

		Node_t *pNode;
		while ( ( pNode = Pop() ) != nullptr )
		{
			m_FreeNodes.Push( pNode );
		}
	}

	[[nodiscard]] bool ValidateQueue()
	{
#ifndef NDEBUG
		{
			bool bResult = true;
			int nNodes = 0;
			if ( m_Tail.value.pNode->GetNextNode() != End() ) //-V2002
			{
				DebuggerBreakIfDebugging();
				bResult = false;
			}

			if ( m_Count.load( std::memory_order_relaxed ) == 0 )
			{
				if ( m_Head.value.pNode != m_Tail.value.pNode )
				{
					DebuggerBreakIfDebugging();
					bResult = false;
				}
			}

			Node_t *pNode = m_Head.value.pNode;
			while ( pNode != End() )
			{
				nNodes++;
				pNode = pNode->GetNextNode(); //-V2002
			}

			nNodes--;// skip dummy node

			if ( nNodes != m_Count.load( std::memory_order_relaxed) )
			{
				DebuggerBreakIfDebugging();
				bResult = false;
			}

			if ( !bResult )
			{
				Warning( "Corrupt CTSQueue %p detected.\n", this );
			}

			return bResult;
		}
#else
		return true;
#endif
	}

	Node_t *Push( Node_t *pNode )
	{
#ifndef NDEBUG
		if ( reinterpret_cast<size_t>(pNode) % TSLIST_NODE_ALIGNMENT != 0 )
		{
			Error( "CTSQueue: Misaligned node %p.\n", pNode );
		}
#endif

		NodeLink_t oldTail;

		pNode->SetNextNode( End() );

		for (;;)
		{
			// dimhotepus: Snapshot via atomic loads (the tail is CASed concurrently); a torn
			// snapshot only makes FinishPush's CAS fail.
			oldTail.value.sequence = tslist_detail::LoadAcquire( &m_Tail.value.sequence );
			oldTail.value.pNode = tslist_detail::LoadAcquire( &m_Tail.value.pNode );
			if ( InterlockedCompareExchangeNode( oldTail.value.pNode->GetNextNodePtr(), pNode, End() ) ) //-V1051
			{
				break;
			}
			else
			{
				// Another thread is trying to push, help it along
				FinishPush( oldTail.value.pNode->GetNextNode(), oldTail ); //-V2002
			}
		}

		// This can fail if another thread pushed between the sequence and node grabs above. Later pushes or pops corrects
		FinishPush( pNode, oldTail ); 

		m_Count.fetch_add(1, std::memory_order_relaxed);

		return oldTail.value.pNode;
	}

	Node_t *Pop()
	{
		// dimhtepus: x64 support.
#ifdef PLATFORM_64BITS
		Node_t * TSQUEUE_BAD_NODE_LINK = static_cast<Node_t *>(reinterpret_cast<void *>(static_cast<size_t>(0xdeadbeefdeadbeef)));
#else
		Node_t * TSQUEUE_BAD_NODE_LINK = static_cast<Node_t *>(reinterpret_cast<void *>(static_cast<size_t>(0xdeadbeef)));
#endif
		// dimhotepus: The volatile pointers below were how this forced re-reads; head/tail
		// fields are now read with atomic loads instead (concurrently CASed, by design).
		NodeLink_t *pHead = &m_Head;

		NodeLink_t head;
		NodeLink_t newHead;
		Node_t *pNext;
		intp tailSequence;
		T elem;

		for (;;)
		{
			head.value.sequence = tslist_detail::LoadAcquire( &m_Head.value.sequence ); // must grab sequence first, which allows condition below to ensure pNext is valid
			ThreadMemoryBarrier(); // need a barrier to prevent reordering of these assignments
			head.value.pNode	= tslist_detail::LoadAcquire( &m_Head.value.pNode );
			tailSequence		= tslist_detail::LoadAcquire( &m_Tail.value.sequence );
			pNext				= head.value.pNode->GetNextNode(); //-V2002

			// Checking pNext only to force optimizer to not reorder the assignment
			// to pNext and the compare of the sequence
			ThreadMemoryBarrier();
			if ( !pNext || head.value.sequence != tslist_detail::LoadAcquire( &m_Head.value.sequence ) ) 
				continue;

			if ( bTestOptimizer )
			{
				if ( pNext == TSQUEUE_BAD_NODE_LINK )
				{
					Warning( "TSQueue: Bad node link detected.\n" );
					continue;
				}
			}

			if ( head.value.pNode == tslist_detail::LoadAcquire( &m_Tail.value.pNode ) )
			{
				if ( pNext == End() )
					return nullptr;

				// Another thread is trying to push, help it along
				NodeLink_t &oldTail = head; // just reuse local memory for head to build old tail
				oldTail.value.sequence = tailSequence; // reuse head pNode
				FinishPush( pNext, oldTail );
				continue;
			}
			
			if ( pNext != End() )
			{
				elem = pNext->elem; // NOTE: next could be a freed node here, by design
				newHead.value.pNode = pNext;
				newHead.value.sequence = head.value.sequence + 1;
				if ( InterlockedCompareExchangeNodeLink( pHead, newHead, head ) )
				{
					ThreadMemoryBarrier();
					if ( bTestOptimizer )
					{
						head.value.pNode->SetNextNode( TSQUEUE_BAD_NODE_LINK );
					}
					break;
				}
			}
		}

		m_Count.fetch_sub(1, std::memory_order_relaxed);
		head.value.pNode->elem = elem;
		return head.value.pNode;
	}

	void PushItem( const T &init )
	{
		// dimhotepus: Documents intent at API boundary.
		static_assert( std::is_copy_constructible_v< T >, "T must be copy constructible for PushItem" );

		auto *pNode = static_cast<Node_t *>(m_FreeNodes.Pop());
		if ( pNode )
		{
			pNode->elem = init;
		}
		else
		{
			pNode = new Node_t( init );
		}
		Push( pNode );
	}

	[[nodiscard]] bool PopItem( T *pResult )
	{
		// dimhotepus: Documents intent at API boundary.
		static_assert(!std::is_reference_v<T>, "T must not be a reference type");

		Node_t *pNode = Pop();
		if ( !pNode )
			return false;

		*pResult = pNode->elem;
		m_FreeNodes.Push( pNode );
		return true;
	}

	[[nodiscard]] int Count() const
	{
		return m_Count.load(std::memory_order_relaxed);
	}

private:
	NodeLink_t m_Head;
	NodeLink_t m_Tail;

	std::atomic_int m_Count;
	
	CTSListBase m_FreeNodes;

	Node_t m_end;

	// just need a unique signifier
	Node_t *End() { return &m_end; }

	[[nodiscard]] static bool InterlockedCompareExchangeNode( Node_t * volatile *ppNode, Node_t *value, Node_t *comperand )
	{
		return ThreadInterlockedCompareExchangePointer( reinterpret_cast<void * volatile *>( ppNode ), value, comperand ) == comperand;
	}

	bool InterlockedCompareExchangeNodeLink( NodeLink_t volatile *pLink, const NodeLink_t &value, const NodeLink_t &comperand )
	{
		return ThreadInterlockedAssignIf64x128( &pLink->value64x128, value.value64x128, comperand.value64x128 );
	}
	
	void FinishPush( Node_t *pNode, const NodeLink_t &oldTail )
	{
		NodeLink_t newTail;

		newTail.value.pNode = pNode;
		newTail.value.sequence = oldTail.value.sequence + 1;

		ThreadMemoryBarrier();

		InterlockedCompareExchangeNodeLink( &m_Tail, newTail, oldTail );
	}

	void FreeNode(Node_t *pNode) { m_FreeNodes.Push(pNode); }
};

#endif  // !SE_PUBLIC_TIER0_TSLIST_H_