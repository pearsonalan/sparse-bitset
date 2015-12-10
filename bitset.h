#ifndef _bitset_h_
#define _bitset_h_

/* return codes */
#define OK           0
#define ERRMEM      -1
#define ERRINPUT    -2
#define ERRNOTIMPL  -3

#define BLOCKSIZE			1024
#define BITSPERINT			64
#define IDSPERBLOCK			(BLOCKSIZE*BITSPERINT)
#define BLOCKCOUNT(idcount)	(((idcount)+IDSPERBLOCK-1)/IDSPERBLOCK)

/* bitset_block contains a block of 64*BLOCKSIZE bits */
struct bitset_block {
	/* the reference count on the block. */
	int ref_count;

	/* the number of bits in the block that are set to 1 */
	int set_count;

	/* the bits themselves */
	uint64_t ints[BLOCKSIZE];
};

/* a bitset contains a set of bits which are 0 or 1. */
struct bitset {
	/* the total number of bits in the set (numbered 0 - bitcount-1) */
	int bitcount;

	/* the number of block pointers in *blocks.  not all of the pointers
	 * in the blocks array are non-NULL, so this is not the count of 
	 * allocated blocks */
	int block_count;

	/* an array of pointers to blocks of bits */
	struct bitset_block **blocks;
};

/* an object to iterate the bits in the bitset */
struct bitset_iterator {
	/* the bitset being iterated */
	struct bitset *bset;

	/* flags controlling iteration */
	int flags;

	/* the iteration location */
	int block_pos;  /* the index of the block */
	int bit_pos;    /* the index of the bit in the block */
};

/* iteration control flags */
#define BITSET_ITER_ALL   0    /* iterate all bits */
#define BITSET_ITER_ON    1    /* iterate only on (1) bits */
#define BITSET_ITER_OFF   2    /* iterate only off (0) bits */


/* GLOBAL OPERATIONS */

/* Read the memory allocation counters. */
void bitset_get_alloc_stats(int *allocs, int *bytes);


/* OBJECT ALLOCATION AND DESTRUCTION */

/* allocate a bitset with count bits in it, numbered 0 - (count-1)
 * all bits are initially set to 0 */
int bitset_alloc(int bitcount, struct bitset **bset_out) ;

/* initialize a bitset structure */
int bitset_init(struct bitset *bset, int bitcount) ;

/* free a bitset structure */
void bitset_free(struct bitset *bset);

/* Duplicate a bitset and return it */
int bitset_dup(struct bitset *s, struct bitset **r);


/* OBJECT INFORMATION */

/* Get the count of total bits in the bitset */
int bitset_bitcount(struct bitset *a);

/* Get the count of bits in the bitset which are set to 1 */
int bitset_popcount(struct bitset *a);

int bitset_set_count(struct bitset *a);

/* BIT OPERATIONS */

/* set a bit to 1 in the bitset */
int bitset_set(struct bitset *a, int bit);

/* set a bit to 0 in the bitset */
int bitset_clr(struct bitset *a, int bit);

/* invert a single bit in the bitset */
int bitset_toggle_bit(struct bitset *a, int bit);

/* Test if a bit in the bitset is on */
int bitset_test_bit(struct bitset *a, int bit, int *out);


/* SET OPERATIONS */

/* Invert all of the bits in the bitset */
int bitset_invert(struct bitset *a);

/* Compute the inverse of the bitset and return it as a new bitset */
int bitset_inverse(struct bitset *a, struct bitset **r);

/* Combine bitset A and B into bitset A by making A be the result of A | B (union) */
int bitset_or(struct bitset *a, struct bitset *b);

/* Combine bitset A and B into bitset A by making A be the result of A & B (intersection) */
int bitset_and(struct bitset *a, struct bitset *b);

/* Set bitset A to be A - B */
int bitset_subtract(struct bitset *a, struct bitset *b);

/* Compute the union of A and B and return it as a new bitset */
int bitset_union(struct bitset *a, struct bitset *b, struct bitset **r);

/* Compute the intersection of A and B (A & B) and return it as a new bitset */
int bitset_intersect(struct bitset *a, struct bitset *b, struct bitset **r);

/* Compute A - B and return it as a new bitset */
int bitset_difference(struct bitset *a, struct bitset *b, struct bitset **r);


/* ITERATION */

void bitset_iter_init(struct bitset_iterator *iter, struct bitset *bset, int flags);
void bitset_iter_next(struct bitset_iterator *iter);
int bitset_iter_at_end(struct bitset_iterator *iter);
int bitset_iter_get(struct bitset_iterator *iter);
int bitset_iter_index(struct bitset_iterator *iter);

#endif

