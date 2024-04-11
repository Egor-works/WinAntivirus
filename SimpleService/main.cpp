#include <Windows.h>
#include <iostream>

extern WCHAR serviceName[];
void WINAPI ServiceMain(DWORD argc, wchar_t** argv);



int wmain(int argc, wchar_t** argv)
{

	for (int i = 0; i < argc; ++i)
		std::wcout << L"\"" << argv[i] << L"\" ";

	

	SERVICE_TABLE_ENTRYW ServiceTable[2];
	ServiceTable[0].lpServiceName = serviceName;
	ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTIONW)ServiceMain;
	ServiceTable[1].lpServiceName = NULL;
	ServiceTable[1].lpServiceProc = NULL;


	if (!StartServiceCtrlDispatcherW(ServiceTable)) {
		std::cerr << "Error: StartServiceCtrlDispatcher: " << GetLastError();
	}

}