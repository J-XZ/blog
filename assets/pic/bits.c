/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-in-bool-context"
#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting if the shift amount
     is less than 0 or greater than 31.


EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implement floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants. You can use any arithmetic,
logical, or comparison operations on int or unsigned data.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operations (integer, logical,
     or comparison) that you are allowed to use for your implementation
     of the function.  The max operator count is checked by dlc.
     Note that assignment ('=') is not counted; you may use as many of
     these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
//1
/*
 * bitXor - x^y using only ~ and & 
 *   Example: bitXor(4, 5) = 1
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
int bitXor(int x, int y) {
    return ~(~(x & ~y) & ~(~x & y));
}

/*
 * tmin - return minimum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
    return 1 << 31;
}
//2
/*
 * isTmax - returns 1 if x is the maximum, two's complement number,
 *     and 0 otherwise 
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 1
 */
int isTmax(int x) {
    /// Tmax 的比特表示是以0开头，其余全是1
    /// 这个数字满足一个性质，它+1 == 它按位取反
    /// 还有另一个数字满足这个性质，它就是全1
    /// 只要排除所有比特位全是1的那种情况，就能判断Tmax
    int y = ~x;
    int x_add_1 = x + 1;
    /// 如果x所有比特位按位取反之后得到的数字 与 x+1 不等
    /// 下面这个变量就不为0
    int if_y_not_equal_x_add_1 = (x & x_add_1) | (y & ~x_add_1);
    /// 如果x的比特表示是全1
    /// 下面这个变量就不为0
    int if_y_not_equal_zero = !y;
    /// (如果x所有比特位按位取反之后得到的数字 == x+1) && (如果x的比特表示不是全1)
    /// !((如果x所有比特位按位取反之后得到的数字 != x+1) || (如果x的比特表示是全1))
    return !(if_y_not_equal_x_add_1 | if_y_not_equal_zero);
}

/*
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   where bits are numbered from 0 (least significant) to 31 (most significant)
 *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allOddBits(int x) {
    /// 下面这个数字只认为他的低16位有效
    int one_half_x = x & (x >> 16);
    /// 下面这个数字只认为他的低8位有效
    int one_quarter_x = one_half_x & (one_half_x >> 8);
    /// 下面这个数字只认为他的低4位有效
    int one_eighth_x = one_quarter_x & (one_quarter_x >> 4);
    /// 下面这个数字只认为他的低2位有效
    int one_sixteenth_x = one_eighth_x & (one_eighth_x >> 2);
    return 1 & (one_sixteenth_x >> 1);
}

/*
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
    ///所有比特位全部翻转 然后再加1
    return ~x + 1;
}
//3
/* 
 * isAsciiDigit - return 1 if 0x30 <= x <= 0x39 (ASCII codes for characters '0' to '9')
 *   Example: isAsciiDigit(0x35) = 1.
 *            isAsciiDigit(0x3a) = 0.
 *            isAsciiDigit(0x05) = 0.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 3
 */
int isAsciiDigit(int x) {
    /*
     from 0x30 to 0x39:
00000000000000000000000000110000
00000000000000000000000000110001
00000000000000000000000000110010
00000000000000000000000000110011
00000000000000000000000000110100
00000000000000000000000000110101
00000000000000000000000000110110
00000000000000000000000000110111
00000000000000000000000000111000
00000000000000000000000000111001
bit-----------------------543210
     最低6位命名为bit5到bit0
     */
    int high_all_0 = !(x >> 6);
    int bit5 = x >> 5;
    int bit4 = x >> 4;
    int bit3 = x >> 3;
    int bit2 = x >> 2;
    int bit1 = x >> 1;
    return (bit5 & bit4) & (~bit3 | (~bit2 & ~bit1)) & high_all_0;
}

/*
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z) {
    /// 如果x为0,mask就构造为全1,否则全0
    /// 如果x为0,!x为1,<<31再>>31变成全1
    /// 如果x非0,!x为0,<<31再>>31还是全0
    int mask = ((!x) << 31) >> 31;
    return (z & mask) | (y & (~mask));
}

/*
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y) {
    int if_x_y_same_sign = !((x ^ y) >> 31);
    int if_x_negative_y_positive = ((x & (~y)) >> 31) & 1;
    int minus_y = ~y + 1;
    int x_minus_y = x + minus_y;
    int x_minus_y_less_than_zero = !(~x_minus_y >> 31);
    int x_minus_y_equal_zero = !x_minus_y;
    return ((x_minus_y_equal_zero | x_minus_y_less_than_zero) & if_x_y_same_sign)
           | if_x_negative_y_positive;
}
//4
/* 
 * logicalNeg - implement the ! operator, using all of 
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int logicalNeg(int x) {
    /*
-5 11111111111111111111111111111011
-4 11111111111111111111111111111100
-3 11111111111111111111111111111101
-2 11111111111111111111111111111110
-1 11111111111111111111111111111111
0 00000000000000000000000000000000
1 00000000000000000000000000000001
2 00000000000000000000000000000010
3 00000000000000000000000000000011
4 00000000000000000000000000000100
5 00000000000000000000000000000101
    有且仅有0，+0和-0符号位总是0
     */
    x = (x | (~x + 1)) >> 31;
    /// 如果x为全1就返回0,否则返回1
    /// 对于每个比特位，如果是1，就返回0
    /// 如果是0,就按照0x1的比特位进行返回
    ///return (x & ~x) | (~x & 0x1);
    /// 把int之间的按位运算看做比特向量的并行计算
    return ~x & 0x1;
}

/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x) {
    /// 如果x<0，则为全1
    /// 如果x>=0,则为全0
    int if_x_negative = x >> 31;
    int x_r_16, x_r_8, x_r_4, x_r_2, x_r_1;
    int if_mask;
    int x_low_16, x_low_8, x_low_4, x_low_2;
    int b0, b1, b2, b3, b4;
    /// 如果x的符号位为1,那就翻转x的所有比特位
    x ^= if_x_negative;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x += 1;
    /*
x变成了一个形如00*****010*****的数字，最高位必为0
假设这个数字的比特从低位向高位编号，需要找到上述1所在的位置的编号
这个编号可用一个5位二进制原码表示
如果该编号最高位为1,则我们要找到1一定出现在高16位置
如果该编号次高位为1,则我们要找到1一定出现在高16位置
0 :00000
1 :00001
2 :00010
3 :00011
4 :00100
5 :00101
6 :00110
7 :00111
8 :01000
9 :01001
10:01010
11:01011
12:01100
13:01101
14:01110
15:01111
16:10000
17:10001
18:10010
19:10011
20:10100
21:10101
22:10110
23:10111
24:11000
25:11001
26:11010
27:11011
28:11100
29:11101
30:11110
31:11111
 */
//    if ((x >> 15) != 0) b4 = 1 << 4;
//    else b4 = 0;
    x_r_16 = (x >> 16);
    if_mask = ~(!(x_r_16 << 16) << 31 >> 31);
    b4 = if_mask & 16;
    x_low_16 = x | x_r_16;
    x_r_8 = x_low_16 >> 8;
    if_mask = ~(!(x_r_8 << 24) << 31 >> 31);
    b3 = if_mask & 8;
    x_low_8 = x_low_16 | x_r_8;
    x_r_4 = x_low_8 >> 4;
    if_mask = ~(!(x_r_4 << 28) << 31 >> 31);
    b2 = if_mask & 4;
    x_low_4 = x_low_8 | x_r_4;
    x_r_2 = x_low_4 >> 2;
    if_mask = ~(!(x_r_2 << 30) << 31 >> 31);
    b1 = if_mask & 2;
    x_low_2 = x_low_4 | x_r_2;
    x_r_1 = x_low_2 >> 1;
    if_mask = ~(!(x_r_1 << 31) << 31 >> 31);
    b0 = if_mask & 1;
    return b0 + b1 + b2 + b3 + b4 + 1;
}
//float
/* 
 * floatScale2 - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatScale2(unsigned uf) {
    unsigned sign = uf >> 31;
    unsigned exp = uf >> 23 & 0xff;
    unsigned frac = uf << 9 >> 9;
    int is_nan = (exp == 0xff) && (frac != 0);
    if (is_nan) {
        return uf;
    }
    if (exp == 0xff) {
        /// inf*2 = inf
        return uf;
    } else if (exp == 0) {
        /// 非规格浮点数，对于小数部分乘2
        frac <<= 1;
    } else {
        /// 指数+1
        exp += 1;
    }
    return sign << 31 | exp << 23 | frac;
}

/*
 * floatFloat2Int - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */

int floatFloat2Int(unsigned uf) {
    unsigned int sign = uf >> 31;
    unsigned exp = uf >> 23 & 0xff;
    unsigned frac = uf << 9 >> 9;
    /*
     对于规格化浮点数，其指数实际上是exp-bias
     其中bias = 2^(k-1)-1，
     k表示指数域的比特数
     float指数域有8位，则bias = 2^7-1 = 0x7f

     如果exp小于0x7f则说明这个规格化浮点数小于1

     对于非规格化浮点数，其指数实际值为1-bias
     */
    unsigned bias = 0x7f;
    int ans = 0;
    if (exp < bias) {
        ///如果指数小于0,则该浮点数的绝对值小于1
        return 0;
    } else if (exp >= 31 + bias) {
        /// 如果exp-bias >= 31，表示这个浮点数的值大于int能表示的最大范围
        return 0x80000000;
    } else {
        /// e是规格化浮点数实际使用的指数值
        int e = exp - bias;
        /// 显式补上规格化浮点数小数部分省略的前导1
        frac = frac | 0x800000;
        /// 下面就是将小数点向右移动e位
        /// 原本是1.xxx···xxx(共有23个x,就是浮点数的小数部分)
        if (e > 23) {
            ///如果小数点可以移动到最末端，那么先隐式地将小数点移到最右端，然后继续移动e-23位
            ans = frac << (e - 23);
        } else {
            ///如果小数点不够移动到最末端，那么先隐式地将小数点移到最右端，
            /// 然后右移上一步多移动的部分,相当于直接截断整型无法表示的小数
            ans = frac >> (23 - e);
        }
    }
    return (sign == 0 ? ans : -ans);
}

/*
 * floatPower2 - Return bit-level equivalent of the expression 2.0^x
 *   (2.0 raised to the power x) for any 32-bit integer x.
 *
 *   The unsigned value that is returned should have the identical bit
 *   representation as the single-precision floating-point number 2.0^x.
 *   If the result is too small to be represented as a denorm, return
 *   0. If too large, return +INF.
 * 
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. Also if, while 
 *   Max ops: 30 
 *   Rating: 4
 */
unsigned floatPower2(int x) {
    /// 补上规格化浮点数要减去的bias
    x = x+0x7f;
    if(x<=0){
        return 0;
    }else if(x>=0xff){
        return 0xff<<23;
    }
    return x<<23;
}

#pragma clang diagnostic pop