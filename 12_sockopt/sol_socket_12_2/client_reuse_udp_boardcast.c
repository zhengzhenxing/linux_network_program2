#include <unistd.h>
#include <stdio.h>
#include <signal.h>			// signal
#include <string.h>			// strlen strstr memset memcpy
#include <errno.h>			// errno
#include <fcntl.h>			// fcntl F_SETFL O_NONBLOCK

#include <netinet/in.h>		// 	socket  AF_INET SOCK_RAW  IPPROTO_ICMP
#include <net/if.h>			//  IFNAMSIZ   struct ifreq
#include <linux/sockios.h>	//  SIOCGIFADDR 网卡的IP地址  SIOCGIFBRDADDR网卡的广播地址
#include <arpa/inet.h>		// 	inet_ntoa(struct in_addr>>字符串) inet_addr(字符串>>长整型)
							//  struct in_addr 是 struct sockaddr_in的成员 sin_addr 类型
							//  struct sockaddr_in 是 struct sockaddr 在以太网的具体实现


#define IP_FOUND "IP_FOUND"       			/*IP发现命令*/
#define IP_FOUND_ACK "IP_FOUND_ACK"			/*IP发现应答命令*/
#define MCAST_PORT 18899
//#define IFNAME "eth0"
#define IFNAME "usb0"
// 在这个网卡(网络接口)发送出去的广播 不会在这个网卡(网络接口)上再获取回来
// 也就是网卡(网络接口)发送出去的广播 本网卡(网络接口)是不能接收到的
// 但是lo本地回环这个网络接口是可以获取到的

int main(int argc, char *argv[]){

	int ret = -1;
	int sock = -1;

	// 建立数据报套接字 只有UDP可以发送广播
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if( sock < 0 ){
		perror("create socket error");
		return -1 ;
	}

	struct ifreq ifr;
	strncpy(ifr.ifr_name,IFNAME,strlen(IFNAME));
	if(ioctl(sock,SIOCGIFBRDADDR,&ifr) == -1){
		perror("ioctl error"); // ioctl error: No such device 如果没有对应网络接口
		return -1 ;
	}

	struct sockaddr_in broadcast_addr;
	memcpy(&broadcast_addr, &ifr.ifr_broadaddr, sizeof(struct sockaddr_in ));
	broadcast_addr.sin_port = htons(MCAST_PORT);
	broadcast_addr.sin_family = AF_INET ;
	printf("broadcast to %s %d\n", inet_ntoa(broadcast_addr.sin_addr), MCAST_PORT );

	int so_broadcast = 1;
	ret = setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&so_broadcast,sizeof so_broadcast);

	fcntl(sock,F_SETFL, O_NONBLOCK);

	int times = 10;
	int i = 0;

#define BUFFER_LEN 32
	char buff[BUFFER_LEN];
	struct sockaddr_in from_addr;		/*服务器端地址*/

	fd_set readfd;
	int count = -1;
	for( i = 0 ; i < times ; i++ )
	{
		// 广播发送服务器地址请求
		printf("send broadcast now %s %d %d\n" ,inet_ntoa( broadcast_addr.sin_addr ),
									 	 	 	 ntohs( broadcast_addr.sin_port) ,
									 	 	 	 broadcast_addr.sin_family );
		ret = sendto(sock, IP_FOUND, strlen(IP_FOUND), 0,
							(struct sockaddr*)&broadcast_addr,
							sizeof(broadcast_addr)); // 往IP广播地址+UDP端口MCAST_PORT,发送广播
		if(ret == -1){
			perror("sendto error");
			sleep(5);
			continue;
		}
		FD_ZERO(&readfd);
		FD_SET(sock, &readfd);
		struct timeval timeout;
		timeout.tv_sec = 5;					/*超时时间2s*/
		timeout.tv_usec = 0;
		ret = select(sock+1, &readfd, NULL, NULL, &timeout);
		switch(ret)
		{
			case -1:
				perror("select error");
				break;
			case 0:
				printf("select time up\n");
				break;
			default:
				if( FD_ISSET( sock, &readfd ) ){
					int from_len = sizeof( struct sockaddr_in);
					count = recvfrom( sock, buff, BUFFER_LEN, MSG_TRUNC,
								( struct sockaddr*) &from_addr, &from_len );

					if( count > BUFFER_LEN){
						printf("消息过大 已被截断\n");
						buff[BUFFER_LEN - 1 ] = '\0';
					}else if(count == BUFFER_LEN) {
						buff[BUFFER_LEN - 1 ] = '\0';
					}else{
						buff[count] = '\0' ;
					}
					printf( "Recv msg is %s\n", buff );
				}
		}
	}
	close(sock);
	return 0 ;
}
