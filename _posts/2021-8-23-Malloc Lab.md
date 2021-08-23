---
layout: post 
category: CSAPP 
---
---

#### 方法1：使用隐式空闲链表、立即边界标记合并、首次适配方式的实现（类似于书上P597的代码）

最大的块大小为$2^{32}B = 4GB$

<img src="../../www/assets/pic/image-20210823151922931.png" alt="image-20210823151922931" style="zoom:50%;" />

隐式空闲链表的每个内存块有头和尾各占4个字节，头和尾中表示的内存块的大小是以字节为单位的内存块的大小，头和尾所占的共计8个字节也计算在内。所以可以看到序言块头和尾存储的是“$8/1$”，表示的是这个块占据8个字节，且已经被分配掉，实际上如果一个块占据8个字节，那么实际可以使用的空间是0字节。

堆起始位置的4个字节是用于保证整个堆的大小是8字节的倍数。因为结尾块仅使用4个字节，这4个字节和堆起始位置填充的4个字节刚好构成8个字节。且堆起始位置的4字节填充可以保证每个可用内存位置的起始地址都是8的倍数。

```c
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
#include "mm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/** rounds up to the nearest multiple of ALIGNMENT
 * 获取一个大于等于size且尽可能小的8的倍数
 * 使得实际分配的块的大小向8对齐
 */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/****************************************************************
 * 操作空闲链表的常数和宏
 */
// 单字4字节
#define WSIZE 4
// 双字8字节（内存分配要求双字对齐）
#define DSIZE 8
// 若堆大小不足，则扩展这么多字节
#define CHUNKSIZE (1 << 12)
// 定义计算MAX(x,y)的宏，避免使用math库
#define MAX(x, y) ((x > y) ? (x) : (y))
#define MIN(x, y) ((x < y) ? (x) : (y))
// 因为内存块都是按照8字节对齐的，所以表示内存块大小的低3比特总是零
// 实际在维护内存分配块时，可以使用这三个比特存储这个块是否被分配的信息
#define PACK(size, alloc) ((size) | (alloc))
// 获取指针p指向的地址的4个字节
#define GET(p) (*(unsigned int *)(p))
// 将4个字节的数据放到指针p指向的地址
#define PUT(p, val) (*(unsigned int *)(p) = (val))
// 获取指针指向的内存块的大小（将不表示大小的低3比特置0）
#define GET_SIZE(p) (GET(p) & ~0X7)
// 获取当前内存块是否被分配的信息（获取不表示大小的低3比特）
#define GET_ALLOC(p) (GET(p) & 0X1)
// 获取当前指针指向的可用内存块的头地址
#define HDRP(bp) ((char *)(bp)-WSIZE)
// 获取当前指针指向的可用内存块的脚地址
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
// 获取下一个内存块的起始地址（是可用内存块的起始地址，不是块头部的起始地址）
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
// 获取上一个内存块的起始地址
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))
/*********************************************************************
 * 定义全局变量
 * */
// 指向堆起始位置的指针
static void *heap_listp;

/**
 * 边界合并
 * */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/**
 * 用一个words个字（每个字4字节）的空块扩展堆
 * 参数words:   堆将要被扩展的字数
 * 返回值:      扩展的内存块的起始位置，如果为NULL则说明扩展失败
 * */
static void *extend_heap(size_t words) {
    char *bp;
    // 获得满足8字节对齐要求的实际size
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;
    // 原先的结尾块的头变成了扩展出来的新块的头
    // bp是指向扩展出来的新的内存区域的起始位置，因为前面有一个旧结尾块的头，
    // 所以bp可视作扩展出来的新块的可用内存区域的起始位置
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 1));
    // 添加一个新的结尾块的头（结尾块没有脚）
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}

/**
 * mm_init - initialize the malloc package.
 * 初始化分配器（初始化空闲链表），如果成功就返回0,否则返回-1
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) return -1;
    // 4个字节的填充块
    PUT(heap_listp, 0);
    // 序言块的头
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    // 序言块的脚
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    // 结尾块的头，结尾块没有脚
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    // 令heap_listp指向序言块当中可用内存的起始位置
    //（其实是只想序言块的脚的起始地址）,因为序言块的头和脚贴在一起，没有可用内存区域
    // 他只是一个方便遍历操作时确定边界而存在的序言，不需要可用内存区域
    heap_listp += 2 * WSIZE;
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;
    return 0;
}
/**
 * 首次适配      find_fit
 * 按需切割空闲块 place
 * */
static char *find_fit(size_t size) {
    char *bp = heap_listp;
    while (GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0) {
        bp = NEXT_BLKP(bp);
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= size) return bp;
    }
    return NULL;
}
static void place(void *bp, size_t size) {
    size_t available_size = GET_SIZE(HDRP(bp));
    if (available_size - size >= (DSIZE << 1)) {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(available_size - size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(available_size - size, 0));
    } else {
        PUT(HDRP(bp), PACK(available_size, 1));
        PUT(FTRP(bp), PACK(available_size, 1));
    }
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) return NULL;
    // 如果请求分配的堆内存空间小于8字节，也要给他分配一个8字节的可用内存空间
    if (size < DSIZE)
        // 最小块的大小是16字节，8字节用来满足对齐要求，另外8字节用来放头部和脚部位
        asize = 2 * DSIZE;
    else
        // 对于超过8字节的请求，先给请求分配的堆内存大小加上8字节的头部和脚部开销
        // 然后再向8字节对齐
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 * 释放一个块，然后立即处理空闲块合并
 * 参数bp: 指向待释放的可用内存块的起始位置
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/**
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * 为bp指针指向的内存块重新分配内存大小，
 */
void *mm_realloc(void *bp, size_t size) {
    if (bp == NULL) return mm_malloc(size);
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }
    // 如果要重新分配的内存空间小于原先的内存块，则不重新分配
    if (size <= GET_SIZE(HDRP(bp)) - DSIZE) return bp;
    void *new_bp = mm_malloc(size);
    if (new_bp == NULL) return NULL;
    size = GET_SIZE(HDRP(bp));
    size_t copy_size = MIN(GET_SIZE(HDRP(new_bp)), size);
    memcpy(new_bp, bp, copy_size - WSIZE);
    mm_free(bp);
    return new_bp;
}

```

![image-20210823213223076](../../www/assets/pic/image-20210823213223076.png)

<font color=blue>结果输出表格中util表示堆的空间利用率，ops表示总的操作次数，Kops表示$千次操作/秒$</font>

