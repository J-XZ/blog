---
layout: post 
category: papers 
---
---
基准测试表明，PMem的顺序写延迟更接近DRAM的性能，而随机读的惩罚比预期的要高。这打破了之前研究建立的一个主要假设，即写是慢的，应该避免，读是快的，可以是随机的。<font color="grey">这个说法和HiKV的说法有矛盾</font>。

文章给出了三个核心设计：direct PMem writes, DIMM-aligned storage segments, and uniform thread-to-DIMM distribution

Viper consists of a **v**olatile **i**ndex and **per**sistent data

在快速DRAM中执行大多数随机操作，同时优化存储布局以高效写入PMem

---

持久内存(PMem)是一种新兴的存储设备，它在DRAM和基于闪存的存储之间架起了桥梁（从广义上看，PMen和闪存都是非易失性存储器，但是PMen和闪存的存储介质和读写粒度都不同，以Intel 傲腾内存为例子，傲腾内存使用的是3D Xpoint介质，传统固态硬盘使用的nand flash介质）。

intel对于其3D Xpoint介质硬盘和NAND Flash硬盘的对比

![低时延特性](../../www/assets/pic/welcome-to-the-storage-media-revolution-fig-2-1-1-1080x1080.png.rendition.intel.web.1072.603.png)

![与上一代英特尔® 傲腾™ 固态盘 DC P4800X 相比](../../www/assets/pic/welcome-to-the-storage-media-revolution-table-2-1-1-1080x1080.png.rendition.intel.web.1072.603.png)

![与以往固态盘比较图](../../www/assets/pic/welcome-to-the-storage-media-revolution-table-1-1-1-1080x1080.png.rendition.intel.web.1072.603.png)

![性能比较](../../www/assets/pic/welcome-to-the-storage-media-revolution-table-3-1-1-1080x1080.png.rendition.intel.web.1072.603.png)

![高耐用性意味着存储成本降低，停机时间缩短](../../www/assets/pic/welcome-to-the-storage-media-revolution-table-6-1-1-1080x1080.png.rendition.intel.web.1072.603.png)

![英特尔® 傲腾™ 固态盘 DC P5800X 系列规格](../../www/assets/pic/welcome-to-the-storage-media-revolution-table-7-1-1-1080x1080.png.rendition.intel.web.1072.603.png)

与三星980Pro（普通SSD中比较高级的）对比

![image-20211005101944104](../../www/assets/pic/image-20211005101944104.png)

---

对于每一个DIMM（双列直插式存储器模块）集成内存控制器(iMC)维护读和写挂起队列(RPQs和WPQs)来缓冲发送给iMC的请求。为了保证数据持久保存，iMC的WPQ是异步DRAM刷新（asynchronous DRAM refresh，ADR）的一部分。即使电源故障CPU缓存中的数据丢失，已经写入写挂起队列的写请求可以保证被写入PMem（因为iMC的两个队列是跟着DRAM一起刷新的，写到队列里面后续操作就不需要CPU干预了）。尽管iMC和DIMM以64字节（cache line的大小）进行通信，物理介质的访问却是以256字节的粒度进行的，所以DIMM片上的管理器会将小于256字节的请求放大到256字节进行处理，导致了读写放大。为了避免上述的问题，DIMM片上的管理器会使用写组合缓冲区合并写请求。

在App Direct模式下运行PMEM为用户提供了对PMEM的访问权限，而Memory Mode使用PMem作为DRAM的易失扩展，其中DRAM充当L4缓存，数据不是持久的。为了保证Viper中的持久性，我们使用App Direct Mode，并通过mmap将PMem映射到应用程序的虚拟内存空间，以利用其字节寻址能力。

PMem也支持通过DIMM交叉编码，非交叉模式下，两个盘独立工作，交叉模式下两个盘分摊读写请求（类似于raid 0），可以提高并行能力并增加吞吐量。在PMEM中，数据通常在4 kB块中交错，在所有六个可用DIMM中分配24 KB的每个连续块。本文的模型假设了一个交错的PMem系统

为了保证应用程序直接模式的持久性，用户必须通过使用，例如CLWB（缓存行回写）指令将缓存行刷新到PMEM。由于编译器和操作系统可以对指令重新排序以获得更好的性能，因此有必要显式地通过发出sfence指令来避免这种行为，该指令保证对PMem的写入已经完成，而不是重新排序。

尽管PMem的性能被假设为类似于DRAM，但较慢。作者使用32个线程执行高负载微基准测试，以获得服务器上KVS工作负载的准确延迟和带宽数据。

![image-20211005105316260](../../www/assets/pic/image-20211005105316260.png)

PMem的随机读延迟是DRAM的2.5倍，但是比SSD低三个数量级。顺序写的延迟基本和DRAM相同。随机写的延迟是DRAM的2.5倍，而且受到带宽的巨大影响。

所以，对于PMem的顺序写入是非常值得使用的，但是随机读取应该尽可能被限制在DRAM内使用。

---

三个核心设计

- **直接PMem写**。直接把所有数据写入PMem不经过DRAM缓冲的中转

- **Uniform thread-to-DIMM distribution**，通过给不同的存储器区域分配线程来最小化线程/DIMM比

- **DIMM-aligned storage segments**，Viper将数据存储在内存边界对齐的VPages中，以平衡内存竞争和并行性。更小的vpage会导致更多的线程访问相同的内存，而更大的vpage会导致单个线程访问多个内存，这两种情况都会导致线程与内存的比率更差，因此也更不利

  ---

使用<font color="red">CCEH</font>可扩展的散列方法，因此允许动态调整大小，而无需进行昂贵的全表重新散列。

Viper的主存储段称为VPages，其中包含单个键值记录以及一些元数据

假设一个系统每个插槽有6个DIMM存储条，按照交错模式使用所有的PMem，在交错模式下，数据被均摊到所有的存储器中，对于连续的24kb数据，六个存储器每个分摊4KB，我们利用这一点，将VPage向4KB对齐，这允许我们每个VPage准确地访问一个DIMM，从而在并行访问期间减少对DIMM的争用。

---

Viper系统的三个重要组件：持久化保存的VBlocks，VPagees和完全保存在内存中的Offset Map。

![image-20211005141924574](../../www/assets/pic/image-20211005141924574.png)

**vblock**本身不包含逻辑，只是作为VPages的分组来减少Viper中的记账开销。每个VPage是4kb(内存对齐)，包含一些元数据和存储在槽中的实际键值记录。他们是Viper的实际存储单元。为了支持更大的键值对，系统将VPage缩放为4KB的倍数，将VBlocks缩放为24KB的倍数，以确保对齐。

**Offset Map**存储在易失性内存中。插入一条记录时，首先将记录永久保存在VPage中，然后再将“键-存储位置”对存入偏移表。offset包含三个部分：VBlock id（45个比特）, VPage id（3比特）, record position in the VPage（16比特）。record position要分固定大小还是可变大小的记录。一个offset占用64比特。对于key太大的情况，使用哈希

**Metadata Management**

为了增长，Viper在PMem中分配vblock，并通过mmap将它们映射到虚拟内存空间。为了跟踪虚拟地址，Viper在DRAM的列表中存储了一个指向每个VBlock的指针，这允许通过隐式id（列表中的偏移量）轻松访问任意的VBlock。一旦可用的VBlocks达到了某种可配置的填充程度，Viper就会分配额外的VBlocks并将它们添加到列表中。元数据恢复和将所有数据映射回Viper的虚拟内存空间只需要几毫秒，因为它主要由mmap调用组成。

---

#### **固定大小的记录**

![image-20211005145004769](../../www/assets/pic/image-20211005145004769.png)

Viper将实际的键值记录存储在**VPages**中。键和值都存储在一个槽中。对于固定大小的记录，slot id用作Offset Map条目(记录位置)的第三部分。

**VPage Metadata**存储在VPage的前几个字节中。它由一个版本锁字节和一个bitset组成，该bitset指示哪些插槽是空闲的或已填充的。用锁字节控制堆VPage的并发数据修改。锁是通过原子比较和交换操作(CAS)获得和释放的，避免使用重量级的互斥对象。尽管有持久性CAS实现来确保正确的持久性语义，但Viper使用常规的内存中CAS操作，开销更小。锁只在活跃使用期间相关，并且在崩溃后重置。bitset中，置为1的位表示对应的slot已经被存放数据，置为0的数据表示slot为空

#### **可变大小的数据**

![image-20211005145915227](../../www/assets/pic/image-20211005145915227.png)

记录不是存储在固定的槽中，因为它们的大小是预先未知的。每条记录连同相应的键和值长度一起连续地写入日志。大小存储在一个32位值中(键值为15位，值为16位)，以允许原子更新。该值的最小有效位指示记录是被设置(= 1)还是被删除(= 0)。对于大小可变的记录，日志中的偏移量用作offset Map条目(记录位置)的第三部分。对于大于4 KB的键值对，Viper动态地将整个VBlock用作单个VPage。对于更大的记录，Viper跨多个大型VPages写入记录，并将这些记录标记为溢出页。因此，大的记录不会影响Viper的设计，因为每个客户端仍然有唯一的vblock，并且客户端线程平均分配到内存中。

**VPage 元数据**

由于VPage不包含任何插槽，空闲插槽bitset将被删除。相反，每个VPage现在包含一个指向它的下一个插入位置的指针，即日志的尾部，以及一个8位整数，用于跟踪大约有多少数据已经被删除(即，元数据位= 0)并需要压缩。对于变长记录，元数据大小固定为10字节，允许每个VPage有4086个字节的记录

---

Viper是一个嵌入式数据库，直接对客户端开放接口，客户端控制向哪一个VBlock的哪一个VPage写数据。同时刻没有两个客户端可以向同一个VBlock写数据，通过再第一个VPage的版本锁置比特位标记一个客户端占用这个VBlock

<font color="red">所谓一致性，是一个操作要么成功要么失败，索引中的值和存储器中的值应该是同一个版本（不能修改了PMem中的值却没有修改索引）</font>

具体的PUT GET UPDATE DELETE操作实现不写下来了，如果需要复现实验再仔细看。

---

**内存回收**

当各种记录被删除或被重新插入之后，VPage中会包含很多的空闲slot或者等待被删除的记录。为了重用这个空闲空间，Viper运行一个周期性的后台空间回收过程。在这种回收中，Viper扫描VPages的bitset以查看有多少空闲槽可用。如果VBlock中的空闲槽位数量高于可配置的阈值，并且该VBlock目前不属于客户端，则该VBlock被压缩成一个新的VBlock，标记为free，并添加到空闲块队列中。压缩VBlock相当于在该VBlock中重新插入每条记录。因此，当压缩许多vblock时，记录会再次被紧密地压缩。如果客户端读取当前正在压缩的记录，它要么读取过期的偏移量并重试，因为压缩的VPage的版本锁已经更改，要么读取新的偏移量。每个VPage在整个压缩期间都被锁定，以避免在整个过程中进行修改。

对于大小可变的记录，Viper检查每个VPage的元数据，以获得该页面上的近似空闲空间，如果VBlock达到一个可配置的阈值，它就会被压缩，就像在固定大小的进程中一样。在对VBlock进行压缩之后，它被标记为free，在它的第一个VPage的锁字节中有一个空闲位。这允许Viper在恢复期间识别空闲的VBlocks。这个过程还可以用来释放VBlock列表末尾的VBlock，从而在许多记录被删除后减少其PMem占用。
