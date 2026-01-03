// Copyright Valve Corporation, All rights reserved.
//
// Hashed intrusive linked list.

#ifndef SE_UTILS_SHADERCOMPILE_UTLNODEHASH_H_
#define SE_UTILS_SHADERCOMPILE_UTLNODEHASH_H_

#include "tier1/utlmemory.h"
#include "tier1/utlintrusivelist.h"

// To use this class, your list node class must have a Key() function defined
// which returns an integer type.  May add this class to main utl tier when i'm
// happy with it.
template <typename T, unsigned HASHSIZE = 7907, typename K = intp>
class CUtlNodeHash {
 public:
  CUtlIntrusiveDList<T> m_HashChains[HASHSIZE];

  CUtlNodeHash() { m_nNumNodes = 0; }

  T *FindByKey(K key, uintp *found_chain = nullptr) const {
    uintp chain = (uintp)key;

    chain %= HASHSIZE;

    if (found_chain) *found_chain = chain;

    for (auto *node = m_HashChains[chain].m_pHead; node; node = node->m_pNext)
      if (node->Key() == key) return node;

    return nullptr;
  }

  void Add(T *pNode) {
    uintp nChain = (uintp)pNode->Key();

    nChain %= HASHSIZE;

    m_HashChains[nChain].AddToHead(pNode);
    ++m_nNumNodes;
  }

  void Purge() {
    m_nNumNodes = 0;

    // delete all nodes
    for (unsigned i = 0; i < HASHSIZE; i++) {
      m_HashChains[i].Purge();
    }
  }

  intp Count() const { return m_nNumNodes; }

  void DeleteByKey(K nMatchKey) {
    uintp chain;
    T *value = FindByKey(nMatchKey, &chain);

    if (value) {
      m_HashChains[chain].RemoveNode(value);

      m_nNumNodes--;
    }
  }

  // delete all lists
  ~CUtlNodeHash() { Purge(); }

 private:
  intp m_nNumNodes;
};

#endif  // !SE_UTILS_SHADERCOMPILE_UTLNODEHASH_H_
