#ifndef SHA3_H
#define SHA3_H

#include <stdint.h>

/* -------------------------------------------------------------------------
* Works when compiled for either 32-bit or 64-bit targets, optimized for 
* 64 bit.
*
* Canonical implementation of Init/Update/Finalize for SHA-3 byte input. 
*
* SHA3-256, SHA3-384, SHA-512 are implemented. SHA-224 can easily be added.
*
* Based on code from http://keccak.noekeon.org/ .
*
* I place the code that I wrote into public domain, free to use. 
*
* I would appreciate if you give credits to this work if you used it to 
* write or test * your code.
*
* Aug 2015. Andrey Jivsov. crypto@brainhub.org
* ---------------------------------------------------------------------- */

/* Ported to C++ and tailored to Aubergine passwords which are always 64 
*   bytes (512 bits, 8 64-bit words) long and are always provided as a 
*   single buffer of type uint64_t[8]
* The only part we need of the <sha3_context> structure in the original 
*   is therefore <s> which is of type uint64_t[25]; <capacityWords> is 
*   always 16 and the other three numbers are always 0
* 
* Modifications (c)2022 Nine Tiles */

/* 'Words' here refers to uint64_t; <sizeof> counts octets */
#define SHA3_KECCAK_SPONGE_WORDS (1600/(sizeof(uint64_t) * 8))
typedef uint64_t sha3_sponge[SHA3_KECCAK_SPONGE_WORDS];

// Hashing for Aubergine passwords
void sha3_512_HashBuffer( 
    const uint64_t * pw, // 4 words containing password
    const uint64_t * rs, // 4 words containing random string
    uint64_t * out);    // for the SHA3-512 hash

#endif