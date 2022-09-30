#include "ftpClient.h"
char recvBuf[1024]; // ���ڽ�����Ϣ�Ļ�����
char *fileBuf;		// �洢�ļ�����
int fileSize;
char fileName[256];		// �����ļ���

int main() {

	initSocket();

	connectToServer();

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
void connectToServer() {
	// ����server socket�׽���
	SOCKET serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);	// ��ʾ���׽�����IPV4Э�飬�����ֽ������񣬲�ʹ��TCPЭ���
	if (INVALID_SOCKET == serverfd) {
		printf("socket faild:%d\n", WSAGetLastError());
		return;
	}

	// ����Ҫ���ʵķ�������ַ
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SPORT);	// ���������̶˿ں�
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");	// ��������IP��ַ

	// ���ӵ�������
	if (0 != connect(serverfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))) {
		printf("connect faild:%d\n", WSAGetLastError());
		return;
	}

	int choose;
	printf("�����빦��: 1�������ļ� 2���ϴ��ļ�\n");
	scanf("%d", &choose);
	
	if (choose == 1) {
		getFileName(serverfd);
	}
	else if (choose == 2) {
		if (!sendFileInfo(serverfd))
			return;
	}
	else {
		printf("�������, �ټ�\n");
		return;
	}

	// ѭ��������Ϣ
	while (processMsg(serverfd)) {
		// Sleep(100);
	}
}

// ������Ϣ
bool processMsg(SOCKET serverfd) {

	recv(serverfd, recvBuf, 1024, 0);	// �����ļ���С��Ϣ
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
		printf("����ɹ�! Goodbye\n");
		return false;
		break;
	
	case MSG_READY_READ:
		sendFile(serverfd, msg);
		break;
	}


	return 1;
}

void getFileName(SOCKET serverfd) {
	printf("�����������ļ���: \n");
	char fileName[1024] = "";
	getchar();
	gets_s(fileName, 1024);	// ��Ҫ���ص��ļ���

	struct Header msgHeader = { MSG_FILENAME };
	strcpy(msgHeader.fileInfo.fileName, fileName);

	send(serverfd, (char*)&msgHeader, sizeof(struct Header) + 1, 0);
}

void readyRead(SOCKET serverfd, struct Header* msgHeader) {

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
		if (SOCKET_ERROR == send(serverfd, (char*)&msg, sizeof(struct Header), 0)) {
			printf("send error:%d\n", WSAGetLastError());
			return;
		}
	}
	printf("���ص��ļ���С��%d    ���ص��ļ�����%s\n", msgHeader->fileInfo.fileSize, msgHeader->fileInfo.fileName);
}

bool writeFile(SOCKET serverfd, struct Header* msgHeader) {
	
	if (fileBuf == NULL) {
		return false;
	}

	int nStart = msgHeader->fileData.nStart;
	int nsize = msgHeader->fileData.nsize;
	memcpy(fileBuf + nStart, msgHeader->fileData.buf, nsize);
	printf("������: --- %f%% ---\n", ((double)nStart + (double)nsize)/(double)fileSize * 100);
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
		printf("�������!!\n");
	}
	return true;
}

bool sendFileInfo(SOCKET serverfd) {
	struct Header msg;
	msg.msgID = MSG_FILESIZE;

	char fileName[1024] = "";
	printf("�������ϴ����ļ���: \n");
	getchar();
	gets_s(fileName, 1024);	// ��Ҫ�ϴ����ļ���
	strcpy(msg.fileInfo.fileName, fileName);

	FILE* pread = fopen(msg.fileInfo.fileName, "rb");
	// �ļ���ʧ�ܴ���
	if (pread == NULL) {
		printf("δ�ҵ��ļ���[%s]...�ټ�! \n", fileName);
		return false;
	}

	fseek(pread, 0, SEEK_END);
	fileSize = ftell(pread);
	fseek(pread, 0, SEEK_SET);

	char tfname[200] = { 0 }, text[100];
	// ������·�����зֽ�
	_splitpath(msg.fileInfo.fileName, NULL, NULL, tfname, text);
	strcat(tfname, text);

	strcpy(msg.fileInfo.fileName, tfname);
	msg.fileInfo.fileSize = fileSize;

	if (SOCKET_ERROR == send(serverfd, (char*)&msg, sizeof(struct Header), 0)) {
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


bool sendFile(SOCKET serverfd, struct Header* msgHeader) {

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

		if (SOCKET_ERROR == send(serverfd, (char*)&msg, sizeof(struct Header), 0)) {
			printf("�ļ�����ʧ��\n");
		}
	}
	return true;
}