---
layout: post 
category: CSAPP 
---
---

### 帮助函数

##### rio.cpp

```c++
#include "rio"
// RIO****************************************************************
/****************************
 * 以fd文件描述符号为源
 * usrbuf缓冲区为目的
 * 读取n个字符
 * 返回实际读取的字符数量，-1表示出错
 * 与系统read函数相同，但是可以自动处理不足值
 * ***************************/
ssize_t rio_readn(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;  //剩余要读的字符数
    size_t nread;
    char *bufp = (char *)usrbuf;
    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;
            else
                return -1;
        } else if (nread == 0)
            break;
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}
/****************************
 * 以usrbuf缓冲区为源
 * 以fd文件描述符为目的
 * 写入n个字符
 * 返回实际写入的字符数量，-1表示出错
 * 与系统write函数相同，但是可以自动处理不足值
 * ***************************/
ssize_t rio_writen(int fd, void *usrbuf, size_t n) {
    size_t nleft = n;
    size_t nwritten;
    char *bufp = (char *)usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }
        nleft -= nwritten;
        bufp += nwritten;
    }
    return n;
}

void rio_readinitb(rio_t *rp, int fd) {
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}
/****************************
 * 带有rio_t缓冲区的rio_read函数
 * *******************/
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n) {
    int cnt;
    while (rp->rio_cnt <= 0) {  // 如果缓冲区空了，就重新填满他
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
        if (rp->rio_cnt < 0) {  // 读取失败
            if (errno != EINTR) return -1;
        } else if (rp->rio_cnt == 0)  // 读到文件末尾
            return 0;
        else  //读取成功，重置rp->rio_bufptr
            rp->rio_bufptr = rp->rio_buf;
    }
    // 从缓冲区中复制min(n,rp->rio_cnt)个字节到用户提供的读缓冲区
    cnt = n;
    if (rp->rio_cnt < n) cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/*****************
 * rio_readlineb和rio_readnb都使用了rio_t缓冲区
 * 都是线程安全的
 * 并且这两个函数可以互相兼容
 ***************/
ssize_t rio_readlineb(rio_t *rp, char *usrbuf, size_t maxlen) {
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n') {
                n++;
                break;
            }
        } else if (rc == 0) {
            if (n == 1)
                return 0;
            else
                break;
        } else
            return -1;
    }
    *bufp = 0;
    return n - 1;
}

ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) {
    size_t nleft = n;
    ssize_t nread;
    char *bufp = (char *)usrbuf;
    while (nleft > 0) {
        if ((nread = rio_read(rp, bufp, nleft)) < 0)
            return -1;
        else if (nread == 0)
            break;
        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}
// RIO****************************************************************
```

##### rio

```c++
#ifndef __RIO__
#define __RIO__

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
// 定义rio_t缓冲区
#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;
    int rio_cnt;
    char *rio_bufptr;
    char rio_buf[RIO_BUFSIZE];
} rio_t;
typedef unsigned int uint;
typedef unsigned long ulong;

ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd);
ssize_t rio_readlineb(rio_t *rp, char *usrbuf, size_t maxlen);
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
#endif
```

##### internet.cpp

```c++
#include "internet"
/****************************
 * 套接字接口辅助函数
 * ***************************/
int open_clientfd(char *hostname, char *port) {
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM; /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV; /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG; /* Recommended for connections */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port,
                gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) <
            0)
            continue; /* Socket failed, try the next */

        /* Connect to the server */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break; /* Success */
        if (close(clientfd) < 0) {
            /* Connect failed, try another */  // line:netp:openclientfd:closefd
            fprintf(stderr, "open_clientfd: close failed: %s\n",
                    strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* All connects failed */
        return -1;
    else /* The last connect succeeded */
        return clientfd;
}

int open_listenfd(char *port) {
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval = 1;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port,
                gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) <
            0)
            continue; /* Socket failed, try the next */

        /* Eliminates "Address already in use" error from bind */
        setsockopt(listenfd, SOL_SOCKET,
                   SO_REUSEADDR,  // line:netp:csapp:setsockopt
                   (const void *)&optval, sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break;                 /* Success
                                    */
        if (close(listenfd) < 0) { /* Bind failed, try the next */
            fprintf(stderr, "open_listenfd close failed: %s\n",
                    strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* No address worked */
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0) {
        close(listenfd);
        return -1;
    }
    return listenfd;
}
```

##### internet

```c++
#ifndef __INTERNET__
#define __INTERNET__
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define MAXLINE 10240
#define LISTENQ 1024 /* Second argument to listen() */
/****************************
 * 建立与服务器的连接，该服务器运行在主机hostname上，并在端口号port上监听连接请求
 * 返回打开的套接字描述符
 * ***************************/
int open_clientfd(char* hostname, char* port);
/****************************
 * 打开和返回一个监听描述符
 * 准备好在端口port上接收连接请求
 * ***************************/
int open_listenfd(char* port);
#endif
```

---

### <span id="jump">Part 1 串行代理</span>

```c++
// 串行版本的代理服务器
#include "internet"
#include "iostream"
#include "rio"
#include "string"
using namespace std;
int parse_uri(char* uri, char* host, char* port, char* path);
void do_proxy(int fd);
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char* argv[]) {
    printf("正在启动代理\n");
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {  //不指定端口号参数就退出
        fprintf(stderr, "usage:%s <port>\n", argv[0]);
        return 1;
    }

    listenfd = -1;
    int times = 0;
    while (listenfd < 0) {
        listenfd = open_listenfd(argv[1]);
        times++;
        if (times == 1000) {
            printf("打开监听描述符时出现错误\n");
            return 1;
        }
    }
    while (1) {
        printf("正在监听连接请求\n");
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        cout << "连接成功" << endl;
        if (connfd < 0) {
            printf("未能成功与客户端建立连接,正在重试\n");
            continue;
        }
        if (getnameinfo((struct sockaddr*)&clientaddr, clientlen, hostname,
                        MAXLINE, port, MAXLINE, 0) == 0) {
            printf("从(%s %s)建立连接\n", hostname, port);
        }
        do_proxy(connfd);
        times = 0;
        while (close(connfd) < 0) {
            times++;
            if (times == 1000) {
                printf("关闭连接时出错");
                return 0;
            }
        }
    }

    return 0;
}

/* 为文件描述符fd对应的连接完成一次http代理 */
void do_proxy(int fd) {
    cout << "正在处理代理" << endl;
    int is_static;
    struct stat sbuf;  // 描述文件系统中文件属性的结构
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio;
    rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, MAXLINE) > 0) {
        cout << "收到请求：" << endl;
        cout << buf;
    }
    sscanf(buf, "%s %s %s", method, uri, version);
    cout << "method: " << method << " url: " << uri << " version: " << version
         << endl;
    if (strcmp(method, "GET") == 0 && strlen(uri) > 0 &&
        (strcmp(version, "HTTP/1.0") == 0 ||
         strcmp(version, "HTTP/1.1") == 0)) {
        strcpy(version, "HTTP/1.0");
    } else {
        cout << "收到的请求不合法" << endl;
        return;
    }

    // 解析来自客户端的http get请求的第一行
    char hostname[MAXLINE];
    char path[MAXLINE];
    char port[MAXLINE];
    parse_uri(uri, hostname, port, path);
    cout << "hostname = " << hostname << endl;
    cout << "path = " << path << endl;
    cout << "port = " << port << endl;

    // 按照实验pdf的要求，将来自客户端的简易http请求修改部分内容
    // HTTP请求的格式：
    /***********************************
     * 请求方法 URL 协议版本 \r\n
     * 头部字段名1:值 \r\n
     * 头部字段名2:值 \r\n
     * 头部字段名3:值 \r\n
     *  ··· ···
     * \r\n
     * 请求正文
     * ********************************
     * 其中头部字段名包括：
     * Host: 接受请求的服务器地址，可以是ip:端口号，或者是域名
     * User-Agent: 发送请求的应用程序名称
     * Connection: 指定与连接相关的属性，比如是不是长连接
     * Accept-Charset: 通知服务器端可以发送的编码格式
     * Accept-Encoding: 通知服务器可以发送到数据压缩格式
     * Accept-Language: 通知服务器可以发送的语言
     * ··· ···
     * ********************************
     * 实验要求
     * 必须有host字段
     * User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305
     *      Firefox/10.0.3 Connection:close
     * Proxy-Connection:close
     * **************/
    // 解析来自客户端的http请求的更多行，并按要求设置一些数据
    string http_request_head = "GET " + string(path) + " " + version + "\r\n";
    bool have_host = false, have_user_agent = false,
         have_proxy_connection = false;
    while (rio_readlineb(&rio, buf, MAXLINE) > 0) {
        cout << "@" << endl;
        if (strcmp(buf, "\r\n") == 0) break;
        string line = buf;
        if (line.find("Host") == 0) {
            have_host = true;
        } else if (line.find("User-Agent") == 0) {
            have_user_agent = true;
            line = user_agent_hdr;
        } else if (line.find("Proxy-Connection") == 0) {
            have_proxy_connection = true;
            line = "Proxy-Connection: close";
        } else if (line.find("\r") == 0) {
            continue;
        }
        if (line[line.length() - 1] == '\r')
            line += "\n";
        else
            line += "\r\n";
        http_request_head += line;
    }
    if (!have_host) {
        http_request_head += "Host: " + string(hostname) + "\r\n";
    }
    if (!have_proxy_connection) {
        http_request_head += "Proxy-Connection: close\r\n";
    }
    if (!have_user_agent) {
        http_request_head += string(user_agent_hdr) + "\r\n";
    }
    http_request_head += "\r\n";  //最后一个空行表示http请求头的结束
    cout << endl << "完整的http请求头为：" << endl;
    cout << http_request_head << endl;
    // 到这里完整的http请求构造完成

    //向目标主机发起连接
    int server_fd = 0;
    int times = 0;
    while (server_fd <= 0) {
        times++;
        server_fd = open_clientfd(&hostname[0], port);
        if (times == 10) {
            cout << "连接目标主机时出错" << endl;
            close(server_fd);
            return;
        }
    }
    cout << "向目标主机" << hostname << " " << port << "发起连接成功" << endl;
    rio_t server_rio;
    rio_readinitb(&server_rio, server_fd);
    rio_writen(server_fd, &http_request_head[0], http_request_head.length());
    int len;
    while ((len = rio_readlineb(&server_rio, buf, MAXLINE)) > 0) {
        times = 0;
        while (rio_writen(fd, buf, len) < 0) {
            times++;
            if (times == 1000) {
                cout << "向请求申请端写回请求结果时出错" << endl;
                close(server_fd);
                return;
            }
        }
    }
    close(server_fd);
}

int parse_uri(char* uri, char* host, char* port, char* path) {
    const char* ptr;
    const char* tmp;
    char scheme[10];
    int len;
    int i;

    ptr = uri;

    tmp = strchr(ptr, ':');
    if (NULL == tmp) return 0;

    len = tmp - ptr;
    (void)strncpy(scheme, ptr, len);
    scheme[len] = '\0';
    for (i = 0; i < len; i++) scheme[i] = tolower(scheme[i]);
    if (strcasecmp(scheme, "http")) return 0;
    tmp++;
    ptr = tmp;

    for (i = 0; i < 2; i++) {
        if ('/' != *ptr) {
            return 0;
        }
        ptr++;
    }

    tmp = ptr;
    while ('\0' != *tmp) {
        if (':' == *tmp || '/' == *tmp) break;
        tmp++;
    }
    len = tmp - ptr;
    (void)strncpy(host, ptr, len);
    host[len] = '\0';

    ptr = tmp;

    if (':' == *ptr) {
        ptr++;
        tmp = ptr;
        /* read port */
        while ('\0' != *tmp && '/' != *tmp) tmp++;
        len = tmp - ptr;
        (void)strncpy(port, ptr, len);
        port[len] = '\0';
        ptr = tmp;
    } else {
        port = "80";
    }

    if ('\0' == *ptr) {
        strcpy(path, "/");
        return 1;
    }

    tmp = ptr;
    while ('\0' != *tmp) tmp++;
    len = tmp - ptr;
    (void)strncpy(path, ptr, len);
    path[len] = '\0';

    return 1;
}
```

![image-20210827103057456](../../www/assets/pic/image-20210827103057456.png)

---

### Part 2

评分脚本有bug，driver.sh第301行开头需要加上python3

![image-20210827105953919](../../www/assets/pic/image-20210827105953919.png)

否则系统可能会找不到python解释器路径

这小问只需要将[Part 1](#Part 1 串行代理)中的

