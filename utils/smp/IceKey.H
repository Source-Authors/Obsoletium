/*
 * Header file for the C++ ICE encryption class.
 *
 * Written by Matthew Kwan - July 1996
 */

#ifndef _IceKey_H
#define _IceKey_H

class IceSubkey;

class IceKey {
 public:
  IceKey(int n);
  ~IceKey();

  void set(const unsigned char *key);

  void encrypt(const unsigned char *plaintext, unsigned char *ciphertext) const;

  void decrypt(const unsigned char *ciphertext, unsigned char *plaintext) const;

  int keySize() const;

  int blockSize() const;

 private:
  void scheduleBuild(unsigned short *k, int n, const int *keyrot);

  int _size;
  int _rounds;
  IceSubkey *_keysched;
};

#endif