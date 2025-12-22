
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
#include "tools.h"
#include "fancontrol.h"


//-------------------------------------------------------------------------
//  read config file
//-------------------------------------------------------------------------
int
FANCONTROL::ReadConfig(const char* configfile)
{
	if (!m_configManager->LoadConfig(configfile)) return false;

	// Sync values from ConfigManager to FANCONTROL (Legacy support)
	this->ActiveMode = m_configManager->ActiveMode;
	this->ManFanSpeed = m_configManager->ManFanSpeed;
	this->Cycle = m_configManager->Cycle;
	this->IconCycle = m_configManager->IconCycle;
	this->ReIcCycle = m_configManager->ReIcCycle;
	this->iFontIconB = m_configManager->IconFontSize;
	this->FanSpeedLowByte = m_configManager->FanSpeedLowByte;
	this->NoExtSensor = m_configManager->NoExtSensor;
	this->SlimDialog = m_configManager->SlimDialog;
	this->FanBeepFreq = m_configManager->FanBeepFreq;
	this->FanBeepDura = m_configManager->FanBeepDura;
	this->NoWaitMessage = m_configManager->NoWaitMessage;
	this->StartMinimized = m_configManager->StartMinimized;
	this->NoBallons = m_configManager->NoBallons;
	this->IconColorFan = m_configManager->IconColorFan;
	this->Lev64Norm = m_configManager->Lev64Norm;
	this->BluetoothEDR = m_configManager->BluetoothEDR;
	this->ManModeExit = m_configManager->ManModeExit;
	this->ShowBiasedTemps = m_configManager->ShowBiasedTemps;
	this->MaxReadErrors = m_configManager->MaxReadErrors;
	this->SecWinUptime = m_configManager->SecWinUptime;
	this->SecStartDelay = m_configManager->SecStartDelay;
	this->Log2File = m_configManager->Log2File;
	this->StayOnTop = m_configManager->StayOnTop;
	this->Log2csv = m_configManager->Log2csv;
	this->ShowAll = m_configManager->ShowAll;
	this->ShowTempIcon = m_configManager->ShowTempIcon;
	this->Fahrenheit = m_configManager->Fahrenheit;
	this->MinimizeToSysTray = m_configManager->MinimizeToSysTray;
	this->MinimizeOnClose = m_configManager->MinimizeOnClose;
	this->UseTWR = m_configManager->UseTWR;

	strcpy_s(this->MenuLabelSM1, sizeof(this->MenuLabelSM1), m_configManager->MenuLabelSM1.c_str());
	strcpy_s(this->MenuLabelSM2, sizeof(this->MenuLabelSM2), m_configManager->MenuLabelSM2.c_str());
	strcpy_s(this->IgnoreSensors, sizeof(this->IgnoreSensors), m_configManager->IgnoreSensors.c_str());

	for (size_t i = 0; i < m_configManager->SmartLevels1.size() && i < 32; i++) {
		this->SmartLevels[i].temp = m_configManager->SmartLevels1[i].temp;
		this->SmartLevels[i].fan = m_configManager->SmartLevels1[i].fan;
		this->SmartLevels[i].hystUp = m_configManager->SmartLevels1[i].hystUp;
		this->SmartLevels[i].hystDown = m_configManager->SmartLevels1[i].hystDown;
		
		this->SmartLevels1[i].temp1 = m_configManager->SmartLevels1[i].temp;
		this->SmartLevels1[i].fan1 = m_configManager->SmartLevels1[i].fan;
		this->SmartLevels1[i].hystUp1 = m_configManager->SmartLevels1[i].hystUp;
		this->SmartLevels1[i].hystDown1 = m_configManager->SmartLevels1[i].hystDown;
	}

	for (size_t i = 0; i < m_configManager->SmartLevels2.size() && i < 32; i++) {
		this->SmartLevels2[i].temp2 = m_configManager->SmartLevels2[i].temp;
		this->SmartLevels2[i].fan2 = m_configManager->SmartLevels2[i].fan;
		this->SmartLevels2[i].hystUp2 = m_configManager->SmartLevels2[i].hystUp;
		this->SmartLevels2[i].hystDown2 = m_configManager->SmartLevels2[i].hystDown;
	}

	for (int i = 0; i < 3; i++) {
		this->IconLevels[i] = m_configManager->IconLevels[i];
	}

	for (int i = 0; i < 16; i++) {
		this->SensorOffset[i].offs = m_configManager->SensorOffsets[i].offset;
		this->SensorOffset[i].hystMin = m_configManager->SensorOffsets[i].hystMin;
		this->SensorOffset[i].hystMax = m_configManager->SensorOffsets[i].hystMax;
		
		// Sync to SensorManager
		m_sensorManager->SetOffset(i, this->SensorOffset[i].offs, this->SensorOffset[i].hystMin, this->SensorOffset[i].hystMax);
		if (i < 12) {
			m_sensorManager->SetSensorName(i, this->gSensorNames[i]);
		}
	}

	// Hotkeys
	this->HK_BIOS_Method = m_configManager->HK_BIOS.method;
	this->HK_BIOS = m_configManager->HK_BIOS.key;
	this->HK_Manual_Method = m_configManager->HK_Manual.method;
	this->HK_Manual = m_configManager->HK_Manual.key;
	this->HK_Smart_Method = m_configManager->HK_Smart.method;
	this->HK_Smart = m_configManager->HK_Smart.key;
	this->HK_SM1_Method = m_configManager->HK_SM1.method;
	this->HK_SM1 = m_configManager->HK_SM1.key;
	this->HK_SM2_Method = m_configManager->HK_SM2.method;
	this->HK_SM2 = m_configManager->HK_SM2.key;
	this->HK_TG_BS_Method = m_configManager->HK_TG_BS.method;
	this->HK_TG_BS = m_configManager->HK_TG_BS.key;
	this->HK_TG_BM_Method = m_configManager->HK_TG_BM.method;
	this->HK_TG_BM = m_configManager->HK_TG_BM.key;
	this->HK_TG_MS_Method = m_configManager->HK_TG_MS.method;
	this->HK_TG_MS = m_configManager->HK_TG_MS.key;
	this->HK_TG_12_Method = m_configManager->HK_TG_12.method;
	this->HK_TG_12 = m_configManager->HK_TG_12.key;

	// Set process priority
	::SetPriorityClass(::GetCurrentProcess(), 
		m_configManager->ProcessPriority == 1 ? HIGH_PRIORITY_CLASS : 
		m_configManager->ProcessPriority == 2 ? ABOVE_NORMAL_PRIORITY_CLASS : 
		NORMAL_PRIORITY_CLASS);

	return true;
}

//-------------------------------------------------------------------------
//  localized date&time
//-------------------------------------------------------------------------
void
FANCONTROL::CurrentDateTimeLocalized(char* result, size_t sizeof_result) {
	SYSTEMTIME s;
	::GetLocalTime(&s);

	char otfmt[64] = "HH:mm:ss", otime[64];
	char odfmt[128], odate[64];

	::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, otfmt, sizeof(otfmt));

	::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &s, otfmt, otime, sizeof(otime));

	::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, odfmt, sizeof(odfmt));

	::GetDateFormat(LOCALE_USER_DEFAULT, 0, &s, odfmt, odate, sizeof(odate));

	setzero(result, sizeof_result);
	strncpy_s(result, sizeof_result, odate, sizeof_result - 2);
	strcat_s(result, sizeof_result, " ");
	strncat_s(result, sizeof_result, otime, sizeof_result - strlen(result) - 1);
}

//-------------------------------------------------------------------------
//  localized time
//-------------------------------------------------------------------------
void
FANCONTROL::CurrentTimeLocalized(char* result, size_t sizeof_result) {
	SYSTEMTIME s;
	::GetLocalTime(&s);

	char otfmt[64] = "HH:mm:ss", otime[64];
	// char odfmt[128], odate[64];

	::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, otfmt, sizeof(otfmt));

	::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &s, otfmt, otime, sizeof(otime));

	// ::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, odfmt, sizeof(odfmt));

	// ::GetDateFormat(LOCALE_USER_DEFAULT, 0,	&s, odfmt, odate, sizeof(odate));

	setzero(result, sizeof_result);
	// strncpy_s(result,sizeof_result, odate, sizeof_result-2);
	// strcat_s(result,sizeof_result, " ");
	strncat_s(result, sizeof_result, otime, sizeof_result - 1);
	// strncat_s(result,sizeof_result, otime, sizeof_result-strlen(result)-1);
}

//-------------------------------------------------------------------------
//  
//-------------------------------------------------------------------------
bool
FANCONTROL::IsMinimized(void) const {
	WINDOWPLACEMENT wp = NULLSTRUCT;

	::GetWindowPlacement(this->hwndDialog, &wp);

	return wp.showCmd == SW_SHOWMINIMIZED;
}

//-------------------------------------------------------------------------
//  show trace output in lower window part
//-------------------------------------------------------------------------
void
FANCONTROL::Trace(const char* text) {
	char trace[16384] = "", datebuf[128] = "", line[512] = "", linecsv[512] = "";

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	if (strlen(text))
		sprintf_s(line, sizeof(line), "[%s] %s\r\n", datebuf, text);	// probably acpi reading conflict
	else
		strcpy_s(line, sizeof(line), "\r\n");

	::GetDlgItemText(this->hwndDialog, 9200, trace, sizeof(trace) - strlen(line) - 1);

	strcat_s(trace, sizeof(trace), line);

	// display 100 lines max
	char* p = trace + strlen(trace);
	int linecount = 0;

	while (p >= trace) {
		if (*p == '\n') {
			linecount++;
			if (linecount > 100)
				break;
		}

		p--;
	}

	// write logfile
	if (this->Log2File == 1) {
		FILE* flog;
		errno_t errflog = fopen_s(&flog, "TPFanControl.log", "ab");
		if (!errflog) {
			//TODO: fwrite_s
			fwrite(line, strlen(line), 1, flog);
			fclose(flog);
		}
	}

	// redisplay log and scroll to bottom
	::SetDlgItemText(this->hwndDialog, 9200, p + 1);
	::SendDlgItemMessage(this->hwndDialog, 9200, EM_SETSEL, strlen(trace) - 2, strlen(trace) - 2);
	::SendDlgItemMessage(this->hwndDialog, 9200, EM_LINESCROLL, 0, 9999);
}

void
FANCONTROL::Tracecsv(const char* text) {
	char trace[16384] = "", datebuf[128] = "", line[512] = "";

	this->CurrentTimeLocalized(datebuf, sizeof(datebuf));

	if (strlen(text))
		sprintf_s(line, sizeof(line), "%s; %s\r\n", datebuf, text);	// probably acpi reading conflict
	else
		strcpy_s(line, sizeof(line), "\r\n");

	// write logfile
	if (this->Log2csv == 1) {
		FILE* flogcsv;
		errno_t errflogcsv = fopen_s(&flogcsv, "TPFanControl_csv.txt", "ab");
		if (!errflogcsv) { 
			fwrite(line, strlen_s(line, sizeof(line)), 1, flogcsv); 
			fclose(flogcsv); 
		}
	}
}

void
FANCONTROL::Tracecsvod(const char* text) {
	char trace[16384] = "", datebuf[128] = "", line[512] = "";

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	if (strlen(text))
		sprintf_s(line, sizeof(line), "%s\r\n", text);	// probably acpi reading conflict
	else
		strcpy_s(line, sizeof(line), "\r\n");

	// write logfile
	if (this->Log2csv == 1) {
		FILE* flogcsv;
		errno_t errflogcsv = fopen_s(&flogcsv, "TPFanControl_csv.txt", "ab");
		if (!errflogcsv) { 
			fwrite(line, strlen(line), 1, flogcsv); 
			fclose(flogcsv); 
		}
	}
}

//-------------------------------------------------------------------------
//  create a thread
//-------------------------------------------------------------------------
HANDLE
FANCONTROL::CreateThread(int(_stdcall* fnct)(ULONG), ULONG p) {
	LPTHREAD_START_ROUTINE thread = (LPTHREAD_START_ROUTINE)fnct;
	DWORD tid;
	HANDLE hThread;
	hThread = ::CreateThread(NULL, 8 * 4096, thread, (void*)p, 0, &tid);
	return hThread;
}