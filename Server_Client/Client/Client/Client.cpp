// Client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "Client.h"
#include "afxsock.h" //thư viện dùng cho socket
#include <iostream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object

CWinApp theApp;

using namespace std;

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // initialize MFC and print and error on failure
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: code your application's behavior here.
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
        }
        else
        {
            // TODO: code your application's behavior here.
			// Đây là phần code
			//-----------------------------------------------
			//Khai báo sử dụng socket trong window
			AfxSocketInit(NULL);

			// Socket Client
			CSocket client;

			//Khởi tạo socket
			client.Create();   

			//Kết nối đến server
			client.Connect(_T("127.0.0.1"), 8888);

			//các biến
			char msg[100]; 
			int len = 0; 

			//bắt đầu chat
			while (true)
			{
				//client chat trước
				cout << "Client says: ";
				gets_s(msg);
				len = strlen(msg);

				//gửi msg đến server
				client.Send(&len, sizeof(int), 0);
				client.Send(msg, len, 0);

				//nhận msg từ server
				client.Receive(&len, sizeof(int), 0);

				char* temp = new char[len + 1];
				client.Receive(temp, len, 0);
				temp[len] = '\0';

				//hiển thị msg từ server
				cout << "Server says: " << temp << endl;
				delete temp;
			}
			client.Close();

			//----------------------------------------------------
        }
    }
    else
    {
        // TODO: change error code to suit your needs
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }
    
    return nRetCode;
}
