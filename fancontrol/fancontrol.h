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

#ifndef FANCONTROL_H
#define FANCONTROL_H

#include "_prec.h"

#pragma once


#include "winstuff.h"
#include "TaskbarTextIcon.h"
#include "ECManager.h"
#include "SensorManager.h"
#include "FanController.h"
#include "ConfigManager.h"
#include <memory>
#include <vector>
#include <string>

#define WM__DISMISSDLG WM_USER+5
#define WM__GETDATA WM_USER+6
#define WM__NEWDATA WM_USER+7
#define WM__TASKBAR WM_USER+8

#define setzero(adr, size) memset((void*)(adr), (char)0x00, (size))
#define ARRAYMAX(tab) (sizeof(tab)/sizeof((tab)[0]))
#define NULLSTRUCT    { 0, }

//begin named pipe TPFanCtrl2_01
#define g_szPipeName "\\\\.\\Pipe\\TPFanCtrl2_01"  //Name given to the pipe
//Pipe name format - \\.\pipe\pipename

#define BUFFER_SIZE 1024 //1k
#define ACK_MESG_RECV "Message received successfully"
//end named pipe TPFanControl01

class FANCONTROL {
protected:
	HINSTANCE hinstapp;
	HINSTANCE m_hinstapp;
	HWND hwndDialog;

	UINT_PTR m_fanTimer;
	UINT_PTR m_titleTimer;
	UINT_PTR m_iconTimer;
	UINT_PTR m_renewTimer;

	struct FCSTATE {

		char FanCtrl,
			Fan1SpeedLo,
			Fan1SpeedHi,
			Fan2SpeedLo,
			Fan2SpeedHi;

		char Sensors[12];
		int SensorAddr[12];
		const char* SensorName[12];

	} State;

	int LastSmartLevel = -1;
	int CurrentIcon;
	int IndSmartLevel;
	int icontemp;
	int FinalSeen;
	int CurrentMode, fanctrl2,
		PreviousMode;
	int TaskbarNew;
	int MaxTemp;
	int iMaxTemp;
	int fan1speed, lastfan1speed, fan2speed, lastfan2speed;
	int Runs_as_service;
	int ReadErrorCount;
	int iFarbeIconB;
	int iFontIconB;
	char gSensorNames[17][4];
	HANDLE hThread;
	
	// Named Pipe handles (consolidated from individual hPipe0-7)
	static constexpr int PIPE_COUNT = 8;
	HANDLE hPipes[PIPE_COUNT] = {};
	bool m_pipesCreated = false;
	
	HANDLE hLock;
	HANDLE hLockS;
	BOOL Closing;
	MUTEXSEM EcAccess;
	bool m_needClose;

	char Title[128];
	char Title2[128];
	char Title3[128];
	char Title4[128];
	char Title5[128];
	char LastTitle[128];
	char LastTooltip[128];
	char CurrentStatus[256];
	char CurrentStatuscsv[256];

	// dialog.cpp
	int CurrentModeFromDialog();

	int ShowAllFromDialog();

	void ModeToDialog(int mode);

	void ShowAllToDialog(int mode);

	ULONG DlgProc(HWND hwnd, ULONG msg, WPARAM mp1, LPARAM mp2);

	static ULONG CALLBACK
		BaseDlgProc(HWND
			hwnd,
			ULONG msg, WPARAM
			mp1,
			LPARAM mp2
		);

	//The default app-icon with changing colors
	TASKBARICON* pTaskbarIcon;
	//
	CTaskbarTextIcon** ppTbTextIcon;
	MUTEXSEM* pTextIconMutex;

	// Modernized Managers
	std::shared_ptr<ECManager> m_ecManager;
	std::shared_ptr<SensorManager> m_sensorManager;
	std::shared_ptr<FanController> m_fanController;
	std::shared_ptr<ConfigManager> m_configManager;

	static int _stdcall
		FANCONTROL_Thread(ULONG
			parm) \
	{ return ((FANCONTROL*)parm)->WorkThread(); }

	int WorkThread();

	// fancontrol.cpp
	bool LockECAccess();

	void FreeECAccess();

	bool SampleMatch(FCSTATE* smp1, FCSTATE* smp2);

	bool ReadEcStatus(FCSTATE* pfcstate);

	bool ReadEcRaw(FCSTATE* pfcstate);

	int HandleData();

	void SmartControl();

	int SetFan(const char* source, int level, bool final = false);

	int SetHdw(const char* source, int hdwctrl, int HdwOffset, int AnyWayBit);

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam);

	// for detecting lid closing
	HPOWERNOTIFY hPowerNotify;
	bool isLidClosed = false;
	int previousModeBeforeLidClose = -1;
	// misc.cpp
	int ReadConfig(const char* filename);

	void Trace(const char* text);

	void Tracecsv(const char* textcsv);

	void Tracecsvod(const char* textcsv);

	void TracecsvInternal(const char* text, bool includeDate, const char* separator);

	bool IsMinimized(void) const;

	void FormatLocalizedTime(char* result, size_t sizeof_result, bool includeDate);

	void CurrentDateTimeLocalized(char* result, size_t sizeof_result);

	void CurrentTimeLocalized(char* result, size_t sizeof_result);

	HANDLE CreateThread(int(_stdcall
		* fnct)(ULONG),
		ULONG p
	);

	// portio.cpp
	bool ReadByteFromEC(int offset, char* pdata);

	bool WriteByteToEC(int offset, char data);

public:

	FANCONTROL(HINSTANCE hinstapp);

	~FANCONTROL();

	void Test(void);

	int ProcessDialog();

	HWND GetDialogWnd() { return hwndDialog; }

	HANDLE GetWorkThread() { return hThread; }

	// The texticons will be shown depending on variables
	void ProcessTextIcons(void);

	void RemoveTextIcons(void);
};

#endif // FANCONTROL_H
