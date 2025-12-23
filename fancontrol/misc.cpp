
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

	// Sync to SensorManager
	for (int i = 0; i < 16; i++) {
		m_sensorManager->SetOffset(i, m_configManager->SensorOffsets[i].offset, m_configManager->SensorOffsets[i].hystMin, m_configManager->SensorOffsets[i].hystMax);
		if (i < 12) {
			m_sensorManager->SetSensorName(i, this->gSensorNames[i]);
		}
	}

	// Set process priority
	::SetPriorityClass(::GetCurrentProcess(), 
		m_configManager->ProcessPriority == 1 ? HIGH_PRIORITY_CLASS : 
		m_configManager->ProcessPriority == 2 ? ABOVE_NORMAL_PRIORITY_CLASS : 
		NORMAL_PRIORITY_CLASS);

	// Initial state setup from config
	this->CurrentMode = m_configManager->ActiveMode;

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
	if (m_configManager->Log2File == 1) {
		FILE* flog;
		errno_t errflog = fopen_s(&flog, "TPFanCtrl2.log", "ab");
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

/**
 * @brief Internal helper for CSV logging
 * @param text The text to log
 * @param includeDate Whether to include date in the timestamp
 * @param separator The separator between timestamp and text
 */
void
FANCONTROL::TracecsvInternal(const char* text, bool includeDate, const char* separator) {
	if (m_configManager->Log2csv != 1) return;

	char datebuf[128] = "", line[512] = "";
	
	if (includeDate) {
		this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));
	} else {
		this->CurrentTimeLocalized(datebuf, sizeof(datebuf));
	}

	if (strlen(text)) {
		sprintf_s(line, sizeof(line), "%s%s%s\r\n", 
				  separator[0] ? datebuf : "", separator, text);
	} else {
		strcpy_s(line, sizeof(line), "\r\n");
	}

	FILE* flogcsv;
	errno_t err = fopen_s(&flogcsv, "TPFanCtrl2_csv.txt", "ab");
	if (!err && flogcsv) {
		fwrite(line, strlen(line), 1, flogcsv);
		fclose(flogcsv);
	}
}

void
FANCONTROL::Tracecsv(const char* text) {
	TracecsvInternal(text, false, "; ");
}

void
FANCONTROL::Tracecsvod(const char* text) {
	TracecsvInternal(text, true, "");
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