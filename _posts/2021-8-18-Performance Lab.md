---
layout: post 
category: CSAPP 
---
---

优化内存操作和计算密集型的程序（图像旋转和图像平滑），以提高性能

题目保证样例的图像为正方形、边长一定是32的整数倍

## 优化图像旋转

基础做法是用一个 $边长\times边长$ 的循环将每个元素放到对应的位置

```c
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) {
    int i, j;

    for (i = 0; i < dim; i++)
        for (j = 0; j < dim; j++)
            dst[RIDX(dim - 1 - j, i, dim)] = src[RIDX(i, j, dim)];
}
```

![image-20210818195856505](../../www/assets/pic/image-20210818195856505.png)

---

参考图：

<img src="../../www/assets/pic/image-20210818211210564.png" alt="image-20210818211210564" style="zoom: 50%;" />

可以简化原始做法中对于宏RIDX(i, j, dim)的调用，代码写为：

```c
char rotate_descr[] = "步长为dim的循环";
void rotate(int dim, pixel *src, pixel *dst) {
    for (int i = 1; i <= dim; i++) {
        for (int j = 1; j <= dim; j++) {
            dst[j - 1] = src[dim * j - i];
        }
        dst += dim;
    }
}
```

![image-20210818212314994](../../www/assets/pic/image-20210818212314994.png)

---

参考[cache lab](https://j-xz.github.io/www/cache-lab.html)使用矩阵分块技术提高程序的空间局部性加速矩阵转置的方法，使用分块的方式处理这个图像旋转

8$\times$8分块：

```c
#define X 8
char rotate_descr[] = "8*8分块";
void rotate(int dim, pixel *src, pixel *dst) {
    for (int i = 0; i < dim; i += X)
        for (int j = 0; j < dim; j += X)
            for (int ii = i; ii < i + X; ii++)
                for (int jj = j; jj < j + X; jj++)
                    dst[RIDX(dim - 1 - jj, ii, dim)] = src[RIDX(ii, jj, dim)];
}
```

![image-20210818200614924](../../www/assets/pic/image-20210818200614924.png)

可以看到有性能提升

---

如果进一步加大分块大小

```c
#define X 16
char rotate_descr[] = "16*16分块_";
void rotate(int dim, pixel *src, pixel *dst) {
    for (int i = 0; i < dim; i += X)
        for (int j = 0; j < dim; j += X)
            for (int ii = i; ii < i + X; ii++)
                for (int jj = j; jj < j + X; jj++)
                    dst[RIDX(dim - 1 - jj, ii, dim)] = src[RIDX(ii, jj, dim)];
}
```

![image-20210818201026245](../../www/assets/pic/image-20210818201026245.png)

还可以有性能提升

---

继续加大分块

```c
#define X 32
char rotate_descr[] = "32*32分块";
void rotate(int dim, pixel *src, pixel *dst) {
    for (int i = 0; i < dim; i += X)
        for (int j = 0; j < dim; j += X)
            for (int ii = i; ii < i + X; ii++)
                for (int jj = j; jj < j + X; jj++)
                    dst[RIDX(dim - 1 - jj, ii, dim)] = src[RIDX(ii, jj, dim)];
}
```

![image-20210818201219231](../../www/assets/pic/image-20210818201219231.png)

性能发生了下降，应该是cache容量不足导致的

---

选择上面性能最好的$16\times 16$分块测试，进一步减少其中的冗余计算

```c
#define X 16
char rotate_descr[] = "16*16分块 展开内层循环";
void rotate(int dim, pixel *src, pixel *dst) {
    for (int i = 0; i < dim; i += X)
        for (int j = 0; j < dim; j += X)
            for (int ii = i; ii < i + X; ii++) {
                dst[RIDX(dim - 1 - j, ii, dim)] = src[RIDX(ii, j, dim)];
                dst[RIDX(dim - 2 - j, ii, dim)] = src[RIDX(ii, j + 1, dim)];
                dst[RIDX(dim - 3 - j, ii, dim)] = src[RIDX(ii, j + 2, dim)];
                dst[RIDX(dim - 4 - j, ii, dim)] = src[RIDX(ii, j + 3, dim)];
                dst[RIDX(dim - 5 - j, ii, dim)] = src[RIDX(ii, j + 4, dim)];
                dst[RIDX(dim - 6 - j, ii, dim)] = src[RIDX(ii, j + 5, dim)];
                dst[RIDX(dim - 7 - j, ii, dim)] = src[RIDX(ii, j + 6, dim)];
                dst[RIDX(dim - 8 - j, ii, dim)] = src[RIDX(ii, j + 7, dim)];
                dst[RIDX(dim - 9 - j, ii, dim)] = src[RIDX(ii, j + 8, dim)];
                dst[RIDX(dim - 10 - j, ii, dim)] = src[RIDX(ii, j + 9, dim)];
                dst[RIDX(dim - 11 - j, ii, dim)] = src[RIDX(ii, j + 10, dim)];
                dst[RIDX(dim - 12 - j, ii, dim)] = src[RIDX(ii, j + 11, dim)];
                dst[RIDX(dim - 13 - j, ii, dim)] = src[RIDX(ii, j + 12, dim)];
                dst[RIDX(dim - 14 - j, ii, dim)] = src[RIDX(ii, j + 13, dim)];
                dst[RIDX(dim - 15 - j, ii, dim)] = src[RIDX(ii, j + 14, dim)];
                dst[RIDX(dim - 16 - j, ii, dim)] = src[RIDX(ii, j + 15, dim)];
            }
}
```

![image-20210818202641219](../../www/assets/pic/image-20210818202641219.png)

仅仅展开最内层的循环，运行效率竟然下降了！猜测原因是这个循环展开并没有带来很大的收益，但是却破坏了步长为1的循环，导致编译器的优化和CPU的指令预取机制都无法高效工作，拉低了整体效率

---

用中间变量减少16*16分块的冗余计算

```c
#define X 16
char rotate_descr[] = "16*16分块";
void rotate(int dim, pixel *src, pixel *dst) {
    int tmp0 = dim - 1;
    for (int i = 0; i < dim; i += X)
        for (int j = 0; j < dim; j += X)
            for (int ii = i; ii < i + X; ii++)
                for (int jj = j; jj < j + X; jj++)
                    dst[RIDX(tmp0 - jj, ii, dim)] = src[RIDX(ii, jj, dim)];
}
```

![image-20210819103713715](../../www/assets/pic/image-20210819103713715.png)

**上面这种方法就是我目前找到的最优解**

## 图像平滑

单纯的分块并没有效果，甚至反而降低了效率

```c
char smooth_descr[] = "smooth: 16*16分块";
void smooth(int dim, pixel *src, pixel *dst) {
    for (int i = 0; i < dim; i += 16)
        for (int j = 0; j < dim; j += 16)
            for (int ii = 0; ii < 16; ii++)
                for (int jj = 0; jj < 16; jj++)
                    dst[RIDX(i + ii, j + jj, dim)] =
                        avg(dim, i + ii, j + jj, src);
}
```

![image-20210819104013455](../../www/assets/pic/image-20210819104013455.png)

---

```c
char smooth_descr[] = "smooth: 分类讨论，减少冗余计算";
#define d_r4(x, r1, r2, r3, r4)                                               \
    dst[x].blue =                                                             \
        (src[r1].blue + src[r2].blue + src[r3].blue + src[r4].blue) / 4;      \
    dst[x].red = (src[r1].red + src[r2].red + src[r3].red + src[r4].red) / 4; \
    dst[x].green =                                                            \
        (src[r1].green + src[r2].green + src[r3].green + src[r4].green) / 4
#define d_r6(x, r1, r2, r3, r4, r5, r6)                                        \
    dst[x].blue = (src[r1].blue + src[r2].blue + src[r3].blue + src[r4].blue + \
                   src[r5].blue + src[r6].blue) /                              \
                  6;                                                           \
    dst[x].red = (src[r1].red + src[r2].red + src[r3].red + src[r4].red +      \
                  src[r5].red + src[r6].red) /                                 \
                 6;                                                            \
    dst[x].green = (src[r1].green + src[r2].green + src[r3].green +            \
                    src[r4].green + src[r5].green + src[r6].green) /           \
                   6
#define d_r9(x, r1, r2, r3, r4, r5, r6, r7, r8, r9)                            \
    dst[x].blue = (src[r1].blue + src[r2].blue + src[r3].blue + src[r4].blue + \
                   src[r5].blue + src[r6].blue + src[r7].blue + src[r8].blue + \
                   src[r9].blue) /                                             \
                  9;                                                           \
    dst[x].red =                                                               \
        (src[r1].red + src[r2].red + src[r3].red + src[r4].red + src[r5].red + \
         src[r6].red + src[r7].red + src[r8].red + src[r9].red) /              \
        9;                                                                     \
    dst[x].green = (src[r1].green + src[r2].green + src[r3].green +            \
                    src[r4].green + src[r5].green + src[r6].green +            \
                    src[r7].green + src[r8].green + src[r9].green) /           \
                   9

void smooth(int dim, pixel *src, pixel *dst) {
    // 4个顶点
    int i, j;
    d_r4(0, 0, 1, dim, dim + 1);
    d_r4(dim - 1, dim - 1, dim - 2, dim + dim - 1, dim + dim - 2);
    int tmp = dim * (dim - 1);
    d_r4(tmp, tmp, tmp + 1, tmp - dim, tmp - dim + 1);
    tmp = dim * dim - 1;
    d_r4(tmp, tmp, tmp - 1, tmp - dim, tmp - dim - 1);
    // 除顶点外的4条边
    // 上方横边
    for (i = 1; i < dim - 1; i++) {
        d_r6(i, i, i - 1, i + 1, i + dim, i + dim - 1, i + dim + 1);
    }
    // 下方横边
    for (i = dim * (dim - 1) + 1; i < dim * dim - 1; i++) {
        d_r6(i, i, i - 1, i + 1, i - dim, i - dim - 1, i - dim + 1);
    }
    // 左侧纵边
    for (j = dim; j < dim * (dim - 1); j += dim) {
        d_r6(j, j, j + 1, j - dim, j - dim + 1, j + dim, j + dim + 1);
    }
    // 右侧纵边
    for (j = 2 * dim - 1; j < dim * dim - 1; j += dim) {
        d_r6(j, j, j - 1, j - dim, j - dim - 1, j + dim, j + dim - 1);
    }
    // 中间所有像素，每一个都可以在一个9*9多的块中计算平均值
    for (i = 1; i < dim - 1; i++) {
        for (j = 1; j < dim - 1; j++) {
            tmp = i * dim + j;
            d_r9(tmp, tmp, tmp - 1, tmp + 1, tmp - dim, tmp - dim - 1,
                 tmp - dim + 1, tmp + dim, tmp + dim - 1, tmp + dim + 1);
        }
    }
}
```

![image-20210819120856304](../../www/assets/pic/image-20210819120856304.png)

