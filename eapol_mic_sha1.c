/*
 *   BSD LICENSE
 * 
 *   Copyright(c) 2007-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *  version: RWPA_VNF.L.18.02.0-42
 */

/*
 * SHA1 hash implementation and interface functions
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <stdint.h>
#include <stdio.h>

#include <rte_memcpy.h>
#include "eapol.h"

#define MD5_MAC_LEN 16
#define SHA384_MAC_LEN 48
#define SHA1_MAC_LEN 20

struct VNF_SHA1Context {
	uint32_t state[5];
	uint32_t count[2];
	unsigned char buffer[64];
};

static int vnf_sha1_vector(size_t num_elem, const uint8_t *addr[], const size_t *len, uint8_t *mac);
static int vnf_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *mac);
static int vnf_hmac_sha1_vector(const uint8_t *key, size_t key_len, size_t num_elem, const uint8_t *addr[], const size_t *len, uint8_t *mac);

typedef struct VNF_SHA1Context VNF_SHA1_CTX;

static void VNF_SHA1Init(VNF_SHA1_CTX* context);
static void VNF_SHA1Update(VNF_SHA1_CTX* context, const void *_data, uint32_t len);
static void VNF_SHA1Final(unsigned char digest[20], VNF_SHA1_CTX* context);
static void VNF_SHA1Transform(uint32_t state[5], const unsigned char buffer[64]);

static void * os_memset(void *s, int c, size_t n)
{
        char *p = s;
        while (n--)
                *p++ = c;
        return s;
}

/**
 * sha1_vector - SHA-1 hash for data vector
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash
 * Returns: 0 on success, -1 of failure
 */
static int vnf_sha1_vector(size_t num_elem, const uint8_t *addr[], const size_t *len, uint8_t *mac)
{
	VNF_SHA1_CTX ctx;
	size_t i;

	VNF_SHA1Init(&ctx);
	for (i = 0; i < num_elem; i++)
		VNF_SHA1Update(&ctx, addr[i], len[i]);
	VNF_SHA1Final(mac, &ctx);
	return 0;
}

/* ===== start - public domain SHA1 implementation ===== */

/*
SHA-1 in C
By Steve Reid <sreid@sea-to-sky.net>
100% Public Domain

-----------------
Modified 7/98
By James H. Brown <jbrown@burgoyne.com>
Still 100% Public Domain

Corrected a problem which generated improper hash values on 16 bit machines
Routine SHA1Update changed from
	void SHA1Update(SHA1_CTX* context, unsigned char* data, unsigned int
len)
to
	void SHA1Update(SHA1_CTX* context, unsigned char* data, unsigned
long len)

The 'len' parameter was declared an int which works fine on 32 bit machines.
However, on 16 bit machines an int is too small for the shifts being done
against
it.  This caused the hash function to generate incorrect values if len was
greater than 8191 (8K - 1) due to the 'len << 3' on line 3 of SHA1Update().

Since the file IO in main() reads 16K at a time, any file 8K or larger would
be guaranteed to generate the wrong hash (e.g. Test Vector #3, a million
"a"s).

I also changed the declaration of variables i & j in SHA1Update to
unsigned long from unsigned int for the same reason.

These changes should make no difference to any 32 bit implementations since
an
int and a long are the same size in those environments.

--
I also corrected a few compiler warnings generated by Borland C.
1. Added #include <process.h> for exit() prototype
2. Removed unused variable 'j' in SHA1Final
3. Changed exit(0) to return(0) at end of main.

ALL changes I made can be located by searching for comments containing 'JHB'
-----------------
Modified 8/98
By Steve Reid <sreid@sea-to-sky.net>
Still 100% public domain

1- Removed #include <process.h> and used return() instead of exit()
2- Fixed overwriting of finalcount in SHA1Final() (discovered by Chris Hall)
3- Changed email address from steve@edmweb.com to sreid@sea-to-sky.net

-----------------
Modified 4/01
By Saul Kravitz <Saul.Kravitz@celera.com>
Still 100% PD
Modified to run on Compaq Alpha hardware.

-----------------
Modified 4/01
By Jouni Malinen <j@w1.fi>
Minor changes to match the coding style used in Dynamics.

Modified September 24, 2004
By Jouni Malinen <j@w1.fi>
Fixed alignment issue in SHA1Transform when SHA1HANDSOFF is defined.

*/

/*
Test Vectors (from FIPS PUB 180-1)
"abc"
  A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
  84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
A million repetitions of "a"
  34AA973C D4C4DAA4 F61EEB2B DBAD2731 6534016F
*/

#define VNF_SHA1HANDSOFF

#define vnf_rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/* blk0() and blk() perform the initial expand. */
/* I got the idea of expanding during the round function from SSLeay */
#ifndef WORDS_BIGENDIAN
#define vnf_blk0(i) (block->l[i] = (vnf_rol(block->l[i], 24) & 0xFF00FF00) | \
	(vnf_rol(block->l[i], 8) & 0x00FF00FF))
#else
#define vnf_blk0(i) block->l[i]
#endif
#define vnf_blk(i) (block->l[i & 15] = vnf_rol(block->l[(i + 13) & 15] ^ \
	block->l[(i + 8) & 15] ^ block->l[(i + 2) & 15] ^ block->l[i & 15], 1))

/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define vnf_R0(v,w,x,y,z,i) \
	z += ((w & (x ^ y)) ^ y) + vnf_blk0(i) + 0x5A827999 + vnf_rol(v, 5); \
	w = vnf_rol(w, 30);
#define vnf_R1(v,w,x,y,z,i) \
	z += ((w & (x ^ y)) ^ y) + vnf_blk(i) + 0x5A827999 + vnf_rol(v, 5); \
	w = vnf_rol(w, 30);
#define vnf_R2(v,w,x,y,z,i) \
	z += (w ^ x ^ y) + vnf_blk(i) + 0x6ED9EBA1 + vnf_rol(v, 5); w = vnf_rol(w, 30);
#define vnf_R3(v,w,x,y,z,i) \
	z += (((w | x) & y) | (w & x)) + vnf_blk(i) + 0x8F1BBCDC + vnf_rol(v, 5); \
	w = vnf_rol(w, 30);
#define vnf_R4(v,w,x,y,z,i) \
	z += (w ^ x ^ y) + vnf_blk(i) + 0xCA62C1D6 + vnf_rol(v, 5); \
	w=vnf_rol(w, 30);

/* Hash a single 512-bit block. This is the core of the algorithm. */
static void VNF_SHA1Transform(uint32_t state[5], const unsigned char buffer[64])
{
	uint32_t a, b, c, d, e;
	typedef union {
		unsigned char c[64];
		uint32_t l[16];
	} CHAR64LONG16;
	CHAR64LONG16* block;
#ifdef VNF_SHA1HANDSOFF
	CHAR64LONG16 workspace;
	block = &workspace;
	rte_memcpy(block, buffer, 64);
#else
	block = (CHAR64LONG16 *) buffer;
#endif
	/* Copy context->state[] to working vars */
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	/* 4 rounds of 20 operations each. Loop unrolled. */
	vnf_R0(a,b,c,d,e, 0); vnf_R0(e,a,b,c,d, 1); vnf_R0(d,e,a,b,c, 2); vnf_R0(c,d,e,a,b, 3);
	vnf_R0(b,c,d,e,a, 4); vnf_R0(a,b,c,d,e, 5); vnf_R0(e,a,b,c,d, 6); vnf_R0(d,e,a,b,c, 7);
	vnf_R0(c,d,e,a,b, 8); vnf_R0(b,c,d,e,a, 9); vnf_R0(a,b,c,d,e,10); vnf_R0(e,a,b,c,d,11);
	vnf_R0(d,e,a,b,c,12); vnf_R0(c,d,e,a,b,13); vnf_R0(b,c,d,e,a,14); vnf_R0(a,b,c,d,e,15);
	vnf_R1(e,a,b,c,d,16); vnf_R1(d,e,a,b,c,17); vnf_R1(c,d,e,a,b,18); vnf_R1(b,c,d,e,a,19);
	vnf_R2(a,b,c,d,e,20); vnf_R2(e,a,b,c,d,21); vnf_R2(d,e,a,b,c,22); vnf_R2(c,d,e,a,b,23);
	vnf_R2(b,c,d,e,a,24); vnf_R2(a,b,c,d,e,25); vnf_R2(e,a,b,c,d,26); vnf_R2(d,e,a,b,c,27);
	vnf_R2(c,d,e,a,b,28); vnf_R2(b,c,d,e,a,29); vnf_R2(a,b,c,d,e,30); vnf_R2(e,a,b,c,d,31);
	vnf_R2(d,e,a,b,c,32); vnf_R2(c,d,e,a,b,33); vnf_R2(b,c,d,e,a,34); vnf_R2(a,b,c,d,e,35);
	vnf_R2(e,a,b,c,d,36); vnf_R2(d,e,a,b,c,37); vnf_R2(c,d,e,a,b,38); vnf_R2(b,c,d,e,a,39);
	vnf_R3(a,b,c,d,e,40); vnf_R3(e,a,b,c,d,41); vnf_R3(d,e,a,b,c,42); vnf_R3(c,d,e,a,b,43);
	vnf_R3(b,c,d,e,a,44); vnf_R3(a,b,c,d,e,45); vnf_R3(e,a,b,c,d,46); vnf_R3(d,e,a,b,c,47);
	vnf_R3(c,d,e,a,b,48); vnf_R3(b,c,d,e,a,49); vnf_R3(a,b,c,d,e,50); vnf_R3(e,a,b,c,d,51);
	vnf_R3(d,e,a,b,c,52); vnf_R3(c,d,e,a,b,53); vnf_R3(b,c,d,e,a,54); vnf_R3(a,b,c,d,e,55);
	vnf_R3(e,a,b,c,d,56); vnf_R3(d,e,a,b,c,57); vnf_R3(c,d,e,a,b,58); vnf_R3(b,c,d,e,a,59);
	vnf_R4(a,b,c,d,e,60); vnf_R4(e,a,b,c,d,61); vnf_R4(d,e,a,b,c,62); vnf_R4(c,d,e,a,b,63);
	vnf_R4(b,c,d,e,a,64); vnf_R4(a,b,c,d,e,65); vnf_R4(e,a,b,c,d,66); vnf_R4(d,e,a,b,c,67);
	vnf_R4(c,d,e,a,b,68); vnf_R4(b,c,d,e,a,69); vnf_R4(a,b,c,d,e,70); vnf_R4(e,a,b,c,d,71);
	vnf_R4(d,e,a,b,c,72); vnf_R4(c,d,e,a,b,73); vnf_R4(b,c,d,e,a,74); vnf_R4(a,b,c,d,e,75);
	vnf_R4(e,a,b,c,d,76); vnf_R4(d,e,a,b,c,77); vnf_R4(c,d,e,a,b,78); vnf_R4(b,c,d,e,a,79);
	/* Add the working vars back into context.state[] */
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	/* Wipe variables */
	a = b = c = d = e = 0;
#ifdef VNF_SHA1HANDSOFF
	os_memset(block, 0, 64);
#endif
}

/* SHA1Init - Initialize new context */
static void VNF_SHA1Init(VNF_SHA1_CTX* context)
{
	/* SHA1 initialization constants */
	context->state[0] = 0x67452301;
	context->state[1] = 0xEFCDAB89;
	context->state[2] = 0x98BADCFE;
	context->state[3] = 0x10325476;
	context->state[4] = 0xC3D2E1F0;
	context->count[0] = context->count[1] = 0;
}

/* Run your data through this. */
static void VNF_SHA1Update(VNF_SHA1_CTX* context, const void *_data, uint32_t len)
{
	uint32_t i, j;
	const unsigned char *data = _data;

#ifdef VERBOSE
	SHAPrintContext(context, "before");
#endif
	j = (context->count[0] >> 3) & 63;
	if ((context->count[0] += len << 3) < (len << 3))
		context->count[1]++;
	context->count[1] += (len >> 29);
	if ((j + len) > 63) {
		rte_memcpy(&context->buffer[j], data, (i = 64-j));
		VNF_SHA1Transform(context->state, context->buffer);
		for ( ; i + 63 < len; i += 64) {
			VNF_SHA1Transform(context->state, &data[i]);
		}
		j = 0;
	}
	else i = 0;
	rte_memcpy(&context->buffer[j], &data[i], len - i);
#ifdef VERBOSE
	SHAPrintContext(context, "after ");
#endif
}

/* Add padding and return the message digest. */
static void VNF_SHA1Final(unsigned char digest[20], VNF_SHA1_CTX* context)
{
	uint32_t i;
	unsigned char finalcount[8];

	for (i = 0; i < 8; i++) {
		finalcount[i] = (unsigned char)
			((context->count[(i >= 4 ? 0 : 1)] >>
			  ((3-(i & 3)) * 8) ) & 255);  /* Endian independent */
	}
	VNF_SHA1Update(context, (const unsigned char *) "\200", 1);
	while ((context->count[0] & 504) != 448) {
		VNF_SHA1Update(context, (const unsigned char *) "\0", 1);
	}
	VNF_SHA1Update(context, finalcount, 8);  /* Should cause a SHA1Transform()
					      */
	for (i = 0; i < 20; i++) {
		digest[i] = (unsigned char)
			((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) &
			 255);
	}
	/* Wipe variables */
	os_memset(context->buffer, 0, 64);
	os_memset(context->state, 0, 20);
	os_memset(context->count, 0, 8);
	os_memset(finalcount, 0, 8);
}

/* ===== end - public domain SHA1 implementation ===== */

static int vnf_hmac_sha1_vector(const uint8_t *key, size_t key_len, size_t num_elem,
				const uint8_t *addr[], const size_t *len, uint8_t *mac)
{
	unsigned char k_pad[64]; /* padding - key XORd with ipad/opad */
	unsigned char tk[20];
	const uint8_t *_addr[6];
	size_t _len[6], i;
	int ret;

	if (num_elem > 5) {
		/*
		 * Fixed limit on the number of fragments to avoid having to
		 * allocate memory (which could fail).
		 */
		return -1;
	}

        /* if key is longer than 64 bytes reset it to key = SHA1(key) */
        if (key_len > 64) {
		if (vnf_sha1_vector(1, &key, &key_len, tk))
			return -1;
		key = tk;
		key_len = 20;
        }

	/* the HMAC_SHA1 transform looks like:
	 *
	 * SHA1(K XOR opad, SHA1(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected */

	/* start out by storing key in ipad */
	size_t key_len_tmp = key_len < sizeof(k_pad) ?
			     key_len : sizeof(k_pad);
	os_memset(k_pad, 0, sizeof(k_pad));
	rte_memcpy(k_pad, key, key_len_tmp);
	/* XOR key with ipad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x36;

	/* perform inner SHA1 */
	_addr[0] = k_pad;
	_len[0] = 64;
	for (i = 0; i < num_elem; i++) {
		_addr[i + 1] = addr[i];
		_len[i + 1] = len[i];
	}
	if (vnf_sha1_vector(1 + num_elem, _addr, _len, mac))
		return -1;

	key_len_tmp = key_len < sizeof(k_pad) ?
		      key_len : sizeof(k_pad);
	os_memset(k_pad, 0, sizeof(k_pad));
	rte_memcpy(k_pad, key, key_len_tmp);
	/* XOR key with opad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x5c;

	/* perform outer SHA1 */
	_addr[0] = k_pad;
	_len[0] = 64;
	_addr[1] = mac;
	_len[1] = SHA1_MAC_LEN;
	ret = vnf_sha1_vector(2, _addr, _len, mac);
	os_memset(k_pad, 0, sizeof(k_pad));
	return ret;
}

static int vnf_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data,
			 size_t data_len, uint8_t *mac)
{
	return vnf_hmac_sha1_vector(key, key_len, 1, &data, &data_len, mac);
}

int vnf_wpa_eapol_key_mic(const uint8_t *key, size_t key_len, int ver,
			  const uint8_t *buf, size_t len, uint8_t *mic)
{
	uint8_t hash[SHA384_MAC_LEN];

	/* Reset MIC field before regenerating. */
	os_memset(mic, 0, 16);

	switch (ver) {

	case 2:
    	if (vnf_hmac_sha1(key, key_len, buf, len, hash))
			return -1;
		rte_memcpy(mic, hash, MD5_MAC_LEN);
		break;
	default:
		return -1;
	}

	return 0;
}
