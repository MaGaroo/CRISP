/*
 ============================================================================
 Name        : ZKBpp_bool.c
 Author      : ANONYMOUS - based on Sobuno's ZKBoo v0.1
 Version     : 1.0
 Description : MPC BITDEC and SHA-256 evaluation using ZKBpp
 ============================================================================
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "shared.h"
#include "omp.h"
/*
============================================================================================================
============================================================================================================
*/
#define CH(e,f,g) ((e & f) ^ ((~e) & g))

int totalRandom = 0;
int totalSha = 0;
int totalSS = 0;
int totalHash = 0;
int NUM_ROUNDS = 219;


void int2bin(uint32_t n, char* out) { 
	for(int j=31; j>=0; j--) {
		int k = n >> j;
		out[31-j] = k & 1;
	} 
} 


uint32_t rand32() {
	uint32_t x;
	x = rand() & 0xff;
	x |= (rand() & 0xff) << 8;
	x |= (rand() & 0xff) << 16;
	x |= (rand() & 0xff) << 24;

	return x;
}


void printbits(uint32_t n) {
	if (n) {
		printbits(n >> 1);
		printf("%d", n & 1);
	}

}


int sha256(unsigned char* result, unsigned char* input, int numBits) {
	// SHA-256 function 
	uint32_t hA[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
			0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

	if (numBits > 447) {
		printf("Input too long, aborting!");
		return -1;
	}
	int chars = numBits >> 3;
	unsigned char* chunk = calloc(64, 1); //512 bits
	memcpy(chunk, input, chars);
	chunk[chars] = 0x80;

	//Last 8 chars used for storing length of input without padding, in big-endian.
	//Since we only care for one block, we are safe with just using last 9 bits and 0'ing the rest

	chunk[62] = numBits >> 8;
	chunk[63] = numBits;

	uint32_t w[64];
	int i;
	for (i = 0; i < 16; i++) {
		w[i] = (chunk[i * 4] << 24) | (chunk[i * 4 + 1] << 16)
						| (chunk[i * 4 + 2] << 8) | chunk[i * 4 + 3];
	}

	uint32_t s0, s1;
	for (i = 16; i < 64; i++) {
		s0 = RIGHTROTATE(w[i - 15], 7) ^ RIGHTROTATE(w[i - 15], 18)
						^ (w[i - 15] >> 3);
		s1 = RIGHTROTATE(w[i - 2], 17) ^ RIGHTROTATE(w[i - 2], 19)
						^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	
	uint32_t a, b, c, d, e, f, g, h, temp1, temp2, maj;
	a = hA[0];
	b = hA[1];
	c = hA[2];
	d = hA[3];
	e = hA[4];
	f = hA[5];
	g = hA[6];
	h = hA[7];

	for (i = 0; i < 64; i++) {
		s1 = RIGHTROTATE(e,6) ^ RIGHTROTATE(e, 11) ^ RIGHTROTATE(e, 25);

		temp1 = h + s1 + CH(e, f, g) + k[i] + w[i];
		s0 = RIGHTROTATE(a,2) ^ RIGHTROTATE(a, 13) ^ RIGHTROTATE(a, 22);


		maj = (a & (b ^ c)) ^ (b & c);
		temp2 = s0 + maj;


		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;

	}

	hA[0] += a;
	hA[1] += b;
	hA[2] += c;
	hA[3] += d;
	hA[4] += e;
	hA[5] += f;
	hA[6] += g;
	hA[7] += h;

	for (i = 0; i < 8; i++) {
		result[i * 4] = (hA[i] >> 24);
		result[i * 4 + 1] = (hA[i] >> 16);
		result[i * 4 + 2] = (hA[i] >> 8);
		result[i * 4 + 3] = hA[i];
	}
	return 0;
}


void mpc_XOR(uint32_t x[3], uint32_t y[3], uint32_t z[3]) {
	z[0] = x[0] ^ y[0];
	z[1] = x[1] ^ y[1];
	z[2] = x[2] ^ y[2];
}


void mpc_AND(uint32_t x[3], uint32_t y[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t r[3] = { getRandom1(randomness[0], *randCount), getRandom1(randomness[1], *randCount), getRandom1(randomness[2], *randCount)};
	*randCount += 1;
	uint32_t t[3] = { 0 };

	t[0] = (x[0] & y[1]) ^ (x[1] & y[0]) ^ (x[0] & y[0]) ^ r[0] ^ r[1];
	t[1] = (x[1] & y[2]) ^ (x[2] & y[1]) ^ (x[1] & y[1]) ^ r[1] ^ r[2];
	t[2] = (x[2] & y[0]) ^ (x[0] & y[2]) ^ (x[2] & y[2]) ^ r[2] ^ r[0];
	z[0] = t[0];
	z[1] = t[1];
	z[2] = t[2];
	views[0].y[*countY] = z[0];
	views[1].y[*countY] = z[1];
	views[2].y[*countY] = z[2];

	(*countY)++;
}


void mpc_NEGATE(uint32_t x[3], uint32_t z[3]) {
	z[0] = ~x[0];
	z[1] = ~x[1];
	z[2] = ~x[2];
}


void mpc_ADD(uint32_t x[3], uint32_t y[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t c[3] = { 0 };
	uint32_t r[3] = { getRandom32(randomness[0], *randCount), getRandom32(randomness[1], *randCount), getRandom32(randomness[2], *randCount)};
	*randCount += 4;

	uint8_t a[3], b[3];

	uint8_t t;

	for(int i=0;i<31;i++)
	{
		a[0]=GETBIT(x[0]^c[0],i);
		a[1]=GETBIT(x[1]^c[1],i);
		a[2]=GETBIT(x[2]^c[2],i);

		b[0]=GETBIT(y[0]^c[0],i);
		b[1]=GETBIT(y[1]^c[1],i);
		b[2]=GETBIT(y[2]^c[2],i);

		t = (a[0]&b[1]) ^ (a[1]&b[0]) ^ GETBIT(r[1],i);
		SETBIT(c[0],i+1, t ^ (a[0]&b[0]) ^ GETBIT(c[0],i) ^ GETBIT(r[0],i));

		t = (a[1]&b[2]) ^ (a[2]&b[1]) ^ GETBIT(r[2],i);
		SETBIT(c[1],i+1, t ^ (a[1]&b[1]) ^ GETBIT(c[1],i) ^ GETBIT(r[1],i));

		t = (a[2]&b[0]) ^ (a[0]&b[2]) ^ GETBIT(r[0],i);
		SETBIT(c[2],i+1, t ^ (a[2]&b[2]) ^ GETBIT(c[2],i) ^ GETBIT(r[2],i));


	}

	z[0]=x[0]^y[0]^c[0];
	z[1]=x[1]^y[1]^c[1];
	z[2]=x[2]^y[2]^c[2];


	views[0].y[*countY] = c[0];
	views[1].y[*countY] = c[1];
	views[2].y[*countY] = c[2];

	*countY += 1;


}


void mpc_ADDK(uint32_t x[3], uint32_t y, uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t c[3] = { 0 };
	uint32_t r[3] = { getRandom32(randomness[0], *randCount), getRandom32(randomness[1], *randCount), getRandom32(randomness[2], *randCount)};
	*randCount += 4;

	uint8_t a[3], b[3];

	uint8_t t;

	for(int i=0;i<31;i++)
	{
		a[0]=GETBIT(x[0]^c[0],i);
		a[1]=GETBIT(x[1]^c[1],i);
		a[2]=GETBIT(x[2]^c[2],i);

		b[0]=GETBIT(y^c[0],i);
		b[1]=GETBIT(y^c[1],i);
		b[2]=GETBIT(y^c[2],i);

		t = (a[0]&b[1]) ^ (a[1]&b[0]) ^ GETBIT(r[1],i);
		SETBIT(c[0],i+1, t ^ (a[0]&b[0]) ^ GETBIT(c[0],i) ^ GETBIT(r[0],i));

		t = (a[1]&b[2]) ^ (a[2]&b[1]) ^ GETBIT(r[2],i);
		SETBIT(c[1],i+1, t ^ (a[1]&b[1]) ^ GETBIT(c[1],i) ^ GETBIT(r[1],i));

		t = (a[2]&b[0]) ^ (a[0]&b[2]) ^ GETBIT(r[0],i);
		SETBIT(c[2],i+1, t ^ (a[2]&b[2]) ^ GETBIT(c[2],i) ^ GETBIT(r[2],i));

	}


	z[0]=x[0]^y^c[0];
	z[1]=x[1]^y^c[1];
	z[2]=x[2]^y^c[2];


	views[0].y[*countY] = c[0];
	views[1].y[*countY] = c[1];
	views[2].y[*countY] = c[2];
	*countY += 1;

}


void mpc_GAMMA(uint32_t x1[3], uint32_t x2[3], uint32_t x3[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t r[3] = { getRandom1(randomness[0], *randCount), getRandom1(randomness[1], *randCount), getRandom1(randomness[2], *randCount)};
	*randCount += 1;
	uint32_t t[3] = { 0 };
	uint32_t AC[3] = { 0 };
	uint32_t BC[3] = { 0 };

	AC[0] = x1[0] ^ x3[0] ^ 1;
	AC[1] = x1[1] ^ x3[1] ^ 1;
	AC[2] = x1[2] ^ x3[2] ^ 1;

	BC[0] = x2[0] ^ x3[0];
	BC[1] = x2[1] ^ x3[1];
	BC[2] = x2[2] ^ x3[2];

	mpc_AND(AC,BC,t,randomness,randCount,views,countY);

	z[0] = t[0] ^ x2[0];
	z[1] = t[1] ^ x2[1];
	z[2] = t[2] ^ x2[2];

	views[0].y[*countY] = z[0];
	views[1].y[*countY] = z[1];
	views[2].y[*countY] = z[2];
	(*countY)++;
}


void mpc_gateA(uint32_t x[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t r[3] = { getRandom1(randomness[0], *randCount), getRandom1(randomness[1], *randCount), getRandom1(randomness[2], *randCount)};
	*randCount += 1;
	uint32_t t[3] = { 0 };

	t[0] = 0 ^ r[0] ^ r[1];
	t[1] = x[1] ^ x[2] ^ r[1] ^ r[2];
	t[2] = 0  ^ r[2] ^ r[0];
	z[0] = t[0];
	z[1] = t[1];
	z[2] = t[2];

	views[0].y[*countY] = z[0];
	views[1].y[*countY] = z[1];
	views[2].y[*countY] = z[2];
	(*countY)++;
}


void mpc_gateB(uint32_t x[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t r[3] = { getRandom1(randomness[0], *randCount), getRandom1(randomness[1], *randCount), getRandom1(randomness[2], *randCount)};
	*randCount += 1;
	uint32_t t[3] = { 0 };

	t[0] = 0 ^ r[0] ^ r[1];
	t[1] = 0 ^ r[1] ^ r[2];
	t[2] = x[0] ^ x[2] ^ 1 ^ r[2] ^ r[0];
	z[0] = t[0];
	z[1] = t[1];
	z[2] = t[2];

	views[0].y[*countY] = z[0];
	views[1].y[*countY] = z[1];
	views[2].y[*countY] = z[2];
	(*countY)++;
}


void mpc_BETA(uint32_t x[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t r[3] = { getRandom1(randomness[0], *randCount), getRandom1(randomness[1], *randCount), getRandom1(randomness[2], *randCount)};
	*randCount += 1;
	uint32_t t[3] = { 0 };
	uint32_t AC[3] = { 0 };
	uint32_t BC[3] = { 0 };

	mpc_gateA(x, BC, randomness, randCount, views, countY);

	mpc_gateB(x, AC, randomness, randCount, views, countY);

	mpc_AND(AC,BC,t,randomness,randCount,views,countY);

	z[0] = t[0];
	z[1] = t[1] ^ x[1];
	z[2] = t[2];

	views[0].y[*countY] = z[0];
	views[1].y[*countY] = z[1];
	views[2].y[*countY] = z[2];

	(*countY)++;
}


void mpc_RIGHTROTATE(uint32_t x[], int i, uint32_t z[]) {
	z[0] = RIGHTROTATE(x[0], i);
	z[1] = RIGHTROTATE(x[1], i);
	z[2] = RIGHTROTATE(x[2], i);
}


void mpc_RIGHTSHIFT(uint32_t x[3], int i, uint32_t z[3]) {
	z[0] = x[0] >> i;
	z[1] = x[1] >> i;
	z[2] = x[2] >> i;
}


void mpc_MAJ(uint32_t a[], uint32_t b[3], uint32_t c[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t t0[3];
	uint32_t t1[3];

	mpc_XOR(a, b, t0);
	mpc_XOR(a, c, t1);
	mpc_AND(t0, t1, z, randomness, randCount, views, countY);
	mpc_XOR(z, a, z);
}


void mpc_CH(uint32_t e[], uint32_t f[3], uint32_t g[3], uint32_t z[3], unsigned char *randomness[3], int* randCount, View views[3], int* countY) {
	uint32_t t0[3];

	//e & (f^g) ^ g
	mpc_XOR(f,g,t0);
	mpc_AND(e,t0,t0, randomness, randCount, views, countY);
	mpc_XOR(t0,g,z);

}


int mpc_bitdec(unsigned char* results[3], unsigned char* inputs[3], int numBytes, unsigned char *randomness[3], View views[3], int* countY, int* randCount) {
	// MPC bit decomposition function that shift from a sharing in Z_q to a sharing in Z_2
	if (numBytes > 32) {
		printf("Input too long, aborting!");
		return -1;
	}

	for (int i = 0; i < 3; i++) {
		memcpy(views[i].x, inputs[i], 32);
	}

	uint32_t X[3];

	uint32_t gamma[3];
	gamma[0] = 0;
	gamma[1] = 0;
	gamma[2] = 0;

	uint32_t beta[3];
	beta[0] = 0;
	beta[1] = 0;
	beta[2] = 0;

	uint32_t alpha[3];
	uint32_t tmp_res[3];

	for (int j=31; j>=0; j--) {

		X[0] = inputs[0][j];
		X[1] = inputs[1][j];
		X[2] = inputs[2][j];

		// Compute [x~]j = [alpha]j ^ [beta]j-1 ^ [gamma]j-1
		results[0][j] = X[0] ^ gamma[0] ^ beta[0];
		results[1][j] = X[1] ^ gamma[1] ^ beta[1];
		results[2][j] = X[2] ^ gamma[2] ^ beta[2];

		// Compute [gamma]j = majority([x]j, [beta]j-1, [gamma]j-1)
		uint32_t AC[3] = { 0 };
		uint32_t BC[3] = { 0 };

		AC[0] = X[0] ^ gamma[0] ^ 1;
		AC[1] = X[1] ^ gamma[1] ^ 1;
		AC[2] = X[2] ^ gamma[2] ^ 1;

		BC[0] = beta[0] ^ gamma[0];
		BC[1] = beta[1] ^ gamma[1];
		BC[2] = beta[2] ^ gamma[2];

		mpc_AND(AC,BC,gamma,randomness,randCount,views,countY);

		gamma[0] = gamma[0] ^ beta[0];
		gamma[1] = gamma[1] ^ beta[1];
		gamma[2] = gamma[2] ^ beta[2];

		// Compute [beta]j = majority(x1j,x2j,x3j)
		mpc_BETA(X, beta, randomness, randCount, views, countY);
		
	} 

	return 0;
}


int mpc_sha256(unsigned char* results[3], unsigned char* inputs[3], int numBits, unsigned char *randomness[3], View views[3], int* countY, int* randCount) {
	// MPC version of SHA256 on Z_2
	if (numBits > 447) {
		printf("Input too long, aborting!");
		return -1;
	}


	int chars = numBits >> 3;
	unsigned char* chunks[3];
	uint32_t w[64][3];

	for (int i = 0; i < 3; i++) {
		chunks[i] = calloc(64, 1);
		memcpy(chunks[i], inputs[i], chars);
		chunks[i][chars] = 0x80;

		chunks[i][62] = numBits >> 8;
		chunks[i][63] = numBits;

		for (int j = 0; j < 16; j++) {
			w[j][i] = (chunks[i][j * 4] << 24) | (chunks[i][j * 4 + 1] << 16)
							| (chunks[i][j * 4 + 2] << 8) | chunks[i][j * 4 + 3];
		}
		free(chunks[i]);
	}

	uint32_t s0[3], s1[3];
	uint32_t t0[3], t1[3];
	for (int j = 16; j < 64; j++) {
		//s0[i] = RIGHTROTATE(w[i][j-15],7) ^ RIGHTROTATE(w[i][j-15],18) ^ (w[i][j-15] >> 3);
		mpc_RIGHTROTATE(w[j-15], 7, t0);

		mpc_RIGHTROTATE(w[j-15], 18, t1);
		mpc_XOR(t0, t1, t0);
		mpc_RIGHTSHIFT(w[j-15], 3, t1);
		mpc_XOR(t0, t1, s0);

		//s1[i] = RIGHTROTATE(w[i][j-2],17) ^ RIGHTROTATE(w[i][j-2],19) ^ (w[i][j-2] >> 10);
		mpc_RIGHTROTATE(w[j-2], 17, t0);
		mpc_RIGHTROTATE(w[j-2], 19, t1);

		mpc_XOR(t0, t1, t0);
		mpc_RIGHTSHIFT(w[j-2], 10, t1);
		mpc_XOR(t0, t1, s1);


		//w[i][j] = w[i][j-16]+s0[i]+w[i][j-7]+s1[i];

		mpc_ADD(w[j-16], s0, t1, randomness, randCount, views, countY);
		mpc_ADD(w[j-7], t1, t1, randomness, randCount, views, countY);
		mpc_ADD(t1, s1, w[j], randomness, randCount, views, countY);

	}

	uint32_t a[3] = { hA[0],hA[0],hA[0] };
	uint32_t b[3] = { hA[1],hA[1],hA[1] };
	uint32_t c[3] = { hA[2],hA[2],hA[2] };
	uint32_t d[3] = { hA[3],hA[3],hA[3] };
	uint32_t e[3] = { hA[4],hA[4],hA[4] };
	uint32_t f[3] = { hA[5],hA[5],hA[5] };
	uint32_t g[3] = { hA[6],hA[6],hA[6] };
	uint32_t h[3] = { hA[7],hA[7],hA[7] };
	uint32_t temp1[3], temp2[3], maj[3];
	for (int i = 0; i < 64; i++) {
		//s1 = RIGHTROTATE(e,6) ^ RIGHTROTATE(e,11) ^ RIGHTROTATE(e,25);
		mpc_RIGHTROTATE(e, 6, t0);
		mpc_RIGHTROTATE(e, 11, t1);
		mpc_XOR(t0, t1, t0);

		mpc_RIGHTROTATE(e, 25, t1);
		mpc_XOR(t0, t1, s1);


		//ch = (e & f) ^ ((~e) & g);
		//temp1 = h + s1 + CH(e,f,g) + k[i]+w[i];
		//t0 = h + s1
		mpc_ADD(h, s1, t0, randomness, randCount, views, countY);
		mpc_CH(e, f, g, t1, randomness, randCount, views, countY);

		//t1 = t0 + t1 (h+s1+ch)
		mpc_ADD(t0, t1, t1, randomness, randCount, views, countY);
		mpc_ADDK(t1, k[i], t1, randomness, randCount, views, countY);
		mpc_ADD(t1, w[i], temp1, randomness, randCount, views, countY);

		//s0 = RIGHTROTATE(a,2) ^ RIGHTROTATE(a,13) ^ RIGHTROTATE(a,22);
		mpc_RIGHTROTATE(a, 2, t0);
		mpc_RIGHTROTATE(a, 13, t1);
		mpc_XOR(t0, t1, t0);
		mpc_RIGHTROTATE(a, 22, t1);
		mpc_XOR(t0, t1, s0);

		mpc_MAJ(a, b, c, maj, randomness, randCount, views, countY);

		//temp2 = s0+maj;
		mpc_ADD(s0, maj, temp2, randomness, randCount, views, countY);
		//e = d+temp1;
		memcpy(h, g, sizeof(uint32_t) * 3);
		memcpy(g, f, sizeof(uint32_t) * 3);
		memcpy(f, e, sizeof(uint32_t) * 3);
		//a = temp1+temp2;
		mpc_ADD(d, temp1, e, randomness, randCount, views, countY);
		memcpy(d, c, sizeof(uint32_t) * 3);
		memcpy(c, b, sizeof(uint32_t) * 3);
		memcpy(b, a, sizeof(uint32_t) * 3);
		mpc_ADD(temp1, temp2, a, randomness, randCount, views, countY);
	}

	uint32_t hHa[8][3] = { { hA[0],hA[0],hA[0]  }, { hA[1],hA[1],hA[1] }, { hA[2],hA[2],hA[2] }, { hA[3],hA[3],hA[3] },
			{ hA[4],hA[4],hA[4] }, { hA[5],hA[5],hA[5] }, { hA[6],hA[6],hA[6] }, { hA[7],hA[7],hA[7] } };
	mpc_ADD(hHa[0], a, hHa[0], randomness, randCount, views, countY);
	mpc_ADD(hHa[1], b, hHa[1], randomness, randCount, views, countY);
	mpc_ADD(hHa[2], c, hHa[2], randomness, randCount, views, countY);
	mpc_ADD(hHa[3], d, hHa[3], randomness, randCount, views, countY);
	mpc_ADD(hHa[4], e, hHa[4], randomness, randCount, views, countY);
	mpc_ADD(hHa[5], f, hHa[5], randomness, randCount, views, countY);
	mpc_ADD(hHa[6], g, hHa[6], randomness, randCount, views, countY);
	mpc_ADD(hHa[7], h, hHa[7], randomness, randCount, views, countY);

	for (int i = 0; i < 8; i++) {
		mpc_RIGHTSHIFT(hHa[i], 24, t0);
		results[0][i * 4] = t0[0];
		results[1][i * 4] = t0[1];
		results[2][i * 4] = t0[2];
		mpc_RIGHTSHIFT(hHa[i], 16, t0);
		results[0][i * 4 + 1] = t0[0];
		results[1][i * 4 + 1] = t0[1];
		results[2][i * 4 + 1] = t0[2];
		mpc_RIGHTSHIFT(hHa[i], 8, t0);
		results[0][i * 4 + 2] = t0[0];
		results[1][i * 4 + 2] = t0[1];
		results[2][i * 4 + 2] = t0[2];

		results[0][i * 4 + 3] = hHa[i][0];
		results[1][i * 4 + 3] = hHa[i][1];
		results[2][i * 4 + 3] = hHa[i][2];
	}

	return 0;
}


int writeToFile(char filename[], void* data, int size, int numItems) {
	FILE *file;

	file = fopen(filename, "wb");
	if (!file) {
		printf("Unable to open file!");
		return 1;
	}
	fwrite(data, size, numItems, file);
	fclose(file);
	return 0;
}


int secretShare(unsigned char* input, int numBytes, unsigned char output[3][numBytes]) {
	if(RAND_bytes(output[0], numBytes) != 1) {
		printf("RAND_bytes failed crypto, aborting\n");
	}
	if(RAND_bytes(output[1], numBytes) != 1) {
		printf("RAND_bytes failed crypto, aborting\n");
	}
	for (int j = 0; j < numBytes; j++) {
		output[2][j] = input[j] ^ output[0][j] ^ output[1][j];
	}
	return 0;
}



a commit(int numBytes,unsigned char shares[3][numBytes], unsigned char *randomness[3], unsigned char rs[3][4], View views[3]) {
	// Split the shares
	unsigned char* inputs[3];
	inputs[0] = shares[0];
	inputs[1] = shares[1];
	inputs[2] = shares[2];
	unsigned char* outputs[3];
	outputs[0] = malloc(32);
	outputs[1] = malloc(32);
	outputs[2] = malloc(32);

	int* randCount = calloc(1, sizeof(int));
	int* countY = calloc(1, sizeof(int));
	mpc_bitdec(outputs, inputs, numBytes, randomness, views, countY, randCount);
	//Explicitly add y to view
	for (int j = 0; j < 32; ++j) {
		views[0].y[*countY] = outputs[0][j];
		views[1].y[*countY] = outputs[1][j];
		views[2].y[*countY] = outputs[2][j];
		(*countY)++;
	} 

// Print for debug
	printf("Y0 = ");
	for (int j = 0; j <32; j++) {
		printf("%u",outputs[0][j]);
	}
	printf("\n");

	printf("Y1 = ");
	for (int j = 0; j <32; j++) {
		printf("%u",outputs[1][j]);
	}
	printf("\n");

	printf("Y2 = ");
	for (int j = 0; j<32; j++) {
		printf("%u",outputs[2][j]);
	}

	unsigned char y_rec[32];
	for (int j = 0; j<32; j++) {
		y_rec[j] = (unsigned char)(outputs[0][j]^outputs[1][j]^outputs[2][j]);
	}

// CONVERT 32 BITS TO 4 BYTES FOR THE MPC_HASH FUNCTION
// /!\ Wires are assumed to be 1 bit although we used uint32_t as data type in the code	
	unsigned char input_clean[4];
	unsigned char input_byte1[4];
	unsigned char input_byte2[4];
	unsigned char input_byte3[4];

	for(int i=0;i<4;i++) {
		// Test O
		SETBIT(input_clean[i], 7,   y_rec[i*8]);
		SETBIT(input_clean[i], 6, y_rec[i*8+1]);
		SETBIT(input_clean[i], 5, y_rec[i*8+2]);
		SETBIT(input_clean[i], 4, y_rec[i*8+3]);
		SETBIT(input_clean[i], 3, y_rec[i*8+4]);
		SETBIT(input_clean[i], 2, y_rec[i*8+5]);
		SETBIT(input_clean[i], 1, y_rec[i*8+6]);
		SETBIT(input_clean[i], 0, y_rec[i*8+7]);

		// Player 1
		SETBIT(input_byte1[i], 7,   outputs[0][i*8]);
		SETBIT(input_byte1[i], 6, outputs[0][i*8+1]);
		SETBIT(input_byte1[i], 5, outputs[0][i*8+2]);
		SETBIT(input_byte1[i], 4, outputs[0][i*8+3]);
		SETBIT(input_byte1[i], 3, outputs[0][i*8+4]);
		SETBIT(input_byte1[i], 2, outputs[0][i*8+5]);
		SETBIT(input_byte1[i], 1, outputs[0][i*8+6]);
		SETBIT(input_byte1[i], 0, outputs[0][i*8+7]);

		// Player 2
		SETBIT(input_byte2[i], 7,   outputs[1][i*8]);
		SETBIT(input_byte2[i], 6, outputs[1][i*8+1]);
		SETBIT(input_byte2[i], 5, outputs[1][i*8+2]);
		SETBIT(input_byte2[i], 4, outputs[1][i*8+3]);
		SETBIT(input_byte2[i], 3, outputs[1][i*8+4]);
		SETBIT(input_byte2[i], 2, outputs[1][i*8+5]);
		SETBIT(input_byte2[i], 1, outputs[1][i*8+6]);
		SETBIT(input_byte2[i], 0, outputs[1][i*8+7]);

		// Player 3
		SETBIT(input_byte3[i], 7,   outputs[2][i*8]);
		SETBIT(input_byte3[i], 6, outputs[2][i*8+1]);
		SETBIT(input_byte3[i], 5, outputs[2][i*8+2]);
		SETBIT(input_byte3[i], 4, outputs[2][i*8+3]);
		SETBIT(input_byte3[i], 3, outputs[2][i*8+4]);
		SETBIT(input_byte3[i], 2, outputs[2][i*8+5]);
		SETBIT(input_byte3[i], 1, outputs[2][i*8+6]);
		SETBIT(input_byte3[i], 0, outputs[2][i*8+7]);
	}

	//CHECK HASH WITH DIRECT COMPUTATION
	unsigned char res_shaB[32];
	sha256(res_shaB, input_clean, 32); // This returns the sha256 of the ASCII encoding of each Byte of the secret
	printf("\n");
	printf("Direct sha of the binary representation of the 32b secret\n"); 
	for(int i = 0; i<8; i++) {
		printf("%02X", (res_shaB[i * 4] << 24) | (res_shaB[i * 4 + 1] << 16) | (res_shaB[i * 4 + 2] << 8) | res_shaB[i * 4 + 3]);
	}
	printf("\n");

	printf("count before sha %u\n", *countY);

	// MPC_SHA on the locally recontructed Bytes
	unsigned char* mpcHASHinput[3];
	mpcHASHinput[0] = input_byte1;
	mpcHASHinput[1] = input_byte2;
	mpcHASHinput[2] = input_byte3;

	unsigned char* hashes[3];
	hashes[0] = malloc(32);
	hashes[1] = malloc(32);
	hashes[2] = malloc(32);

	mpc_sha256(hashes, mpcHASHinput, 4*8, randomness, views, countY, randCount);

	//Explicitly add y to view
	uint32_t shamag[8];
	uint32_t result1[8];
	uint32_t result2[8];
	uint32_t result3[8];

	for(int i = 0; i<8; i++) {
		views[0].y[*countY] = (hashes[0][i * 4] << 24) | (hashes[0][i * 4 + 1] << 16) | (hashes[0][i * 4 + 2] << 8) | hashes[0][i * 4 + 3];
		views[1].y[*countY] = (hashes[1][i * 4] << 24) | (hashes[1][i * 4 + 1] << 16) | (hashes[1][i * 4 + 2] << 8) | hashes[1][i * 4 + 3];
		views[2].y[*countY] = (hashes[2][i * 4] << 24) | (hashes[2][i * 4 + 1] << 16) | (hashes[2][i * 4 + 2] << 8) | hashes[2][i * 4 + 3];
		result1[i] = views[0].y[*countY];
		result2[i] = views[1].y[*countY];
		result3[i] = views[2].y[*countY];
		*countY += 1;
	}

// Save outuput in A container
	uint32_t* result1mem = malloc(32);
	outputSHA(views[0], result1mem);
	uint32_t* result2mem = malloc(32);
	outputSHA(views[1], result2mem);
	uint32_t* result3mem = malloc(32);
	outputSHA(views[2], result3mem);

	a a;
	memset(&a, 0, sizeof(a));
	memcpy(a.yp[0], result1mem, 32);
	memcpy(a.yp[1], result2mem, 32);
	memcpy(a.yp[2], result3mem, 32);

	free(countY);
	free(randCount);

	return a;
}

z prove(int e, unsigned char keys[3][16], unsigned char rs[3][4], View views[3]) {
	z z;
	// prove a la ZKBoo
	memcpy(z.ke, keys[e], 16);
	memcpy(z.ke1, keys[(e + 1) % 3], 16);
	z.ve = views[e];
	z.ve1 = views[(e + 1) % 3];
	//z.x3 = ;
	memcpy(z.re, rs[e],4);
	memcpy(z.re1, rs[(e + 1) % 3],4);

	return z;
}

zz proveZ(int e, int keys[3], unsigned char rs[3][4], View views[3]) {
	// Prove using ZKB++ style
	zz zz;

	if(e==0){
		zz.ve1 = views[(e+1)%3];
		zz.ke = keys[(e)%3];
		zz.ke1 = keys[(e+1)%3];
	}else if(e==1){
		zz.ve1 = views[(e+1)%3];
		zz.ke = keys[(e)%3];
		zz.ke1 = keys[(e+1)%3];
		memcpy(zz.x, views[2].x, 32);;
	}else{
		zz.ve1 = views[(e+1)%3];
		zz.ke = keys[(e)%3];
		zz.ke1 = keys[(e+1)%3];
		memcpy(zz.x, views[2].x, 32);;
	}

	return zz;
}


int main(void) {
	setbuf(stdout, NULL);
	srand((unsigned) time(NULL));
	init_EVP();
	openmp_thread_setup();

	unsigned char garbage[4];
	if(RAND_bytes(garbage, 4) != 1) {
		printf("RAND_bytes failed crypto, aborting\n");
		return 0;
	}
	
	printf("Enter the integer to be converted: ");
	char userInput[55]; //55 is max length
	fgets(userInput, sizeof(userInput), stdin);
	
	
	printf("Iterations of BITDEC: %d\n", NUM_ROUNDS);

	uint32_t user_input = atoi(userInput);
	int i = 32;
	printf("input = %d\n", user_input );

	// Create binary vector for input
	unsigned char input[32];
	int2bin(user_input, input);
	printf("Xb  ");
	for (int j = 0; j < 32; ++j) {
			printf("%u",input[j]);
	}
	printf("\n");

	clock_t begin = clock(), delta, deltaA;
	unsigned char rs[NUM_ROUNDS][3][4];
	unsigned char keys[NUM_ROUNDS][3][16];
	a as[NUM_ROUNDS];
	memset(&as, 0, sizeof(a)*NUM_ROUNDS);
	b bs[NUM_ROUNDS];
	View localViews[NUM_ROUNDS][3];
	int totalCrypto = 0;
	
	//Generating keys
	clock_t beginCrypto = clock(), deltaCrypto;

	int k0 = rand(); // seed for testing - set to random for evaluation
	srand(k0);
	int k_seed[NUM_ROUNDS][3];
	for  (int j = 0; j < NUM_ROUNDS; ++j) {
		k_seed[j][0] = rand();
		k_seed[j][1] = rand();
		k_seed[j][2] = rand();
	}

	deltaCrypto = clock() - beginCrypto;
	int inMilliCrypto = deltaCrypto * 1000 / CLOCKS_PER_SEC;
	totalCrypto = inMilliCrypto;

	
	clock_t beginSS = clock(), deltaSS;
	unsigned char shares[NUM_ROUNDS][3][i];


for (int j = 0; j < NUM_ROUNDS; ++j) {

printf("k0  %u\n", k_seed[j][0]);
		srand(k_seed[j][0]);
		for(int k=0; k<16; k++){
			keys[j][0][k] = rand() % 256;
		}
		for(int k=0; k<4; k++){
			rs[j][0][k] = rand() % 256;
		}
		for(int k=0; k<i; k++){
			shares[j][0][k] = rand() % 256;
		}

printf("k1  %u\n", k_seed[j][1]);
		srand(k_seed[j][1]);
		for(int k=0; k<16; k++){
			keys[j][1][k] = rand() % 256;
		}
		for(int k=0; k<4; k++){
			rs[j][1][k] = rand() % 256;
		}
		for(int k=0; k<i; k++){
			shares[j][1][k] = rand() % 256;
		}

printf("k2  %u\n", k_seed[j][2]);
		srand(k_seed[j][2]);
		for(int k=0; k<16; k++){
			keys[j][2][k] = rand() % 256;
		}
		for(int k=0; k<4; k++){
			rs[j][2][k] = rand() % 256;
		}
		for(int k=0; k<i; k++){
			shares[j][2][k] = rand() % 256;
		}
	}

printf("\nSharing the secret in Z32");
	//#pragma omp parallel for  // Removing parallelisation

// For every round do :
	for(int k=0; k<NUM_ROUNDS; k++) {
		printf("\nX0b ");
		for (int j = 0; j < 32; ++j) {
			shares[k][0][j] = shares[k][0][j] %2;
			printf("%u",shares[k][0][j]);
		}
		// Convert X0b to arithmetic value
		uint32_t inputA0 = bits2int(shares[k][0]);
		printf("  X0d = %u\n", inputA0);

		printf("X1b ");
		for (int j = 0; j < 32; ++j) {
			shares[k][1][j] = shares[k][1][j] %2;
			printf("%u",shares[k][1][j]);
		}
		// Convert X1b to arithmetic value
		uint32_t inputA1 = bits2int(shares[k][1]);
		printf("  X1d = %u\n", inputA1);


		for (int j = 0; j < 32; ++j) {
			shares[k][2][j] = shares[k][2][j] %2;
		}

		// Create third share in Z32
		uint32_t inputA2 = user_input - inputA0 - inputA1;

		unsigned char input2[32];
		int2bin(inputA2,input2);


	// Create the share for Player3 by taking the bit representation of X3
		for (int j = 31; j >=0; j--) {
			shares[k][2][j] = input2[j];
		}
		printf("X2b ");
		for (int j = 0; j < 32; ++j) {
			printf("%u",shares[k][2][j]);
		}
		printf("  X2d = %u\n", inputA2);
	}


	deltaSS = clock() - beginSS;
	int inMilli = deltaSS * 1000 / CLOCKS_PER_SEC;
	totalSS = inMilli;


	//Generating randomness
	clock_t beginRandom = clock(), deltaRandom;
	unsigned char *randomness[NUM_ROUNDS][3];
	//#pragma omp parallel for
	for(int k=0; k<NUM_ROUNDS; k++) {
		for(int j = 0; j<3; j++) {
			randomness[k][j] = malloc(3552*sizeof(unsigned char));
			getAllRandomness(keys[k][j], randomness[k][j]);
		}
	}

	deltaRandom = clock() - beginRandom;
	inMilli = deltaRandom * 1000 / CLOCKS_PER_SEC;
	totalRandom = inMilli;


	//Running MPC-BITDEC
	clock_t beginSha = clock(), deltaSha;
	printf("\nRunning MPC BOOL\n");
	//#pragma omp parallel for
	for(int k=0; k<NUM_ROUNDS; k++) {
		as[k] = commit(i, shares[k], randomness[k], rs[k], localViews[k]);
		for(int j=0; j<3; j++) {
			free(randomness[k][j]);
		}
	}
	deltaSha = clock() - beginSha;
	inMilli = deltaSha * 1000 / CLOCKS_PER_SEC;
	totalSha = inMilli;
	

	// Generating A
	clock_t beginHash = clock(), deltaHash;
	//#pragma omp parallel for
	for(int k=0; k<NUM_ROUNDS; k++) {
		unsigned char hash1[SHA256_DIGEST_LENGTH];
		H(keys[k][0], localViews[k][0], rs[k][0], hash1);
		memcpy(as[k].h[0], &hash1, 32);

		H(keys[k][1], localViews[k][1], rs[k][1], hash1);
		memcpy(as[k].h[1], &hash1, 32);

		H(keys[k][2], localViews[k][2], rs[k][2], hash1);
		memcpy(as[k].h[2], &hash1, 32);

	}
	deltaHash = clock() - beginHash;
				inMilli = deltaHash * 1000 / CLOCKS_PER_SEC;
				totalHash += inMilli;
				
	deltaA = clock() - begin;
	int inMilliA = deltaA * 1000 / CLOCKS_PER_SEC;


	//Generating E
	clock_t beginE = clock(), deltaE;
	int es[NUM_ROUNDS];
	uint32_t finalHash[8];
	for (int j = 0; j < 8; j++) {
		finalHash[j] = as[0].yp[0][j]^as[0].yp[1][j]^as[0].yp[2][j];
	}
	H3(finalHash, as, NUM_ROUNDS, es);
	deltaE = clock() - beginE;
	int inMilliE = deltaE * 1000 / CLOCKS_PER_SEC;


	// Generating B
	clock_t beginB = clock(), deltaB;
	//#pragma omp parallel for
	for(int k=0; k<NUM_ROUNDS; k++) {
		memcpy(bs[k].h, as[k].h[(es[k]+2)%3], 32);
		memcpy(bs[k].y, as[k].yp[(es[k]+2)%3], 32);
	}
	deltaB = clock() - beginB;
	int inMilliB = deltaB * 1000 / CLOCKS_PER_SEC;


	//Packing ZZ (z container for ZKBpp)
	clock_t beginZ = clock(), deltaZ;
	zz* zzs = malloc(sizeof(zz)*NUM_ROUNDS);
	//#pragma omp parallel for
	for(int i = 0; i<NUM_ROUNDS; i++) {
		zzs[i] = proveZ(es[i],k_seed[i],rs[i], localViews[i]);
	}
	deltaZ = clock() - beginZ;
	int inMilliZ = deltaZ * 1000 / CLOCKS_PER_SEC;
	

	//Writing to file
	clock_t beginWrite = clock();
	FILE *file;

	char outputFile[30*sizeof(int) + 8];
	sprintf(outputFile, "out%i.bin", NUM_ROUNDS);
	file = fopen(outputFile, "wb");
	if (!file) {
		printf("Unable to open file!");
		return 1;
	}
	fwrite(finalHash,sizeof(uint32_t), 8,file);
	fwrite(es, sizeof(int), NUM_ROUNDS, file);
	fwrite(bs, sizeof(b), NUM_ROUNDS, file);
	fwrite(zzs, sizeof(zz), NUM_ROUNDS, file);

	fclose(file);

	clock_t deltaWrite = clock()-beginWrite;
	free(zzs);
	int inMilliWrite = deltaWrite * 1000 / CLOCKS_PER_SEC;


	delta = clock() - begin;
	inMilli = delta * 1000 / CLOCKS_PER_SEC;

	int sumOfParts = 0;


	printf("Generating A: %ju ms\n", (uintmax_t)inMilliA);
	printf("	Generating keys: %ju ms\n", (uintmax_t)totalCrypto);
	sumOfParts += totalCrypto;
	printf("	Generating randomness: %ju ms\n", (uintmax_t)totalRandom);
	sumOfParts += totalRandom;
	printf("	Sharing secrets: %ju ms\n", (uintmax_t)totalSS);
	sumOfParts += totalSS;
	printf("	Running MPC-BOOL: %ju ms\n", (uintmax_t)totalSha);
	sumOfParts += totalSha;
	printf("	Committing: %ju ms\n", (uintmax_t)totalHash);
	sumOfParts += totalHash;
	printf("	*Accounted for*: %ju ms\n", (uintmax_t)sumOfParts);
	printf("Generating E: %ju ms\n", (uintmax_t)inMilliE);
	printf("Packing B: %ju ms\n", (uintmax_t)inMilliB);
	printf("Packing Z: %ju ms\n", (uintmax_t)inMilliZ);
	printf("Writing file: %ju ms\n", (uintmax_t)inMilliWrite);
	printf("Total time: %d ms\n",inMilli);
	printf("\n");
	printf("Proof output to file %s", outputFile);

	FILE *fp;
	fp = fopen("TimeProve.csv", "a+");
	fprintf(fp,"%ju,", user_input);
	fprintf(fp,"%ju,", inMilli);
	fprintf(fp,"%ju,", totalCrypto+totalRandom);
	fprintf(fp,"%ju,", totalSS);
	fprintf(fp,"%ju\n", totalSha+totalHash+inMilliE+inMilliZ+inMilliWrite);
  	fclose(fp);


	openmp_thread_cleanup();
	cleanup_EVP();
	return EXIT_SUCCESS;
}
