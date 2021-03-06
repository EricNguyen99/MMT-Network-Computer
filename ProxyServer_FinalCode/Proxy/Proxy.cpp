// Proxy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Proxy.h"
#include "afxsock.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define HTTP "http://"
#define HTTPPORT 80  
#define BSIZE 20 * 1024
UINT pport = 1180;

///////////////////////////////////////////////////////////////////////////////////////////
// The one and only application object
CWinApp theApp;

using namespace std;

//struct chứa thông tin của Client và Server để chia sẻ giữa các Thread
struct ProxySockets 
{
	SOCKET proxy_server;
	SOCKET user_proxy;
	bool IsUser_ProxyClosed;
	bool IsProxy_ServerClosed;
};
//struct chứa các thông tin của một truy vấn
struct ProxyParam 
{ 
	string address;      //địa chỉ máy chủ từ xa
	int port;            //cổng được sử dụng 
	HANDLE User_SvrOK;   //trạng thái của proxy kết nối đến web server
	ProxySockets *pPair;  //Duy trì 1 con trỏ socket
};


/////////////////////////////////////////////////////////////////////////////////////////////
//Hàm khởi tạo Server để lắng nghe các kết nối
int StartProxyServer();
//Luồng giao tiếp giữa Client và Proxy Server
UINT UserToProxy(void *lParam);
//Luòng giao tiếp giữa Proxy Server và Remote Server
UINT ProxyToServer(void *lParam);
//Đóng các giao tiếp lại
int CloseServer();

//Phân tích các chuỗi nhận được để lấy địa chỉ của web server
void GetAddrNPort(string &buf, string &address, int &port);
//Tách chuỗi
void Split(string str, vector<string> &cont, char delim = ' ');

//Chuyển char array sang dạng LPCWSTR
wchar_t *convertCharArrayToLPCWSTR(const char* charArray);
//Load file blacklist.conf
void LoadBlackList(vector<string> &arr);
//Kiểm tra xem địa chỉ có nằm trong black list hay không
bool CheckServerName(string server_name);

//Lấy địa chỉ IP để yêu cầu kết nối
sockaddr_in* GetServer(string server_name, char*hostname);

typedef SOCKET Socket; 
vector<string> black_list;

//Chuỗi trả về khi nằm trong black list
string msg_403 = "HTTP/1.0 403 Forbidden\r\n\Cache-Control: no-cache\r\n\Connection: close\r\n";

//Socket dùng để lắng nghe các truy cập mới
Socket gListen;
//bool run = 1;

int main()
{
	//int nRetCode = 0;
	cout << "Vui long dat so cong cua may chu Proxy! " << endl << "Port: ";
	cin >> pport;

	if (pport < 1024)
	{
		cout << "So cong nho hon 1024,co the gay ra xung dot cong !!!" << endl;
	}
	cout << "Dang lang nghe cac ket noi!" << endl << "Neu ban muon ket thuc chuong trinh, hay nhan \\e\\ key!" << endl;

	StartProxyServer();
	while (1) {
		if (getchar() == 'e') break;
		//Sleep(1000);
	}
	CloseServer();
	//return nRetCode;
	return 0;
}

int StartProxyServer()
{
	sockaddr_in local;
	Socket listen_socket;
	WSADATA wsaData;
	//Cài đặt các Socket
	if (WSAStartup(0x202, &wsaData) != 0)
	{
		cout << "Loi khoi tao socket" << endl;
		WSACleanup(); 
		return -1;
	}
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(pport);
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	//Khỏi tạo socket
	if (listen_socket == INVALID_SOCKET)
	{
		cout << "Socket khoi tao khong hop le." << endl;
		WSACleanup();
		return -2;
	}
	//Bind Socket 
	if (bind(listen_socket, (sockaddr *)&local, sizeof(local)) != 0)
	{
		cout << "Loi khi bind socket." << endl;
		WSACleanup();
		return -3;
	};
	//Bắt đầu lắng nghe các truy cập
	if (listen(listen_socket, 5) != 0)
	{
		cout << "Loi khi nghe." << endl;
		WSACleanup(); 
		return -4;
	}
	LoadBlackList(black_list);
	if (black_list.size() == 0)
		cout << "Khong su dung blacklist" << endl;
	else
		cout << "Su dung blacklist " << endl;

	gListen = listen_socket;
	//Bắt đầu thread giao tiếp giữa Client và Proxy Server
	AfxBeginThread(UserToProxy, (LPVOID)listen_socket);

	return 1;
}

UINT UserToProxy(void * pParam)
{
	Socket socket = (Socket)pParam;
	ProxySockets Pair; 
	SOCKET msg_socket;
	sockaddr_in addr;
	int addrLen = sizeof(addr);

	//Truy cập mới
	msg_socket = accept(socket, (sockaddr*)&addr, &addrLen);
	//Khởi tạo một thread khác để tiếp tục lắng nghe
	AfxBeginThread(UserToProxy, pParam);
	if (msg_socket == INVALID_SOCKET)
	{
		cout << "Error  in accept " << endl;
		return -5;
	}
	char Buffer[BSIZE];
	int Len;
	//Đọc dòng dữ liệu đầu tiên của user
	Pair.IsUser_ProxyClosed = FALSE;
	Pair.IsProxy_ServerClosed = FALSE;
	Pair.user_proxy = msg_socket; //tại vùng nhớ đệm
	//Nhận truy vấn gởi tới từ Client
	int returnValue = recv(Pair.user_proxy, Buffer, sizeof(Buffer), 0);
	if (returnValue == SOCKET_ERROR) 
	{
		cout << "Error Recv ";
		if (!Pair.IsProxy_ServerClosed) //Nếu client chưa đóng thì đóng đi
		{
			closesocket(Pair.user_proxy);
			Pair.IsProxy_ServerClosed = TRUE;
		}
	}
	if (returnValue == 0)
	{
		cout << "\nClient close connection" << endl;
		if (!Pair.IsProxy_ServerClosed) 
		{
			closesocket(Pair.user_proxy);
			Pair.IsProxy_ServerClosed = TRUE;
		}
	}
	//returnValue >= BSIZE ? Buffer[returnValue - 1] = 0 : (returnValue > 0 ? Buffer[returnValue] = 0 : Buffer[0] = 0);
	//if (returnValue >= BSIZE)
	//	Buffer[returnValue - 1] = 0;
	//else
	//	if (returnValue > 0)
	//		Buffer[returnValue];
	//	else
	//		Buffer[0] = 0;

	cout << "Received " << returnValue << " bytes, data \n[" << Buffer << "] from server \n";


	string buf(Buffer), address;
	int port;
	GetAddrNPort(buf, address, port);
	bool check = FALSE;
	if (!CheckServerName(address)) {
		//Nếu địa chỉ nằm trong black list sẽ trả về Error 403 mà không phải gởi truy vấn tới remote server
		returnValue = send(Pair.user_proxy, msg_403.c_str(), msg_403.size(), 0);
		check = TRUE;
		Sleep(2000);
	}
	ProxyParam P;
	P.User_SvrOK = CreateEvent(NULL, TRUE, FALSE, NULL); //Xử lý
	P.address = address;
	P.port = port;
	P.pPair = &Pair;
	if (check == FALSE) {
		//Bắt đầu thread giao tiếp giữa Proxy Server và Remote Server
		CWinThread* pThread = AfxBeginThread(ProxyToServer, (LPVOID)&P);
		//Đợi cho Proxy kết nối với Server
		WaitForSingleObject(P.User_SvrOK, 6000);
		CloseHandle(P.User_SvrOK);
		while (Pair.IsProxy_ServerClosed == FALSE && Pair.IsUser_ProxyClosed == FALSE) {
			//Proxy gởi truy vấn
			returnValue = send(Pair.proxy_server, buf.c_str(), buf.size(), 0);
			if (returnValue == SOCKET_ERROR) {
				//cout << "Send Failed, Error: " << GetLastError();
				if (Pair.IsUser_ProxyClosed == FALSE) {
					closesocket(Pair.proxy_server);
					Pair.IsUser_ProxyClosed = TRUE;
				}
				continue;
			}
			//Tiếp tục nhận các truy vấn từ Client
			//Vòng lặp này sẽ chạy đến khi nhận hết data, 1 trong 2 Client và Server sẽ ngắt kết nối
			returnValue = recv(Pair.user_proxy, Buffer, sizeof(Buffer), 0);
			if (returnValue == SOCKET_ERROR) 
			{
				cout << "\nError recv: ";// << GetLastError();
				if (Pair.IsProxy_ServerClosed == FALSE) 
				{
					closesocket(Pair.user_proxy);
					Pair.IsProxy_ServerClosed = TRUE;
				}
				continue;
			}
			if (returnValue == 0) 
			{
				cout << "\nClient close connection " << endl;
				if (Pair.IsProxy_ServerClosed == FALSE) 
				{
					closesocket(Pair.proxy_server);
					Pair.IsProxy_ServerClosed = TRUE;
				}
				break;
			}
			//returnValue >= BSIZE ? Buffer[returnValue - 1] = 0 : (returnValue > 0 ? Buffer[returnValue] = 0 : Buffer[0] = 0);
			/*if (returnValue >= BSIZE)
				Buffer[returnValue - 1] = 0;
			else
				if (returnValue > 0)
					Buffer[returnValue];
				else
					Buffer[0] = 0;*/
			cout << "Received " << returnValue << " bytes, data \n[" << Buffer << "] from server \n";
		}
		if (Pair.IsUser_ProxyClosed == FALSE) {
			closesocket(Pair.proxy_server);
			Pair.IsUser_ProxyClosed = TRUE;
		}
		if (Pair.IsProxy_ServerClosed == FALSE) {
			closesocket(Pair.user_proxy);
			Pair.IsProxy_ServerClosed = TRUE;
		}
		WaitForSingleObject(pThread->m_hThread, 20000); //Should check the return value
	}
	else {
		if (Pair.IsProxy_ServerClosed == FALSE) {
			closesocket(Pair.user_proxy);
			Pair.IsProxy_ServerClosed = TRUE;
		}
	}
	return 0;
}

UINT ProxyToServer(void * pParam)
{
	int count = 0;
	ProxyParam *pPar = (ProxyParam*)pParam;
	string server_name = pPar->address;
	int port = pPar->port;
	int status;
	int addr;
	char hostname[32] = "";
	sockaddr_in *server = NULL;
	//cout << "Server Name: " << server_name << endl;
	server = GetServer(server_name, hostname);
	if (server == NULL) {
		//cout << "Khong the lay dia chi IP" << endl;
		send(pPar->pPair->user_proxy, msg_403.c_str(), msg_403.size(), 0);
		return -1;
	}
	if (strlen(hostname) > 0) {
		cout << "Client connecting to: " << hostname << endl;
		int retval;
		char Buffer[BSIZE];
		SOCKET conn_socket;
		conn_socket = socket(AF_INET, SOCK_STREAM, 0);
		//Kết nối tới địa chỉ IP vừa lấy được
		if (!(connect(conn_socket, (sockaddr*)server, sizeof(sockaddr)) == 0))
		{
			//cout << "Connect failed" << WSAGetLastError() << endl;
			send(pPar->pPair->user_proxy, msg_403.c_str(), msg_403.size(), 0);
			pPar->pPair->IsProxy_ServerClosed = TRUE;
			SetEvent(pPar->User_SvrOK);
			return -1;
		}
		else {
			cout << "\nKet noi thanh cong \n";
			pPar->pPair->proxy_server = conn_socket;
			pPar->pPair->IsUser_ProxyClosed == FALSE;
			SetEvent(pPar->User_SvrOK);
			int c = 0;

			// cook up a string to send
			while (pPar->pPair->IsProxy_ServerClosed == FALSE && pPar->pPair->IsUser_ProxyClosed == FALSE) {
				//Nhận data gởi từ Server tới Proxy
				retval = recv(pPar->pPair->proxy_server, Buffer, BSIZE, 0);
				if (retval == SOCKET_ERROR)
				{
					closesocket(pPar->pPair->proxy_server);
					pPar->pPair->IsUser_ProxyClosed = TRUE;
					break;
				}
				if (retval == 0) {
					cout << "Server closed connection" << endl;
					closesocket(pPar->pPair->proxy_server);
					pPar->pPair->IsUser_ProxyClosed = TRUE;
				}
				//Gởi data đó tới Client
				//Kết thúc vòng lặp khi đã nhận và gởi hết data
				retval = send(pPar->pPair->user_proxy, Buffer, retval, 0);
				if (retval == SOCKET_ERROR) {
					//cout << "Send failed, error: " << WSAGetLastError() << endl;
					closesocket(pPar->pPair->user_proxy);
					pPar->pPair->IsProxy_ServerClosed = TRUE;
					break;
				}
				//retval >= BSIZE ? Buffer[retval - 1] = 0 : Buffer[retval] = 0;
				cout << "Received " << retval << " bytes, data \n[" << Buffer << "] from server \n";
				ZeroMemory(Buffer, BSIZE);
			}
			//Đóng socket
			//Việc thay đổi giá trị ở thread này sẽ ảnh hưởng tới thread ClientToProxy
			//Việc đóng Socket ở thread này => các giá trị thread kia cũng thay đổi
			if (pPar->pPair->IsProxy_ServerClosed == FALSE) {
				closesocket(pPar->pPair->user_proxy);
				pPar->pPair->IsProxy_ServerClosed = TRUE;
			}
			if (pPar->pPair->IsUser_ProxyClosed == FALSE) {
				closesocket(pPar->pPair->proxy_server);
				pPar->pPair->IsUser_ProxyClosed = TRUE;
			}
		}
	}
	return 0;
}

int CloseServer()
{
	//Đóng Socket lắng nghe
	cout << "Close Socket" << endl;
	closesocket(gListen);
	WSACleanup();
	return 1;
}


void GetAddrNPort(string &buf, string &address, int &port)
{
	vector<string> cont;
	//cont 0: command, 1: link, 2: proto
	Split(buf, cont);
	if (cont.size() > 0) {
		int pos = cont[1].find(HTTP);
		if (pos != -1) {
			string add = cont[1].substr(pos + strlen(HTTP));
			address = add.substr(0, add.find('/'));
			//Port của HTTP là 80
			port = 80;
			string temp;
			int len = strlen(HTTP) + address.length();
			//GET http://www.cpluscplus.com HTTP1.0 ==> GET / HTTP1.0
			while (len > 0) {
				temp.push_back(' ');
				len--;
			}
			buf = buf.replace(buf.find(HTTP + address), strlen(HTTP) + address.length(), temp);
		}
	}
}

void Split(string str, vector<string> &cont, char delim)
{
	istringstream ss(str);
	string token;
	while (getline(ss, token, delim)) {
		cont.push_back(token);
	}
}

wchar_t *convertCharArrayToLPCWSTR(const char* charArray)
{
	wchar_t* wString = new wchar_t[4096];
	MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, 4096);
	return wString;
}

void LoadBlackList(vector<string>& arr)
{
	fstream f;
	f.open("blacklist.conf", ios::in | ios::out);
	if (f.is_open()) {
		while (!f.eof()) {
			string temp;
			getline(f, temp);
			if (temp.back() == '\n') {
				temp.pop_back();
			}
			arr.push_back(temp);
		}
	}
}

bool CheckServerName(string server_name) {
	if (black_list.size() > 0) {
		for (auto i : black_list)
		{
			if (i.find(server_name) != string::npos) {
				cout << i.find(server_name);
				return 0;
			}
		}
	}
	return 1;
}

sockaddr_in* GetServer(string server_name,char * hostname)
{
	int status;
	sockaddr_in *server = NULL;
	if (server_name.size() > 0) {
		//Kiểm tra xem địa chỉ lấy được ở địa dạng nào www.abc.com hay 192.168.100.1
		if (isalpha(server_name.at(0))) {
			addrinfo hints, *res = NULL;
			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			//Lấy thông tin từ địa chỉ lấy được 
			if ((status = getaddrinfo(server_name.c_str(), "80", &hints, &res)) != 0) {
				printf("getaddrinfo failed: %s", gai_strerror(status));
				return NULL;
			}
			while (res->ai_next != NULL) {
				res = res->ai_next;
			}
			sockaddr_in * temp = (sockaddr_in*)res->ai_addr;
			inet_ntop(res->ai_family, &temp->sin_addr, hostname, 32);
			server = (sockaddr_in*)res->ai_addr;
			unsigned long addr;
			inet_pton(AF_INET, hostname, &addr);
			server->sin_addr.s_addr = addr;
			server->sin_port = htons(80);
			server->sin_family = AF_INET;
		}
		else {
			unsigned long addr;
			inet_pton(AF_INET, server_name.c_str(), &addr);
			sockaddr_in sa;
			sa.sin_family = AF_INET;
			sa.sin_addr.s_addr = addr;
			if ((status = getnameinfo((sockaddr*)&sa,
				sizeof(sockaddr), hostname, NI_MAXHOST, NULL, NI_MAXSERV, NI_NUMERICSERV)) != 0) {
				cout << "Error";
				return NULL;
			}
			server->sin_addr.s_addr = addr;
			server->sin_family = AF_INET;
			server->sin_port = htons(80);
		}
	}

	return server;
}



