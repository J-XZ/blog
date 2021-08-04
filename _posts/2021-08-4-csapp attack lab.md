---
layout: post 
category: CSAPP 
---
---

当函数A调用函数B时，call指令会将返回地址push到栈顶，然后再将控制权交给函数B

push指令实际上是先将rsp寄存器的值减去要push的字节数，然后将要push的内容写入rsp指向的内存位置

![](../../www/assets/pic/attack%20lab/Notability%20Notes-1.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-2.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-3.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-4.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-5.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-6.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-7.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-8.png)
![](../../www/assets/pic/attack%20lab/Notability%20Notes-9.png)