#include "ftpClient.h"
char recvBuf[1024]; // 用于接收消息的缓冲区
char *fileBuf;		// 存储文件内容
int fileSize;
char fileName[256];		// 保存文件名

int main() {

	initSocket();

	connectToServer();

	closeSocket();

	system("pause");
	return 0;
}

// 初始化socket库
bool initSocket() {

	WSADATA wsadata;
	// 初始化socket函数
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata)) {
		printf("WSAStartup error:%d\n", WSAGetLastError());
		return false;
	}
	return true;
}

// 关闭socket库
bool closeSocket() {
	if (0 != WSACleanup()) {
		printf("WSACleanup error:%d\n", WSAGetLastError());
		return false;
	}
	return true;
}

// 监听客户端连接
void connectToServer() {
	// 创建server socket套接字
	SOCKET serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	// 表示该套接字是IPV4协议，面向字节流服务，并使用TCP协议的
	if (INVALID_SOCKET == serverfd) {
		printf("socket faild:%d\n", WSAGetLastError());
		return;
	}

	// 绑定需要访问的服务器地址
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SPORT);	// 服务器进程端口号
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");	// 服务器的IP地址

	// 连接到服务器
	if (0 != connect(serverfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printf("connect faild:%d\n", WSAGetLastError());
		return;
	}

	int choose;
	printf("请输入功能: 1、下载文件 2、上传文件\n");
	scanf("%d", &choose);
	
	if (choose == 1) {
		getFileName(serverfd);
	}
	else if (choose == 2) {
		if (!sendFileInfo(serverfd))
			return;
	}
	else {
		printf("输入错误, 再见\n");
		return;
	}

	// 循环处理消息
	while (processMsg(serverfd)) {
		// Sleep(100);
	}
}

// 处理消息
bool processMsg(SOCKET serverfd) {

	recv(serverfd, recvBuf, 1024, 0);	// 接收文件大小消息
	struct Header* msg = (struct Header*)recvBuf;

	switch (msg->msgID)
	{
	case MSG_OPENFILE_ERROR:
		getFileName(serverfd);
		break;
	
	case MSG_FILESIZE:
		readyRead(serverfd, msg);
		break;

	case MSG_SEND:
		writeFile(serverfd, msg);
		break;	
	case MSG_SUCCESS:
		printf("传输成功! Goodbye\n");
		return false;
		break;
	
	case MSG_READY_READ:
		sendFile(serverfd, msg);
		break;
	}


	return 1;
}

void getFileName(SOCKET serverfd) {
	printf("请输入下载文件名: \n");
	char fileName[1024] = "";
	getchar();
	gets_s(fileName, 1024);	// 需要下载的文件名

	struct Header msgHeader = { MSG_FILENAME };
	strcpy(msgHeader.fileInfo.fileName, fileName);

	send(serverfd, (char*)&msgHeader, sizeof(struct Header) + 1, 0);
}

void readyRead(SOCKET serverfd, struct Header* msgHeader) {

	strcpy(fileName, msgHeader->fileInfo.fileName);

	// 准备内存
	fileSize = msgHeader->fileInfo.fileSize;
	fileBuf = (char*)calloc(fileSize + 1, sizeof(char));	// 动态申请空间
	// 给服务器发送 MSG_READY_READ
	if (fileBuf == NULL) {
		printf("空间申请失败\n");
	}
	else {
		struct Header msg;
		msg.msgID = MSG_READY_READ;
		Sleep(2000);
		if (SOCKET_ERROR == send(serverfd, (char*)&msg, sizeof(struct Header), 0)) {
			printf("send error:%d\n", WSAGetLastError());
			return;
		}
	}
	printf("下载的文件大小：%d    下载的文件名：%s\n", msgHeader->fileInfo.fileSize, msgHeader->fileInfo.fileName);
}

bool writeFile(SOCKET serverfd, struct Header* msgHeader) {
	
	if (fileBuf == NULL) {
		return false;
	}

	int nStart = msgHeader->fileData.nStart;
	int nsize = msgHeader->fileData.nsize;
	memcpy(fileBuf + nStart, msgHeader->fileData.buf, nsize);
	printf("接收中: --- %f%% ---\n", ((double)nStart + (double)nsize)/(double)fileSize * 100);
	if (nStart + nsize >= fileSize) {
		FILE* pwrite = fopen(fileName, "wb");
		if (pwrite == NULL) {
			printf("write file error..\n");
			return false;
		}
		fwrite(fileBuf, sizeof(char), fileSize, pwrite);
		fclose(pwrite);
		free(fileBuf);
		fileBuf = NULL;

		struct Header msg = { MSG_SUCCESS };
		send(serverfd, (char*)&msg, sizeof(struct Header), 0);
		printf("接收完毕!!\n");
	}
	return true;
}

bool sendFileInfo(SOCKET serverfd) {
	struct Header msg;
	msg.msgID = MSG_FILESIZE;

	char fileName[1024] = "";
	printf("请输入上传的文件名: \n");
	getchar();
	gets_s(fileName, 1024);	// 需要上传的文件名
	strcpy(msg.fileInfo.fileName, fileName);

	FILE* pread = fopen(msg.fileInfo.fileName, "rb");
	// 文件打开失败处理
	if (pread == NULL) {
		printf("未找到文件：[%s]...再见! \n", fileName);
		return false;
	}

	fseek(pread, 0, SEEK_END);
	fileSize = ftell(pread);
	fseek(pread, 0, SEEK_SET);

	char tfname[200] = { 0 }, text[100];
	// 将绝对路径进行分解
	_splitpath(msg.fileInfo.fileName, NULL, NULL, tfname, text);
	strcat(tfname, text);

	strcpy(msg.fileInfo.fileName, tfname);
	msg.fileInfo.fileSize = fileSize;

	if (SOCKET_ERROR == send(serverfd, (char*)&msg, sizeof(struct Header), 0)) {
		printf("send error:%d\n", WSAGetLastError());
	}
	else {
		printf("文件大小发送成功\n");
	}

	// 读取文件内容到全局变量中
	fileBuf = (char*)calloc(fileSize + 1, sizeof(char));
	if (fileBuf == NULL) {
		printf("内存申请失败\n");
		return false;
	}
	fread(fileBuf, sizeof(char), fileSize, pread);
	fileBuf[fileSize] = '\0';

	fclose(pread);
	return true;
}


bool sendFile(SOCKET serverfd, struct Header* msgHeader) {

	printf("发送中...请稍后\n");
	// 拆包情况		
	struct Header msg = { MSG_SEND };	// 告诉客户端这是发送的消息

	for (size_t i = 0; i < fileSize; i += PACKET_SIZE) {

		msg.fileData.nStart = i;

		if (i + PACKET_SIZE + 1 >= fileSize) {
			msg.fileData.nsize = fileSize - i;
		}
		else {
			msg.fileData.nsize = PACKET_SIZE;
		}

		// 关键！原本用的strncpy,就导致非文本文件传输失败
		memcpy(msg.fileData.buf, fileBuf + msg.fileData.nStart, msg.fileData.nsize);

		if (SOCKET_ERROR == send(serverfd, (char*)&msg, sizeof(struct Header), 0)) {
			printf("文件发送失败\n");
		}
	}
	return true;
}