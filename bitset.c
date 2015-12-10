#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bitset.h"

#define DIVMOD(a,b,q,r) { q = (a)/(b); r = (a)%(b); }

static int block_allocs = 0;
static int block_mem = 0;


/* BLOCK FUNCTION DECLARATIONS */
static int bitset_block_new(struct bitset_block **blk_out);

static int bitset_block_alloc(struct bitset *bset, int block, struct bitset_block **blk_out);
static int bitset_block_realloc(struct bitset *bset, int block, struct bitset_block **blk_out);

static void bitset_block_incref(struct bitset_block *blk);
static void bitset_block_decref(struct bitset_block *blk);

static void bitset_block_set_bit(struct bitset_block *blk, int bit);
static void bitset_block_clr_bit(struct bitset_block *blk, int bit);
static void bitset_block_toggle_bit(struct bitset_block *blk, int bit);

static int bitset_block_test_bit(struct bitset_block *blk, int bit);

static void bitset_block_or(struct bitset_block *a, struct bitset_block *b);
static void bitset_block_and(struct bitset_block *a, struct bitset_block *b);
static void bitset_block_subtract(struct bitset_block *a, struct bitset_block *b);
static void bitset_block_invert(struct bitset_block *b);



/******************************************************************************
 * GLOBAL OPERATIONS
 */

void bitset_get_alloc_stats(int *allocs, int *bytes)
{
	*allocs = block_allocs;
	*bytes = block_mem;
}



/******************************************************************************
 * OBJECT ALLOCATION AND DESTRUCTION
 */

/* allocate a bitset with count bits in it, numbered 0 - (count-1)
 * all bits are initially set to 0 */
int bitset_alloc(int bitcount, struct bitset **bset_out) 
{
	struct bitset *bset = NULL;
	int ret;

	if (bset_out == NULL || *bset_out != NULL) 
	{
		ret = ERRINPUT;
		goto exit;
	}

	bset = (struct bitset *)malloc(sizeof(struct bitset));
	if (bset == NULL) 
	{
		ret = ERRMEM;
		goto exit;
	}

	block_allocs++;
	block_mem += sizeof(struct bitset);

	if ((ret = bitset_init(bset, bitcount)) != OK)
	{
		goto exit;
	}

	*bset_out = bset;
	bset = NULL;

exit:
	if (bset != NULL) 
	{
		bitset_free(bset);
	}

	return ret;
}


/* initialize a bitset structure */
int bitset_init(struct bitset *bset, int bitcount) 
{
	size_t sz;

	memset(bset, 0, sizeof(struct bitset));

	bset->bitcount = bitcount;
	bset->block_count = BLOCKCOUNT(bitcount);

	sz = sizeof(struct bitset_block *) * bset->block_count;
	bset->blocks = (struct bitset_block **) malloc(sz);
	if (bset->blocks == NULL)
		return ERRMEM;
	
	block_allocs++;
	block_mem += sz;

	memset(bset->blocks, 0, sz);

	return OK;
}


/* free a bitset structure */
void bitset_free(struct bitset *bset)
{
	int i;

	if (bset) 
	{
		if (bset->blocks)
		{
			for (i = 0; i < bset->block_count; i++)
			{
				if (bset->blocks[i] != NULL)
					bitset_block_decref(bset->blocks[i]);
			}
			free(bset->blocks);
		}

		free(bset);
	}
}


/* duplicate a bitset and return it */
int bitset_dup(struct bitset *s, struct bitset **r)
{
	struct bitset *bset = NULL;
	int i, ret;
	size_t sz;

	if (!s)
	{
		ret = ERRINPUT;
		goto exit;
	}
		
	if (r == NULL || *r != NULL) 
	{
		ret = ERRINPUT;
		goto exit;
	}

	bset = (struct bitset *)malloc(sizeof(struct bitset));
	if (bset == NULL) 
	{
		ret = ERRMEM;
		goto exit;
	}
	memset(bset, 0, sizeof(struct bitset));

	block_allocs++;
	block_mem += sizeof(struct bitset);

	bset->bitcount = s->bitcount;
	bset->block_count = s->block_count;

	sz = sizeof(struct bitset_block *) * bset->block_count;
	bset->blocks = (struct bitset_block **) malloc(sz);
	if (bset->blocks == NULL)
	{
		ret = ERRMEM;
		goto exit;
	}
	
	block_allocs++;
	block_mem += sz;

	memcpy(bset->blocks, s->blocks, sz);
	for (i = 0; i < bset->block_count; i++) 
	{
		if (bset->blocks[i] != NULL)
		{
			bitset_block_incref(bset->blocks[i]);
		}
	}

	*r = bset;
	bset = NULL;

	ret = OK;

exit:
	if (bset != NULL) 
	{
		bitset_free(bset);
	}

	return ret;
}



/******************************************************************************
 * OBJECT INFORMATION
 */

/* get the count of total bits in the bitset */
int bitset_bitcount(struct bitset *a)
{
	if (!a)
		return ERRINPUT;
	return a->bitcount;
}

/* get the count of bits in the bitset which are set to 1 */
int bitset_set_count(struct bitset *a)
{
	int n, i;

	if (!a)
		return ERRINPUT;

	for (n = 0, i = 0; i < a->block_count; i++)
	{
		if (a->blocks[i] != NULL)
			n += a->blocks[i]->set_count;
	}

	return n;
}

/* find the first allocated block in the bitset at index start or greater
 * if bset->blocks[start] is an allocated block, then start will be returned
 * if there is not a block at blocks[start], then the lowest index containing
 * an allocated block which is greater than start will be returned.
 * if there is no allocated block, then the value returned will be
 * equal to bset->block_count */
static int bitset_find_allocated_block(struct bitset *bset, int start)
{
	assert(start >= 0 && start < bset->block_count);

	/* locate the first allocated block in the structure */
	for (; start < bset->block_count; start++) 
	{
		if (bset->blocks[start] != NULL)
			break;
	}

	return start;
}


/******************************************************************************
 * BIT OPERATIONS
 */

int bitset_set(struct bitset *bset, int bit)
{
	int block, block_bit, ret;
	struct bitset_block *blk;

	if (bit < 0 || bit >= bset->bitcount)
		return ERRINPUT;

	DIVMOD(bit, IDSPERBLOCK, block, block_bit);
	assert(block < bset->block_count);

	if ((blk = bset->blocks[block]) == NULL) 
	{
		/* need to allocate the block */
		if ((ret = bitset_block_alloc(bset, block, &blk)) != OK)
			return ret;
	}
	else
	{
		/* if the bit is already set, this is a no-op */
		if (bitset_block_test_bit(blk, block_bit))
			return OK;

		/* if the block is a shared block, need to allocate a new one */
		if (bset->blocks[block]->ref_count > 1) 
		{
			if ((ret = bitset_block_realloc(bset, block, &blk)) != OK)
				return ret;
		}
	}

	bitset_block_set_bit(blk, block_bit);

	return OK;
}


/* set a bit to 0 in the bitset */
int bitset_clr(struct bitset *bset, int bit)
{
	int block, block_bit, ret;
	struct bitset_block *blk;

	if (bit < 0 || bit >= bset->bitcount)
		return ERRINPUT;

	DIVMOD(bit, IDSPERBLOCK, block, block_bit);
	assert(block < bset->block_count);

	if ((blk = bset->blocks[block]) == NULL) 
	{
		/* no need to clear if there is no block there */
		return OK;
	}

	/* if the bit is already clear, this is a no-op */
	if (bitset_block_test_bit(blk, block_bit) == 0)
		return OK;

	/* if the block is a shared block, need to allocate a new one */
	if (bset->blocks[block]->ref_count > 1) 
	{
		if ((ret = bitset_block_realloc(bset, block, &blk)) != OK)
			return ret;
	}

	bitset_block_clr_bit(blk, block_bit);

	return OK;
}


/* invert a single bit in the bitset */
int bitset_toggle_bit(struct bitset *bset, int bit)
{
	int block, block_bit, ret;
	struct bitset_block *blk = NULL;

	if (bit < 0 || bit >= bset->bitcount)
		return ERRINPUT;

	DIVMOD(bit, IDSPERBLOCK, block, block_bit);
	assert(block < bset->block_count);

	if ((blk = bset->blocks[block]) == NULL) 
	{
		/* need to allocate the block */
		if ((ret = bitset_block_alloc(bset, block, &blk)) != OK)
			return ret;
	}
	else
	{
		/* if the block is a shared block, need to allocate a new one */
		if (blk->ref_count > 1) 
		{
			if ((ret = bitset_block_realloc(bset, block, &blk)) != OK)
				return ret;
		}
	}

	assert(blk != NULL);

	bitset_block_toggle_bit(blk, block_bit);

	return OK;
}


/* test if a bit in the bitset is on */
int bitset_test_bit(struct bitset *a, int bit, int *out)
{
	int block, block_bit;
	struct bitset_block *blk;

	if (bit < 0 || bit >= a->bitcount)
		return ERRINPUT;
	if (out == NULL)
		return ERRINPUT;

	DIVMOD(bit, IDSPERBLOCK, block, block_bit);
	assert(block < a->block_count);

	if ((blk = a->blocks[block]) == NULL) 
	{
		*out = 0;
	}
	else
	{
		*out = bitset_block_test_bit(blk, block_bit);
	}

	return OK;
}



/******************************************************************************
 * SET OPERATIONS
 */

/* Invert all of the bits in the bitset */
int bitset_invert(struct bitset *a)
{
	struct bitset_block *blk = NULL;
	int i, ret;

	if (a == NULL)
		return ERRINPUT;

	for (i = 0; i < a->block_count; i++)
	{
		if (a->blocks[i] == NULL)
		{
			/* block is a NULL pointer, so the invert is a block of all 1 bits */
			ret = bitset_block_new(&blk);
			if (ret != OK)
				return ret;

			/* initialize the block */
			blk->ref_count = 1;
			blk->set_count = IDSPERBLOCK;
			memset(blk->ints, -1, sizeof(int64_t) * BLOCKSIZE);

			/* transfer ownership of the block to bitset a */
			a->blocks[i] = blk;
			blk = NULL;
		} 
		else
		{
			if (a->blocks[i]->set_count == 64*BLOCKSIZE)
			{
				/* the block is all 1's so the inverse will be all empty
				 * this can be represented by a NULL block pointer, so
				 * decref the block and set the pointer NULL */
				bitset_block_decref(a->blocks[i]);
				a->blocks[i] = NULL;
			}
			else
			{
				/* do real inversion of the bits in the block */
				bitset_block_invert(a->blocks[i]);
			}
		}
	}

	return OK;
}


/* Compute the inverse of the bitset and return it as a new bitset */
int bitset_inverse(struct bitset *a, struct bitset **result_out)
{
	struct bitset *r = NULL;
	int ret;

	if (result_out == NULL || *result_out != NULL)
		return ERRINPUT;

	ret = bitset_dup(a, &r);
	if (ret != OK)
		goto exit;
	
	ret = bitset_invert(r);
	if (ret != OK)
		goto exit;

	*result_out = r;
	r = NULL;

	ret = OK;

exit:
	if (r != NULL) 
		bitset_free(r);

	return ret;
}


/* combine bitset A and B into bitset A by making A be the result of A | B (union) */
int bitset_or(struct bitset *a, struct bitset *b)
{
	int i, ret;

	if (a == NULL || b == NULL) 
		return ERRINPUT;

	/* the bitsets must be the same size for this operation to make sense */
	if (a->bitcount != b->bitcount)
		return ERRINPUT;

	/* if the bit counts are the same, then the block counts must be
	 * since the latter is computed from the former */
	assert(a->block_count == b->block_count);

	/* OR all the blocks together */
	for (i = 0; i < a->block_count; i++)
	{
		if (a->blocks[i] == NULL && b->blocks[i] != NULL)
		{
			/* OR-ing a NON null block into a NULL block is simply copying the other block over */
			a->blocks[i] = b->blocks[i];
			bitset_block_incref(a->blocks[i]);
		}
		else if (a->blocks[i] != NULL && b->blocks[i] != NULL)
		{
			/* if both blocks are non NULL, we need to OR the contents together */

			/* first, since we are going to modify the block at a->blocks[i],
			 * we need to re-allocate it if it is a shared block */
			if (a->blocks[i]->ref_count > 1) 
			{
				if ((ret = bitset_block_realloc(a, i, NULL)) != OK)
					return ret;
			}

			bitset_block_or(a->blocks[i], b->blocks[i]);
		}
	}

	return OK;
}


/* combine bitset A and B into bitset A by making A be the result of A & B (intersection) */
int bitset_and(struct bitset *a, struct bitset *b)
{
	int i, ret;

	if (a == NULL || b == NULL) 
		return ERRINPUT;

	/* the bitsets must be the same size for this operation to make sense */
	if (a->bitcount != b->bitcount)
		return ERRINPUT;

	/* if the bit counts are the same, then the block counts must be
	 * since the latter is computed from the former */
	assert(a->block_count == b->block_count);

	/* AND all the blocks together */
	for (i = 0; i < a->block_count; i++)
	{
		if (a->blocks[i] != NULL && b->blocks[i] == NULL)
		{
			/* AND-ing a NULL block into a not-NULL block sets all the bits to
			 * 0 so we, drop the block of bitset A. */
			bitset_block_decref(a->blocks[i]);
			a->blocks[i] = NULL;
		}
		else if (a->blocks[i] != NULL && b->blocks[i] != NULL)
		{
			/* if both blocks are non NULL, we need to AND the contents together */

			/* first, since we are going to modify the block at a->blocks[i],
			 * we need to re-allocate it if it is a shared block */
			if (a->blocks[i]->ref_count > 1) 
			{
				if ((ret = bitset_block_realloc(a, i, NULL)) != OK)
					return ret;
			}

			bitset_block_and(a->blocks[i], b->blocks[i]);
		}
	}

	return OK;
}


/* set bitset A to be A - B */
int bitset_subtract(struct bitset *a, struct bitset *b)
{
	int i, ret;

	if (a == NULL || b == NULL) 
		return ERRINPUT;

	/* the bitsets must be the same size for this operation to make sense */
	if (a->bitcount != b->bitcount)
		return ERRINPUT;

	/* if the bit counts are the same, then the block counts must be
	 * since the latter is computed from the former */
	assert(a->block_count == b->block_count);

	/* subtract all the blocks */
	for (i = 0; i < a->block_count; i++)
	{
		if (a->blocks[i] != NULL && b->blocks[i] != NULL)
		{
			/* if both blocks are non NULL, we subtract the bits in b from a */

			/* first, since we are going to modify the block at a->blocks[i],
			 * we need to re-allocate it if it is a shared block */
			if (a->blocks[i]->ref_count > 1) 
			{
				if ((ret = bitset_block_realloc(a, i, NULL)) != OK)
					return ret;
			}

			bitset_block_subtract(a->blocks[i], b->blocks[i]);
		}
	}

	return OK;
}


/* Compute the union of A and B and return it as a new bitset */
int bitset_union(struct bitset *a, struct bitset *b, struct bitset **result_out)
{
	struct bitset *r = NULL;
	int ret;

	if (result_out == NULL || *result_out != NULL)
		return ERRINPUT;

	ret = bitset_dup(a, &r);
	if (ret != OK)
		goto exit;
	
	ret = bitset_or(r, b);
	if (ret != OK)
		goto exit;

	*result_out = r;
	r = NULL;

	ret = OK;

exit:
	if (r != NULL) 
		bitset_free(r);

	return ret;
}


/* Compute the intersection of A and B (A & B) and return it as a new bitset */
int bitset_intersect(struct bitset *a, struct bitset *b, struct bitset **result_out)
{
	struct bitset *r = NULL;
	int ret;

	if (result_out == NULL || *result_out != NULL)
		return ERRINPUT;

	ret = bitset_dup(a, &r);
	if (ret != OK)
		goto exit;
	
	ret = bitset_and(r, b);
	if (ret != OK)
		goto exit;

	*result_out = r;
	r = NULL;

	ret = OK;

exit:
	if (r != NULL) 
		bitset_free(r);

	return ret;
}


/* Compute A - B and return it as a new bitset */
int bitset_difference(struct bitset *a, struct bitset *b, struct bitset **result_out)
{
	struct bitset *r = NULL;
	int ret;

	if (result_out == NULL || *result_out != NULL)
		return ERRINPUT;

	ret = bitset_dup(a, &r);
	if (ret != OK)
		goto exit;
	
	ret = bitset_subtract(r, b);
	if (ret != OK)
		goto exit;

	*result_out = r;
	r = NULL;

	ret = OK;

exit:
	if (r != NULL) 
		bitset_free(r);

	return ret;
}



/******************************************************************************
 * BLOCK OPERATIONS
 */

/* create and return an uninitialized bitset_block object */
static int bitset_block_new(struct bitset_block **blk_out)
{
	struct bitset_block *blk;

	blk = (struct bitset_block *) malloc(sizeof(struct bitset_block));
	if (blk == NULL)
		return ERRMEM;

	/* update the memory stats */
	block_allocs++;
	block_mem += sizeof(struct bitset_block);

	*blk_out = blk;

	return OK;
}

/* allocate a block to be stored at bset->blocks[block].  return it in
 * blk_out if it is not NULL */
static int bitset_block_alloc(struct bitset *bset, int block, struct bitset_block **blk_out)
{
	struct bitset_block *blk = NULL;
	int ret;

	ret = bitset_block_new(&blk);
	if (ret != OK)
		return ret;

	/* initialize the block */
	memset(blk, 0, sizeof(struct bitset_block));
	blk->ref_count = 1;

	assert(bset->blocks[block] == NULL);
	bset->blocks[block] = blk;

	if (blk_out != NULL)
		*blk_out = blk;

	return OK;
}


/* re-allocate a shared block at bset->blocks[block].  return it in
 * blk_out if it is not NULL */
static int bitset_block_realloc(struct bitset *bset, int block, struct bitset_block **blk_out)
{
	struct bitset_block *orig, *blk;
	int ret;

	assert(bset->blocks[block] != NULL);
	assert(bset->blocks[block]->ref_count > 1);

	/* save the original block (needed later to copy stuff) */
	orig = bset->blocks[block];

	/* decrement the reference count on it */
	bitset_block_decref(orig);

	/* for now, clear the block pointer there */
	bset->blocks[block] = NULL;

	if ((ret = bitset_block_new(&blk)) != OK)
		return ret;

	blk->ref_count = 1;
	bset->blocks[block] = blk;

	/* copy the set_count and the bits to the new block */
	blk->set_count = orig->set_count;
	memcpy(blk->ints, orig->ints, sizeof(int64_t) * BLOCKSIZE);

	if (blk_out != NULL)
		*blk_out = blk;

	return OK;
}


static void bitset_block_incref(struct bitset_block *blk)
{
	blk->ref_count++;
}


static void bitset_block_decref(struct bitset_block *blk)
{
	if (--blk->ref_count == 0)
	{
		free(blk);
	}
}


static void bitset_block_set_bit(struct bitset_block *blk, int bit)
{
	int n, b;

	DIVMOD(bit, 64, n, b);

	blk->ints[n] |= (1ull << b);
	blk->set_count++;
}


static void bitset_block_clr_bit(struct bitset_block *blk, int bit)
{
	int n, b;

	DIVMOD(bit, 64, n, b);

	blk->ints[n] &= ~(1ull << b);
	blk->set_count--;
}


static void bitset_block_toggle_bit(struct bitset_block *blk, int bit)
{
	int n, b;
	int64_t mask;

	DIVMOD(bit, 64, n, b);

	mask = 1ull << b;
	blk->ints[n] ^= mask;

	if ((blk->ints[n] & mask) == 0ull)
	{
		/* bit was cleared */
		blk->set_count--;
	} 
	else
	{
		/* bit was set */
		blk->set_count++;
	}
}


static int bitset_block_test_bit(struct bitset_block *blk, int bit)
{
	int n, b;

	DIVMOD(bit, 64, n, b);

	return (blk->ints[n] & (1ull << b)) != 0;
}

static void bitset_block_or(struct bitset_block *a, struct bitset_block *b)
{
	int i, c;
	
	assert(a->ref_count == 1);

	for (i = 0, c = 0; i < BLOCKSIZE; i++) 
	{
		a->ints[i] |= b->ints[i];
		c += __builtin_popcountll(a->ints[i]);
	}

	/* update popcount on the block */
	a->set_count = c;
}

static void bitset_block_and(struct bitset_block *a, struct bitset_block *b)
{
	int i, c;
	
	assert(a->ref_count == 1);

	for (i = 0, c = 0; i < BLOCKSIZE; i++) 
	{
		a->ints[i] &= b->ints[i];
		c += __builtin_popcountll(a->ints[i]);
	}

	/* update popcount on the block */
	a->set_count = c;
}

static void bitset_block_subtract(struct bitset_block *a, struct bitset_block *b)
{
	int i, c;
	
	assert(a->ref_count == 1);

	for (i = 0, c = 0; i < BLOCKSIZE; i++) 
	{
		a->ints[i] &= ~b->ints[i];
		c += __builtin_popcountll(a->ints[i]);
	}

	/* update popcount on the block */
	a->set_count = c;
}

static void bitset_block_invert(struct bitset_block *b)
{
	int i;
	
	assert(b->ref_count == 1);

	for (i = 0; i < BLOCKSIZE; i++) 
	{
		b->ints[i] = ~b->ints[i];
	}

	/* update popcount on the block */
	b->set_count = IDSPERBLOCK - b->set_count;
}

/* locate a 1 bit at the given pos or greater.
 *
 * pos must satisfy 0 <= pos < IDSPERBLOCK
 *
 * the bit number of the first 1 bit at pos or greater is returned.
 * if no such bit is found, -1 is returned
 */
static int bitset_block_find_next_on_bit(struct bitset_block *block, int pos)
{
	int n, b;
	uint64_t v, m;

	assert(block != NULL);
	assert(block->ref_count == 1);
	assert(pos >= 0);
	assert(pos < IDSPERBLOCK);

	DIVMOD(pos, BITSPERINT, n, b);

	assert(n < BLOCKSIZE);
	assert(b < BITSPERINT);

	/* iterate through the ints to find a non-zero int */
	for (; n < BLOCKSIZE; n++, b=0) 
	{
		if (block->ints[n] != 0)
		{
			v = block->ints[n];
			for (; b < BITSPERINT; b++)
			{
				m = 1ull << b;
				if ((v & m) == m)
					break;
			}

			/* exit the loop if we found an on bit */
			if (b < BITSPERINT)
				break;
		}
	}

	if (n < BLOCKSIZE)
		return n*BITSPERINT + b;
	else
		return -1;
}



/******************************************************************************
 * ITERATION
 */

static void bitset_iter_first(struct bitset_iterator *iter);
static void bitset_iter_next_on_bit(struct bitset_iterator *iter);

void bitset_iter_init(struct bitset_iterator *iter, struct bitset *bset, int flags)
{		
	assert(iter != NULL);
	assert(bset != NULL);

	iter->bset = bset;
	iter->flags = flags;

	bitset_iter_first(iter);
}

static void bitset_iter_first(struct bitset_iterator *iter)
{
	switch (iter->flags)
	{
	case BITSET_ITER_ALL:
		iter->block_pos = 0;
		iter->bit_pos = 0;
		break;

	case BITSET_ITER_ON:
		/* set initial position to before 1st bit of 0th block and
		 * search for an on bit after that */
		iter->block_pos = 0;
		iter->bit_pos = -1;
		bitset_iter_next_on_bit(iter);
		break;

	default:
		assert(!"Invalid flags");
	}
}

/* bitset_iter_next_on_bit
 * 
 * advance the iterator to the next 1 bit after the current position.
 * the iter position may indicate (0,-1) to be before the first bit.
 */
static void bitset_iter_next_on_bit(struct bitset_iterator *iter)
{
	struct bitset *bset;

	assert(iter != NULL);
	assert(iter->bset != NULL);
	assert(iter->block_pos >= 0);
	assert(iter->bit_pos >= -1 && iter->bit_pos < IDSPERBLOCK);
	
	bset = iter->bset;

	/* advance to the next bit */
	iter->bit_pos++;

	/* see if that advanced the iterator past the last bit in the block */
	if (iter->bit_pos == IDSPERBLOCK)
	{
		iter->block_pos++;
		iter->bit_pos = 0;
	}
	
	/* look for an on bit until we've gone through all the blocks */
	while (iter->block_pos < bset->block_count)
	{
		/* check if looking at an allocated block */
		if (bset->blocks[iter->block_pos] == NULL)
		{
			/* move on to the next block */
			iter->block_pos++;
			iter->bit_pos = 0;
		}
		else
		{
			/* find the next on bit in the current block */
			iter->bit_pos = bitset_block_find_next_on_bit(bset->blocks[iter->block_pos], iter->bit_pos);
			if (iter->bit_pos == -1)
			{
				/* the current block didn't have another on bit after the current
				 * bit_pos, so move to the next block */
				iter->block_pos++;
				iter->bit_pos = 0;
			}
			else
			{
				/* found an on bit */
				assert(iter->block_pos >= 0 && iter->block_pos < bset->block_count);
				assert(iter->bit_pos >= 0 && iter->bit_pos < IDSPERBLOCK);
				break;
			}
		}
	}
}

/* bitset_iter_next
 * 
 * Public iterator interface to advance the iterator to the
 * next position. The meaning of the next position for the 
 * iterator is dependent upon the iterator flags.
 */
void bitset_iter_next(struct bitset_iterator *iter)
{
	assert(iter != NULL);
	assert(iter->bset != NULL);

	switch (iter->flags)
	{
	case BITSET_ITER_ALL:
		iter->bit_pos++;
		if (iter->bit_pos == IDSPERBLOCK)
		{
			iter->block_pos++;
			iter->bit_pos = 0;
		}
		break;

	case BITSET_ITER_ON:
		bitset_iter_next_on_bit(iter);
		break;

	default:
		assert(!"Invalid flags");
	}
}

int bitset_iter_at_end(struct bitset_iterator *iter)
{
	switch (iter->flags)
	{
	case BITSET_ITER_ALL:
		return iter->block_pos >= iter->bset->block_count;

	case BITSET_ITER_ON:
		return iter->block_pos >= iter->bset->block_count;

	default:
		assert(!"Invalid flags");
	}

	return 0;
}

int bitset_iter_get(struct bitset_iterator *iter)
{
	struct bitset_block *block;

	assert(iter->block_pos < iter->bset->block_count);
	assert(iter->bit_pos < IDSPERBLOCK);

	block = iter->bset->blocks[iter->block_pos];
	if (block == NULL)
		return 0;

	return bitset_block_test_bit(block, iter->bit_pos);
}

int bitset_iter_index(struct bitset_iterator *iter)
{
	return iter->block_pos * IDSPERBLOCK + iter->bit_pos;
}

