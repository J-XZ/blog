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

**<font color=blue>结果输出表格中util表示堆的空间利用率，ops表示总的操作次数，Kops表示$千次操作/秒$</font>**

---

#### 下次适配

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
// 用于下次适配的指针
static char *now_bp;
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
    //（其实是指向序言块的脚的起始地址）,因为序言块的头和脚贴在一起，没有可用内存区域
    // 他只是一个方便遍历操作时确定边界而存在的序言，不需要可用内存区域
    heap_listp += 2 * WSIZE;
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) return -1;
    now_bp = heap_listp;
    return 0;
}
/**
 * 下次适配      find_fit
 * 按需切割空闲块 place
 * */
static char *find_fit(size_t size) {
    char *bp = now_bp;
    while (GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0) {
        bp = NEXT_BLKP(bp);
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= size) {
            now_bp = bp;
            return bp;
        }
    }
    bp = heap_listp;
    while (bp != now_bp) {
        bp = NEXT_BLKP(bp);
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= size) {
            now_bp = bp;
            return bp;
        }
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
    // 当发生空闲块合并时，当前扫描到的位置随着空闲块起始位置的改变而进行修正，
    // 防止下一次扫描时从一个空闲块的中间开始扫描
    now_bp = coalesce(bp);
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

修改的地方是：

增加一个用于记录当前扫描到的位置的指针

![image-20210823223401902](../../www/assets/pic/image-20210823223401902.png)

在mm_init函数中添加对上述变量的初始化，将其初始化为指向序言块的可用地址

![image-20210823223507736](../../www/assets/pic/image-20210823223507736.png)

在寻找适配位置的函数中，每次从上次扫描停止的地方开始扫描，如果扫码到底还没有找到合适位置，再从开始位置扫描到上次扫描位置。在返回适配位置之前，更新存储上次扫描位置的变量为当前扫描到的位置

![image-20210823223650076](../../www/assets/pic/image-20210823223650076.png)

在进行空闲块合并时，为了避免当前扫描到的位置恰好在某两个空闲块的中间，合并空闲块之后当前扫描到的位置指向一个大的空闲块内部的不合法情况，将当前扫描到的位置修改为空闲块合并之后的大空闲块起始位置

![image-20210823223913166](../../www/assets/pic/image-20210823223913166.png)

最终得分：

![image-20210823224320255](../../www/assets/pic/image-20210823224320255.png)

**下次适配**的空间利用率比**首次适配**低，但是时间效率高于首次适配，总体效果好于首次适配

首次适配可以避免在堆接近开头的位置留下大量的碎片，减少了对较大块的搜索时间

---

#### <font color=red>进一步改进思路：</font>

已分配的块可以省略脚部，因为是向8字节对齐，所以头部中保存块大小的size字段低3比特都是可以使用的，上面的做法仅使用了最低的一个比特用来看当前块是否被分配，还可以再使用次低比特来存储上一个块是否被分配。每个块只要看自己头部存储的信息，就可以知道前面一个块有没有被分配。如果前一个块处于已经被分配的状态，那么查询他的脚部是没有意义的，实际就可以省略这个脚部。脚部仅仅在空闲块合并时发挥作用

还可以用空闲块的主体部分维护一个双向链表将空闲块全部串起来，这样搜寻合适的空闲块消耗的时间就只和空闲块的数量正相关。（之前的做法中搜寻合适块的时间和总的块数量正相关）

再进一步可以使用分离适配，维护多个空闲块大小范围的链表，每次搜寻合适的空闲块时可以缩小搜寻范围，继续提高搜索效率

---

#### 使用显式空闲链表（双向链表组织所有空闲块）+ 立即合并空闲块 + 最优适配 + 去除已分配块的脚部

使用最优适配是因为只有这种适配方法可以尽可能提高内存利用率，否则无法通过样例。

可以看到使用显式空闲链表可以大幅提升吞吐量（因为搜索合适块的范围变成了所有的空闲块，而隐式空闲链表要搜索全部的内存块），但是牺牲了内存利用率。

![image-20210825134523486](../../www/assets/pic/image-20210825134523486.png)

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

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//自定义////////////////////////////////////////////////////////////////////////
#define min(x, y) (x < y ? x : y)
typedef unsigned int uint;
typedef unsigned long ulong;
typedef struct node {
    uint inf;
    struct node *prev;
    struct node *next;
} node, *node_p;
static const uint mask1_1000 = ~(uint)0b111;
static const uint mask0_0010 = (uint)0b10;
static const uint mask0_0001 = (uint)0b1;
static const uint mask0_0111 = (uint)0b111;
static inline uint get_block_size(node_p x) {
    uint inf = x->inf;
    return inf & mask1_1000;
}
static inline int get_useful_size(node_p x) { return get_block_size(x) - 4; }
static inline uint get_prev_allocated(node_p x) {
    uint inf = x->inf;
    return inf & mask0_0010;
}
static inline uint get_allocated(node_p x) {
    uint inf = x->inf;
    return inf & mask0_0001;
}
static inline uint get_prev_foot(node_p x) {
    if (!get_prev_allocated(x)) {
        void *ret = x;
        ret -= 4;
        return ((node *)ret);
    }
    printf("error!");
    return NULL;
}
static inline uint make_inf(uint size, uint is_prev_allocated,
                            uint is_allocated) {
    uint value = size;
    value &= mask1_1000;
    value |= (uint)(is_prev_allocated ? 0b10 : 0);
    value |= (uint)(is_allocated ? 0b1 : 0);
    return value;
}
static inline node_p get_ret_pointer(node_p x) {
    void *ret = x;
    ret += 4;
    return ret;
}
static inline node_p get_next_neighbour(node_p x) {
    void *ret = (void *)x;
    ret += get_block_size(x);
    return (node_p)ret;
}
static inline node_p get_prev_neighbour(node_p x) {
    if (!get_prev_allocated(x)) {
        uint foot = *(uint *)get_prev_foot(x);
        uint prev_block_size = foot & mask1_1000;
        void *ret = (void *)x;
        ret -= prev_block_size;
        return ret;
    } else {
        printf("error\n");
        return NULL;
    }
}
static inline void copy_head_2_foot(node_p x) {
    void *foot = x;
    foot += get_block_size(x) - 4;
    *(uint *)foot = x->inf;
}
static inline void set_inf(node_p x, uint size, uint is_prev_allocated,
                           uint is_allocated) {
    uint value = size;
    value &= mask1_1000;
    value |= (uint)(is_prev_allocated ? 0b10 : 0);
    value |= (uint)(is_allocated ? 0b1 : 0);
    x->inf = value;
}
static node begin;
static node end;
static inline void prepare_list() {
    begin.next = &end;
    end.prev = &begin;
    set_inf(&begin, 0, 0, 0);
    set_inf(&end, 0, 0, 0);
}
static inline void insert_node(node_p x) {
    node_p start = &begin;
    node_p second = start->next;
    start->next = x;
    x->next = second;
    x->prev = start;
    second->prev = x;
}
static inline void erase_node(node_p x) {
    node_p prev = x->prev;
    node_p next = x->next;
    prev->next = next;
    next->prev = prev;
}
static node_p second_last_node;
static inline node_p find_fit(uint size) {
    node_p x = &begin;
    node_p ret = NULL;
    uint min_size = __INT_MAX__;
    while (get_useful_size(x->next) > 0) {
        x = x->next;
        uint tmp_size = get_block_size(x);
        if (tmp_size >= size) {
            if (tmp_size <= min_size) {
                min_size = tmp_size;
                ret = x;
            }
        }
    }
    return ret;
}
static inline node_p merge_node(node_p x) {
    uint prev_allocated = get_prev_allocated(x);
    node_p next_neighbour = get_next_neighbour(x);
    uint next_allocated = get_allocated(next_neighbour);
    if (prev_allocated && next_allocated) {
        return x;
    }
    if (prev_allocated && !next_allocated) {
        if (next_neighbour == second_last_node || x == second_last_node) {
            second_last_node = x;
        }
        erase_node(next_neighbour);
        erase_node(x);

        set_inf(x, get_block_size(x) + get_block_size(next_neighbour), 1, 0);
        copy_head_2_foot(x);
        insert_node(x);

        return x;
    }
    if (!prev_allocated && next_allocated) {
        node_p prev_neighbour = get_prev_neighbour(x);
        if (x == second_last_node || prev_neighbour == second_last_node) {
            second_last_node = prev_neighbour;
        }
        erase_node(x);
        erase_node(prev_neighbour);

        set_inf(prev_neighbour,
                get_block_size(prev_neighbour) + get_block_size(x),
                get_prev_allocated(prev_neighbour), 0);
        copy_head_2_foot(prev_neighbour);
        insert_node(prev_neighbour);

        return prev_neighbour;
    }
    if (!prev_allocated && !next_allocated) {
        node_p prev_neighbour = get_prev_neighbour(x);
        if (next_neighbour == second_last_node || x == second_last_node ||
            prev_neighbour == second_last_node) {
            second_last_node = prev_neighbour;
        }
        erase_node(prev_neighbour);
        erase_node(x);
        erase_node(next_neighbour);

        set_inf(prev_neighbour,
                get_block_size(prev_neighbour) + get_block_size(x) +
                    get_block_size(next_neighbour),
                get_prev_allocated(prev_neighbour), 0);
        copy_head_2_foot(prev_neighbour);
        insert_node(prev_neighbour);

        return prev_neighbour;
    }
}
static inline node_p get_more_space(uint size) {
    if (!get_allocated(second_last_node)) {
        size -= get_block_size(second_last_node);
    }
    if (size <= 0) {
        return second_last_node;
    }
    if (size < 16) {
        size = 16;
    }
    node_p ret = mem_sbrk(size);
    ret = (node_p)((void *)ret - 4);

    set_inf(ret, size, get_allocated(second_last_node), 0);
    copy_head_2_foot(ret);
    insert_node(ret);

    set_inf(get_next_neighbour(ret), 0, 0, 1);
    ret = merge_node(ret);
    // second_last_node = ret;
    return ret;
}
static inline void allocate(node_p x, uint size) {
    erase_node(x);
    uint old_block_size = get_block_size(x);
    if (old_block_size - size > 16) {
        node_p next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, get_block_size(next_neighbour), 0,
                get_allocated(next_neighbour));
        set_inf(x, size, get_prev_allocated(x), 1);
        next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, old_block_size - size, 1, 0);
        copy_head_2_foot(next_neighbour);
        insert_node(next_neighbour);
        merge_node(next_neighbour);
    } else {
        set_inf(x, get_block_size(x), get_prev_allocated(x), 1);
        node_p next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, get_block_size(next_neighbour), 1,
                get_allocated(next_neighbour));
    }
}
//自定义////////////////////////////////////////////////////////////////////////

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    prepare_list();
    void *p = mem_sbrk(4 * 4);
    second_last_node = (node *)(p + 4);
    uint *pp = (uint *)p;
    pp[1] = make_inf(8, 1, 1);
    pp[3] = make_inf(0, 1, 1);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size += 4;
    size = ALIGN(size);
    if (size < 16) {
        size = 16;
    }
    node_p insert_p = find_fit(size);
    if (!insert_p) {
        insert_p = get_more_space(size);
    }
    if (insert_p == NULL) {
        return NULL;
    }
    allocate(insert_p, size);
    return get_ret_pointer(insert_p);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    node_p x = (node_p)(ptr - 4);
    set_inf(x, get_block_size(x), get_prev_allocated(x), 0);
    copy_head_2_foot(x);
    insert_node(x);
    merge_node(x);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    size = ALIGN(size + 4);
    if (get_block_size(ptr - 4) >= size) {
        return ptr;
    }
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    copySize = min(size, get_useful_size(oldptr));
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

```

#### 使用分离存储的空闲链表，将不同大小范围的空闲内存块用不同的链表进行组织，进一步提高搜索效率

空闲链表的分组方式：

![image-20210825140507506](../../www/assets/pic/image-20210825140507506.png)

![image-20210825140440468](../../www/assets/pic/image-20210825140440468.png)

我的代码实现主要是在大样例和remalloc函数实现上对于内存利用效率太低，最后两个样例极低的内存利用率拉低了得分。应当想办法优化remalloc函数的实现

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

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//自定义////////////////////////////////////////////////////////////////////////
#define min(x, y) (x < y ? x : y)
typedef unsigned int uint;
typedef unsigned long ulong;
typedef struct node {
    uint inf;
    struct node *prev;
    struct node *next;
} node, *node_p;
static const uint mask1_1000 = ~(uint)0b111;
static const uint mask0_0010 = (uint)0b10;
static const uint mask0_0001 = (uint)0b1;
static const uint mask0_0111 = (uint)0b111;
static inline uint get_block_size(node_p x) {
    uint inf = x->inf;
    return inf & mask1_1000;
}
static inline int get_useful_size(node_p x) { return get_block_size(x) - 4; }
static inline uint get_prev_allocated(node_p x) {
    uint inf = x->inf;
    return inf & mask0_0010;
}
static inline uint get_allocated(node_p x) {
    uint inf = x->inf;
    return inf & mask0_0001;
}
static inline uint get_prev_foot(node_p x) {
    if (!get_prev_allocated(x)) {
        void *ret = x;
        ret -= 4;
        return ((node *)ret);
    }
    printf("error!");
    return NULL;
}
static inline uint make_inf(uint size, uint is_prev_allocated,
                            uint is_allocated) {
    uint value = size;
    value &= mask1_1000;
    value |= (uint)(is_prev_allocated ? 0b10 : 0);
    value |= (uint)(is_allocated ? 0b1 : 0);
    return value;
}
static inline node_p get_ret_pointer(node_p x) {
    void *ret = x;
    ret += 4;
    return ret;
}
static inline node_p get_next_neighbour(node_p x) {
    void *ret = (void *)x;
    ret += get_block_size(x);
    return (node_p)ret;
}
static inline node_p get_prev_neighbour(node_p x) {
    if (!get_prev_allocated(x)) {
        uint foot = *(uint *)get_prev_foot(x);
        uint prev_block_size = foot & mask1_1000;
        void *ret = (void *)x;
        ret -= prev_block_size;
        return ret;
    } else {
        printf("error\n");
        return NULL;
    }
}
static inline void copy_head_2_foot(node_p x) {
    void *foot = x;
    foot += get_block_size(x) - 4;
    *(uint *)foot = x->inf;
}
static inline void set_inf(node_p x, uint size, uint is_prev_allocated,
                           uint is_allocated) {
    uint value = size;
    value &= mask1_1000;
    value |= (uint)(is_prev_allocated ? 0b10 : 0);
    value |= (uint)(is_allocated ? 0b1 : 0);
    x->inf = value;
}
static node begin[10];
static node end[10];
static inline uint get_index(uint size) {
    if (size <= 16) {
        return 1;
    } else if (size <= 32) {
        return 2;
    } else if (size <= 64) {
        return 3;
    } else if (size <= 128) {
        return 4;
    } else {
        return 5;
    }
}
static inline void prepare_list() {
    for (int i = 0; i < 9; i++) {
        begin[i].next = &end[i];
        end[i].prev = &begin[i];
        set_inf(&begin[i], 0, 0, 0);
        set_inf(&end[i], 0, 0, 0);
    }
}
static inline void insert_node(node_p x) {
    node_p start = &begin[get_index(get_block_size(x))];
    node_p second = start->next;
    start->next = x;
    x->next = second;
    x->prev = start;
    second->prev = x;
}
static inline void erase_node(node_p x) {
    node_p prev = x->prev;
    node_p next = x->next;
    prev->next = next;
    next->prev = prev;
}
static node_p second_last_node;
static inline node_p find_fit(uint size) {
    node_p x = &begin[get_index(size)];
    node_p ret = NULL;
    uint min_size = __INT_MAX__;
    while (get_useful_size(x->next) > 0) {
        x = x->next;
        uint tmp_size = get_block_size(x);
        if (tmp_size >= size) {
            if (tmp_size <= min_size) {
                min_size = tmp_size;
                ret = x;
            }
        }
    }
    return ret;
}
static inline node_p merge_node(node_p x) {
    uint prev_allocated = get_prev_allocated(x);
    node_p next_neighbour = get_next_neighbour(x);
    uint next_allocated = get_allocated(next_neighbour);
    if (prev_allocated && next_allocated) {
        return x;
    }
    if (prev_allocated && !next_allocated) {
        if (next_neighbour == second_last_node || x == second_last_node) {
            second_last_node = x;
        }
        erase_node(next_neighbour);
        erase_node(x);

        set_inf(x, get_block_size(x) + get_block_size(next_neighbour), 1, 0);
        copy_head_2_foot(x);
        insert_node(x);

        return x;
    }
    if (!prev_allocated && next_allocated) {
        node_p prev_neighbour = get_prev_neighbour(x);
        if (x == second_last_node || prev_neighbour == second_last_node) {
            second_last_node = prev_neighbour;
        }
        erase_node(x);
        erase_node(prev_neighbour);

        set_inf(prev_neighbour,
                get_block_size(prev_neighbour) + get_block_size(x),
                get_prev_allocated(prev_neighbour), 0);
        copy_head_2_foot(prev_neighbour);
        insert_node(prev_neighbour);

        return prev_neighbour;
    }
    if (!prev_allocated && !next_allocated) {
        node_p prev_neighbour = get_prev_neighbour(x);
        if (next_neighbour == second_last_node || x == second_last_node ||
            prev_neighbour == second_last_node) {
            second_last_node = prev_neighbour;
        }
        erase_node(prev_neighbour);
        erase_node(x);
        erase_node(next_neighbour);

        set_inf(prev_neighbour,
                get_block_size(prev_neighbour) + get_block_size(x) +
                    get_block_size(next_neighbour),
                get_prev_allocated(prev_neighbour), 0);
        copy_head_2_foot(prev_neighbour);
        insert_node(prev_neighbour);

        return prev_neighbour;
    }
}
static inline node_p get_more_space(uint size) {
    if (!get_allocated(second_last_node)) {
        size -= get_block_size(second_last_node);
    }
    if (size <= 0) {
        return second_last_node;
    }
    if (size < 16) {
        size = 16;
    }
    node_p ret = mem_sbrk(size);
    ret = (node_p)((void *)ret - 4);

    set_inf(ret, size, get_allocated(second_last_node), 0);
    copy_head_2_foot(ret);
    insert_node(ret);

    set_inf(get_next_neighbour(ret), 0, 0, 1);
    ret = merge_node(ret);
    // second_last_node = ret;
    return ret;
}
static inline void allocate(node_p x, uint size) {
    erase_node(x);
    uint old_block_size = get_block_size(x);
    if (old_block_size - size > 16) {
        node_p next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, get_block_size(next_neighbour), 0,
                get_allocated(next_neighbour));
        set_inf(x, size, get_prev_allocated(x), 1);
        next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, old_block_size - size, 1, 0);
        copy_head_2_foot(next_neighbour);
        insert_node(next_neighbour);
        merge_node(next_neighbour);
    } else {
        set_inf(x, get_block_size(x), get_prev_allocated(x), 1);
        node_p next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, get_block_size(next_neighbour), 1,
                get_allocated(next_neighbour));
    }
}
//自定义////////////////////////////////////////////////////////////////////////

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    prepare_list();
    void *p = mem_sbrk(4 * 4);
    second_last_node = (node *)(p + 4);
    uint *pp = (uint *)p;
    pp[1] = make_inf(8, 1, 1);
    pp[3] = make_inf(0, 1, 1);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size += 4;
    size = ALIGN(size);
    if (size < 16) {
        size = 16;
    }
    node_p insert_p = find_fit(size);
    if (!insert_p) {
        insert_p = get_more_space(size);
    }
    if (insert_p == NULL) {
        return NULL;
    }
    allocate(insert_p, size);
    return get_ret_pointer(insert_p);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    node_p x = (node_p)(ptr - 4);
    set_inf(x, get_block_size(x), get_prev_allocated(x), 0);
    copy_head_2_foot(x);
    insert_node(x);
    merge_node(x);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    size = ALIGN(size + 4);
    if (get_block_size(ptr - 4) >= size) {
        return ptr;
    }
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    copySize = min(size, get_useful_size(oldptr));
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

```

优化realloc函数，如果要求重新分配的块的后面一个块是空块，并且使用后面一个块扩充当前块可以满足重新分配内存的要求，则直接使用后一个块填充当前块

```c
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size = ALIGN(size + 4);
    if (get_block_size(ptr - 4) >= size) {
        // allocate(ptr - 4, size);
        return ptr;
    } else if (!get_allocated(get_next_neighbour(ptr - 4)) &&
               get_useful_size(ptr - 4) +
                       get_useful_size(get_next_neighbour(ptr - 4)) >=
                   size) {
        if (get_next_neighbour(ptr - 4) == second_last_node) {
            second_last_node = ptr - 4;
        }
        erase_node(get_next_neighbour(ptr - 4));
        set_inf(ptr - 4,
                get_block_size(ptr - 4) +
                    get_block_size(get_next_neighbour(ptr - 4)),
                get_prev_allocated(ptr - 4), 1);
        set_inf(get_next_neighbour(ptr - 4),
                get_block_size(get_next_neighbour(ptr - 4)), 1,
                get_allocated(get_next_neighbour(ptr - 4)));
        return ptr;
    }
    // if (!get_prev_allocated(ptr - 4) &&
    //     get_useful_size(get_prev_neighbour(ptr - 4)) +
    //             get_useful_size(ptr - 4) >=
    //         size) {
    //     node *prev_neighbour = get_prev_neighbour(ptr - 4);
    //     erase_node(prev_neighbour);
    //     if (ptr - 4 == second_last_node) {
    //         second_last_node = prev_neighbour;
    //     }
    //     set_inf(prev_neighbour,
    //             get_block_size(ptr - 4) + get_block_size(prev_neighbour),
    //             get_prev_allocated(prev_neighbour), 1);
    //     set_inf(get_next_neighbour(prev_neighbour),
    //             get_block_size(get_next_neighbour(prev_neighbour)), 1,
    //             get_allocated(get_next_neighbour(prev_neighbour)));
    //     memcpy((void *)prev_neighbour + 4, (void *)ptr,
    //            get_useful_size(ptr - 4));
    //     return get_ret_pointer(prev_neighbour);
    // }

    // node_p this_node = (void *)ptr - 4;
    // uint num1 = *(uint *)ptr;
    // uint num2 = *(uint *)((void *)ptr + 4);
    // uint old_block_size = get_block_size(this_node);
    // uint foot1 = *((uint *)(((void *)this_node) + old_block_size - 4));
    // mm_free(ptr);
    // node_p new_ptr = (node_p)(mm_malloc(size) - 4);
    // size_t copySize = min(size, old_block_size - 4);
    // memcpy((void *)new_ptr + 4, (void *)ptr, copySize);
    // *(uint *)(&(new_ptr->next)) = num1;
    // *(uint *)(&(new_ptr->prev)) = num2;
    // uint new_block_size = get_block_size(new_ptr);
    // if (new_block_size >= old_block_size) {
    //     *((uint *)(((void *)new_ptr) + old_block_size - 4)) = foot1;
    // }
    // return get_ret_pointer(new_ptr);

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;
    copySize = min(size, get_useful_size(oldptr - 4));
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

```

![image-20210828233723125](../../www/assets/pic/image-20210828233723125.png)

对于堆栈合法性检查的一点思路

```c

int mm_check(void) {
    for (int i = 1; i <= 5; i++) {
        node_p x = begin + i;
        while (get_block_size(x->next) > 0) {
            x = x->next;
            if (get_allocated(x->inf)) {
                printf("有非空闲的块被错误地放到了空闲链表中\n");
                return -1;
            }
        }
    }
    ///跳过第一个填充的4字节
    void *x = very_begin + 4;
    int last = 1;
    while (get_block_size(x) > 0) {
        if (last == 0 && get_allocated(x) == 0) {
            printf("存在连续的空闲块未合并\n");
            return -1;
        }
        last = get_allocated(x);
        // 如果当前扫描到的内存块是空的
        if (!get_allocated(x)) {
            int have_found = 0;
            // 遍历整个空闲链表
            for (int i = 1; i <= 5; i++) {
                node_p xx = begin + i;
                while (get_block_size(xx->next) > 0) {
                    xx = xx->next;
                    if (xx == x) {
                        have_found = 1;
                    }
                }
            }
            if (!have_found) {
                printf("存在空闲但是没有加入空闲链表的块\n");
                return -1;
            }
        }
    }

    return 1;
}
```

---

---

后续又修改了一些小bug

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

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//自定义////////////////////////////////////////////////////////////////////////
#define min(x, y) (x < y ? x : y)
typedef unsigned int uint;
typedef unsigned long ulong;
typedef struct node {
    uint inf;
    struct node *prev;
    struct node *next;
} node, *node_p;
static const uint mask1_1000 = ~(uint)0b111;
static const uint mask0_0010 = (uint)0b10;
static const uint mask0_0001 = (uint)0b1;
static inline uint get_block_size(node_p x) {
    uint inf = x->inf;
    return inf & mask1_1000;
}
static inline int get_useful_size(node_p x) { return get_block_size(x) - 4; }
static inline uint get_prev_allocated(node_p x) {
    uint inf = x->inf;
    return inf & mask0_0010;
}
static inline uint get_allocated(node_p x) {
    uint inf = x->inf;
    return inf & mask0_0001;
}
static inline void *get_prev_foot(node_p x) {
    if (!get_prev_allocated(x)) {
        void *ret = x;
        ret -= 4;
        return ((node *)ret);
    }
    printf("error!");
    return NULL;
}
static inline uint make_inf(uint size, uint is_prev_allocated,
                            uint is_allocated) {
    uint value = size;
    value &= mask1_1000;
    value |= (uint)(is_prev_allocated ? 0b10 : 0);
    value |= (uint)(is_allocated ? 0b1 : 0);
    return value;
}
static inline node_p get_ret_pointer(node_p x) {
    void *ret = x;
    ret += 4;
    return ret;
}
static inline node_p get_next_neighbour(node_p x) {
    void *ret = (void *)x;
    ret += get_block_size(x);
    return (node_p)ret;
}
static inline node_p get_prev_neighbour(node_p x) {
    if (!get_prev_allocated(x)) {
        uint foot = *(uint *)get_prev_foot(x);
        uint prev_block_size = foot & mask1_1000;
        void *ret = (void *)x;
        ret -= prev_block_size;
        return ret;
    } else {
        printf("error\n");
        return NULL;
    }
}
static inline void copy_head_2_foot(node_p x) {
    void *foot = x;
    foot += get_block_size(x) - 4;
    *(uint *)foot = x->inf;
}
static inline void set_inf(node_p x, uint size, uint is_prev_allocated,
                           uint is_allocated) {
    uint value = size;
    value &= mask1_1000;
    value |= (uint)(is_prev_allocated ? 0b10 : 0);
    value |= (uint)(is_allocated ? 0b1 : 0);
    x->inf = value;
    if (!is_allocated) {
        copy_head_2_foot(x);
    }
}
static node begin[10];
static node end[10];
static inline uint get_index(uint x) {
    return (x <= 64 ? (x <= 32 ? (x <= 16 ? 1 : 2) : 3) : (x <= 128 ? 4 : 5));
}
static inline void prepare_list() {
    for (int i = 0; i < 9; i++) {
        begin[i].next = &end[i];
        end[i].prev = &begin[i];
    }
}
static inline void insert_node(node_p x) {
    node_p start = &begin[get_index(get_block_size(x))];
    node_p second = start->next;
    start->next = x;
    x->next = second;
    x->prev = start;
    second->prev = x;
}
static inline void erase_node(node_p x) {
    node_p prev = x->prev;
    node_p next = x->next;
    prev->next = next;
    next->prev = prev;
}
static node_p second_last_node;
static inline node_p find_fit(uint size) {
    node_p x = &begin[get_index(size)];
    node_p ret = NULL;
    uint min_size = __INT_MAX__;
    while (get_useful_size(x->next) > 0) {
        x = x->next;
        uint tmp_size = get_block_size(x);
        if (tmp_size >= size) {
            if (tmp_size < min_size) {
                min_size = tmp_size;
                ret = x;
            }
        }
    }
    return ret;
}
static inline node_p merge_node(node_p x) {
    uint prev_allocated = get_prev_allocated(x);
    node_p next_neighbour = get_next_neighbour(x);
    uint next_allocated = get_allocated(next_neighbour);
    if (prev_allocated && next_allocated) {
        return x;
    } else if (prev_allocated && !next_allocated) {
        if (next_neighbour == second_last_node || x == second_last_node) {
            second_last_node = x;
        }
        erase_node(next_neighbour);
        erase_node(x);

        set_inf(x, get_block_size(x) + get_block_size(next_neighbour),
                get_prev_allocated(x), 0);
        insert_node(x);

        return x;
    } else if (!prev_allocated && next_allocated) {
        node_p prev_neighbour = get_prev_neighbour(x);
        if (x == second_last_node || prev_neighbour == second_last_node) {
            second_last_node = prev_neighbour;
        }
        erase_node(x);
        erase_node(prev_neighbour);

        set_inf(prev_neighbour,
                get_block_size(prev_neighbour) + get_block_size(x),
                get_prev_allocated(prev_neighbour), 0);
        insert_node(prev_neighbour);

        return prev_neighbour;
    } else if (!prev_allocated && !next_allocated) {
        node_p prev_neighbour = get_prev_neighbour(x);
        if (next_neighbour == second_last_node || x == second_last_node ||
            prev_neighbour == second_last_node) {
            second_last_node = prev_neighbour;
        }
        erase_node(prev_neighbour);
        erase_node(x);
        erase_node(next_neighbour);

        set_inf(prev_neighbour,
                get_block_size(prev_neighbour) + get_block_size(x) +
                    get_block_size(next_neighbour),
                get_prev_allocated(prev_neighbour), 0);
        // copy_head_2_foot(prev_neighbour);
        insert_node(prev_neighbour);

        return prev_neighbour;
    }
    return x;
}
static inline node_p get_more_space(uint size) {
    if (!get_allocated(second_last_node)) {
        size -= get_block_size(second_last_node);
    }
    if (size <= 0) {
        return second_last_node;
    }
    if (size < 16) {
        size = 16;
    }
    node_p ret = mem_sbrk(size);
    ret = (node_p)((void *)ret - 4);

    set_inf(ret, size, get_allocated(second_last_node), 0);
    // copy_head_2_foot(ret);
    insert_node(ret);

    set_inf(get_next_neighbour(ret), 0, 0, 1);
    ret = merge_node(ret);
    return ret;
}

static inline void allocate(node_p x, uint size) {
    erase_node(x);
    uint old_block_size = get_block_size(x);
    if (old_block_size - size >= 16) {
        node_p next_neighbour = get_next_neighbour(x);

        set_inf(next_neighbour, get_block_size(next_neighbour), 0,
                get_allocated(next_neighbour));

        set_inf(x, size, get_prev_allocated(x), 1);

        next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, old_block_size - size, 1, 0);
        insert_node(next_neighbour);
        if (x == second_last_node) {
            second_last_node = next_neighbour;
        }
        merge_node(next_neighbour);
    } else {
        set_inf(x, get_block_size(x), get_prev_allocated(x), 1);
        node_p next_neighbour = get_next_neighbour(x);
        set_inf(next_neighbour, get_block_size(next_neighbour), 1,
                get_allocated(next_neighbour));
    }
}
//自定义////////////////////////////////////////////////////////////////////////

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    prepare_list();
    void *p = mem_sbrk(4 * 4);
    second_last_node = (node *)(p + 4);
    uint *pp = (uint *)p;
    pp[1] = make_inf(8, 1, 1);
    pp[3] = make_inf(0, 1, 1);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size += 4;
    size = ALIGN(size);
    if (size < 16) {
        size = 16;
    }
    node_p insert_p = find_fit(size);
    if (!insert_p) {
        insert_p = get_more_space(size);
    }
    if (insert_p == NULL) {
        return NULL;
    }
    allocate(insert_p, size);
    return get_ret_pointer(insert_p);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    node_p x = (node_p)(ptr - 4);
    set_inf(x, get_block_size(x), get_prev_allocated(x), 0);
    insert_node(x);
    merge_node(x);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    uint oldsize = size;
    size = ALIGN(size + 4);
    if (get_block_size(ptr - 4) >= size) {
        // allocate(ptr - 4, size);
        return ptr;
    } else if (!get_allocated(get_next_neighbour(ptr - 4)) &&
               get_useful_size(ptr - 4) +
                       get_useful_size(get_next_neighbour(ptr - 4)) >=
                   size) {
        if (get_next_neighbour(ptr - 4) == second_last_node) {
            second_last_node = ptr - 4;
        }
        erase_node(get_next_neighbour(ptr - 4));
        set_inf(ptr - 4,
                get_block_size(ptr - 4) +
                    get_block_size(get_next_neighbour(ptr - 4)),
                get_prev_allocated(ptr - 4), 1);
        set_inf(get_next_neighbour(ptr - 4),
                get_block_size(get_next_neighbour(ptr - 4)), 1,
                get_allocated(get_next_neighbour(ptr - 4)));
        return ptr;
    }
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(oldsize);
    if (newptr == NULL) return NULL;
    copySize = min(size, get_useful_size(oldptr - 4));
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

```

![image-20210829154402215](../../www/assets/pic/image-20210829154402215.png)
