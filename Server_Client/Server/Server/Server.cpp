// Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "Server.h"
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
			CSocket server;
			CSocket client;

			//Khởi tạo socket với port
			//Ở đây mình chọn port là 8888

			server.Create(8888);
			
			//Lắng nghe kết nối từ client
			server.Listen();

			cout << "Waiting for client. . ." << endl;

			//chấp nhận kết nối từ client
			if (server.Accept(client))
			{
				cout << "Client connect . . ." << endl;
			}

            //các biến
			char msg[100];
			int len = 0;
			//bắt đầu chat
			while (true)
			{
				// nhận msg từ client
				client.Receive(&len, sizeof(int), 0);
				
				//khai báo vùng nhớ
				char* temp = new char[len + 1];

				//nhận msg
				client.Receive(temp, len, 0);
				temp[len] = '\0'; //kí hiệu kết thúc chuỗi

				//in msg từ client gửi
				cout << "Client says: " << temp << endl;


				//gửi msg cho client
				cout << "Server says: ";
				gets_s(msg);
				len = strlen(msg);
				client.Send(&len, sizeof(int), 0);
				client.Send(msg, len, 0);

				delete temp;
			}
			server.Close();
			client.Close();
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
