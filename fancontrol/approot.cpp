#include "_prec.h"
#include "approot.h"
#include "fancontrol.h"
#include "TVicPort.h"
#include <stop_token>
#include <thread>
#include <format>

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE, LPSTR aArgs, int) {
    hInstRes = instance;
    hInstApp = instance;

	HANDLE hLock = CreateMutex(NULL,FALSE,"TPFanCtrl2Mutex01");

  if (hLock == NULL) {
      DWORD ec = GetLastError();
      ShowError(ec, "program or service already running");

      return ec;
  }

  if (WAIT_OBJECT_0 != WaitForSingleObject(hLock,0)) {
      DWORD ec = GetLastError();
      ShowError(ec, "program or service already running");
	
      return ec;
  }

    if (aArgs && *aArgs) {
        bool install = false;
        bool uninstall = false;
        bool quiet = false;
		bool debug = false;
		bool run = false;
        char *args = aArgs;
        while (*args) {
            if (*args == '-' || *args == '/') {
                ++args;
				switch (*args) {
				case 'i':
				case 'I': install = true; break;
				case 'u':
				case 'U': uninstall = true; break;
				case 'q':
				case 'Q': quiet = true; break;
				case 'd':
				case 'D': debug = true; break;
				case 's':
				case 'S': run = true; break;
				default: ShowHelp(); return -1;
                }
                ++args;
            }
            else if (*args == ' ') {
                ++args;
            }
            else {
                ShowHelp();
                return -1;
            }
        }
        
		if (install) {
            return InstallService(quiet);
        }

        if (uninstall) {
            return UninstallService(quiet);
        }

		if (debug) {
			WorkerThread(std::stop_token{});
			return 0;
		}

		if (run) {
			// HANDLE hLockS = CreateMutex(NULL,FALSE,"TPFanControlMutex02");
			SERVICE_TABLE_ENTRY svcEntry[2];
			svcEntry[0].lpServiceName = const_cast<LPSTR>(g_ServiceName);
			svcEntry[0].lpServiceProc = ServiceMain;
			svcEntry[1].lpServiceName = NULL;
			svcEntry[1].lpServiceProc = NULL;
			StartServiceCtrlDispatcher(svcEntry);
		}
    }
    else {
		WorkerThread(std::stop_token{});
		return 0;
    }

    return 0;
}

void ShowHelp() {
    MessageBox(NULL, "Usage:\n\n-i Install service\n-u Uninstall service\n-q Quiet - Don't show possible error messages", "Usage", MB_OK);
}

DWORD InstallService(bool quiet) {
    SC_HANDLE SCMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!SCMgr) {
        DWORD ec = GetLastError();
        if (!quiet) ShowError(ec, "Could not open Service Control Manager");
        return ec;
    }

    char ExePath[MAX_PATH];
    GetModuleFileName(NULL, ExePath, sizeof(ExePath));
    std::string fullPath = std::format("{} -s", ExePath);

    SC_HANDLE svc = CreateService(SCMgr, g_ServiceName, g_ServiceName, SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, 
        fullPath.c_str(), NULL, NULL, NULL, NULL, NULL);

    if (!svc) {
        CloseServiceHandle(SCMgr);
        DWORD ec = GetLastError();
        if (!quiet) ShowError(ec, "Could not install service");
        return ec;
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(SCMgr);

    return 0;
}

DWORD UninstallService(bool quiet) {
    SC_HANDLE SCMgr = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!SCMgr) {
        DWORD ec = GetLastError();
        if (!quiet) ShowError(ec, "Could not open Service Control Manager");
        return ec;
    }

    SC_HANDLE hdl = OpenService(SCMgr, g_ServiceName, DELETE);
    if (!hdl) {
        return 0;
    }

    if (!DeleteService(hdl)) {
        DWORD ec = GetLastError();
        if (!quiet) ShowError(ec, "Could not delete service");
        CloseServiceHandle(SCMgr);
        return ec;
    }

    CloseServiceHandle(hdl);
    CloseServiceHandle(SCMgr);

    return 0;
}

void ShowError(DWORD ec, const char *description) { 
    char *msgBuf;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        ec,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &msgBuf,
        0, NULL );

    std::string dispStr = std::format("{}, error code {}: {}", description, ec, msgBuf);
    MessageBox(NULL, dispStr.c_str(), "Error", MB_OK);

    LocalFree(msgBuf);
}

void ShowMessage(const char *title, const char *description) { 
    MessageBox(NULL, description, title, MB_OK);
}

VOID WINAPI ServiceMain(DWORD aArgc, LPTSTR* aArgv) {
    g_SvcHandle = RegisterServiceCtrlHandler(g_ServiceName, Handler);

    g_SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_SvcStatus.dwCurrentState = SERVICE_START_PENDING;
    g_SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_SvcStatus.dwWin32ExitCode = NO_ERROR;
    g_SvcStatus.dwServiceSpecificExitCode = NO_ERROR;
    g_SvcStatus.dwCheckPoint = 0;
    g_SvcStatus.dwWaitHint = 0;
    SetServiceStatus(g_SvcHandle, &g_SvcStatus);

    StartWorkerThread();
    
    g_SvcStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_SvcHandle, &g_SvcStatus);

    return;
}

VOID WINAPI Handler(DWORD fdwControl) {
    switch(fdwControl) {
    case SERVICE_CONTROL_STOP:
        g_SvcStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_SvcHandle, &g_SvcStatus);

        StopWorkerThread();
                
        g_SvcStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_SvcHandle, &g_SvcStatus);

        break;

    default:
        break;
    }
}

static std::unique_ptr<std::jthread> g_jthread;

void StartWorkerThread() {
    g_jthread = std::make_unique<std::jthread>(WorkerThread);
}

void StopWorkerThread() {
    // Send close message to the dialog's message loop
    ::PostMessage(g_dialogWnd, WM_COMMAND, 5020, 0);
    // jthread automatically calls request_stop() and join() on destruction
    g_jthread.reset();
}

void WorkerThread(std::stop_token stopToken) {
	hInstRes=GetModuleHandle(NULL);
	hInstApp=hInstRes;

	::InitCommonControls();

	// Change to the directory where the exe resides
	char exepath[MAX_PATH];
	*exepath = '\0';
	if (GetModuleFileName(NULL, exepath, MAX_PATH))	{
		char *p = exepath + strlen(exepath) - 1;
		while (p > exepath) {
			if (*p == '\\')	{
				*p = '\0';
				::SetCurrentDirectory(exepath);
				break;
			}
			--p;
		}
	}

    bool ok = false;
	bool HardAccess = false;
	bool NewHardAccess = true;

    // Try to open TVicPort driver, with cooperative cancellation support
    for (int i = 0; i < 180 && !stopToken.stop_requested(); i++) {
        if (OpenTVicPort()) {
            ok = true;
            break;
        }
        // Use shorter sleep intervals to respond to stop requests faster
        for (int j = 0; j < 10 && !stopToken.stop_requested(); j++) {
            ::Sleep(100);
        }
    }
    
    // Early exit if stop was requested during initialization
    if (stopToken.stop_requested()) {
        return;
    }
	if (ok) {	
		HardAccess = TestHardAccess();
		SetHardAccess(NewHardAccess);
		HardAccess = TestHardAccess();

		FANCONTROL fc(hInstApp);

        g_dialogWnd = fc.GetDialogWnd();

		fc.ProcessDialog();

		::PostMessage(g_dialogWnd, WM_COMMAND, 5020, 0);
		CloseTVicPort();
	}
	else {
		::MessageBox(HWND_DESKTOP, 
					"Error during initialization of Port Driver.\r\n"
					"(tvicport.sys missing in app folder or failed to load)",
					"Fan Control", 
					MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
	}
}

void debug(const char *msg) {
    spdlog::debug("{}", msg);
}
