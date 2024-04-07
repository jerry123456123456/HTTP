#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<string.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define CONNETION_TYPE "Connection: close\r\n"
#define BUFFER_SIZE 4096
#define HTTP_VERSION "HTTP/1.1"

/*  现代网络编程中这种方式已经被废弃
char *host_to _ip(const char *hostname){
	struct hostent *host_entry=gethostbyname(hostname);
	
	if(host_entry){
		return inet_ntoa((struct in_addr*)*host_entry->h_addr_list);
	}
	return NULL;
}
*/


char *host_to_ip(const char *hostname) {
    struct addrinfo *result;  //用于存储地址信息的结构体指针
    struct addrinfo *res;
    int status;          //存储函数调用的返回状态
    char ipstr[INET6_ADDRSTRLEN];

    // 使用 getaddrinfo 函数获取主机信息
    // 传入主机名，服务名（默认服务），协议（默认协议），以及一个指向存储结果的结构体指针
    status = getaddrinfo(hostname, NULL, NULL, &result);
    //这个函数会返回一个result结构体指针，里面是链表形式的数据，而一个域名可能对应多个地址
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return NULL;
    }

    // 遍历结果，找到第一个合适的地址，虽然这里只执行了一次，但是为了提高代码的复用性，所以写循环
    for (res = result; res != NULL; res = res->ai_next) {
        void *addr;
        if (res->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
            addr = &(ipv4->sin_addr);
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        // 将地址转换为点分十进制字符串
        inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
        break; // 只获取第一个地址
    }

    freeaddrinfo(result); // 释放结果链表

    // 返回转换后的 IP 地址字符串
    return strdup(ipstr);
}

int http_create_socket(char *ip){
	int sockfd=socket(AF_INET,SOCK_STREAM,0);

	struct sockaddr_in sin={0};
	sin.sin_family=AF_INET;
	sin.sin_port=htons(80);
	sin.sin_addr.s_addr=inet_addr(ip);

	if(0!=connect(sockfd,(struct sockaddr *)&sin,sizeof(struct sockaddr_in))){
		return -1;
	}

	fcntl(sockfd,F_SETFL,O_NONBLOCK);
	return sockfd;
}

char* http_send_request(const char *hostname,const char *resource){
	char *ip=host_to_ip(hostname);
	int sockfd=http_create_socket(ip);

	char buffer[BUFFER_SIZE]={0};
	sprintf(buffer,
			"GET %s %s\r\n\
Host: %s\r\n\
%s\r\n\
\r\n",resource,HTTP_VERSION,hostname,CONNETION_TYPE);

	send(sockfd,buffer,strlen(buffer),0);

		//select 

	fd_set fdread;
	
	FD_ZERO(&fdread);
	FD_SET(sockfd, &fdread);

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	char *result = malloc(sizeof(int));
	memset(result, 0, sizeof(int));
	
	while (1) {

		int selection = select(sockfd+1, &fdread, NULL, NULL, &tv);
		if (!selection || !FD_ISSET(sockfd, &fdread)) {
			break;
		} else {

			memset(buffer, 0, BUFFER_SIZE);
			int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
			if (len == 0) { // disconnect
				break;
			}

			result = realloc(result, (strlen(result) + len + 1) * sizeof(char));
			strncat(result, buffer, len);
		}

	}

	return result;
}

int main(int argc,char *argv[]){
	if(argc<3){
		return -1;
	}
	char *response=http_send_request(argv[1],argv[2]);
	printf("response : %s\n",response);
	free(response);

	return 0;
}
