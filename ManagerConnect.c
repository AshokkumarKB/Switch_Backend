#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h> // exit
#include <unistd.h> // sleep
#include <poll.h>

#define MAX_DATA_SIZE  25+1
#define VALID(CDN,LBL) {if(!(CDN)){goto LBL;}}
#define PORT 8080
#define PROCS 5
#define RECVDATA  1000

/*typedef struct
{
	bool valid;
	int Start;
	int Length;
	int type;
	int Attribute;
}RiskMsgArr;

typedef struct
{
	RiskMsgArr MsgStr[129];
	unsigned char Message[RECVDATA+1];
	int Len;
	int pos;
}RiskMessage;*/

void dataValidation(char *Data);
int sockStartReader();
int sockCreate();
int sockBind(int sockfd,int port);
void sockListen(int sockfd,int procs);
int newConnect(int sock_fd);
int sockOptSet(int sockfd);
int managerStartRead(int socketFd,int timeout);
int sockClosedChk(int socket);
int Recv_Timeout(int socket,char *data,int maxsize,int waittime);
int reqValidationStart(char *data);
int sendClientData();

int main()
{
    char str[MAX_DATA_SIZE];
    memset(str,0,sizeof(str));
    int ret = -1;
    dataValidation(str);
    //printf("Your Data [%s]",str);
    ret = sockStartReader();
}

void dataValidation(char *Data)
{
    printf("Enter the data to send client\n");
    fgets(Data, MAX_DATA_SIZE, stdin);
    Data[strcspn(Data, "\n")] = '\0'; // Data[index_ret]='\0'
}

int sockStartReader()
{
    int sock_fd = 0;
    int newConn = 0;
    sock_fd = sockCreate();
    int ret = 0;
    int Timeout=10;
    if(sock_fd<0)
    {
        sleep(5);
        exit(0);
    }

    if(sockBind(sock_fd,PORT)<0)
    {
        sleep(5);
        exit(0);
    }
    //else
        //printf("Bind sucess\n");
    printf("SOCKET IN LISTEN MODE[%d] ON PORT[%d] ProcsID[%d]\n",sock_fd,PORT,getpid());

    sockListen(sock_fd,PROCS);
    for(;;)
    {
        newConn = newConnect(sock_fd);
        if(newConn > 0)
        {
            printf("Connection established\n");
            ret = managerStartRead(newConn,Timeout);
            if(ret==-1)
            {
                printf("Error in fork\n");
                close(newConn);
                shutdown(newConn,SHUT_WR);
                shutdown(newConn,SHUT_RDWR);
                sockClosedChk(newConn);
            }
            else
            {
                close(newConn);
            }
        }
    }
}

int sockCreate()
{
    int sockfd = 0;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd<0)
    {
        perror("Socket Connection Failed from Server\n");
        return -1;
    }
    else
    {
        printf("Socket connected ret[%d]\n",sockfd);
    }
    return sockfd;
}

int sockBind(int sockfd,int port)
{
    int ret = 0;
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(port);

    ret = bind(sockfd,(struct sockaddr *)&address,sizeof(address)); // Bind
    return ret;
}

void sockListen(int sockfd,int procs)
{
    listen(sockfd,procs);
}

int newConnect(int sock_fd)
{
    int ret = 0;
    struct sockaddr_in cli_addr;
	int newsockfd, clilen;
	char ip[30];
	struct linger l;
    clilen = sizeof(cli_addr);
    newsockfd = accept(sock_fd, (struct sockaddr *) &cli_addr,&clilen);
    if(newsockfd < 0)
    {
        perror("server: accept error\n");
        exit(0);
    }
    l.l_onoff = 1; 
    l.l_linger = 0;

    ret = sockOptSet(newsockfd);
    memset(ip,0,sizeof(ip));
    inet_ntop(AF_INET, &(cli_addr.sin_addr), ip, 20);
    return newsockfd;
}

int sockOptSet(int sockfd)
{
    struct linger ling;
	int flag;
	int len=0;

	ling.l_onoff=1;
	ling.l_linger=0;
    setsockopt(sockfd,SOL_SOCKET,SO_LINGER,(char *)&ling,sizeof(ling));

    if((setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char *)&flag,sizeof(flag)))<0)
	{
		return -1;
	}
	flag=1;
	return 0;
}

int managerStartRead(int socketFd,int timeout)
{
    int ret = 0;
    int pidReader = fork();
    int Len = 0;
    char data[1000];
    memset(data,0,sizeof(data));

    if(pidReader < 0)
    {
        printf("%s() Error in Forking\n",__func__);
        ret = -1;
        return ret;
    }
    else if(pidReader > 0)
    {
        return pidReader;
    }

    Len = Recv_Timeout(socketFd,data,sizeof(data),timeout);
    if(Len <= 0)
    {
        goto label;
    }
    ret = reqValidationStart(data);

    if(ret <= 0)
    {
        goto label;
    }
    ret = sendClientData();

label:
    if(socketFd > 0)
    {
        close(socketFd);
		shutdown(socketFd,SHUT_WR);
		shutdown(socketFd,SHUT_RDWR);
		sockClosedChk(socketFd);
    }
}

int sockClosedChk(int socket)
{
    char data[10+1];
	int i = 0;
	int Bytes = 0;
	while(i < 20)
	{
		memset(data,0,sizeof(data));
		if(Bytes = recv(socket,data,10,0) <= 0)
		{
				shutdown(socket, 2);
				close(socket);
				break;
		}
		else if(i==19)
		{
			shutdown(socket, 2);
			close(socket);
			break;
		}
		usleep(300000);
		i++;
	}
	return 0;
}

int Recv_Timeout(int socket,char *data,int maxsize,int waittime)
{
    struct pollfd fds[1];
    int ret = 0;
    int timeout = waittime;

    fds[0].fd = socket;
    fds[0].events = 0;
    fds[0].events |= POLLIN; // bitwise OR 

    while(poll(fds,1,timeout) > 0)
    {
        ret = recv(socket,data,maxsize,0);
        if(ret > 0)
        {
            if(ret == maxsize)
            {
                printf("Data Receieved [%s] Size Matched ret[%d]\n",data,ret);
            }
            else
            {
                printf("Data Receieved [%s] Size not Matched ret[%d]\n",data,ret);
            }
            return ret;  
            //timeout = 5000;
        }
        else
        {
            printf("Data Receive Failed..!\n");
            break;
        }
    }

    printf("Poll Ended\n");
    ret = -1;
    return ret;
}

int reqValidationStart(char *data)
{
    // Validation
    return 0;
}

int sendClientData()
{
    return 0;
}
