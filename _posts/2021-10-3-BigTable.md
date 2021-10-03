---
layout: post 
category: papers 
---
---
bigtable是一个稀疏的、分布式的、持久化存储的多维排序Map

他的映射是

(row: string, column: string, time: int64) -> string

行名和列名定义表格中的一个位置，每个位置可以存储多份数据，数据之间通过时间戳进行区分。用户可以选择只保留最后几个版本的数据或者只保留足够新的数据

对同一行的操作是原子的

通过行关键字的字典序组织数据

列关键字通过列族限定列的内容

![image-20211003121318745](../../www/assets/pic/image-20211003121318745.png)

BigTable使用Google的分布式文姬爱你系统（<font color="red">GFS</font>）存储日志文件和数据文件。BigTable集群通常运行在一个共享的机器池中。

BigTable内部存储数据的文件是Google SSTable格式的，SSTable是一个持久化的、排序的、不可更改的Map结构（键-值对）

可以对SSTable执行如下的操作：查询与一个key相关的value，或者遍历某个key值范围内的所有的k-v对。从内部看，SSTable是一系列的数据块（大小可配置，通常64KB）SSTable使用块索引（通常储存在SSTable的最后）来定位数据块；在打开SSTable的时候，索引被加载到内存。每次查找可以通过一次磁盘搜索完成：首先使用二分法在内存中的索引理找到数据块的位置，然后再从磁盘读取相应的数据块。也可以把整个SSTable都放在内存中，就不必访问磁盘。

BigTable依赖一个高可用、持久性的分布式锁服务<font color="red">Chubby</font>。一个Chubby服务包含五个动态副本，其中一个被选为主副本对外提供服务。当大部分副本处于运行状态并能够彼此通信时，这个服务就是可用的。Chubby使用Paxos算法来使他的副本再失败时保持一致性。

BigTable使用Chubby来完成许多任务

- 保证每个时间点只有一个主副本是活跃的
- 存储BigTable数据自引导的位置
- 来发现tablet服务器，包括tablet服务器加入和下线
- 存储BigTable模式信息（即每个表的列族信息）
- 存储访问控制表

---

主要功能组件

- 客户端库 直接和Tablet服务器通信（方法是读取chunny的tablet目录，获取所有tablet的元数据信息），不经过master
- 主服务器（一个）
  - 子表分配 为tablet服务器分配tablet
  - 子表服务器状态监控 检查新加入或者过期失效的Tablet服务器
  - 字表服务器之间的负载均衡
  - GFS文件的垃圾回收
  - 模式修改（建立表和列簇）
- 子表服务器（多个）
  - 一个子表服务器包含一个Tablet集合，每个Tablet包含某个范围内的所有行数据
  - 初始化状态下，一个Tablet只有一个Tablet。随着数据量的增加，会被自动分割程多个Tablet，默认情况下，每个Tablet的大小阈值为100M~200M

![image-20211003152201406](../../www/assets/pic/image-20211003152201406.png)

**tablet实现**

![image-20211003153029230](../../www/assets/pic/image-20211003153029230.png)

第一层：一个Chubby文件，该文件存储了*root tablet*的位置信息，由于该文件是Chubby文件，也就意味着，一旦Chubby服务不可用，整个BigTable就丢失了*root tablet*的位置，整个服务也就不可用了。

第二层：*root tablet*，*root tablet*其实就是元数据表`METADATA Table`的第一个Tablet，该Tablet中保存着元数据表其他Tablet的位置信息，*root tablet*很特殊，为了保证整个树的深度不变，*root tablet*从不分裂。

注意：对于元数据表`METADATA Table`来说，除了第一个特殊的Tablet来说，其余每个Tablet包含一组用户Tablet位置信息集合。

注意：`METADATA Table`存储Tablet位置信息时，`Row Key`是通过对`Tablet Table Identifier`和该Tablet的`End Row`生成的。

注意：每个`METADATA Table`的`Row Key`大约占用1KB的内存，一般情况下，配置`METADATA Table`的大小限制为128MB，也就是说，三层的定位模式大约可以寻址$2^{34}$个Tablets。

第三层：其他元数据表的Tablet，这些Tablet与*root tablet*共同构成整个元数据表。

![image-20211003152424503](../../www/assets/pic/image-20211003152424503.png)



