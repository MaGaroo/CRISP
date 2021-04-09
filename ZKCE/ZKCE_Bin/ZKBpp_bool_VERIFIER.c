/*
 ===============================================================================
 Name        : ZKBpp_bool_VERIFIER.c
 Author      : ANONYMOUS - based on Sobuno's ZKBoo v0.1
 Version     : 1.0
 Description : Verifies a proof for BITDEC generated by ZKBpp_bool.c using ZKBpp
 ===============================================================================
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "shared.h"

int NUM_ROUNDS = 219;

void printbits(uint32_t n) {
	if (n) {
		printbits(n >> 1);
		printf("%d", n & 1);
	}
}


int main(void) {
	setbuf(stdout, NULL);
	init_EVP();
	openmp_thread_setup();
	
	printf("Iterations of BITDEC: %d\n", NUM_ROUNDS);

	clock_t begin = clock(), delta, deltaFiles;
	
	uint32_t finalHash[8];
	int es[NUM_ROUNDS];
	b bs[NUM_ROUNDS];
	zz zs[NUM_ROUNDS];
	FILE *file;

	char outputFile[3*sizeof(int) + 8];
	sprintf(outputFile, "out%i.bin", NUM_ROUNDS);
	file = fopen(outputFile, "rb");
	if (!file) {
		printf("Unable to open file!");
	}

	fread(&finalHash, sizeof(uint32_t), 8, file);
	fread(&es, sizeof(int), NUM_ROUNDS, file);
	fread(&bs, sizeof(b), NUM_ROUNDS, file);
	fread(&zs, sizeof(zz), NUM_ROUNDS, file);
	fclose(file);

	printf("Y : ");
	for(int i=0;i<8;i++) {
		printf("%02X", finalHash[i]);
	}
	printf("\n");

	deltaFiles = clock() - begin;
	int inMilliFiles = deltaFiles * 1000 / CLOCKS_PER_SEC;
	printf("Loading files: %ju ms\n", (uintmax_t)inMilliFiles);


	// VERIFY THE PROOF
	clock_t beginV = clock(), deltaV;
	a a_res[NUM_ROUNDS];
	memset(&a_res, 0, sizeof(a)*NUM_ROUNDS);


	//#pragma omp parallel for_ //make para
	for(int i = 0; i<NUM_ROUNDS; i++) {
		a_res[i] = verifyZ(finalHash, es[i], bs[i], zs[i]);	
	}

	uint32_t finalHash_rec[8];
	for (int j = 0; j < 8; j++) {
		finalHash_rec[j] = a_res[0].yp[0][j]^a_res[0].yp[1][j]^a_res[0].yp[2][j];
	}

	// Compute the challenge e'= H(a0,a1,..,a_it)
	clock_t beginE = clock(), deltaE;
	int es_prime[NUM_ROUNDS];
	H3(finalHash_rec, a_res, NUM_ROUNDS, es_prime);
	deltaE = clock() - beginE;
	int inMilliE = deltaE * 1000 / CLOCKS_PER_SEC;
	printf("Generating E: %ju ms\n", (uintmax_t)inMilliE);

	for(int l=0; l<NUM_ROUNDS; l++){
		if(es[l] != es_prime[l]){
			printf("Error -- round %u\n",l);
			exit(1);
		}else{
			printf("challenge %u -- %u success \n",l, es_prime[l]);
		}
	}
	printf("Yrec : ");
	for(int i=0;i<8;i++) {
		printf("%02X", finalHash_rec[i]);
	}
	printf("\n\n___SUCCESS___\n\n");


	deltaV = clock() - beginV;
	int inMilliV = deltaV * 1000 / CLOCKS_PER_SEC;
	printf("Verifying: %ju ms\n", (uintmax_t)inMilliV);
	
	
	delta = clock() - begin;
	int inMilli = delta * 1000 / CLOCKS_PER_SEC;

	printf("Total time: %ju ms\n", (uintmax_t)inMilli);
	

	FILE *fp;
	fp = fopen("TimeVer.csv", "a+");
	fprintf(fp,"%ju\n", inMilli);
  	fclose(fp);

	openmp_thread_cleanup();
	cleanup_EVP();
	return EXIT_SUCCESS;
}

