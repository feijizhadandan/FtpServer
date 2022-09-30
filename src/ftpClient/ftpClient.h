#pragma once
#include<bits/stdc++.h>
#include<WinSock2.h>
// socket��
#pragma comment(lib, "ws2_32.lib")
#define SPORT 8888	// �������˿ں�
#define PACKET_SIZE (1024 - 4*sizeof(int) - 256)

// ������
enum MSGTAG {
	MSG_FILENAME = 1,	// ��������ļ���
	MSG_FILESIZE = 2,	// ��������ļ���С
	MSG_READY_READ = 3,	// ׼�������ļ�
	MSG_SEND = 4,		// �����ļ�
	MSG_SUCCESS = 5,		// �������

	MSG_OPENFILE_ERROR = 6	// �����ļ�ʧ��

};

// ��װ��Ϣͷ���ļ������ļ���С
#pragma pack(1)	// �ṹ��1�ֽڶ���
struct Header {
	enum MSGTAG msgID;	//	��ǰ��Ϣ�������
	// ���ýṹ���װ�ļ�������Ϣ
	struct {
		int fileSize;
		char fileName[256];
	}fileInfo;

	struct {
		int nStart;
		int nsize;	// �������ݴ�С
		char buf[PACKET_SIZE];
	}fileData;
};
#pragma pack()


// ��ʼ��socket��
bool initSocket();

// �ر�socket��
bool closeSocket();

// �����ͻ�������
void connectToServer();

// ������Ϣ
bool processMsg(SOCKET);

// ������Ϣ����ȡ�ļ�������С
void getFileName(SOCKET);

// ������Ϣ��׼���ڴ�ռ䣬׼������
void readyRead(SOCKET, struct Header*);

// ������Ϣ��д�ļ�
bool writeFile(SOCKET, struct Header*);

// �ϴ��ļ��������ļ���С������
bool sendFileInfo(SOCKET);

// �ϴ��ļ��������ļ�
bool sendFile(SOCKET, struct Header*);