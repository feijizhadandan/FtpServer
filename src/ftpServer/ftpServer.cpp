#include "ftpServer.h"
char recvBuf[1024];	// ���տͻ��˷��͵���Ϣ
int fileSize;
char fileName[256];		// �����ļ���
char* fileBuf;	// �洢�ļ�

int main() {

	initSocket();

	listenToClient();

	closeSocket();

	system("pause");
	return 0;
}

// ��ʼ��socket��
bool initSocket() {

	WSADATA wsadata;
	// ��ʼ��socket����
	if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata)) {
		printf("WSAStartup error:%d\n", WSAGetLastError());
		return false;
	}
	return true;
}

// �ر�socket��
bool closeSocket() {
	if (0 != WSACleanup()) {
		printf("WSACleanup error:%d\n", WSAGetLastError());
		return false;
	}
	return true;
}

// �����ͻ�������
void listenToClient() {
	// ����server socket�׽���
	SOCKET serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	// ��ʾ���׽�����IPV4Э�飬�����ֽ������񣬲�ʹ��TCPЭ���
	if (INVALID_SOCKET == serverfd) {
		printf("socket faild:%d\n", WSAGetLastError());
		return;
	}

	// ��socket�󶨵�ַ��IP��ַ�Ͷ˿ڣ�
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SPORT);	// �����ֽ���ת���������ֽ���
	serverAddr.sin_addr.S_un.S_addr = ADDR_ANY;	// ����������������

	if (0 != bind(serverfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printf("bind faild:%d\n", WSAGetLastError());
		return;
	}

	// �����ͻ�������
	if (0 != listen(serverfd, 20)) {
		printf("listen error:%d\n", WSAGetLastError());
		return;
	}
	printf("�����������! \n");
	// �пͻ������ӣ��ͽ�������.
	// �ڶ���������ָ�ͻ��˵ĵ�ַ��Ϣ���������Ҫ����дNULL
	struct sockaddr_in clientAddr;
	int len = sizeof(clientAddr);
	// �����׽���
	SOCKET connect_socket = accept(serverfd, (struct sockaddr*)&clientAddr, &len);
	if (INVALID_SOCKET == connect_socket) {
		printf("accept error:%d\n", WSAGetLastError());
		return;
	}

	printf("�ͻ���������! \n");

	// ѭ��������Ϣ
	while (processMsg(connect_socket)) {
		// Sleep(100);	
	}
}

// ������Ϣ
bool processMsg(SOCKET connect_socket) {
	int nRes = recv(connect_socket, recvBuf, 1024, 0);
	if (nRes <= 0) {
		printf("�ͻ�������...%d\n", WSAGetLastError());
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
		// ���յ��ͻ��˵ĳɹ��źź��ڸ��ͻ��˷����ɹ��źţ��������йر���
		{
			struct Header msg = { MSG_SUCCESS };
			send(connect_socket, (char*)&msg, sizeof(struct Header), 0);
			printf("����ɹ�! Goodbye\n");
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

	// �ļ���ʧ�ܴ���
	if (pread == NULL) {
		printf("�Ҳ����ļ���[%s]...\n", msgHeader->fileInfo.fileName);
		struct Header msg = { MSG_OPENFILE_ERROR };
		if (SOCKET_ERROR == send(connect_socket, (char*)&msg, sizeof(struct Header), 0)) {
			printf("send error:%d\n", WSAGetLastError());
		}
		return false;
	}

	// ��ȡ�ļ���С
	fseek(pread, 0, SEEK_END);
	fileSize = ftell(pread);
	fseek(pread, 0, SEEK_SET);

	// ���ļ���С�����ͻ���
	struct Header msg = { MSG_FILESIZE };
	char tfname[200] = { 0 }, text[100];
	// ������·�����зֽ�
	_splitpath(msgHeader->fileInfo.fileName, NULL, NULL, tfname, text);
	strcat(tfname, text);

	strcpy(msg.fileInfo.fileName, tfname);
	msg.fileInfo.fileSize = fileSize;

	if (SOCKET_ERROR == send(connect_socket, (char*)&msg, sizeof(struct Header), 0)) {
		printf("send error:%d\n", WSAGetLastError());
	}
	else {
		printf("�ļ���С���ͳɹ�\n");
	}


	// ��ȡ�ļ����ݵ�ȫ�ֱ�����
	fileBuf = (char*)calloc(fileSize + 1, sizeof(char));
	if (fileBuf == NULL) {
		printf("�ڴ�����ʧ��\n");
		return false;
	}
	fread(fileBuf, sizeof(char), fileSize, pread);
	fileBuf[fileSize] = '\0';

	fclose(pread);
	return true;
}

bool sendFile(SOCKET connect_socket, struct Header* msgHeader) {

	printf("������...���Ժ�\n");
	// ������		
	struct Header msg = { MSG_SEND };	// ���߿ͻ������Ƿ��͵���Ϣ
	
	for (size_t i = 0; i < fileSize; i += PACKET_SIZE) {

		msg.fileData.nStart = i;

		if (i + PACKET_SIZE + 1 >= fileSize) {
			msg.fileData.nsize = fileSize - i;
		}
		else {
			msg.fileData.nsize = PACKET_SIZE;
		}

		// �ؼ���ԭ���õ�strncpy,�͵��·��ı��ļ�����ʧ��
		memcpy(msg.fileData.buf, fileBuf + msg.fileData.nStart, msg.fileData.nsize);

		if (SOCKET_ERROR == send(connect_socket, (char*)&msg, sizeof(struct Header), 0)) {
			printf("�ļ�����ʧ��\n");
		}
	}
	return true;
}

void readyRead(SOCKET connnect_socket, struct Header* msgHeader) {

	strcpy(fileName, msgHeader->fileInfo.fileName);

	// ׼���ڴ�
	fileSize = msgHeader->fileInfo.fileSize;
	fileBuf = (char*)calloc(fileSize + 1, sizeof(char));	// ��̬����ռ�
	// ������������ MSG_READY_READ
	if (fileBuf == NULL) {
		printf("�ռ�����ʧ��\n");
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
	printf("�ϴ����ļ���С��%d    �ϴ����ļ�����%s\n", msgHeader->fileInfo.fileSize, msgHeader->fileInfo.fileName);
}


bool writeFile(SOCKET connnect_socket, struct Header* msgHeader) {

	if (fileBuf == NULL) {
		return false;
	}

	int nStart = msgHeader->fileData.nStart;
	int nsize = msgHeader->fileData.nsize;
	memcpy(fileBuf + nStart, msgHeader->fileData.buf, nsize);
	printf("������: --- %f%% ---\n", ((double)nStart + (double)nsize) / (double)fileSize * 100);
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
		printf("�ϴ����! �ټ�!\n");
	}
	return true;
}