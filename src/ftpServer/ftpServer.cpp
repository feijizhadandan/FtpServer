#include "ftpServer.h"
char recvBuf[1024];	// 接收客户端发送的消息
int fileSize;
char fileName[256];		// 保存文件名
char* fileBuf;	// 存储文件

int main() {

	initSocket();

	listenToClient();

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
void listenToClient() {
	// 创建server socket套接字
	SOCKET serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	// 表示该套接字是IPV4协议，面向字节流服务，并使用TCP协议的
	if (INVALID_SOCKET == serverfd) {
		printf("socket faild:%d\n", WSAGetLastError());
		return;
	}

	// 给socket绑定地址（IP地址和端口）
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SPORT);	// 本地字节序转换成网络字节序
	serverAddr.sin_addr.S_un.S_addr = ADDR_ANY;	// 监听本机所有网卡

	if (0 != bind(serverfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printf("bind faild:%d\n", WSAGetLastError());
		return;
	}

	// 监听客户端连接
	if (0 != listen(serverfd, 20)) {
		printf("listen error:%d\n", WSAGetLastError());
		return;
	}
	printf("服务端已启动! \n");
	// 有客户端连接，就接受连接.
	// 第二个参数是指客户端的地址信息，如果不需要可以写NULL
	struct sockaddr_in clientAddr;
	int len = sizeof(clientAddr);
	// 连接套接字
	SOCKET connect_socket = accept(serverfd, (struct sockaddr*)&clientAddr, &len);
	if (INVALID_SOCKET == connect_socket) {
		printf("accept error:%d\n", WSAGetLastError());
		return;
	}

	printf("客户端已连接! \n");

	// 循环处理消息
	while (processMsg(connect_socket)) {
		// Sleep(100);	
	}
}

// 处理消息
bool processMsg(SOCKET connect_socket) {
	int nRes = recv(connect_socket, recvBuf, 1024, 0);
	if (nRes <= 0) {
		printf("客户端下线...%d\n", WSAGetLastError());
		return false;
	}

	struct Header* msgHeader = (struct Header*)recvBuf;

	switch (msgHeader->msgID)
	{
	case MSG_FILENAME:
		printf("%s\n", msgHeader->fileInfo.fileName);
		readFile(connect_socket, msgHeader);
		break;

	case MSG_READY_READ:
		sendFile(connect_socket, msgHeader);
		break;

	case MSG_SUCCESS:
		// 接收到客户端的成功信号后，在给客户端发个成功信号，就先自行关闭了
		{
			struct Header msg = { MSG_SUCCESS };
			send(connect_socket, (char*)&msg, sizeof(struct Header), 0);
			printf("传输成功! Goodbye\n");
			return false;
			break; 
		}
	case MSG_FILESIZE:
		readyRead(connect_socket, msgHeader);
		break;
	case MSG_SEND:
		writeFile(connect_socket, msgHeader);
		break;
	}

	return 1;
}


bool readFile(SOCKET connect_socket, struct Header* msgHeader) {
	FILE* pread = fopen(msgHeader->fileInfo.fileName, "rb");

	// 文件打开失败处理
	if (pread == NULL) {
		printf("找不到文件：[%s]...\n", msgHeader->fileInfo.fileName);
		struct Header msg = { MSG_OPENFILE_ERROR };
		if (SOCKET_ERROR == send(connect_socket, (char*)&msg, sizeof(struct Header), 0)) {
			printf("send error:%d\n", WSAGetLastError());
		}
		return false;
	}

	// 获取文件大小
	fseek(pread, 0, SEEK_END);
	fileSize = ftell(pread);
	fseek(pread, 0, SEEK_SET);

	// 把文件大小发给客户端
	struct Header msg = { MSG_FILESIZE };
	char tfname[200] = { 0 }, text[100];
	// 将绝对路径进行分解
	_splitpath(msgHeader->fileInfo.fileName, NULL, NULL, tfname, text);
	strcat(tfname, text);

	strcpy(msg.fileInfo.fileName, tfname);
	msg.fileInfo.fileSize = fileSize;

	if (SOCKET_ERROR == send(connect_socket, (char*)&msg, sizeof(struct Header), 0)) {
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

bool sendFile(SOCKET connect_socket, struct Header* msgHeader) {

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

		if (SOCKET_ERROR == send(connect_socket, (char*)&msg, sizeof(struct Header), 0)) {
			printf("文件发送失败\n");
		}
	}
	return true;
}

void readyRead(SOCKET connnect_socket, struct Header* msgHeader) {

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
		if (SOCKET_ERROR == send(connnect_socket, (char*)&msg, sizeof(struct Header), 0)) {
			printf("send error:%d\n", WSAGetLastError());
			return;
		}
	}
	printf("上传的文件大小：%d    上传的文件名：%s\n", msgHeader->fileInfo.fileSize, msgHeader->fileInfo.fileName);
}


bool writeFile(SOCKET connnect_socket, struct Header* msgHeader) {

	if (fileBuf == NULL) {
		return false;
	}

	int nStart = msgHeader->fileData.nStart;
	int nsize = msgHeader->fileData.nsize;
	memcpy(fileBuf + nStart, msgHeader->fileData.buf, nsize);
	printf("接收中: --- %f%% ---\n", ((double)nStart + (double)nsize) / (double)fileSize * 100);
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
		send(connnect_socket, (char*)&msg, sizeof(struct Header), 0);
		printf("上传完毕! 再见!\n");
	}
	return true;
}