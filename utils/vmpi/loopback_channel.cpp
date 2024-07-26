// Copyright Valve Corporation, All rights reserved.
//
//

#include "loopback_channel.h"

#include "tier1/utllinkedlist.h"
#include "iphelpers.h"

struct LoopbackMsg_t {
  ptrdiff_t m_Len;
  unsigned char m_Data[1];
};

class CLoopbackChannel : public IChannel {
 public:
  virtual ~CLoopbackChannel() {
    FOR_EACH_LL(m_Messages, i) { free(m_Messages[i]); }

    m_Messages.Purge();
  }

  void Release() override { delete this; }

  bool Send(const void *pData, ptrdiff_t len) override {
    const void *pChunks[1] = {pData};
    ptrdiff_t chunkLengths[1] = {len};

    return SendChunks(pChunks, chunkLengths, 1);
  }

  bool SendChunks(void const *const *pChunks, const ptrdiff_t *pChunkLengths,
                  ptrdiff_t nChunks) override {
    CChunkWalker walker(pChunks, pChunkLengths, nChunks);

    auto *pMsg = (LoopbackMsg_t *)malloc(sizeof(LoopbackMsg_t) - 1 +
                                                  walker.GetTotalLength());
    if (!pMsg) return false;

    walker.CopyTo(pMsg->m_Data, walker.GetTotalLength());
    pMsg->m_Len = walker.GetTotalLength();
    m_Messages.AddToTail(pMsg);
    return true;
  }

  bool Recv(CUtlVector<unsigned char> &data, double flTimeout) override {
    int iNext = m_Messages.Head();
    if (iNext == m_Messages.InvalidIndex()) {
      return false;
    }

    LoopbackMsg_t *pMsg = m_Messages[iNext];
    data.CopyArray(pMsg->m_Data, pMsg->m_Len);

    free(pMsg);
    m_Messages.Remove(iNext);

    return true;
  }

  bool IsConnected() override { return true; }

  void GetDisconnectReason(CUtlVector<char> &reason) override {}

 private:
  // FIFO for messages we've sent.
  CUtlLinkedList<LoopbackMsg_t *, int> m_Messages;
};

IChannel *CreateLoopbackChannel() { return new CLoopbackChannel; }
