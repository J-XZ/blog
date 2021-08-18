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

![image-20210818233459405](../../www/assets/pic/image-20210818233459405.png)

**上面这种方法就是我目前找到的最优解**

## 图像平滑