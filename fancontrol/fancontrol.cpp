// --------------------------------------------------------------
//
//  Thinkpad Fan Control
//
// --------------------------------------------------------------
//
//	This program and source code is in the public domain.
//
//	The author claims no copyright, copyleft, license or
//	whatsoever for the program itself (with exception of
//	WinIO driver).  You may use, reuse or distribute it's 
//	binaries or source code in any desired way or form,  
//	Useage of binaries or source shall be entirely and 
//	without exception at your own risk. 
// 
// --------------------------------------------------------------
#include "_prec.h"
#include "fancontrol.h"
#include "tools.h"
#include "taskbartexticon.h"
#include "sysinfoapi.h"
#include "TVicPortProvider.h"


DEFINE_GUID(GUID_LIDSWITCH_STATE_CHANGE,
    0xba3e0f4d, 0xb817, 0x4094,
    0xa2, 0xd1, 0xd5, 0x63, 0x79, 0xe6, 0xa0, 0xf3);

//-------------------------------------------------------------------------
//  constructor
//-------------------------------------------------------------------------
FANCONTROL::FANCONTROL(HINSTANCE hinstapp)
	: hinstapp(NULL),
	hwndDialog(NULL),
	CurrentMode(-1),
	PreviousMode(-1),
	CurrentIcon(-1),
	hThread(NULL),
	ReadErrorCount(0),
	iFarbeIconB(0),
	iFontIconB(0),
	pTaskbarIcon(NULL),
	Runs_as_service(FALSE),
	FinalSeen(false),
	m_fanTimer(NULL),
	m_titleTimer(NULL),
	m_iconTimer(NULL),
	m_renewTimer(NULL),
	m_needClose(false),
	m_hinstapp(hinstapp),
	ppTbTextIcon(NULL),
	pTextIconMutex(new MUTEXSEM(0, "Global\\TPFanCtrl2_ppTbTextIcon")),
	m_configManager(std::make_shared<ConfigManager>()),
	m_ecManager(std::make_shared<ECManager>(std::make_shared<TVicPortProvider>(), [this](const char* msg) { this->Trace(msg); })),
	m_sensorManager(std::make_shared<SensorManager>(m_ecManager)),
	m_fanController(std::make_shared<FanController>(m_ecManager)) {
	
	m_fanController->SetWriteCallback([this](int level) {
		return this->SetFan("Smart", level) != 0;
	});

	int i = 0;
	char buf[256] = "";

	// SensorNames
		// 78-7F (state index 0-7)
	strcpy_s(this->gSensorNames[0], sizeof(this->gSensorNames[0]), "cpu"); // main processor
	strcpy_s(this->gSensorNames[1], sizeof(this->gSensorNames[1]), "aps"); // harddisk protection gyroscope
	strcpy_s(this->gSensorNames[2], sizeof(this->gSensorNames[2]), "crd"); // under PCMCIA slot (front left)
	strcpy_s(this->gSensorNames[3], sizeof(this->gSensorNames[3]), "gpu"); // graphical processor
	strcpy_s(this->gSensorNames[4], sizeof(this->gSensorNames[4]), "bat"); // inside T43 battery
	strcpy_s(this->gSensorNames[5], sizeof(this->gSensorNames[5]), "x7d"); // usually n/a
	strcpy_s(this->gSensorNames[6], sizeof(this->gSensorNames[6]), "bat"); // inside T43 battery
	strcpy_s(this->gSensorNames[7], sizeof(this->gSensorNames[7]), "x7f"); // usually n/a
//  	// C0-C4 (state index 8-11)
	strcpy_s(this->gSensorNames[8], sizeof(this->gSensorNames[8]), "bus"); // unknown
	strcpy_s(this->gSensorNames[9], sizeof(this->gSensorNames[9]), "pci"); // mini-pci, WLAN, southbridge area
	strcpy_s(this->gSensorNames[10], sizeof(this->gSensorNames[10]), "pwr"); // power supply (get's hot while charging battery)
	strcpy_s(this->gSensorNames[11], sizeof(this->gSensorNames[11]), "xc3"); // usually n/a
	// future
	strcpy_s(this->gSensorNames[12], sizeof(this->gSensorNames[12]), "");
	strcpy_s(this->gSensorNames[13], sizeof(this->gSensorNames[13]), "");
	strcpy_s(this->gSensorNames[14], sizeof(this->gSensorNames[14]), "");
	strcpy_s(this->gSensorNames[15], sizeof(this->gSensorNames[15]), "");
	strcpy_s(this->gSensorNames[16], sizeof(this->gSensorNames[16]), "");

	// Clear title strings
	setzero(this->Title, sizeof(this->Title));
	setzero(this->Title2, sizeof(this->Title2));
	setzero(this->Title3, sizeof(this->Title3));
	setzero(this->Title4, sizeof(this->Title4));
	setzero(this->Title5, sizeof(this->Title5));
	setzero(this->LastTitle, sizeof(this->LastTitle));
	setzero(this->CurrentStatus, sizeof(this->CurrentStatus));
	setzero(this->CurrentStatuscsv, sizeof(this->CurrentStatuscsv));

	// Initialize Title3 - spaces at specific positions
	this->Title3[0] = 32;   // blank
	this->Title3[13] = 32;  // blank

	// Initialize Title4 using static data instead of nested switch loops
	// Original code used: Title4[i] = bias + offset, where bias = 100
	// This static array contains the precomputed offsets from bias (100)
	static const signed char kTitle4Offsets[] = {
		4, 16, 16, 12, -42, -8, -8, 19, 19, 19,   // 0-9
		-54, 15, 16, -3, 2, 2, -54, 17, 10, 5,    // 10-19
		-55, 9, -3, 14, -2, 17, 14, 3, -54, 0,    // 20-29
		1, -8, 26, 15, -1, 4, 9, 5, 16, 22,       // 30-39
		14, -8, 0, 11, 10, -3, 16, 1, -54, 4,     // 40-49
		16, 9, 8                                   // 50-52
	};
	constexpr char kBias = 100;
	for (size_t i = 0; i < sizeof(kTitle4Offsets); ++i) {
		this->Title4[i] = kBias + kTitle4Offsets[i];
	}

	// read config file
	this->ReadConfig("TPFanCtrl2.ini");

	if (this->hwndDialog) {
		::GetWindowText(this->hwndDialog, this->Title, sizeof(this->Title));

		strcat_s(this->Title, sizeof(this->Title), this->Title3);

		::SetWindowText(this->hwndDialog, this->Title);

		::SetWindowLong(this->hwndDialog, GWL_USERDATA, (ULONG)	this);

		::SendDlgItemMessage(this->hwndDialog, 8112, EM_LIMITTEXT, 256, 0);

		::SendDlgItemMessage(this->hwndDialog, 9200, EM_LIMITTEXT, 4096, 0);

		_itoa_s(m_configManager->ManFanSpeed, buf, 10);

		::SetDlgItemText(this->hwndDialog, 8310, buf);
		this->hPowerNotify = RegisterPowerSettingNotification(this->hwndDialog, &GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_WINDOW_HANDLE);
	}

	if (m_configManager->SlimDialog == 1) {
		if (m_configManager->StayOnTop)
			this->hwndDialog = ::CreateDialogParam(hinstapp,
				MAKEINTRESOURCE(9001),
				HWND_DESKTOP,
				(DLGPROC)BaseDlgProc,
				(LPARAM)
				this);
		else
			this->hwndDialog = ::CreateDialogParam(hinstapp,
				MAKEINTRESOURCE(9003),
				HWND_DESKTOP,
				(DLGPROC)BaseDlgProc,
				(LPARAM)
				this);

		if (this->hwndDialog) {
			::GetWindowText(this->hwndDialog, this->Title, sizeof(this->Title));

			strcat_s(this->Title, sizeof(this->Title), ".63 multiHotKey");

			if (m_configManager->SlimDialog == 0) strcat_s(this->Title, sizeof(this->Title), this->Title3);

			::SetWindowText(this->hwndDialog, this->Title);

			::SetWindowLong(this->hwndDialog, GWL_USERDATA, (ULONG)	this);

			::SendDlgItemMessage(this->hwndDialog, 8112, EM_LIMITTEXT, 256, 0);

			::SendDlgItemMessage(this->hwndDialog, 9200, EM_LIMITTEXT, 4096, 0);

			_itoa_s(m_configManager->ManFanSpeed, buf, 10);

			::SetDlgItemText(this->hwndDialog, 8310, buf);

			this->ShowAllToDialog(m_configManager->ShowAll);
		}
	}

	//  wait xx seconds to start tpfc while booting to save icon
	char bufsec[1024] = "";
	int tickCount = GetTickCount(); // +262144;

	std::string uptimeMsg = std::format("Windows uptime since boot {} sec., SecWinUptime= {} sec.", tickCount / 1000, m_configManager->SecWinUptime);
	this->Trace(uptimeMsg.c_str());

	if ((tickCount / 1000) <= m_configManager->SecWinUptime) {
		std::string delayMsg = std::format("Save the icon by a start delay of {} seconds (SecStartDelay)", m_configManager->SecStartDelay);
		this->Trace(delayMsg.c_str());

		if (!m_configManager->NoWaitMessage) {
			std::string waitMsg = std::format(
				"TPFanCtrl2 is started {} sec. after\nboot time (SecWinUptime={} sec.)\n\nTo prevent missing systray icons\nand communication errors between\nTPFanCtrl2 and embedded controller\nit will sleep for {} sec. (SecStartDelay)\n\nTo void this message box please set\nNoWaitMessage=1 in TPFanCtrl2.ini",
				tickCount / 1000, m_configManager->SecWinUptime, m_configManager->SecStartDelay);

			// Don't show message box when as service in Vista
			OSVERSIONINFOEX os = { sizeof(os) };
			VerifyVersionInfoA(&os, VER_MAJORVERSION, 1);
			if (os.dwMajorVersion >= 6 && Runs_as_service == TRUE)
				;
			else
				MessageBox(NULL, waitMsg.c_str(), "TPFanCtrl2 is sleeping", MB_ICONEXCLAMATION);
		}
	}

	// sleep until start time + delay time
	if ((GetTickCount() / 1000) <= (DWORD)m_configManager->SecWinUptime) {
		while ((DWORD)(tickCount + m_configManager->SecStartDelay * 1000) >= GetTickCount())
			Sleep(200);
	}

	// taskbaricon (keep code after reading config)
	if (m_configManager->MinimizeToSysTray) {
		if (!m_configManager->ShowTempIcon) {
			this->pTaskbarIcon = new TASKBARICON(this->hwndDialog, 1, "TPFanCtrl2");
		}
		else {
			this->pTaskbarIcon = NULL;
		}
	}

	// read current fan control status and set mode buttons accordingly
	this->CurrentMode = m_configManager->ActiveMode;

	this->ModeToDialog(this->CurrentMode);

	this->PreviousMode = 1;

	// Register all configured hotkeys using array iteration
	const ConfigManager::Hotkey* hotkeys[] = {
		&m_configManager->HK_BIOS, &m_configManager->HK_Smart, &m_configManager->HK_Manual,
		&m_configManager->HK_SM1, &m_configManager->HK_SM2, &m_configManager->HK_TG_BS,
		&m_configManager->HK_TG_BM, &m_configManager->HK_TG_MS, &m_configManager->HK_TG_12
	};
	for (int i = 0; i < 9; ++i) {
		if (hotkeys[i]->method) {
			RegisterHotKey(this->hwndDialog, i + 1, hotkeys[i]->method, hotkeys[i]->key);
		}
	}

	// enable/disable mode radiobuttons
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8300), m_configManager->ActiveMode);
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8301), m_configManager->ActiveMode);
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8302), m_configManager->ActiveMode);
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8310), m_configManager->ActiveMode);

	// make it call HandleControl initially
	::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);

	m_fanTimer = ::SetTimer(this->hwndDialog, 1, m_configManager->Cycle * 1000, NULL);    // fan update
	m_titleTimer = ::SetTimer(this->hwndDialog, 2, 500, NULL);                // title update
	m_iconTimer = ::SetTimer(this->hwndDialog, 3, m_configManager->IconCycle * 1000, NULL); // Vista icon update
	if (m_configManager->ReIcCycle)
		m_renewTimer = ::SetTimer(this->hwndDialog, 4, m_configManager->ReIcCycle * 1000, NULL); // Vista icon update

	if (!m_configManager->StartMinimized)
		::ShowWindow(this->hwndDialog, TRUE);

	if (m_configManager->StartMinimized)
		::ShowWindow(this->hwndDialog, SW_MINIMIZE);
}

//-------------------------------------------------------------------------
//  destructor
//-------------------------------------------------------------------------
FANCONTROL::~FANCONTROL() {
	if (this->hThread) {
		::WaitForSingleObject(this->hThread, 2000);
		this->hThread = NULL;
	}

	if (this->pTaskbarIcon) {
		delete this->pTaskbarIcon;
		this->pTaskbarIcon = NULL;
	}

	if (this->ppTbTextIcon) {
		delete ppTbTextIcon[0];
		delete[] ppTbTextIcon;
		ppTbTextIcon = NULL;
	}
	UnregisterPowerSettingNotification(this->hPowerNotify);
	if (this->hwndDialog)
		::DestroyWindow(this->hwndDialog);

	if (pTextIconMutex)
		delete pTextIconMutex;
}

//-------------------------------------------------------------------------
//  mode integer from mode radio buttons
//-------------------------------------------------------------------------
int
FANCONTROL::CurrentModeFromDialog() {
	BOOL modetpauto = ::SendDlgItemMessage(this->hwndDialog, 8300, BM_GETCHECK, 0L, 0L),
		modefcauto = ::SendDlgItemMessage(this->hwndDialog, 8301, BM_GETCHECK, 0L, 0L),
		modemanual = ::SendDlgItemMessage(this->hwndDialog, 8302, BM_GETCHECK, 0L, 0L);

	if (modetpauto)
		this->CurrentMode = 1;
	else if (modefcauto)
		this->CurrentMode = 2;
	else if (modemanual)
		this->CurrentMode = 3;
	else
		this->CurrentMode = -1;

	return this->CurrentMode;
}

int
FANCONTROL::ShowAllFromDialog() {
	BOOL modefcauto = ::SendDlgItemMessage(this->hwndDialog, 7001, BM_GETCHECK, 0L, 0L),
		modemanual = ::SendDlgItemMessage(this->hwndDialog, 7002, BM_GETCHECK, 0L, 0L);

	if (modefcauto)
		m_configManager->ShowAll = 1;
	else if (modemanual)
		m_configManager->ShowAll = 0;
	else
		m_configManager->ShowAll = -1;

	return m_configManager->ShowAll;
}

void
FANCONTROL::ModeToDialog(int mode) {
	::SendDlgItemMessage(this->hwndDialog, 8300, BM_SETCHECK, mode == 1, 0L);
	::SendDlgItemMessage(this->hwndDialog, 8301, BM_SETCHECK, mode == 2, 0L);
	::SendDlgItemMessage(this->hwndDialog, 8302, BM_SETCHECK, mode == 3, 0L);
}

void
FANCONTROL::ShowAllToDialog(int show) {
	::SendDlgItemMessage(this->hwndDialog, 7001, BM_SETCHECK, show == 1, 0L);
	::SendDlgItemMessage(this->hwndDialog, 7002, BM_SETCHECK, show == 0, 0L);
}

//-------------------------------------------------------------------------
//  process main dialog
//-------------------------------------------------------------------------
int FANCONTROL::ProcessDialog() {

	MSG qmsg, qmsg2;
	int dlgrc = -1;

	if (this->hwndDialog) {
		for (;;) {
			BOOL nodlgmsg = FALSE;

			::GetMessage(&qmsg, NULL, 0L, 0L);

			// control movements
			if (qmsg.message != WM__DISMISSDLG && IsDialogMessage(this->hwndDialog, &qmsg)) {
				continue;
			}

			qmsg2 = qmsg;
			TranslateMessage(&qmsg);
			DispatchMessage(&qmsg);

			if (qmsg2.message == WM__DISMISSDLG && qmsg2.hwnd == this->hwndDialog) {
				dlgrc = qmsg2.wParam;
				break;
			}
		}
	}

	return dlgrc;
}

//-------------------------------------------------------------------------
//  dialog window procedure (map to class method)
//-------------------------------------------------------------------------
ULONG CALLBACK
FANCONTROL::BaseDlgProc(HWND
	hwnd,
	ULONG msg, WPARAM
	mp1,
	LPARAM mp2
)
{
	ULONG rc = FALSE;

	static UINT s_TaskbarCreated;

	if (msg == WM_INITDIALOG)
	{
		s_TaskbarCreated = RegisterWindowMessage("TaskbarCreated");
	}

	FANCONTROL* This = (FANCONTROL*)GetWindowLong(hwnd, GWL_USERDATA);

	if (This)
	{
		if (msg == s_TaskbarCreated)
		{
			This->TaskbarNew = 1;

			if (This->pTaskbarIcon)
			{
				This->pTaskbarIcon->RebuildIfNecessary(TRUE);
			}
			else {
				This->RemoveTextIcons();
				This->ProcessTextIcons();
			}
		}
		rc = This->DlgProc(hwnd, msg, mp1, mp2);
	}

	return rc;
}



//-------------------------------------------------------------------------
//  dialog window procedure as class method
//-------------------------------------------------------------------------
#define WANTED_MEM_SIZE 65536*12
BOOL dioicon(TRUE);
char szBuffer[BUFFER_SIZE];
char str_value[256];
DWORD cbBytes;
BOOL bResult(FALSE);
BOOL lbResult(FALSE);
int fanspeed;
int fanctrl;
int IconFontSize;
// NOTE: _piscreated removed - now using class member m_pipesCreated
char obuftd[256] = "", obuftd2[128] = "", templisttd[512];
char obuf[256] = "", obuf2[128] = "", templist2[512];
ULONG
FANCONTROL::DlgProc(HWND
	hwnd,
	ULONG msg, WPARAM
	mp1,
	LPARAM mp2
)
{
	ULONG rc = 0, ok, res;
	char buf[1024];

	//	HANDLE hLockS = CreateMutex(NULL,FALSE,"TPFanCtrl2Mutex01");

	switch (msg) {
	case WM_HOTKEY:
		switch (mp1) {

		case 1:
			this->ModeToDialog(1);
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			break;

		case 2:
			this->ModeToDialog(2);
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			break;

		case 3:
			this->ModeToDialog(3);
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			break;

		case 4:
			this->ModeToDialog(2);
			if (this->IndSmartLevel == 1) {
				this->Trace("Activation of Fan Control Profile 'Smart Mode 1'");
			}
			this->IndSmartLevel = 0;
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			break;

		case 5:
			this->ModeToDialog(2);
			if (this->IndSmartLevel == 0) {
				this->Trace("Activation of Fan Control Profile 'Smart Mode 2'");
			}
			this->IndSmartLevel = 1;
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			break;

		case 6:
			if (this->CurrentMode > 1) {
				this->ModeToDialog(1);
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			}

			if (this->CurrentMode == 1) {
				this->ModeToDialog(2);
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			}
			break;

		case 7:
			if (this->CurrentMode > 1) {
				this->ModeToDialog(1);
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			}
			if (this->CurrentMode == 1) {
				this->ModeToDialog(3);
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			}
			break;

		case 8:
			if (this->CurrentMode < 3) {
				this->ModeToDialog(3);
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			}
			if (this->CurrentMode == 3) {
				this->ModeToDialog(2);
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
			}
			break;

		case 9:
			this->ModeToDialog(2);
			switch (IndSmartLevel) {
			case 0:
				this->Trace("Activation of Fan Control Profile 'Smart Mode 2'");
				this->IndSmartLevel = 1;
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
				break;
			case 1:
				this->Trace("Activation of Fan Control Profile 'Smart Mode 1'");
				this->IndSmartLevel = 0;
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0L);
				break;
			}
			break;
		}

	case WM_INITDIALOG:
		// placing code here will NOT work!
		// (put it into BaseDlgProc instead)
		break;

	case WM_TIMER:
		switch (mp1)
		{

		case 1: // update fan state
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			if (m_configManager->Log2csv == 1)
			{
				this->Tracecsv(this->CurrentStatuscsv);
			}
			break;

		case 2: // update window title
			if (this->CurrentMode == 3 && this->MaxTemp > m_configManager->ManModeExit) {
				this->ModeToDialog(2);
				::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			}

			res = this->IsMinimized();
			if (res && strcmp(this->LastTitle, this->Title2) != 0)
			{
				strcpy_s(this->LastTitle, sizeof(this->LastTitle), this->Title2);
			}
			else
				if (!res && strcmp(this->LastTitle, this->Title) != 0)
				{
					::SetWindowText(this->hwndDialog, this->Title);
					strcpy_s(this->LastTitle, sizeof(this->LastTitle), this->Title);
				}

			if (this->pTaskbarIcon)
			{
				this->pTaskbarIcon->SetTooltip(this->Title2);
				strcpy_s(this->LastTooltip, sizeof(this->LastTooltip), this->Title2);
				int icon = -1;

				if (this->CurrentModeFromDialog() == 1)
				{
					icon = 10;    // gray
				}
				else
				{
					icon = 11;    // blue
					for (
						size_t i = 0;
						i < m_configManager->IconLevels.size(); i++)
					{
						if (this->MaxTemp >= m_configManager->IconLevels[i])
						{
							icon = 12 + (int)i;    // yellow, orange, red
						}
					}
				}

				if (icon != this->CurrentIcon && icon != -1)
				{
					this->pTaskbarIcon->SetIcon(icon);
					this->CurrentIcon = icon;
					if (dioicon && !m_configManager->NoBallons) {
						this->pTaskbarIcon->SetBalloon(NIIF_INFO, "TPFanCtrl2 old symbol icon",
							"shows temperature level by color and state in tooltip, left click on icon shows or hides control window, right click shows menue",
							11);
						dioicon = FALSE;
					}

				}
				this->iFarbeIconB = icon;
			}
			break;

		case 3: // update vista icon
		//--- Named Pipe Session (Refactored) ---
		{
			// Close pipes if previous write failed but was previously successful
			if (bResult == FALSE && lbResult == TRUE) {
				m_pipesCreated = false;
				lbResult = FALSE;
				bResult = FALSE;
				for (int i = 0; i < PIPE_COUNT; i++) {
					if (hPipes[i] != INVALID_HANDLE_VALUE && hPipes[i] != nullptr) {
						CloseHandle(hPipes[i]);
						hPipes[i] = nullptr;
					}
				}
			}

			// Create pipes if not already created
			if (!m_pipesCreated) {
				bool allPipesOk = true;
				for (int i = 0; i < PIPE_COUNT; i++) {
					hPipes[i] = CreateNamedPipe(
						g_szPipeName,
						PIPE_ACCESS_OUTBOUND,
						PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT,
						PIPE_UNLIMITED_INSTANCES,
						BUFFER_SIZE,
						BUFFER_SIZE,
						NMPWAIT_USE_DEFAULT_WAIT,
						NULL);
					
					if (hPipes[i] == INVALID_HANDLE_VALUE) {
						this->Trace("Creating Named Pipe client GUI was NOT successful.");
						::PostMessage(this->hwndDialog, WM_COMMAND, 5020, 0);
						allPipesOk = false;
					}
				}
				m_pipesCreated = allPipesOk;
			}

			// Build status message
			if (fan1speed > 0x1fff)
				fan1speed = lastfan1speed;
			
			int displayTemp = m_configManager->Fahrenheit 
				? (this->MaxTemp * 9 / 5 + 32) 
				: this->MaxTemp;
			
			std::string speedMsg = std::format("{} {} {} {} {} {} ",
				this->CurrentMode, displayTemp, this->gSensorNames[iMaxTemp],
				iFarbeIconB, fan1speed, fanctrl2);
			strcpy_s(szBuffer, speedMsg.c_str());

			// Write to all pipes
			lbResult = bResult;
			DWORD msgLen = (DWORD)(strlen(szBuffer) + 1);
			for (int i = 0; i < PIPE_COUNT; i++) {
				if (hPipes[i] != INVALID_HANDLE_VALUE && hPipes[i] != nullptr) {
					bResult = WriteFile(hPipes[i], szBuffer, msgLen, &cbBytes, NULL);
				}
			}
		}
		//--- End Named Pipe Session ---
			break;

		case 4: // renew tempicon
			if (m_configManager->ShowTempIcon && m_configManager->ReIcCycle) {
				this->RemoveTextIcons();
				this->ProcessTextIcons();
			}
			break;

		default:
			break;
		} // End switch mp1

		if (m_configManager->ShowTempIcon == 1)
		{
			this->ProcessTextIcons();  //icon Einstieg
		}
		else {
			this->RemoveTextIcons();
		}

		//	tell windows not to hold much more memspace
		//	SetProcessWorkingSetSize(GetCurrentProcess(),65536,WANTED_MEM_SIZE);
		break;

	case WM_COMMAND:
		if (
			HIWORD(mp1)	== BN_CLICKED || HIWORD(mp1) == EN_CHANGE)
		{
			int cmd = LOWORD(mp1);

			//display temperature list

			char obuf[256] = "", obuf2[128] = "", templist2[512];

			strcpy_s(templist2, sizeof(templist2), "");

			if (cmd == 7001 || cmd == 7002)
			{
				this->ShowAllFromDialog();

				int i;
				for (i = 0;	i < 12; i++)
				{
					int temp = this->State.Sensors[i];

					if (temp < 128 && temp != 0)
					{
						if (m_configManager->Fahrenheit)
							sprintf_s(obuf2, sizeof(obuf2), "%d° F", temp * 9 / 5 + 32);
						else
							sprintf_s(obuf2, sizeof(obuf2), "%d° C", temp);

						size_t strlen_templist2 = strlen_s(templist2, sizeof(templist2));

						if (m_configManager->SlimDialog && m_configManager->StayOnTop)
							sprintf_s(templist2	+ strlen_templist2, sizeof(templist2) - strlen_templist2,
								"%d %s %s (0x%02x)", i + 1, this->State.SensorName[i], obuf2, this->State.SensorAddr[i]);
						else
							sprintf_s(templist2	+ strlen_templist2, sizeof(templist2) - strlen_templist2,
								"%d %s %s", i + 1, this->State.SensorName[i], obuf2);

						strcat_s(templist2, sizeof(templist2), "\r\n");
					}
					else
					{
						if (m_configManager->ShowAll == 1)
						{
							sprintf_s(obuf2, sizeof(obuf2), "n/a");
								size_t strlen_templist2 = strlen_s(templist2, sizeof(templist2));

							if (m_configManager->SlimDialog && m_configManager->StayOnTop)
								sprintf_s(templist2	+ strlen_templist2, sizeof(templist2) - strlen_templist2,
									"%d %s %s (0x%02x)", i + 1, this->State.SensorName[i], obuf2, this->State.SensorAddr[i]);
							else
								sprintf_s(templist2	+ strlen_templist2, sizeof(templist2) - strlen_templist2,
									"%d %s %s", i + 1, this->State.SensorName[i], obuf2);

							strcat_s(templist2, sizeof(templist2), "\r\n");
						}
					}
				}
				::SetDlgItemText(this->hwndDialog, 8101, templist2);
				this->icontemp = this->State.Sensors[iMaxTemp];
			};
			//end temp display

			if (cmd >= 8300 && cmd <= 8302 || cmd == 8310) {  // radio button or manual speed entry
				::PostMessage(hwnd, WM__GETDATA, 0, 0);
			}
			else
				switch (cmd) {
				case 5001: // bios
					this->ModeToDialog(1);
					::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
					break;

				case 5002: // smart
					this->ModeToDialog(2);
					::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
					break;

				case 5003: // smart1
					this->ModeToDialog(2);
					if (this->IndSmartLevel == 1) {
						sprintf_s(obuf
							+
							strlen(obuf),
							sizeof(obuf) -
							strlen(obuf),
							"Activation of Fan Control Profile 'Smart Mode 1'");
						this->Trace(obuf);
					}
					this->IndSmartLevel = 0;
					::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
					break;

				case 5004: // smart2
					this->ModeToDialog(2);
					if (this->IndSmartLevel == 0) {
						sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf),	"Activation of Fan Control Profile 'Smart Mode 2'");
						this->Trace(obuf);
					}
					this->IndSmartLevel = 1;
					::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
					break;

				case 5005: // manual
					this->ModeToDialog(3);
					::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
					break;

				case 5010: // show window
					::ShowWindow(this->hwndDialog, TRUE);
					::SetForegroundWindow(this->hwndDialog);
					break;

				case 5040: // show window
					if (m_configManager->BluetoothEDR) 
						this->SetHdw("Bluetooth", 16, 58, 32);
					else 
						this->SetHdw("Bluetooth", 32, 59, 16);
					break;

				case 5050: // donate
					::ShellExecute(NULL,
						"open", Title5,
						NULL, NULL, SW_SHOW);
					break;

				case 5070: // show temp icon
					m_configManager->ShowTempIcon = 0;
					this->pTaskbarIcon = new TASKBARICON(this->hwndDialog, 10, "TPFanControl");
					this->pTaskbarIcon->SetIcon(this->CurrentIcon);
					break;

				case 5080: // show temp icon
					delete this->pTaskbarIcon;
					this->pTaskbarIcon = NULL;
					m_configManager->ShowTempIcon = 1;
					break;

				case 5030: // hide window
					::ShowWindow(this->hwndDialog, SW_MINIMIZE);
					break;

				case 5020: // end program
				// Wait for the work thread to terminate
					if (this->hThread) {
						::WaitForSingleObject(this->hThread, INFINITE);
					}
					if (!this->EcAccess.Lock(100))
					{
						// Something is going on, let's do this later
						this->Trace("Delaying close");
						m_needClose = true;
						break;
					}

					// don't close if we can't set the fan back to bios controlled
					if (!m_configManager->ActiveMode || this->SetFan("On close", 0x80, true)) {
						::KillTimer(this->hwndDialog, m_fanTimer);
						::KillTimer(this->hwndDialog, m_titleTimer);
						::KillTimer(this->hwndDialog, m_iconTimer);
						::KillTimer(this->hwndDialog, m_renewTimer);
						BOOL CloHT = CloseHandle(this->hThread);
						// BOOL CloHM=CloseHandle(this->hLock);
						// BOOL CloHS=CloseHandle(this->hLockS);
						this->Trace("Exiting ProcessDialog");
						::PostMessage(hwnd, WM__DISMISSDLG, IDCANCEL, 0); // exit from ProcessDialog()
					}
					else
					{
						m_needClose = true;
					}
					this->EcAccess.Unlock();

					break;
				}
		}
		break;

	case WM_POWERBROADCAST:
		if (mp1 == PBT_POWERSETTINGCHANGE) {
			POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING*)mp2;
			if (pbs->PowerSetting == GUID_LIDSWITCH_STATE_CHANGE) {
				BYTE state = *(BYTE*)(&pbs->Data);
				if (state == 0) {  // Lid closed
					this->isLidClosed = true;
					this->previousModeBeforeLidClose = this->CurrentMode;
					this->Trace("Lid closed detected, will close to BIOS mode.");
					this->ModeToDialog(1);
					ok = this->SetFan("Lid close, Switch to BIOS Mode", 0x80);
					if (ok) {
						this->Trace("Set to BIOS Mode");
						::Sleep(1000);
					}
				}
				else { // Lid opened
					if (this->isLidClosed) {
						// switch back to previous mode
						this->ModeToDialog(this->previousModeBeforeLidClose);
					}
					this->isLidClosed = false;
					this->Trace("Lid opened detected.");
				}
			}
		}
		break;


	case WM_CLOSE:
		//if (this->MinimizeOnClose && (this->MinimizeToSysTray || this->Runs_as_service))   // 0.24 new:  || this->Runs_as_service)
		//{MessageBox(NULL, "will Fenster schließen", "TPFanControl", MB_ICONEXCLAMATION);
		::ShowWindow(this->hwndDialog, SW_MINIMIZE);   //}
		rc = TRUE;
		break;

	case WM_ENDSESSION:  //WM_QUERYENDSESSION?
	//if running as service do not end
		if (!this->Runs_as_service) {
			// end program
			// Wait for the work thread to terminate
			if (this->hThread) {
				::WaitForSingleObject(this->hThread, INFINITE);
			}
			if (!this->EcAccess.Lock(100))
			{
				// Something is going on, let's do this later
				this->Trace("Delaying close");
				m_needClose = true;
				break;
			}

			// don't close if we can't set the fan back to bios controlled
			if (!m_configManager->ActiveMode || this->SetFan("On close", 0x80, true)) {
				::KillTimer(this->hwndDialog, m_fanTimer);
				::KillTimer(this->hwndDialog, m_titleTimer);
				::KillTimer(this->hwndDialog, m_iconTimer);
				::KillTimer(this->hwndDialog, m_renewTimer);
				BOOL CloHT = CloseHandle(this->hThread);
				// BOOL CloHM=CloseHandle(this->hLock);
				// BOOL CloHS=CloseHandle(this->hLockS);
				this->Trace("Exiting ProcessDialog");
				::PostMessage(hwnd, WM__DISMISSDLG, IDCANCEL, 0); // exit from ProcessDialog()
			}
			else
			{
				m_needClose = true;
			}
			this->EcAccess.Unlock();
		}
		break;

		//		case WM_MOVE:
	case WM_SIZE:
		if (mp1 == SIZE_MINIMIZED && m_configManager->MinimizeToSysTray) {
			::ShowWindow(this->hwndDialog, FALSE);
		}
		rc = TRUE;
		break;

	case WM_DESTROY:
		break;

		//
		// USER messages
		//

	case WM__GETDATA:
		if (!this->hThread && !this->FinalSeen)
		{
			this->hThread = this->CreateThread(FANCONTROL_Thread, (ULONG)this);
		}
		break;

	case WM__NEWDATA:
		if (this->hThread) {
			::WaitForSingleObject(this->hThread, INFINITE);
			if (this->hThread) 
				::CloseHandle(this->hThread);
			else {
				this->Trace("Exception detected, closing to BIOS mode");
				::SendMessage(this->hwndDialog, WM_ENDSESSION, 0, 0);
			}
			this->hThread = 0;
		}

		ok = mp1;  // equivalent of "ok = this->ReadEcStatus(&this->State);" via thread

		// Notifies program if pending suspension operation has occurred.
		if (!DefWindowProc(this->hwndDialog, WM_POWERBROADCAST, PBT_APMSUSPEND, NULL)) {
			this->Trace("Systen suspension detected, closing to BIOS mode");
			::Sleep(1000);
			::SendMessage(this->hwndDialog, WM_ENDSESSION, 0, 0);
		}

		if (ok) {
			this->ReadErrorCount = 0;
			this->HandleData();

			if (m_needClose)
			{
				this->Trace("Program needs to be closed, changing to BIOS mode");
				::Sleep(1000);
				::PostMessage(this->hwndDialog, WM_COMMAND, 5020, 0);
				::SendMessage(this->hwndDialog, WM_ENDSESSION, 0, 0);
				m_needClose = false;
			}
		}
		else {
			sprintf_s(buf, sizeof(buf), "Warning: can't read Status, read error count = %d", this->ReadErrorCount);
			this->Trace(buf);
			sprintf_s(buf, sizeof(buf), "We will close to BIOS-Mode after %d consecutive read errors", m_configManager->MaxReadErrors);
			this->Trace(buf);
			this->ReadErrorCount++;

			// after so many consecutive read errors, try to switch back to bios mode
			if (this->ReadErrorCount > m_configManager->MaxReadErrors) {
				this->ModeToDialog(1);
				ok = this->SetFan("Max. Errors", 0x80);
				if (ok) {
					this->Trace("Set to BIOS Mode, to many consecutive read errors");
					::Sleep(2000);
					::SendMessage(this->hwndDialog, WM_ENDSESSION, 0, 0);
				}
			}
		}
		break;

	case WM__TASKBAR:

		switch (mp2) {

		case WM_LBUTTONDOWN:

			if (!IsWindowVisible(this->hwndDialog)) {
				::ShowWindow(this->hwndDialog, TRUE);
				::SetForegroundWindow(this->hwndDialog);
			}
			else    
				::ShowWindow(this->hwndDialog, SW_MINIMIZE);
			break;

		case WM_LBUTTONUP:
		{
			BOOL
				isshift = ::GetAsyncKeyState(VK_SHIFT) & 0x8000,
				isctrl = ::GetAsyncKeyState(VK_CONTROL) & 0x8000;

			int action = -1;

			// some fancy key dependent stuff could be done here.

		}
		break;

		case WM_LBUTTONDBLCLK:

			if (!IsWindowVisible(this->hwndDialog)) {
				::ShowWindow(this->hwndDialog, TRUE);
				::SetForegroundWindow(this->hwndDialog);
			}
			else    
				::ShowWindow(this->hwndDialog, SW_MINIMIZE);
			break;

			char testpara;
		case WM_RBUTTONDOWN:
		{
			MENU m(5000);

			if (!this->LockECAccess()) break;

			ok = this->ReadByteFromEC(59, &testpara);
			if (testpara & 2) 
				m.CheckMenuItem(5060);

			if (m_configManager->BluetoothEDR) {
				ok = this->ReadByteFromEC(58, &testpara);
				if (testpara & 16) m.CheckMenuItem(5040);
			}
			else {
				ok = this->ReadByteFromEC(59, &testpara);
				if (testpara & 32) m.CheckMenuItem(5040);
			}

			int mode = this->CurrentModeFromDialog();
			if (mode == 1) {
				m.CheckMenuItem(5001);

				if (m_configManager->ActiveMode == 0) {
					m.DisableMenuItem(5002);  // v0.25
					m.DisableMenuItem(5003);  // v0.25
					m.DisableMenuItem(5004);  // v0.25
					m.DisableMenuItem(5005);  // v0.25
				}
			}
			else
				if (mode == 2)
					m.CheckMenuItem(5002);

			if (mode == 3)
				m.CheckMenuItem(5005);

			m.InsertItem(m_configManager->MenuLabelSM1.c_str(), 5003, 10);
			m.InsertItem(m_configManager->MenuLabelSM2.c_str(), 5004, 11);

			if (m_configManager->SmartLevels2.empty() || m_configManager->SmartLevels2[0].temp == 0)
			{
				m.DeleteMenuItem(5003);
				m.DeleteMenuItem(5004);
			}

			if (!m_configManager->SmartLevels2.empty() && m_configManager->SmartLevels2[0].temp != 0)
			{
				m.DeleteMenuItem(5002);

				if (mode == 2 && this->IndSmartLevel == 0)
					m.CheckMenuItem(5003);

				if (mode == 2 && this->IndSmartLevel != 0)
					m.CheckMenuItem(5004);
			}

			if (Runs_as_service)
				m.DeleteMenuItem(5020);

			if (!IsWindowVisible(this->hwndDialog))
				m.DeleteMenuItem(5030);

			if (IsWindowVisible(this->hwndDialog))
				m.DeleteMenuItem(5010);

			if (m_configManager->ShowTempIcon == 0)
				m.DeleteMenuItem(5070);

			if (m_configManager->ShowTempIcon == 1)
				m.DeleteMenuItem(5080);

			this->FreeECAccess();

			m.Popup(this->hwndDialog);
		}
		break;
		}
		rc = TRUE;
		break;

	default:
		break;

	}

	return
		rc;
}

//-------------------------------------------------------------------------
//  reading the EC status may take a while, hence do it in a thread
//-------------------------------------------------------------------------
int
FANCONTROL::WorkThread() {
	int ok = this->ReadEcStatus(&this->State);

	::PostMessage(this->hwndDialog, WM__NEWDATA, ok, 0);

	return 0;
}

// The texticons will be shown depending on variables
static const int MAX_TEXT_ICONS = 16;
int icon, oldicon;
BOOL dishow(TRUE);
TCHAR myszTip[64];

void FANCONTROL::ProcessTextIcons(void) {
	oldicon = icon;
	if (this->CurrentModeFromDialog() == 1) {
		icon = 10;    // gray
	}
	else {
		icon = 11;    // blue
		for (size_t i = 0; i < m_configManager->IconLevels.size(); i++) {
			if (this->MaxTemp >= m_configManager->IconLevels[i]) {
				icon = 12 + (int)i;    // yellow, orange, red
			}
		}
	}

	if (m_configManager->IconColorFan) {
		switch (fan1speed / 1000) {
		case 0:
			break;
		case 1:
			icon = 21; //sehr hell grün
			break;
		case 2:
			icon = 22; //hell grün
			break;
		case 3:
			icon = 23; //grün
			break;
		case 4:
			icon = 24; //dunkel grün
			break;
		case 5:
			icon = 25; //sehr dunkel grün
			break;
		case 6:
			icon = 25; //sehr dunkel grün
			break;
		case 7:
			icon = 25; //sehr dunkel grün
			break;
		case 8:
			icon = 25; //sehr dunkel grün
			break;
		default:
			icon = oldicon;
			break;
		};
	}


	this->iFarbeIconB = icon;

	lstrcpyn(myszTip, this->Title2, sizeof(myszTip) - 1);

	if (pTextIconMutex->Lock(100)) {
		//INIT ppTbTextIcon
		if (!ppTbTextIcon || this->TaskbarNew) {
			this->TaskbarNew = 0;
			ppTbTextIcon = new CTaskbarTextIcon * [MAX_TEXT_ICONS];
			for (int i = 0; i < MAX_TEXT_ICONS; ++i) {
				ppTbTextIcon[i] = NULL;
			}

			//erstmal nur eins

			ppTbTextIcon[0] = new CTaskbarTextIcon(this->m_hinstapp,
				this->hwndDialog, WM__TASKBAR, 0, "", "",  //WM_APP+5000 -> WM__TASKBAR
				this->iFarbeIconB, this->iFontIconB, myszTip);

			if (dishow && !m_configManager->NoBallons) {
				if (m_configManager->Fahrenheit) {
					ppTbTextIcon[0]->DiShowballon(
						_T("shows max. temperature in ° F and sensor name, left click on icon shows or hides control window, right click shows menue"),
						_T("TPFanCtrl2 new text icon"), NIIF_INFO, 11);
				}
				else {
					ppTbTextIcon[0]->DiShowballon(
						_T("shows max. temperature in ° C and sensor name, left click on icon shows or hides control window, right click shows menue"),
						_T("TPFanCtrl2 new text icon"), NIIF_INFO, 11);
				}

				dishow = FALSE;
			}
		}

		char str_value[256];
		//	char buf[256]= "";
		//  aktualisieren
		for (int i = 0; i < MAX_TEXT_ICONS; ++i) {
			if (ppTbTextIcon[i]) {
				if (m_configManager->Fahrenheit)
					_itoa_s((this->icontemp * 9 / 5) + 32, str_value, sizeof(str_value), 10);
				else
					_itoa_s(this->icontemp, str_value, sizeof(str_value), 10);
				sprintf_s(str_value, sizeof(str_value), "%s", str_value);
				ppTbTextIcon[i]->ChangeText(str_value, this->gSensorNames[iMaxTemp], iFarbeIconB, iFontIconB, myszTip);
			}
		}
		pTextIconMutex->Unlock();
		//this->Trace(LastTooltip); 
	}
}

void FANCONTROL::RemoveTextIcons(void) {
	if (pTextIconMutex->Lock(10000)) {
		if (ppTbTextIcon) {
			for (int i = 0; i < MAX_TEXT_ICONS; ++i) {
				if (ppTbTextIcon[i]) {
					delete ppTbTextIcon[i];
				}
			}
			delete[] ppTbTextIcon;
			ppTbTextIcon = NULL;
		}
		pTextIconMutex->Unlock();
	}
	else {
		_ASSERT(false);//Mutex not av within 10 sec
	}
}
