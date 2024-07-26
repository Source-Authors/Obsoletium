// Copyright Valve Corporation, All rights reserved.
//
//
// MessageBuffer - handy for packing and upacking structures to be sent as
// messages.

#ifndef SRC_UTILS_VMPI_MESSAGEBUFFER_H_
#define SRC_UTILS_VMPI_MESSAGEBUFFER_H_

#include <cstdio>

constexpr inline ptrdiff_t DEFAULT_MESSAGE_BUFFER_SIZE{2048};

class MessageBuffer {
 public:
  char *data;

  MessageBuffer();
  MessageBuffer(ptrdiff_t size);
  ~MessageBuffer();

  ptrdiff_t getSize() const { return size; }
  ptrdiff_t getLen() const { return len; }
  ptrdiff_t setLen(ptrdiff_t len);
  ptrdiff_t getOffset() const { return offset; }
  ptrdiff_t setOffset(ptrdiff_t offset);

  ptrdiff_t write(void const *p, ptrdiff_t bytes);
  ptrdiff_t update(ptrdiff_t loc, void const *p, ptrdiff_t bytes);
  ptrdiff_t extract(ptrdiff_t loc, void *p, ptrdiff_t bytes);
  ptrdiff_t read(void *p, ptrdiff_t bytes);

  ptrdiff_t WriteString(const char *pString);
  ptrdiff_t ReadString(char *pOut, ptrdiff_t bufferLength);

  void clear();
  void clear(ptrdiff_t minsize);
  void reset(ptrdiff_t minsize);
  void print(FILE *ofile, ptrdiff_t num) const;

 private:
  void resize(ptrdiff_t minsize);

  ptrdiff_t size;
  ptrdiff_t offset;
  ptrdiff_t len;
};

#endif  // !SRC_UTILS_VMPI_MESSAGEBUFFER_H_
