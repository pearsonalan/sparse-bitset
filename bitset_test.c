#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bitset.h"

#define RUN_TEST(t) { puts("Running " #t); t(); }
#define VERIFY(x) (assert((x) == OK))

int test_alloc()
{
	struct bitset *bset = NULL;
	int ret;

	assert(IDSPERBLOCK == 64 * BLOCKSIZE);

	ret = bitset_alloc(IDSPERBLOCK * 1000, &bset);
	assert(ret == OK);
	assert(bset != NULL);
	assert(bset->bitcount == IDSPERBLOCK * 1000);
	assert(bset->block_count == 1000);

	ret = bitset_bitcount(bset);
	assert(ret == IDSPERBLOCK*1000);

	ret = bitset_set_count(bset);
	assert(ret == 0);

	bitset_free(bset);
}

int test_set_bit()
{
	struct bitset *bset = NULL;
	int ret;
	int b;

	ret = bitset_alloc(IDSPERBLOCK * 10, &bset);
	assert(ret == OK);
	assert(bset != NULL);

	ret = bitset_set(bset, IDSPERBLOCK + 10);
	assert(ret == OK);

	assert(bset->blocks[0] == NULL);
	assert(bset->blocks[1] != NULL);

	assert(bset->blocks[1]->ref_count == 1);
	assert(bset->blocks[1]->set_count == 1);

	ret = bitset_test_bit(bset, IDSPERBLOCK + 10, &b);
	assert(ret == OK);
	assert(b == 1);

	ret = bitset_test_bit(bset, IDSPERBLOCK + 11, &b);
	assert(ret == OK);
	assert(b == 0);

	ret = bitset_set_count(bset);
	assert(ret == 1);

	bitset_free(bset);
}


int test_set_bad_input()
{
	struct bitset *bset = NULL;
	int ret;
	int b;

	ret = bitset_alloc(IDSPERBLOCK * 10, &bset);
	assert(ret == OK);
	assert(bset != NULL);

	ret = bitset_set(bset, IDSPERBLOCK*10);
	assert(ret == ERRINPUT);

	ret = bitset_set(bset, -1);
	assert(ret == ERRINPUT);

	bitset_free(bset);
}


int test_set_all_bits()
{
	struct bitset *bset = NULL;
	int ret, i, b;

	ret = bitset_alloc(IDSPERBLOCK * 2, &bset);
	assert(ret == OK);
	assert(bset != NULL);

	for (i = 0; i < IDSPERBLOCK * 2; i++)
	{
		ret = bitset_test_bit(bset, i, &b);
		assert(ret == OK);
		assert(b == 0);

		ret = bitset_set(bset, i);
		assert(ret == OK);

		ret = bitset_test_bit(bset, i, &b);
		assert(ret == OK);
		assert(b == 1);
	}

	ret = bitset_set_count(bset);
	assert(ret == IDSPERBLOCK*2);

	bitset_free(bset);
}


int test_clear_bit()
{
	struct bitset *bset = NULL;
	int ret;
	int b;

	ret = bitset_alloc(IDSPERBLOCK * 10, &bset);
	assert(ret == OK);
	assert(bset != NULL);

	ret = bitset_set(bset, IDSPERBLOCK + 10);
	assert(ret == OK);

	assert(bset->blocks[0] == NULL);
	assert(bset->blocks[1] != NULL);

	assert(bset->blocks[1]->ref_count == 1);
	assert(bset->blocks[1]->set_count == 1);

	ret = bitset_test_bit(bset, IDSPERBLOCK + 10, &b);
	assert(ret == OK);
	assert(b == 1);

	ret = bitset_clr(bset, IDSPERBLOCK + 10);
	assert(ret == OK);

	ret = bitset_test_bit(bset, IDSPERBLOCK + 10, &b);
	assert(ret == OK);
	assert(b == 0);

	ret = bitset_set_count(bset);
	assert(ret == 0);

	bitset_free(bset);
}


int test_toggle_bit()
{
	struct bitset *bset = NULL, *d = NULL;
	int ret;
	int b;

	/* test 3 allocation cases:
	 *   toggle a bit in an allocated block
	 *   toggle a bit in an unallocated block
	 *   toggle a bit in a shared block 
	 */

	ret = bitset_alloc(IDSPERBLOCK * 3, &bset);
	assert(ret == OK);
	assert(bset != NULL);

	/* block 2 is to be a shared block... set a bit in it, and dup */
	ret = bitset_set(bset, IDSPERBLOCK*2 + 5);
	assert(ret == OK);

	ret = bitset_dup(bset, &d);
	assert(ret == 0);

	/* block 0 is to be only present in the original bitset */
	ret = bitset_set(bset, 10);
	assert(ret == OK);

	/* Verify the setup */

	assert(bset->blocks[0] != NULL);
	assert(bset->blocks[1] == NULL);
	assert(bset->blocks[2] != NULL);

	assert(bset->blocks[0]->ref_count == 1);
	assert(bset->blocks[0]->set_count == 1);

	assert(bset->blocks[2]->ref_count == 2);
	assert(bset->blocks[2]->set_count == 1);

	ret = bitset_test_bit(bset, 10, &b);
	assert(ret == OK);
	assert(b == 1);

	ret = bitset_test_bit(bset, IDSPERBLOCK*2 + 5, &b);
	assert(ret == OK);
	assert(b == 1);

	/* toggle some bits */

	/* case 1: toggle some bits in the allocated block */
	assert(bset->blocks[0]->set_count == 1);
	ret = bitset_toggle_bit(bset, 10);
	assert(ret == OK);
	assert(bset->blocks[0]->set_count == 0);
	ret = bitset_toggle_bit(bset, 11);
	assert(ret == OK);
	assert(bset->blocks[0]->set_count == 1);
	ret = bitset_toggle_bit(bset, 12);
	assert(ret == OK);
	assert(bset->blocks[0]->set_count == 2);

	/* case 2: toggle a bits in the unallocated block */
	ret = bitset_toggle_bit(bset, IDSPERBLOCK);
	assert(ret == OK);

	/* case 3: toggle a bit in the shared block */
	ret = bitset_toggle_bit(bset, IDSPERBLOCK*2 + 100);
	assert(ret == OK);


	/* check the results */


	ret = bitset_test_bit(bset, 10, &b);
	assert(ret == OK);
	assert(b == 0);

	ret = bitset_test_bit(bset, 11, &b);
	assert(ret == OK);
	assert(b == 1);

	ret = bitset_test_bit(bset, 12, &b);
	assert(ret == OK);
	assert(b == 1);

	ret = bitset_test_bit(bset, IDSPERBLOCK, &b);
	assert(ret == OK);
	assert(b == 1);

	ret = bitset_test_bit(bset, IDSPERBLOCK*2 + 5, &b);
	assert(ret == OK);
	assert(b == 1);

	ret = bitset_test_bit(bset, IDSPERBLOCK*2 + 100, &b);
	assert(ret == OK);
	assert(b == 1);


	assert(bset->blocks[0] != NULL);
	assert(bset->blocks[1] != NULL);
	assert(bset->blocks[2] != NULL);

	assert(bset->blocks[0]->ref_count == 1);
	assert(bset->blocks[0]->set_count == 2);

	assert(bset->blocks[1]->ref_count == 1);
	assert(bset->blocks[1]->set_count == 1);

	assert(bset->blocks[2]->ref_count == 1);
	assert(bset->blocks[2]->set_count == 2);


	ret = bitset_set_count(bset);
	assert(ret == 5);

	
	assert(d->blocks[0] == NULL);
	assert(d->blocks[1] == NULL);
	assert(d->blocks[2] != NULL);

	assert(d->blocks[2]->ref_count == 1);
	assert(d->blocks[2]->set_count == 1);

	ret = bitset_set_count(d);
	assert(ret == 1);

	
	bitset_free(bset);
	bitset_free(d);
}

void test_dup()
{
	struct bitset *s = NULL, *d = NULL;
	int ret;
	int b;

	/* allocate a bitset */
	ret = bitset_alloc(IDSPERBLOCK * 10, &s);
	assert(ret == OK);
	assert(s != NULL);

	/* set a couple of bits in different blocks */
	ret = bitset_set(s, IDSPERBLOCK*1 + 10);
	assert(ret == OK);
	ret = bitset_set(s, IDSPERBLOCK*4 + 10);
	assert(ret == OK);

	assert(s->blocks[1] != NULL);
	assert(s->blocks[4] != NULL);

	assert(s->blocks[1]->ref_count == 1);
	assert(s->blocks[4]->ref_count == 1);

	assert(s->blocks[1]->set_count == 1);
	assert(s->blocks[4]->set_count == 1);

	/* dup the bitset */
	ret = bitset_dup(s, &d);
	assert(ret == 0);

	/* check that the dup was correct */
	assert(s != d);

	ret = bitset_bitcount(d);
	assert(ret == IDSPERBLOCK*10);

	ret = bitset_set_count(d);
	assert(ret == 2);

	assert(d->blocks[1]->ref_count == 2);
	assert(d->blocks[4]->ref_count == 2);

	assert(s->blocks[1] == d->blocks[1]);
	assert(s->blocks[4] == d->blocks[4]);

	/* now set a bit in the dup in a block not in the original */
	ret = bitset_set(d, IDSPERBLOCK*2 + 10);
	assert(ret == OK);
	
	/* validate that the dup is changed but the original isn't */
	assert(s->blocks[2] == NULL);
	assert(d->blocks[2] != NULL);
	assert(d->blocks[2]->ref_count == 1);
	assert(d->blocks[2]->set_count == 1);

	ret = bitset_set_count(d);
	assert(ret == 3);

	/* now set a bit in the dup in a block that was in the original */
	ret = bitset_set(d, IDSPERBLOCK*4 + 40);
	assert(ret == OK);
	
	/* validate that the dup is changed but the original isn't */
	ret = bitset_set_count(s);
	assert(ret == 2);

	ret = bitset_set_count(d);
	assert(ret == 4);

	assert(s->blocks[4] != NULL);
	assert(d->blocks[4] != NULL);

	assert(s->blocks[4]->set_count == 1);
	assert(d->blocks[4]->set_count == 2);

	/* validate that the refcounts are not shared for that block anymore */
	assert(s->blocks[4]->ref_count == 1);
	assert(d->blocks[4]->ref_count == 1);

	/* free both */
	bitset_free(s);
	bitset_free(d);
}

void test_or()
{
	struct bitset *a = NULL, *b = NULL;
	int i, ret, bit;

	/* allocate two bitsets of the same size */
	ret = bitset_alloc(IDSPERBLOCK * 4, &a);
	assert(ret == OK);
	ret = bitset_alloc(IDSPERBLOCK * 4, &b);
	assert(ret == OK);

	/* set all even bits in set a */
	for (i = 0; i < IDSPERBLOCK * 4; i += 2)
	{
		ret = bitset_set(a, i);
		assert(ret == OK);
	}

	ret = bitset_set_count(a);
	assert(ret == (IDSPERBLOCK * 4 + 1)/ 2);

	/* set every 5th bit in set b */
	for (i = 0; i < IDSPERBLOCK * 4; i += 5)
	{
		ret = bitset_set(b, i);
		assert(ret == OK);
	}

	ret = bitset_set_count(b);
	assert(ret == (IDSPERBLOCK * 4 + 4) / 5);

	/* or them together */
	ret = bitset_or(a, b);
	assert(ret == OK);

	/* validate the result */
	for (i = 0; i < IDSPERBLOCK * 4; i++)
	{
		ret = bitset_test_bit(a, i, &bit);
		assert(ret == OK);
		assert(bit == ((i%2 == 0) || (i%5 == 0)));
	}

	ret = bitset_set_count(a);
	assert(ret == (IDSPERBLOCK * 4 + 1) / 2 + (IDSPERBLOCK * 4 + 4) / 5 / 2);

	/* free both */
	bitset_free(a);
	bitset_free(b);
}

void test_and()
{
	struct bitset *a = NULL, *b = NULL;
	int i, ret, bit;

	/* allocate two bitsets of the same size */
	ret = bitset_alloc(IDSPERBLOCK * 4, &a);
	assert(ret == OK);
	ret = bitset_alloc(IDSPERBLOCK * 4, &b);
	assert(ret == OK);

	/* set all even bits in set a */
	for (i = 0; i < IDSPERBLOCK * 4; i+=2)
	{
		ret = bitset_set(a, i);
		assert(ret == OK);
	}

	/* set every 5th bit in set b */
	for (i = 0; i < IDSPERBLOCK * 4; i+=5)
	{
		ret = bitset_set(b, i);
		assert(ret == OK);
	}

	/* or them together */
	ret = bitset_and(a, b);
	assert(ret == OK);

	/* validate the result */
	for (i = 0; i < IDSPERBLOCK * 4; i++)
	{
		ret = bitset_test_bit(a, i, &bit);
		assert(ret == OK);
		assert(bit == ((i%2 == 0) && (i%5 == 0)));
	}

	ret = bitset_set_count(a);
	assert(ret == (IDSPERBLOCK * 4 + 9) / 10);

	/* free both */
	bitset_free(a);
	bitset_free(b);
}


void test_subtract()
{
	struct bitset *a = NULL, *b = NULL;
	int i, ret, bit;

	/* allocate two bitsets of the same size */
	ret = bitset_alloc(IDSPERBLOCK * 4, &a);
	assert(ret == OK);
	ret = bitset_alloc(IDSPERBLOCK * 4, &b);
	assert(ret == OK);

	/* set all even bits in set a */
	for (i = 0; i < IDSPERBLOCK * 4; i+=2)
	{
		ret = bitset_set(a, i);
		assert(ret == OK);
	}

	/* set every 3rd bit in set b */
	for (i = 0; i < IDSPERBLOCK * 4; i+=3)
	{
		ret = bitset_set(b, i);
		assert(ret == OK);
	}

	/* subtract b from a*/
	ret = bitset_subtract(a, b);
	assert(ret == OK);

	/* validate the result */
	for (i = 0; i < IDSPERBLOCK * 4; i++)
	{
		ret = bitset_test_bit(a, i, &bit);
		assert(ret == OK);
		assert(bit == ((i%2 == 0) && !(i%3 == 0)));
	}

	ret = bitset_set_count(a);
	assert(ret == (IDSPERBLOCK * 4 + 1) / 2 - (IDSPERBLOCK * 4 + 5) / 6);

	/* free both */
	bitset_free(a);
	bitset_free(b);
}

void test_invert()
{
	struct bitset *b = NULL;
	int i, ret, bit;

	/* allocate a bitset with 4 blocks */
	ret = bitset_alloc(IDSPERBLOCK * 4, &b);
	assert(ret == OK);

	/* test the 3 invert cases:
	 *   1) invert block bits
	 *   2) all 1's => NULL
	 *   3) NULL => all 1's
	 */
	
	/* set block 0 to have some bits on */
	for (i = 0; i < IDSPERBLOCK; i+=2)
	{
		ret = bitset_set(b, i);
		assert(ret == OK);
	}

	/* set every bit in block 1 */
	for (i = IDSPERBLOCK; i < 2 * IDSPERBLOCK; i++)
	{
		ret = bitset_set(b, i);
		assert(ret == OK);
	}

	/* leave block 2 and 3 empty */

	/* verify the population count of the bitset */
	ret = bitset_set_count(b);
	assert(ret == IDSPERBLOCK + IDSPERBLOCK/2);

	/* invert the bitset b */
	ret = bitset_invert(b);
	assert(ret == OK);

	/* validate the result */

	/* block 0 should be flipped */
	for (i = 0; i < IDSPERBLOCK; i++)
	{
		ret = bitset_test_bit(b, i, &bit);
		assert(ret == OK);
		assert(bit == (i%2 != 0));
	}

	/* block 1 should now be a null pointer */
	assert(b->blocks[1] == NULL);

	/* block 2 and 3 should be all 1's */
	for (i = IDSPERBLOCK*2; i < IDSPERBLOCK*4; i++)
	{
		ret = bitset_test_bit(b, i, &bit);
		assert(ret == OK);
		assert(bit == 1);
	}

	/* validate the population count */
	ret = bitset_set_count(b);
	assert(ret == 2*IDSPERBLOCK + IDSPERBLOCK/2);

	/* free the bitset */
	bitset_free(b);
}

void test_iter_all()
{
	struct bitset *b = NULL;
	struct bitset_iterator iter;
	int ret, bit, index, on_count=0, n=0;

	/* allocate a bitset with 4 blocks */
	ret = bitset_alloc(IDSPERBLOCK * 4, &b);
	assert(ret == OK);

	/* set some bits */
	VERIFY(bitset_set(b, 10));
	VERIFY(bitset_set(b, IDSPERBLOCK+100));
	VERIFY(bitset_set(b, 3*IDSPERBLOCK+400));

	for (bitset_iter_init(&iter, b, BITSET_ITER_ALL);
		 !bitset_iter_at_end(&iter);
		 bitset_iter_next(&iter)) 
	{
		n++;

		bit = bitset_iter_get(&iter);
		index = bitset_iter_index(&iter);

		if (index == 10 ||
			index == IDSPERBLOCK + 100 ||
			index == 3*IDSPERBLOCK + 400)
		{
			assert(bit == 1);
			on_count++;
		}
		else
		{
			assert(bit == 0);
		}
	}

	assert(n == 4 * IDSPERBLOCK);
	assert(on_count == 3);
}

void test_iter_on()
{
	struct bitset *b = NULL;
	struct bitset_iterator iter;
	int ret, bit, index, on_count=0, n=0;

	/* allocate a bitset with 4 blocks */
	ret = bitset_alloc(IDSPERBLOCK * 4, &b);
	assert(ret == OK);

	/* set some bits */
	VERIFY(bitset_set(b, 1000));
	VERIFY(bitset_set(b, IDSPERBLOCK+100));
	VERIFY(bitset_set(b, 3*IDSPERBLOCK+400));

	for (bitset_iter_init(&iter, b, BITSET_ITER_ON);
		 !bitset_iter_at_end(&iter);
		 bitset_iter_next(&iter)) 
	{
		n++;

		bit = bitset_iter_get(&iter);
		index = bitset_iter_index(&iter);

		if (index == 1000 ||
			index == IDSPERBLOCK + 100 ||
			index == 3*IDSPERBLOCK + 400)
		{
			assert(bit == 1);
			on_count++;
		}
		else
		{
			/* BITSET_ITER_ON mode should never return an off bit */
			assert(0);
		}
	}

	assert(n == 3);
	assert(on_count == 3);
}

int main(int argc, char **argv)
{
	RUN_TEST(test_alloc);
	RUN_TEST(test_set_bad_input);
	RUN_TEST(test_set_bit);
	RUN_TEST(test_set_all_bits);
	RUN_TEST(test_clear_bit);
	RUN_TEST(test_toggle_bit);
	RUN_TEST(test_dup);
	RUN_TEST(test_or);
	RUN_TEST(test_and);
	RUN_TEST(test_subtract);
	RUN_TEST(test_invert);
	RUN_TEST(test_iter_all);
	RUN_TEST(test_iter_on);

	return 0;
}
