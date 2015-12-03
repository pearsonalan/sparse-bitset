#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "bitset.h"

int main(int argc, char **argv)
{
	FILE *in = NULL;
	char *p, line[80];
	int i, j, id, ids, bs, bc, fb, err;
	int allocs, bytes_allocated;
	int ret = 0;
	struct bitset *bset = NULL;
	struct bitset_block *blk;
	int maxid = 999999999;

	printf("BlockSize = %d, IdsPerBlock = %d, IdCount = %d, BlockCount = %d\n", 
		BLOCKSIZE, IDSPERBLOCK, maxid+1, BLOCKCOUNT(maxid+1));

	printf("sizeof bitset_block = %lu, sizeof bitset = %lu\n", sizeof(struct bitset_block), sizeof(struct bitset));

	ret = bitset_alloc(maxid+1, &bset);
	if (ret != OK) 
	{
		printf("Error %d allocating bitset\n", ret);
		return ret;
	}

	if (argc != 2)
	{
		in = stdin;
	}
	else
	{
		in = fopen(argv[1], "r");
		if (in == NULL) 
		{
			fprintf(stderr, "cannot open %s\n", argv[1]);
			return 1;
		}
	}

	printf("Loading IDs into bitset\n");

	ids = 0;
	while (fgets(line, sizeof(line)-1, in) != NULL) 
	{
		id = strtoul(line, &p, 10);
		if (p == line)
		{
			fprintf(stderr, "cannot parse %s as integer\n", line);
			ret = 2;
			goto exit;
		}

		if (id < 0 || id > maxid) 
		{
			fprintf(stderr, "id %d out of range\n", id);
			ret = 2;
			goto exit;
		}

#if 0
		printf("setting bit %d of long %d to on\n", id%64, id/64);
#endif
		if ((err = bitset_set(bset, id)) != OK) 
		{
			fprintf(stdout, "Error %d in bitset_set\n", err);
			return err;
		}

		ids++;
	}

#if 0
	printf("Counting filled blocks\n");

	bs = 0;
	bc = 0;
	fb = 0;

	for (i = 0; i < BLOCKCOUNT; i++) 
	{
		if (bset->blocks[i] != NULL)
		{
			blk = bset->blocks[i];

			fb++;

			bc = 0;
			for (j = 0; j < IDSPERBLOCK; j++)
			{
				if (bitset_block_is_set(blk, j))
				{
					bc++;
				}
			}

			printf("block %4d [%09d - %09d]: %5d filled\n",
				i, i*IDSPERBLOCK, ((i+1)*IDSPERBLOCK)-1, bc);
		} 
		else
		{
			printf("block %4d [%09d - %09d]: %5d filled\n",
				i, i*IDSPERBLOCK, ((i+1)*IDSPERBLOCK)-1, 0);
		}
	}

	fprintf(stdout, "%d IDs read. %d blocks, %d blocks filled, %d blocks empty\n",
		ids, BLOCKCOUNT, fb, BLOCKCOUNT - fb);
#endif


#if 0
	fprintf(stdout, "Counting set bits\n");

	filled = 0;
	for (i = 0; i <= MAXID; i++) 
	{
		if (ISSET(bitset,i))
			filled++;
	}

	fprintf(stdout, "%d bits set\n", filled);
#endif

	bitset_get_alloc_stats(&allocs, &bytes_allocated);
	printf("%d ids: %d block allocs, %0.2f Mb in block memory\n", ids, allocs, (float)bytes_allocated / (1024.0*1024.0));
	ret = 0;

exit:
	if (in != stdin) 
	{
		fclose(in);
	}

	bitset_free(bset);

	return ret;
}
