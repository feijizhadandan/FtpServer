#pragma once
#include<bits/stdc++.h>
#include<WinSock2.h>
// socket库
#pragma comment(lib, "ws2_32.lib")
#define SPORT 8888	// 服务器端口号
#define PACKET_SIZE (1024 - 4*sizeof(int) - 256)

// 定义标记
enum MSGTAG {
	MSG_FILENAME = 1,	// 传输的是文件名
	MSG_FILESIZE = 2,	// 传输的是文件大小
	MSG_READY_READ = 3,	// 准备接收文件
	MSG_SEND = 4,		// 发送文件
	MSG_SUCCESS = 5,		// 传输完成

	MSG_OPENFILE_ERROR = 6	// 查找文件失败
};

// 封装消息头：文件名，文件大小
#pragma pack(1)	// 结构体1字节对齐
struct Header {
	enum MSGTAG msgID;	//	当前消息标记类型
	// 再用结构体封装文件属性信息
	struct {
		int fileSize;
		char fileName[256];
	}fileInfo;

	struct {
		int nStart;	// 包的编号
		int nsize;	// 包的数据大小
		char buf[PACKET_SIZE];
	}fileData;
};
#pragma pack()

// 初始化socket库
bool initSocket();

// 关闭socket库
bool closeSocket();

// 监听客户端连接
void listenToClient();

// 处理消息
bool processMsg(SOCKET);

// 处理消息：获得文件大小
bool readFile(SOCKET, struct Header*);

// 处理消息：发送文件
bool sendFile(SOCKET, struct Header*);

// 接收文件：准备内存空间，准备接收
void readyRead(SOCKET, struct Header*);

// 接收文件：写文件
bool writeFile(SOCKET, struct Header*);