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
#include "TVicPort.h"
#include <chrono>

namespace {
	/**
	 * @brief Get the display name for a fan control mode
	 * @param mode The mode number (1=BIOS, 2=Smart, 3=Manual)
	 * @return Mode name string
	 */
	const char* GetModeName(int mode) {
		switch (mode) {
			case 1: return "BIOS";
			case 2: return "Smart";
			case 3: return "Manual";
			default: return "Unknown";
		}
	}

	/**
	 * @brief Get the action description for a fan control mode
	 * @param mode The mode number
	 * @return Action description string
	 */
	const char* GetModeAction(int mode) {
		switch (mode) {
			case 1: return "setting fan speed";
			case 2: return "recalculate fan speed";
			case 3: return "setting fan speed";
			default: return "";
		}
	}
} // anonymous namespace

//-------------------------------------------------------------------------
//  switch fan according to settings
//-------------------------------------------------------------------------
int
FANCONTROL::HandleData(void) {
	char obuf[256] = "", obuf2[128] = "",
		templist[256] = "", templist2[512],
		manlevel[16] = "", title2[128] = "";
	int i, maxtemp, imaxtemp, ok = 0;

	//
	// determine highest temp.
	//

	// Get max temp from SensorManager
	this->MaxTemp = m_sensorManager->GetMaxTemp(this->iMaxTemp, m_configManager->IgnoreSensors);
	maxtemp = this->MaxTemp;
	imaxtemp = this->iMaxTemp;

	//
	// update dialog elements
	//

	// title string (for minimized window)
	std::string title2Str = std::format("{}° {}", this->MaxTemp * (m_configManager->Fahrenheit ? 9 / 5 : 1) + (m_configManager->Fahrenheit ? 32 : 0), m_configManager->Fahrenheit ? "F" : "C");
	strcpy_s(title2, sizeof(title2), title2Str.c_str());

	// display fan state
	int fanctrl = this->State.FanCtrl;
	fanctrl2 = fanctrl;

	std::string obuf2Str;
	std::string title2Extra;
	if (m_configManager->SlimDialog == 1) {
		obuf2Str = std::format("Fan {} ", fanctrl);
		if (fanctrl & 0x80) {
			if (!(m_configManager->SlimDialog && m_configManager->StayOnTop))
				obuf2Str += "(= BIOS)";
			title2Extra = " Default Fan";
		}
		else {
			if (!(m_configManager->SlimDialog && m_configManager->StayOnTop))
				obuf2Str += " Non Bios";
			title2Extra = std::format(" Fan {} ({})", fanctrl & 0x3F, this->CurrentModeFromDialog() == 2 ? "Smart" : "Fixed");
		}
	}
	else {
		obuf2Str = std::format("0x{:02x} (", (unsigned char)fanctrl);
		if (fanctrl & 0x80) {
			obuf2Str += "BIOS Controlled)";
			title2Extra = " Default Fan";
		}
		else {
			obuf2Str += std::format("Fan Level {}, Non Bios)", fanctrl & 0x3F);
			title2Extra = std::format(" Fan {} ({})", fanctrl & 0x3F, this->CurrentModeFromDialog() == 2 ? "Smart" : "Fixed");
		}
	}
	strcpy_s(obuf2, sizeof(obuf2), obuf2Str.c_str());
	strcat_s(title2, sizeof(title2), title2Extra.c_str());

	::SetDlgItemText(this->hwndDialog, 8100, obuf2);

	strcpy_s(this->Title2, sizeof(this->Title2), title2);

	// display fan speeds
	this->lastfan1speed = this->fan1speed;
	this->fan1speed = (this->State.Fan1SpeedHi << 8) | this->State.Fan1SpeedLo;
	if (this->fan1speed > 0x1fff)
		fan1speed = lastfan1speed;

	this->lastfan2speed = this->fan2speed;
	this->fan2speed = (this->State.Fan2SpeedHi << 8) | this->State.Fan2SpeedLo;
	if (this->fan2speed > 0x1fff)
		fan2speed = lastfan2speed;

	std::string rpmStr = std::format("{}/{} RPM", this->fan1speed, this->fan2speed);
	::SetDlgItemText(this->hwndDialog, 8102, rpmStr.c_str());

	// display temperature list
	std::string maxTempStr = std::format("{}° {}", this->MaxTemp * (m_configManager->Fahrenheit ? 9 / 5 : 1) + (m_configManager->Fahrenheit ? 32 : 0), m_configManager->Fahrenheit ? "F" : "C");
	::SetDlgItemText(this->hwndDialog, 8103, maxTempStr.c_str());

	std::string templist2Str;
	for (i = 0; i < 12; i++) {
		int temp = this->State.Sensors[i];

		if (temp != 0 && temp < 128) {
			std::string tempStr = std::format("{}° {}", temp * (m_configManager->Fahrenheit ? 9 / 5 : 1) + (m_configManager->Fahrenheit ? 32 : 0), m_configManager->Fahrenheit ? "F" : "C");

			if (m_configManager->SlimDialog && m_configManager->StayOnTop)
				templist2Str += std::format("{} {} {}", i + 1, this->State.SensorName[i], tempStr);
			else
				templist2Str += std::format("{} {} {} (0x{:02x})", i + 1, this->State.SensorName[i], tempStr, (unsigned char)this->State.SensorAddr[i]);

			templist2Str += "\r\n";
		}
		else if (m_configManager->ShowAll == 1) {
			if (m_configManager->SlimDialog && m_configManager->StayOnTop)
				templist2Str += std::format("{} {} n/a", i + 1, this->State.SensorName[i]);
			else
				templist2Str += std::format("{} {} n/a (0x{:02x})", i + 1, this->State.SensorName[i], (unsigned char)this->State.SensorAddr[i]);

			templist2Str += "\r\n";
		}
	}

	::SetDlgItemText(this->hwndDialog, 8101, templist2Str.c_str());

	this->icontemp = this->State.Sensors[iMaxTemp];

	// compact single line status (combined)
	std::string templistStr;
	for (i = 0; i < 12; i++) {
		if (this->State.Sensors[i] < 128) {
			int tempVal = (this->State.Sensors[i] != 0) 
				? (m_configManager->Fahrenheit ? this->State.Sensors[i] * 9 / 5 + 32 : this->State.Sensors[i])
				: 0;
			templistStr += std::format("{};{}", tempVal, m_configManager->Fahrenheit ? "" : " ");
		}
		else {
			templistStr += std::format("0;{}", m_configManager->Fahrenheit ? "" : " ");
		}
	}

	if (!templistStr.empty()) templistStr.pop_back();

	std::string statusStr = std::format("Fan: 0x{:02x} / Switch: {}° {} ({})", 
		(unsigned char)State.FanCtrl, 
		MaxTemp * (m_configManager->Fahrenheit ? 9 / 5 : 1) + (m_configManager->Fahrenheit ? 32 : 0), 
		m_configManager->Fahrenheit ? "F" : "C",
		templistStr);
	strcpy_s(CurrentStatus, sizeof(CurrentStatus), statusStr.c_str());

	std::string csvStatusStr = std::format("{} {}/{}; {:02x}; {}; ", templistStr, this->fan1speed, this->fan2speed, (unsigned char)State.FanCtrl, MaxTemp);
	strcpy_s(CurrentStatuscsv, sizeof(CurrentStatuscsv), csvStatusStr.c_str());

	::SetDlgItemText(this->hwndDialog, 8112, this->CurrentStatus);

	//
	// handle fan control according to mode
	//

	this->CurrentModeFromDialog();
	this->ShowAllFromDialog();

	switch (this->CurrentMode) {

	case 1: // BIOS
		if (this->PreviousMode != this->CurrentMode) {
			std::string msg = std::format("Change Mode from {}->{}, {}",
				GetModeName(this->PreviousMode), GetModeName(this->CurrentMode),
				GetModeAction(this->CurrentMode));
			this->Trace(msg.c_str());
		}

		if (this->State.FanCtrl != 0x080)
			ok = this->SetFan("BIOS", 0x80);
		break;

	case 2: // Smart
		this->SmartControl();
		break;

	case 3: // Manual
		if (this->PreviousMode != this->CurrentMode) {
			std::string msg = std::format("Change Mode from {}->{}, {}",
				GetModeName(this->PreviousMode), GetModeName(this->CurrentMode),
				GetModeAction(this->CurrentMode));
			this->Trace(msg.c_str());
		}

		::GetDlgItemText(this->hwndDialog, 8310, manlevel, sizeof(manlevel));

		if (isdigit(manlevel[0]) && atoi(manlevel) >= 0 && atoi(manlevel) <= 255) {
			if (this->State.FanCtrl != atoi(manlevel))
				ok = this->SetFan("Manual", atoi(manlevel));
			else
				ok = true;
		}

		break;
	}

	this->PreviousMode = this->CurrentMode;

	if (this->CurrentMode == 3 && this->MaxTemp > m_configManager->ManModeExit)
		this->CurrentMode = 2;

	return ok;
}

//-------------------------------------------------------------------------
//  smart fan control depending on temperature
//-------------------------------------------------------------------------
void
FANCONTROL::SmartControl(void) {
	char obuf[256] = "";

	// Log mode change from BIOS or Manual to Smart
	if (this->PreviousMode != 2 && (this->PreviousMode == 1 || this->PreviousMode == 3)) {
		std::string msg = std::format("Change Mode from {}->Smart, recalculate fan speed",
			GetModeName(this->PreviousMode));
		this->Trace(msg.c_str());
	}

	const std::vector<SmartLevel>& levels = (this->IndSmartLevel == 0) 
		? m_configManager->SmartLevels1 
		: m_configManager->SmartLevels2;

	// Use modernized FanController
	m_fanController->UpdateSmartControl(this->MaxTemp, levels);
	
	// Sync back the state for legacy UI support
	this->State.FanCtrl = m_fanController->GetCurrentFanCtrl();
}

//-------------------------------------------------------------------------
//  set fan state via EC
//-------------------------------------------------------------------------
int
FANCONTROL::SetFan(const char* source, int fanctrl, bool final) {
	int ok = 0;
	char obuf[256] = "", obuf2[256], datebuf[128];

	if (m_configManager->FanBeepFreq && m_configManager->FanBeepDura)
		::Beep(m_configManager->FanBeepFreq, m_configManager->FanBeepDura);

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	std::string obufStr = std::format("{}: Set fan control to 0x{:02x}, ", source, (unsigned char)fanctrl);
	if (!m_configManager->SmartLevels2.empty() && source == std::string("Smart"))
		obufStr += std::format("Mode {}, ", this->IndSmartLevel == 1 ? 2 : 1);
	
	obufStr += "Result: ";

	if (m_configManager->ActiveMode && !this->FinalSeen) {
		if (!this->LockECAccess()) return false;

		// Use modernized FanController
		ok = m_fanController->SetFanLevel(fanctrl, true); // Assuming dual fan as per original code
		
		// Sync back the state
		this->State.FanCtrl = m_fanController->GetCurrentFanCtrl();

		this->FreeECAccess();

		if (ok) {
			obufStr += "OK";
			if (final)
				this->FinalSeen = true;    // prevent further changes when setting final mode

		}
		else {
			obufStr += "FAILED!!";
			ok = false;
		}
	}
	else {
		obufStr += "IGNORED!(passive mode";
	}

	// display result
	std::string obuf2Str = std::format("{}   ({})", obufStr, datebuf);
	::SetDlgItemText(this->hwndDialog, 8113, obuf2Str.c_str());

	this->Trace(this->CurrentStatus);
	this->Trace(obufStr.c_str());

	if (!final)
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);

	return ok;
}

BOOL
FANCONTROL::SetHdw(const char* source, int hdwctrl, int HdwOffset, int AnyWayBit) {
	int ok = 0;
	char obuf[256] = "", obuf2[256], datebuf[128];
	char resultValue = 0;

	if (!this->LockECAccess()) return false;

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	// Use modernized ECManager
	ok = m_ecManager->ToggleBitsWithVerify(HdwOffset, (char)hdwctrl, (char)AnyWayBit, resultValue);

	std::string obufStr = std::format("{}: Set EC register 0x{:02x} to {}, Result: ", source, HdwOffset, (unsigned char)resultValue);

	if (ok) {
		obufStr += "OK";
	}
	else {
		obufStr += "COULD NOT SET HARDWARE STATE!!!!";
	}

	// display result
	std::string obuf2Str = std::format("{}   ({})", obufStr, datebuf);

	::SetDlgItemText(this->hwndDialog, 8113, obuf2Str.c_str());

	this->Trace(obufStr.c_str());

	this->FreeECAccess();

	return ok;
}

//-------------------------------------------------------------------------
//  check two EC status samples for accpetable equivalence
//-------------------------------------------------------------------------
bool
FANCONTROL::SampleMatch(FCSTATE* smp1, FCSTATE* smp2) {
	
	// match for identical fanctrl settings
	if (smp1->FanCtrl != smp2->FanCtrl) return false;

	// insert any further match criteria here:
	// -----------------------
	//
	// if (......) ......
	//
	// -----------------------

	return TRUE;
}

//-------------------------------------------------------------------------
//  lock access to the EC controller
//-------------------------------------------------------------------------
bool
FANCONTROL::LockECAccess() {
	if (m_ecManager->GetMutex().try_lock_for(std::chrono::milliseconds(1000))) {
		return true;
	}

	this->Trace("Could not acquire mutex to read EC status");
	return false;
}

//-------------------------------------------------------------------------
//  unlock access to the EC controller
//-------------------------------------------------------------------------
void
FANCONTROL::FreeECAccess() {
	m_ecManager->GetMutex().unlock();
}

//-------------------------------------------------------------------------
//  read fan and temperatures from embedded controller
//-------------------------------------------------------------------------
bool
FANCONTROL::ReadEcStatus(FCSTATE* pfcstate) {
	int numTries = 10, sleepTicks = 200;

	FCSTATE sample1, sample2;

	if (!this->LockECAccess()) return false;

	// reading from the EC seems to yield erratic results at times (probably
	// due to collision with other drivers reading from the port).  So try
	// up to ten times to read two samples which look ok and have matching
	// values, using the above match function

	for (int i = 0; i < numTries; i++) {
		if (this->ReadEcRaw(&sample1) && this->ReadEcRaw(&sample2) && this->SampleMatch(&sample1, &sample2)) {
			memcpy(pfcstate, &sample2, sizeof(*pfcstate));
			this->FreeECAccess();
			return TRUE;
		}
		if (i < numTries) ::Sleep(sleepTicks);
	}

	this->FreeECAccess();

	this->Trace("failed to read reliable status values from EC");

	return false;
}

//-------------------------------------------------------------------------
//  read fan and temperatures from embedded controller
//-------------------------------------------------------------------------
bool
FANCONTROL::ReadEcRaw(FCSTATE* pfcstate) {
	if (!m_sensorManager->UpdateSensors(m_configManager->ShowBiasedTemps, m_configManager->NoExtSensor, m_configManager->UseTWR)) {
		this->Trace("failed to read sensors from EC");
		return false;
	}

	const auto& sensors = m_sensorManager->GetSensors();
	for (int i = 0; i < 12 && i < (int)sensors.size(); i++) {
		pfcstate->Sensors[i] = (char)sensors[i].biasedTemp;
		pfcstate->SensorAddr[i] = sensors[i].addr;
		pfcstate->SensorName[i] = sensors[i].name.c_str();
	}

	// Fan status
	char fanCtrl;
	if (m_ecManager->ReadByte(TP_ECOFFSET_FAN, &fanCtrl)) {
		pfcstate->FanCtrl = fanCtrl;
		m_fanController->SetCurrentFanCtrl(fanCtrl);
	} else {
		return false;
	}

	int fan1, fan2;
	if (m_fanController->GetFanSpeeds(fan1, fan2)) {
		pfcstate->Fan1SpeedLo = fan1 & 0xFF;
		pfcstate->Fan1SpeedHi = (fan1 >> 8) & 0xFF;
		pfcstate->Fan2SpeedLo = fan2 & 0xFF;
		pfcstate->Fan2SpeedHi = (fan2 >> 8) & 0xFF;
	}

	return true;
}
