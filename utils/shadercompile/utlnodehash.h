// Copyright Valve Corporation, All rights reserved.
//
// Hashed intrusive linked list.

#ifndef SRC_UTILS_SHADERCOMPILE_UTLNODEHASH_H_
#define SRC_UTILS_SHADERCOMPILE_UTLNODEHASH_H_

#include "tier1/utlmemory.h"
#include "tier1/utlintrusivelist.h"

// To use this class, your list node class must have a Key() function defined
// which returns an integer type.  May add this class to main utl tier when i'm
// happy with it.
template <typename T, int HASHSIZE = 7907, typename K = int>
class CUtlNodeHash {
 public:
  CUtlIntrusiveDList<T> m_HashChains[HASHSIZE];

  CUtlNodeHash() { m_nNumNodes = 0; }

  T *FindByKey(K key, int *found_chain = nullptr) {
    unsigned int chain = (unsigned int)key;

    chain %= HASHSIZE;

    if (found_chain) *found_chain = chain;

    for (auto *node = m_HashChains[chain].m_pHead; node; node = node->m_pNext)
      if (node->Key() == key) return node;

    return nullptr;
  }

  void Add(T *pNode) {
    unsigned int nChain = (unsigned int)pNode->Key();

    nChain %= HASHSIZE;

    m_HashChains[nChain].AddToHead(pNode);
    ++m_nNumNodes;
  }

  void Purge() {
    m_nNumNodes = 0;

    // delete all nodes
    for (int i = 0; i < HASHSIZE; i++) {
      m_HashChains[i].Purge();
    }
  }

  int Count() const { return m_nNumNodes; }

  void DeleteByKey(K nMatchKey) {
    int chain;
    T *value = FindByKey(nMatchKey, &chain);

    if (value) {
      m_HashChains[chain].RemoveNode(value);

      m_nNumNodes--;
    }
  }

  // delete all lists
  ~CUtlNodeHash() { Purge(); }

 private:
  int m_nNumNodes;
};

#endif  // !SRC_UTILS_SHADERCOMPILE_UTLNODEHASH_H_
