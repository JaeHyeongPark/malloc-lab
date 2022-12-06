/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */

/* rounds up to the nearest multiple of ALIGNMENT */

#define WSIZE 4           /* word & header/footer size(bytes) */
#define DSIZE 8           /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc) )  /* size, 할당 비트를 통합해서, header, footer에 저장할 수 있는 값 return */

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p)) /* p가 가리키는 워드를 읽어서 return */
#define PUT(p, val) (*(unsigned int *)(p) = (val)) /* p가 가리키는 word에 val을 저장 */

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) /* p에 있는 header/footer size return */
#define GET_ALLOC(p) (GET(p) & 0x1) /* p에 있는 header/footer 할당 비트 return */

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Given block ptr bp,compute address of next and previous blocks*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* pointer at prologue block */
static char *heap_listp;


/*
 * first fit function: 
 */ 
static void *find_fit(size_t asize)
{
    void* ptr;
    for (ptr = heap_listp; GET_SIZE(HDRP(ptr))>0; ptr = NEXT_BLKP(ptr)) /* until meet Epilogue */
        if( GET_SIZE(HDRP(ptr)) >= asize && !GET_ALLOC(HDRP(ptr)))
        {
            return ptr;
        }

    return NULL;
}

/*
 * place function - allocate block which has 'asize' size
 */
static void place(void *bp, size_t asize)
{
    size_t original_size = GET_SIZE(HDRP(bp));

    if ( (original_size-asize) >= 2*DSIZE )
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(original_size-asize, 0));
        PUT(FTRP(bp), PACK(original_size-asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(original_size, 1));
        PUT(FTRP(bp), PACK(original_size, 1));
    }
}

/*
 * coalesce
 */

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); /* 0 or 1 */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }

    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * extend_heap - When (1) initiate heap or, (2) malloc cannot find fit free blocks
                 Extend heap to make new free blocks
 */

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; /* 조건식 ? 반환값1(true):반환값2(false) */
    if ((long)(bp = mem_sbrk(size)) == -1) /* bp는 old_brk 포인터. */
        return NULL;

    /* initialize free block header,footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); /* new epilogue header */

    /* coalesce if the previous block was free */
    return coalesce(bp);
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) /* word 4개짜리만큼 brk 옮기는 작업*/
        return -1;
    PUT(heap_listp, 0);                         /* Alignment Padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); /* Prologues footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += 2*WSIZE; /* heap의 시작점에서 2word 뒤에 둠. 항상 prologue block 중앙에 위치. */

    /*extend the empty heap with a free block of CHUNKSIZE bytes*/
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize; /* fit 한 free block없으면 extend_heap 호출 */
    char *bp;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE + DSIZE * ((size + DSIZE - 1)/DSIZE);
    
    /* searching FIT free block */
    if ( (bp = find_fit(asize)) != NULL )
    {
        place(bp, asize);
        return bp;
    }

    /* NO Fit found. Extend memory and place the block*/
    extendsize = MAX(asize, CHUNKSIZE);
    if ( (bp = extend_heap(extendsize/WSIZE)) == NULL )
        return NULL;
    place(bp, asize);
    return bp;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);

    return newptr;
}

