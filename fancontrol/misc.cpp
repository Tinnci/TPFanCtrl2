
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
		m_sensorManager->SetSensorWeight(i, m_configManager->SensorWeights[i]);
		
		if (i < 12) {
			if (!m_configManager->SensorNames[i].empty()) {
				m_sensorManager->SetSensorName(i, m_configManager->SensorNames[i]);
			} else {
				m_sensorManager->SetSensorName(i, this->gSensorNames[i]);
			}
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
//  localized date&time - unified implementation
//-------------------------------------------------------------------------
/**
 * @brief Internal helper for localized time formatting
 * @param result Output buffer
 * @param sizeof_result Size of output buffer
 * @param includeDate Whether to include date in the output
 */
void
FANCONTROL::FormatLocalizedTime(char* result, size_t sizeof_result, bool includeDate) {
	SYSTEMTIME s;
	::GetLocalTime(&s);

	char otfmt[64] = "HH:mm:ss", otime[64];
	
	::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, otfmt, sizeof(otfmt));
	::GetTimeFormat(LOCALE_USER_DEFAULT, 0, &s, otfmt, otime, sizeof(otime));

	std::string timeStr;
	if (includeDate) {
		char odfmt[128], odate[64];
		::GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, odfmt, sizeof(odfmt));
		::GetDateFormat(LOCALE_USER_DEFAULT, 0, &s, odfmt, odate, sizeof(odate));
		timeStr = std::format("{} {}", odate, otime);
	} else {
		timeStr = otime;
	}

	setzero(result, sizeof_result);
	strcpy_s(result, sizeof_result, timeStr.c_str());
}

void
FANCONTROL::CurrentDateTimeLocalized(char* result, size_t sizeof_result) {
	FormatLocalizedTime(result, sizeof_result, true);
}

void
FANCONTROL::CurrentTimeLocalized(char* result, size_t sizeof_result) {
	FormatLocalizedTime(result, sizeof_result, false);
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
	char trace[16384] = "", datebuf[128] = "";

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	std::string line = (strlen(text)) ? std::format("[{}] {}\r\n", datebuf, text) : "\r\n";
	
	// spdlog integration
	if (strlen(text)) spdlog::info("{}", text);

	::GetDlgItemText(this->hwndDialog, 9200, trace, sizeof(trace) - line.length() - 1);

	strcat_s(trace, sizeof(trace), line.c_str());

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

	// redisplay log and scroll to bottom
	::SetDlgItemText(this->hwndDialog, 9200, p + 1);
	::SendDlgItemMessage(this->hwndDialog, 9200, EM_SETSEL, (int)strlen(trace) - 2, (int)strlen(trace) - 2);
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

	char datebuf[128] = "";
	
	if (includeDate) {
		this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));
	} else {
		this->CurrentTimeLocalized(datebuf, sizeof(datebuf));
	}

	std::string line;
	if (strlen(text)) {
		line = std::format("{}{}{}\r\n", separator[0] ? datebuf : "", separator, text);
	} else {
		line = "\r\n";
	}

	FILE* flogcsv;
	errno_t err = fopen_s(&flogcsv, "TPFanCtrl2_csv.txt", "ab");
	if (!err && flogcsv) {
		fwrite(line.c_str(), line.length(), 1, flogcsv);
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