/// Copyright (c) 2013, Aldrin's Notebook.
/// http://opensource.org/licenses/BSD-2-Clause
#include "crypto.h"

using namespace ssu;

void random_generation()
{
  assert(crypto::prng_ok());                                       // check PRNG state

  crypto::block buffer;                                            // use the convenience typedef
  crypto::fill_random(buffer);                                     // fill it with random bytes
  unsigned char arr[1024];                                         // use a static POD array
  crypto::fill_random(arr);                                        // fill it with random bytes
  std::vector<unsigned char> vec(16);                              // use a std::vector
  crypto::fill_random(vec);                                        // fill it with random bytes
}

void key_generation()
{
  crypto::block key;                                               // 128 bit key
  crypto::block salt;                                              // 128 bit salt
  crypto::fill_random(salt);                                       // random salt.
  crypto::derive_key(key, "password", salt);                       // password derived key
  crypto::cleanse(key);                                            // clear sensitive data
}

void message_digest()
{
  crypto::hash md;                                                 // the hash object
  crypto::hash::value sha;                                         // the hash value
  md.update("hello world!");                                       // add data
  md.update("see you world!");                                     // add more data
  md.finalize(sha);                                                // get digest value
}

void message_authentication_code()
{
  crypto::block key;                                               // the hash key
  crypto::fill_random(key);                                        // random key will do for now
  crypto::hash h(key);                                             // the keyed-hash object
  crypto::hash::value mac;                                         // the mac value
  h.update("hello world!");                                        // add data
  h.update("see you world!");                                      // more data
  h.finalize(mac);                                                 // get the MAC code
  crypto::cleanse(key);                                            // clear sensitive data
}

void encryption()
{
  crypto::block iv;                                                // initialization vector
  crypto::block key;                                               // encryption key
  crypto::block seal;                                              // container for the seal
  crypto::fill_random(iv);                                         // random initialization vector
  crypto::fill_random(key);                                        // random key will do (for now)
  unsigned char date[] = {14, 1, 13};                              // associated data
  std::string text("can you keep a secret?");                      // message (plain-text)
  std::vector<unsigned char> ciphertext(text.size());              // container for encrypted data
  {
    crypto::cipher cipher(key, iv);                                // initialize cipher (encrypt mode)
    cipher.associate_data(date);                                   // add associated data first
    cipher.transform(text, ciphertext);                            // do transform (i.e. encrypt)
    cipher.seal(seal);                                             // get the encryption seal
  }
  std::vector<unsigned char> decrypted(ciphertext.size());         // container for decrypted data
  {
    crypto::cipher cipher(key, iv, seal);                          // initialize cipher (decrypt mode)
    cipher.associate_data(date);                                   // add associated data first
    cipher.transform(ciphertext, decrypted);                       // do transform (i.e. decrypt)
    cipher.verify();                                               // check the seal
  }
  assert(std::equal(text.begin(), text.end(), decrypted.begin())); // sanity (decrypted == plaintext)

  date[0] = 15;                                                    // modify the associated data
  {
    crypto::cipher cipher(key, iv, seal);                          // initialize cipher (decrypt mode)
    cipher.associate_data(date);                                   // add associated data first
    cipher.transform(ciphertext, decrypted);                       // try decryption again
    try
    {
      cipher.verify();                                             // should throw an exception
      assert(false);                                               // should never be reached
    }
    catch (...) {}
  }

  date[0] = 14;                                                    // revert associated data
  ciphertext[0] = '\0';                                            // modify ciphertext.
  {
    crypto::cipher cipher(key, iv, seal);                          // initialize cipher (decrypt mode)
    cipher.associate_data(date);                                   // add associated data first
    cipher.transform(ciphertext, decrypted);                       // try decryption again
    try
    {
      cipher.verify();                                             // should throw an exception
      assert(false);                                               // should never be reached
    }
    catch (...) {}
  }
  crypto::cleanse(key);                                            // clear sensitive data
}

int main()
{
  random_generation();
  key_generation();
  message_digest();
  encryption();
}
