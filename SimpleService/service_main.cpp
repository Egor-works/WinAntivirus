#pragma comment(lib, "WtsApi32.lib") 

#include <Windows.h>
#include <WtsApi32.h>
#include <sddl.h>
#include <ntstatus.h>
#include <fstream>
#include <format>
#include <string>
#include <thread>
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <filesystem>




#define BUFSIZE 512
using namespace std;

std::wofstream errorLog;
WCHAR serviceName[] = L"ServiceSample";

SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle;

std::vector<uint8_t*> avBase;

template<typename T>
void WriteLog(const T& data, std::wstring prefix = L"")
{
	if (!errorLog.is_open()) {
		errorLog.open("C:\\Users\\egoro\\source\\repos\\service_log.txt", std::ios::out | std::ios::app);
		errorLog << prefix << data << std::endl;
	}
}

bool Read(HANDLE handle, uint8_t* data, uint64_t length, DWORD& bytesRead)
{
	bytesRead = 0;
	BOOL fSuccess = ReadFile(
		handle,
		data,
		length,
		&bytesRead,
		NULL);
	if (!fSuccess || bytesRead == 0)
	{
		return false;
	}
	return true;
}

bool Write(HANDLE handle, uint8_t* data, uint64_t length)
{
	DWORD cbWritten = 0;
	BOOL fSuccess = WriteFile(
		handle,
		data,
		length,
		&cbWritten,
		NULL);
	if (!fSuccess || length != cbWritten)
	{
		return false;
	}
	return true;
}

std::vector<uint8_t*> LoadSignatureFromFile(const std::wstring& signatureFilePath) {
	std::ifstream file(signatureFilePath, std::ios::binary);

	std::vector<uint8_t*> data;

	const size_t block_size = 8;
	uint8_t buffer[block_size];

	while (file.read(reinterpret_cast<char*>(buffer), block_size)) {
		uint8_t* block_data = new uint8_t[block_size];
		std::copy(buffer, buffer + block_size, block_data);
		data.push_back(block_data);
	}

	return data;
}

std::wstring GetUserSid(HANDLE userToken)
{
	std::wstring userSid;
	DWORD err = 0;
	LPVOID pvInfo = NULL;
	DWORD cbSize = 0;
	if (!GetTokenInformation(userToken, TokenUser, NULL, 0, &cbSize))
	{
		err = GetLastError();
		if (ERROR_INSUFFICIENT_BUFFER == err)
		{
			err = 0;
			pvInfo = LocalAlloc(LPTR, cbSize);
			if (!pvInfo)
			{
				err = ERROR_OUTOFMEMORY;
			}
			else if (!GetTokenInformation(userToken, TokenUser, pvInfo, cbSize, &cbSize))
			{
				err = GetLastError();
			}
			else
			{
				err = 0;
				const TOKEN_USER* pUser = (const TOKEN_USER*)pvInfo;
				LPWSTR userSidBuf;
				ConvertSidToStringSidW(pUser->User.Sid, &userSidBuf);
				userSid.assign(userSidBuf);
				LocalFree(userSidBuf);
			}
		}
	}
	return userSid;
}


SECURITY_ATTRIBUTES GetSecurityAttributes(const std::wstring& sddl)
{
	SECURITY_ATTRIBUTES securityAttributes{};
	securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	securityAttributes.bInheritHandle = TRUE;

	PSECURITY_DESCRIPTOR psd = nullptr;

	if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &psd, nullptr)) {
		securityAttributes.lpSecurityDescriptor = psd;
	}
	return securityAttributes;
}

void StartUiProcessInSession(DWORD wtsSession)
{
	
	std::thread clientThread([wtsSession]() {

		HANDLE userToken;
		if (WTSQueryUserToken(wtsSession, &userToken))
		{
			WCHAR commandLine[] = L"\"BVT_GUI.exe\"";

			std::wstring processSddl = std::format(L"O:SYG:SYD:(D;OICI;0x{:08x};;;WD)(A;OICI;0x{:08x};;;WD)",
				PROCESS_TERMINATE, PROCESS_ALL_ACCESS);
			std::wstring threadSddl = std::format(L"O:SYG:SYD:(D;OICI;0x{:08x};;;WD)(A;OICI;0x{:08x};;;WD)",
				THREAD_TERMINATE, THREAD_ALL_ACCESS);

			PROCESS_INFORMATION pi{};
			STARTUPINFO si{};

			SECURITY_ATTRIBUTES psa = GetSecurityAttributes(processSddl);
			SECURITY_ATTRIBUTES tsa = GetSecurityAttributes(threadSddl);
			if (psa.lpSecurityDescriptor != nullptr &&
				tsa.lpSecurityDescriptor != nullptr)
			{
				std::wstring path = std::format(L"\\\\.\\pipe\\SimpleService_{}", wtsSession);
				std::wstring userSid = GetUserSid(userToken);
				std::wstring pipeSddl = std::format(L"O:SYG:SYD:(A;OICI;GA;;;{})", userSid);
				SECURITY_ATTRIBUTES npsa = GetSecurityAttributes(pipeSddl);
				HANDLE pipe = CreateNamedPipeW(
					path.c_str(),
					PIPE_ACCESS_DUPLEX,
					PIPE_TYPE_MESSAGE |
					PIPE_READMODE_MESSAGE |
					PIPE_WAIT,
					1,
					BUFSIZE,
					BUFSIZE,
					0,
					&npsa
				);

				if (CreateProcessAsUserW(
					userToken,
					NULL,
					commandLine,
					&psa,
					&tsa,
					FALSE,
					0,
					NULL,
					NULL,
					&si,
					&pi))
				{
					ULONG clientProcessId;
					BOOL clientIdentified;
					do
					{
						BOOL fConnected = ConnectNamedPipe(pipe, NULL) ?
							TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
						clientIdentified = GetNamedPipeClientProcessId(pipe, &clientProcessId);
						if (clientIdentified)
						{
							if (clientProcessId == pi.dwProcessId)
							{
								break;
							}
							else
							{
								DisconnectNamedPipe(pipe);
							}
						}
					} while (true);

					
					uint8_t buf[8];
					DWORD bytesRead = 0;
					while (Read(pipe, buf,8, bytesRead))
					{
						ImpersonateNamedPipeClient(pipe);
						RevertToSelf();
						if(find(avBase.begin(), avBase.end(), buf) == end(avBase))
						{
							uint8_t buff2[] = "cleansg";
							Write(pipe, buff2, bytesRead);
						}
						else Write(pipe, buf, bytesRead);
					}
					CloseHandle(pipe);
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
				}

				auto sd = tsa.lpSecurityDescriptor;
				tsa.lpSecurityDescriptor = nullptr;
				LocalFree(sd);

				sd = psa.lpSecurityDescriptor;
				psa.lpSecurityDescriptor = nullptr;
				LocalFree(sd);
			}
		}

		});
	clientThread.detach();
}

DWORD WINAPI ControlHandler(DWORD dwControl, DWORD dwEvenType, LPVOID lpEventData, LPVOID lpContext)
{
	DWORD result = ERROR_CALL_NOT_IMPLEMENTED;
	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		result = NO_ERROR;
		break;
	case SERVICE_CONTROL_SHUTDOWN:
		serviceStatus.dwCurrentState = SERVICE_STOPPED;
		result = NO_ERROR;
		break;
	case SERVICE_CONTROL_SESSIONCHANGE:

		if (dwEvenType == WTS_SESSION_LOGON)
		{
			WTSSESSION_NOTIFICATION* sessionNotification = static_cast<WTSSESSION_NOTIFICATION*>(lpEventData);
			StartUiProcessInSession(sessionNotification->dwSessionId);
		}
		break;
	case SERVICE_CONTROL_INTERROGATE:
		result = NO_ERROR;
		break;
	}

	SetServiceStatus(serviceStatusHandle, &serviceStatus);
	return result;
}


void WINAPI ServiceMain(DWORD argc, wchar_t** argv)
{
	serviceStatusHandle = RegisterServiceCtrlHandlerExW(serviceName, (LPHANDLER_FUNCTION_EX)ControlHandler, argv[0]);
	if (serviceStatusHandle == (SERVICE_STATUS_HANDLE)0) {
		return;
	}

	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	serviceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(serviceStatusHandle, &serviceStatus);
	//SECURITY_ATTRIBUTES jsa = GetSecurityAttributes(L"O:SYG:SYD:");
	avBase = LoadSignatureFromFile(L"C:\\Users\\egoro\\Downloads\\binary-output.bin");
	

	PWTS_SESSION_INFOW wtsSessions;
	DWORD sessionsCount;
	if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &wtsSessions, &sessionsCount))
	{

		for (DWORD i = 0; i < sessionsCount; ++i)
		{
			auto wtsSession = wtsSessions[i].SessionId;

			if (wtsSession != 0)
			{
				StartUiProcessInSession(wtsSession);
			}
		}
	}
}
