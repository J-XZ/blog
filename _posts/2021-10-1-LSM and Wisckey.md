---
layout: post 
category: papers 
---
---
​		适合磁盘的索引通常有两种，一种是就地更新（B+Tree），一种是非就地更新（LSM）。就地更新的索引结构拥有最好的读性能（随机读和顺序读），而随机写性能很差，无法满足显示工业负载要求。（B+树适合读多写少的任务）而非就地更新数据结构LSM充分发挥顺序写入的高性能特性，成为写入密集数据系统的基础。

​		<font color="red">基本思想</font>：既然硬盘的特性决定了他随机写入的性能很差，那么就设法将随机写操作转化为顺序写。将所有键-值对一个接一个放到硬盘中(<font color="red">日志仅追加</font>)，因为每一个键-值对之间并没有关联，他们在硬盘中存放的顺序也没有关联，所以是“随机”的，但是这样在写入键-值对的时候对硬盘来说又是连续的。不需要涉及随机写入。如果需要修改前面已经写入的键值对，并不去随机读写，而是把修改后的键-值对按照日志仅追加的方式添加到日志尾部。这样就会导致一个key被写入多次的情况，会在磁盘中留下无效数据。这种情况会导致两种问题：浪费磁盘空间、导致读产生问题（必须for循环整个键-值对序列日志，才能找到最新的有效数据）。

​		上面这种简单的思路是为了满足写入密集型数据库的要求，但是他的读取性能会非常差。<font color="red">实际需求：支持很高的写吞吐量、不太高的读吞吐量、很低的读延迟</font>

​		为了解决修改键–值对时插入新日志导致旧日志失效，浪费存储空间且拉低读取效率的问题，一个思路是垃圾回收（GC），删除无效的键-值对。垃圾回收遇到的问题：系统无法确定当前的键是否是已经存在的键，解决方案：<font color="red">将日志分段</font>。（日志分段的另一个好处：文件系统对大文件缓存比较难以处理，将大文件分成小段可以方便操作）将日志文件分段之后，只有一个分段是活跃的文件（可以向其中写入），当写入的日志量达到阈值之后，就将它设置为一个不可写的文件，然后创建一个新的可以写入的文件（<font color="red">滚动地写入</font>）。那些不可变的文件就可以提交给垃圾回收进程进行清理。gc只要遍历这些文件，维护一个C++ map，可能还需要维护每条日志的写入顺序，然后按照顺序将所需的键-值对重新写入即可。通过这样的垃圾回收操作，可以保证垃圾回收后的日志和回收前的日志是等效的。

​		leveldb需要满足的操作：get、set、range（顺序读取，比如想要顺序获取当天24小时内的日志，思路是结合上面对日志的分段，使用归并排序，只需要将有重叠日志的文件进行归并，没有重叠日志的文件可以不归并）

​		那么如何优化读取性能呢？

​		思路1：<font color="red">利用内存</font>，将最近最新写入的kv存储在内存数据结构中（红黑树、**跳表**等）。leveldb使用的是跳表。首先在内存中用数据结构存储日志，当内存中的数据结构足够大的时候，将它整体dump到硬盘中形成一个日志文件。（不需要每次写入都调用硬盘，积累一批再调用硬盘）<font color="red">上面的方法带来的问题</font>：写入是连续的，而将内存中的数据结构dump到硬盘的过程可能没有新数据写入快，那么dump的过程是不是永远无法停止？需要<font color="red">在内存中维护两个跳表</font>：活跃跳表和不变跳表，活跃跳表接受新的写入请求，写入数据量达到阈值就将他转化为不可变跳表，然后这个不可变跳表会被dump进程dump到硬盘中。<font color="red">另一个问题</font>：数据写入到内存时成功了，但是还没有持久化。如果此时发生错误（机器故障），而内存又是易失存储，如何保证可靠性？使用<font color="red">预写日志</font>,使用日志仅追加并且没有其他任何优化的方式（预写日志没有必要保存其读取性能）暂存日志，如果发生系统故障，可以用预写日志恢复内存中丢失的数据。预写日志仅仅保留那些没有被写入到磁盘的数据。<font color="red">还有一个问题</font>：内存大小有限，不能缓存所有数据，加入要读取的键是很老的键，还是需要遍历所有的日志文件。<font color="red">**将日志段文件分层分级**</font>:

### leveldb最核心的思想

这也是leveldb名称中level的由来。日志段归并的时候有最大key和最小key，从内存中直接dump下来的文件，由于内存中的跳表是有序的，所以dump到硬盘中也是有序的（文件从出生开始就是有序的），但是由于写入是随机的，文件之间可能有重叠，所以需要归并，定义从来没有被归并的文件是第零层归并的文件，归并一次之后的文件是第一次归并的文件，归并之后可以保证**所有的第一次归并的文件一定不会有重叠的情况出现**因为这些文件在归并过程中已经把所有重复文件处理掉了。随着归并的文件数量增加，文件会越来越大。再对文件进行第二层的归并。我们需要考虑到当第零层归并文件逐渐被归并到第一层的文件中时，还会有新的文件从内存中被dump到第零层归并文件。也就是潜在的日志条目交叉会不断发生，需要不断地进行归并。leveldb就要执行这样一个将日志文件一层一层不断归并的过程。按照归并的次数对文件进行分层。

​		分多层的好处是：除了第零层的日志文件之外，每一层的文件互相之间不会有重复覆盖的情况发生。当我们需要找到特定的key时，只需要根据key值的范围找到某一层中对应的文件，查找该文件中是否有这个key，如果这个文件中没找到，那么这一层的其他文件中也不会找到这个key。（每一层中只需要遍历一个文件，且层数是有限的，那么需要遍历的文件数也是有限的）。第零层比较特殊，因为他是直接从内存dump得到的，无法保证文件之间key的范围没有重叠，所以必须遍历第零层的所有文件。解决方案是限制第零层中文件的数目，一旦第零层中的文件数目大于规定的数目，就触发一次merge操作。通过以上方法，可以保证读取一个特定的key时，需要遍历的文件总是常数数量级的。

​		随着归并次数增多，每一层的日志文件大小会增加，每一级会比上一级大一个数量级的文件大小。一般C0文件定义为2M。高层文件难以被一次性加入到内存，因此需要一定的磁盘索引机制。我们对每个磁盘文件进行布局设计，分为元数据块、索引块、数据块三大块。元数据块中存储布隆过滤器（这种过滤器判定不存在的话就一定不存在，那么就可以忽略这个文件）快速判断这个文件中是否存在某个key，同时通过对排序索引（通常缓存在内存中）二分查找定位key所在磁盘的位置。进而加速读取的速度，我们叫这种数据文件为SSTABLE（字符串排序表）。

---

​		LSMtree的缺点：非就地更新的索引结构牺牲了读取性能和空间利用率以换取顺序写入性能。

​		<font color="red">对LSMtree的优化重点在于提高读取性能和空间利用率</font>。

​		![image-20211002093743860](../../www/assets/pic/image-20211002093743860.png)

**优化读放大及合并压缩**

写放大：因为多次归并，每一个key可能需要被写多次。读放大：部分key可能需要读取很多份磁盘文件才能得到。

跳跃合并：假设L0层有一个key，但是L1和L2都没有重叠的key，那么可以直接把这个key从L0层直接合并到L3层。可以减少几次逐级合并的过程。（跨级合并）

冷热分离：把高热的数据缓存到内存中，减少读写放大。

并行化io调度：并行地写入数据或者并行地归并数据。（但是会占用磁盘更多的ops，如何通过提高并行性将磁盘带宽push到上限）

**更丰富的功能**

二级索引：只有k-v索引可能比较慢，可以基于value中的某一些字段构建索引

原子性事务：对一组key同时更新，要么全部成功，要么全部失败（比如转账）

一致性快照：生成一个增量的快照，不允许某些数据在读取过程中（快照生效的过程中）被更改

**自动调参**

leveldb的分层多少层比较合适？每个日志文件多大比较合适？每个布隆过滤器占用多大的内存空间才可以减少误差率呢？

读、写性能和空间占用参数之间是互相矛盾的，无法静态得到一个完美的参数，需要根据上游负载情况动态调整。

数据放置位置：高热数据放在前层，低热数据放在底层

**兼容新硬件**

大内存场景，可以在内存中缓存更多的信息

多核并行，并行归并

<font color="red">SSD</font>

本地存储：抛弃文件系统，直接操作裸机的块存储或硬件磁盘驱动，自己去调用磁盘存储，是否可以得到性能优化？

---

## <font color="red">KV分离对于SSD的优化</font>

​		SSD的随机读写性能比传统磁盘好，但是有使用寿命的限制

​		在SSD上使用LSM 技术带来两个问题：写放大（多次归并）缩短了SSD的使用寿命，传统的LSM无法充分发挥SSD的并行读写特性

​		相较于机械硬盘，随机读取和顺序读取的性能对SSD来说没有那么明显的差距，所以过分追求顺序读取而进行的激进优化是没有必要的，其中最明显的是将key和value放在一个文件内（sstable），在机械硬盘上这种结构可以保证一次读取就能获取数据，减少磁头寻道的次数，而对于固态硬盘，这样做会导致压缩合并过程中，对同一对kv进行多次写入和移动，进而降低了SSD的寿命。虽然也是读取一次，但是对于SSD来说两次读取也要快于机械硬盘的一次读取，而为了有效减少LSM造成的读写放大，wisckey中将key和value分离存储，value仅存储在vlog中以仅追加的方式，而key存储在之前的lsm tree结构中。这样带来两个好处，第一不需要频繁移动value的值，所以写放大减少，第二lsm仅存储固定大小的key使得其存储占用变小（原本存放value的位置变成一个指向value的指针），在内存中可以同时存储更多的key进而提高了缓存key的数量，间接降低了读放大问题。

​		对于随机读请求，固态硬盘远高于机械硬盘。而对于范围查询，由于真正的value都存储在vlog中因此是无序的，所以进行范围查询就需要进行随机读取（先从lsm顺序杜群key再逐个随机读value），这必然造成性能的下降，传统的随机读取是串行的难以发挥固态硬盘并行随机读取的特性，因此在wisckey中根据迭代器的调用方法prev、next来从排序好的lsm中预读一定的key到内存中，加速随机的范围查询性能，这种预读取是异步进行的，充分发挥固态硬盘的并行读取性能。

#### kv分离带来的挑战

kv分离带来的问题：vlog文件会不断增大，如何合并才能使对性能的影响最小化？由于移动value的位置，lsm结构中维护的指向value位置的指针也需要更新。数据具体如何布局？vlog日志如何拆分？wisckey崩溃后如何恢复才能保证数据一致性呢？

![image-20211002103125352](../../www/assets/pic/image-20211002103125352.png)

vlog不仅作为value的存储文件，也作为预写日志的存储文件，当数据时同样需要读取值日志来进行恢复，所以vlog数据中需要存储额外的信息，需要将key也存储在里面

**随机查询（点查）**kv引擎以数据块为最小单元进行存储操作，而不是以kv作为最小单元

1、访问内存表中是否命中key，如果找到地址信息判断是在内存中还是在磁盘中（LRU缓存）<br>2、如果在内存中缓存了value就直接返回，否则去磁盘中查找，根据vlog的名字找到具体的vlog文件<br>3、根据offset定位字节的首地址，根据size读取内容并返回<br>4、基于一定的策略将整个vlog涉及的block缓存下来

**范围查询**

1、根据迭代器next还是prev判断，在游标之前读还是之后读<br>2、预先读取一定的key，交由底层的线程池异步的去ssd中获取数据<br>3、将异步结构（快照）缓存在内存中，等待迭代器的调用

所以如果不断地向一个方向读取（总是向next或总是向prev）会获得更好的性能，摇摆不定就无法发挥ssd的并行性能（机械硬盘随机读写性能太差，这种设计完全不适合机械硬盘）

**随机/顺序写入操作**

1、将set操作先写入vlog日志中（存储key的作用之一就是既作为预写日志又作为值日志）<br>2、将返回的地址信息写入lsm中返回（如果这一步失败，vlog中对应的信息就是无效的日志，后续会被垃圾回收机制清理）<br>3、返回写入成功

**合并压缩**（权衡读、写、空间三者之间的关系）跟垃圾回收关系很大

1、log分为多个文件，其中存在一个活跃的vlog文件，用于写入数据作为head地址<br>2、最先写入的日志在最后的vlog中，存在tail地址<br>

3、多个写入线程运行在head地址处追加日志<br>4、只有一个后台线程执行垃圾回收运行在尾部<br>5、每次选择一个vlog文件袋内容去lsm结构中随机查询（可并行化）将没有失效的key重新写入到head地址处重新追加到vlog中<br>6、然后更新lsm中这些key的新地址<br>7、释放老的vlog文件<br>8、这一过程中需要先将数据写入到新的vlog文件中后刷新到固态硬盘后异步更新索引<br>9、最后删除老文件，保证在此过程中进程崩溃导致数据丢失

垃圾回收线程会从尾部开始读取一块数据，将里面的k-v解析出来，逐一去lsmtree中判断当前的key是不是有效的key，如果是无效的key就清理掉，清理完无效的key之后，形成一块新的数据，将新的数据写回到head位置，然后需要对整个文件进行一次flash操作，将整个文件刷新到磁盘，然后修改尾部指针的位置，释放清理完毕的一块空间。这样做是为了防止在垃圾回收过程中发生数据库崩溃现象，如果数据库在写入过程中崩溃，需要保存那些数据写入成功、哪些数据写入失败，保证没有写入的日志不会丢失

<font color="red">gc导致数据移动，移动就要修改lsm tree中的指针</font>

**故障恢复**

1、在wisckey堆设计中预写日志就是值日志<br>2、因此引擎进程只需要定时保存head和tail的地址即可<br>3、数据库恢复时需要获取崩溃前最新的地址然后从tail到head将日志进行redo即可<br>4、同时为了保证一致性在查询key时要做必要的一致性检查（当前key如果在tail和head索引范围之外就忽略、当前位置上的值具有的key与查询到key不匹配则忽略、发生上述情况时，引擎直接返回错误信息）

#### 优化与改进

**写缓冲区**

为了减少系统调用次数，多个小key的写入会被缓存在内存中汇聚在一起统一地写入 lsm的sstable中，但会优先写入vlog中当做预写日志处理

**空间放大率**

数据存储的实际大小与逻辑上的大小比值用来衡量空间放大情况，对于固态硬盘来说其价格昂贵，降低空间放大率会大大的节省存储成本

**在线垃圾收集**

GC的过程会造成引擎的性能尖刺，通过并发的在线等分批次的进行GC操作来降低对前台读写性能的影响

**小value的存储**

经论文的测试数据value大于4KB时其读取性能才会有极大的提升，因此可以设置一个阈值，当value大于此阈值时才进行kv分离，而在此之间使用传统的lsm-tree模式