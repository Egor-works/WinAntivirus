#pragma comment(lib, "WtsApi32.lib") 

#include <Windows.h>
#include <WtsApi32.h>
#include <sddl.h>
#include <ntstatus.h>
#include <fstream>


WCHAR serviceName[] = L"ServiceSample";

SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle;

void StartUiProcessInSession(DWORD wtsSession)
{
	HANDLE userToken;
	if (WTSQueryUserToken(wtsSession, &userToken))
	{
		WCHAR commandLine[] = L"\"BVT_GUI.exe\" user";
		WCHAR localSystemSddl[] = L"O:SYG:SYD:";
		PROCESS_INFORMATION pi{};
		STARTUPINFO si{};

		SECURITY_ATTRIBUTES processSecurityAttributes{};
		processSecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		processSecurityAttributes.bInheritHandle = TRUE;

		SECURITY_ATTRIBUTES threadSecurityAttributes{};
		threadSecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		threadSecurityAttributes.bInheritHandle = TRUE;

		PSECURITY_DESCRIPTOR psd = nullptr;

		if (ConvertStringSecurityDescriptorToSecurityDescriptorW(localSystemSddl, SDDL_REVISION_1, &psd, nullptr))
		{
			processSecurityAttributes.lpSecurityDescriptor = psd;
			threadSecurityAttributes.lpSecurityDescriptor = psd;

			if (CreateProcessAsUserW(
				userToken,
				NULL,
				commandLine,
				&processSecurityAttributes,
				&threadSecurityAttributes,
				FALSE,
				0,
				NULL,
				NULL,
				&si,
				&pi))
			{
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
			}
			LocalFree(psd);
		}
	}
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

	std::wofstream log("C:\\service_log.txt");

	PWTS_SESSION_INFO wtsSessions;
	DWORD sessionsCount;
	if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &wtsSessions, &sessionsCount))
	{
		log << L"WTSEnumerateSessionsW returns TRUE, sessionCount = " << sessionsCount << std::endl;
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
