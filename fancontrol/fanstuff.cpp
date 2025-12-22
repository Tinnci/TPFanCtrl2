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

	// Sync ignored sensors to SensorManager
	m_sensorManager->SetIgnoreSensors(this->IgnoreSensors);
	
	// Get max temp from SensorManager
	this->MaxTemp = m_sensorManager->GetMaxTemp(this->iMaxTemp);
	maxtemp = this->MaxTemp;
	imaxtemp = this->iMaxTemp;

	//
	// update dialog elements
	//

	// title string (for minimized window)
	if (Fahrenheit)
		sprintf_s(title2, sizeof(title2), "%d° F", this->MaxTemp * 9 / 5 + 32);
	else
		sprintf_s(title2, sizeof(title2), "%d° C", this->MaxTemp);

	// display fan state
	int fanctrl = this->State.FanCtrl;
	fanctrl2 = fanctrl;

	if (this->SlimDialog == 1) {
		sprintf_s(obuf2, sizeof(obuf2), "Fan %d ", fanctrl);
		if (fanctrl & 0x80) {
			if (!(SlimDialog && StayOnTop))
				strcat_s(obuf2, sizeof(obuf2), "(= BIOS)");
			strcat_s(title2, sizeof(title2), " Default Fan");
		}
		else {
			if (!(SlimDialog && StayOnTop))
				sprintf_s(obuf2 + strlen(obuf2), sizeof(obuf2) - strlen(obuf2), " Non Bios");
			sprintf_s(title2 + strlen(title2), sizeof(title2) - strlen(title2), " Fan %d (%s)",	fanctrl & 0x3F,	this->CurrentModeFromDialog() == 2 ? "Smart" : "Fixed");
		}
	}
	else {
		sprintf_s(obuf2, sizeof(obuf2), "0x%02x (", fanctrl);
		if (fanctrl & 0x80) {
			strcat_s(obuf2, sizeof(obuf2), "BIOS Controlled)");
			strcat_s(title2, sizeof(title2), " Default Fan");
		}
		else {
			sprintf_s(obuf2 + strlen(obuf2), sizeof(obuf2) - strlen(obuf2), "Fan Level %d, Non Bios)", fanctrl & 0x3F);
			sprintf_s(title2 + strlen(title2), sizeof(title2) - strlen(title2), " Fan %d (%s)",	fanctrl & 0x3F,	this->CurrentModeFromDialog() == 2 ? "Smart" : "Fixed");
		}
	}

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

	sprintf_s(obuf2, sizeof(obuf2), "%d/%d RPM", this->fan1speed, this->fan2speed);

	::SetDlgItemText(this->hwndDialog, 8102, obuf2);

	// display temperature list
	if (Fahrenheit)
		sprintf_s(obuf2, sizeof(obuf2), "%d° F", this->MaxTemp * 9 / 5 + 32);
	else
		sprintf_s(obuf2, sizeof(obuf2), "%d° C", this->MaxTemp);

	::SetDlgItemText(this->hwndDialog, 8103, obuf2);

	strcpy_s(templist2, sizeof(templist2), "");

	for (i = 0; i < 12; i++) {
		int temp = this->State.Sensors[i];

		if (temp != 0 && temp < 128) {
			if (Fahrenheit)
				sprintf_s(obuf2, sizeof(obuf2), "%d° F", temp * 9 / 5 + 32);
			else
				sprintf_s(obuf2, sizeof(obuf2), "%d° C", temp);

			if (SlimDialog && StayOnTop)
				sprintf_s(templist2 + strlen(templist2), sizeof(templist2) - strlen(templist2), "%d %s %s", i + 1, this->State.SensorName[i], obuf2);
			else
				sprintf_s(templist2 + strlen(templist2), sizeof(templist2) - strlen(templist2), "%d %s %s (0x%02x)", i + 1, this->State.SensorName[i], obuf2, this->State.SensorAddr[i]);

			strcat_s(templist2, sizeof(templist2), "\r\n");
		}
		else {
			if (this->ShowAll == 1) {
				sprintf_s(obuf2, sizeof(obuf2), "n/a");

				size_t strlen_templist = strlen_s(templist2, sizeof(templist2));

				if (SlimDialog && StayOnTop)
					sprintf_s(templist2 + strlen_templist, sizeof(templist2) - strlen_templist, "%d %s %s", i + 1, this->State.SensorName[i], obuf2);
				else
					sprintf_s(templist2 + strlen_templist, sizeof(templist2) - strlen_templist, "%d %s %s (0x%02x)", i + 1, this->State.SensorName[i], obuf2, this->State.SensorAddr[i]);

				strcat_s(templist2, sizeof(templist2), "\r\n");
			}
		}
	}

	::SetDlgItemText(this->hwndDialog, 8101, templist2);

	this->icontemp = this->State.Sensors[iMaxTemp];

	// compact single line status (combined)
	strcpy_s(templist, sizeof(templist), "");

	if (Fahrenheit) {
		for (i = 0; i < 12; i++) {
			if (this->State.Sensors[i] < 128) {
				if (this->State.Sensors[i] != 0)
					sprintf_s(templist + strlen(templist), sizeof(templist) - strlen(templist), "%d;", this->State.Sensors[i] * 9 / 5 + 32);
				else
					sprintf_s(templist + strlen(templist), sizeof(templist) - strlen(templist), "%d;", 0);
			}
			else {
				strcat_s(templist, sizeof(templist), "0;");
			}
		}
	}
	else {
		for (i = 0; i < 12; i++) {
			if (this->State.Sensors[i] != 128) {
				sprintf_s(templist + strlen(templist), sizeof(templist) - strlen(templist), "%d; ", this->State.Sensors[i]);
			}
			else {
				strcat_s(templist, sizeof(templist), "0; ");
			}
		}
	}

	templist[strlen(templist) - 1] = '\0';

	if (Fahrenheit)
		sprintf_s(CurrentStatus, sizeof(CurrentStatus), "Fan: 0x%02x / Switch: %d° F (%s)", State.FanCtrl, MaxTemp * 9 / 5 + 32, templist);
	else
		sprintf_s(CurrentStatus, sizeof(CurrentStatus), "Fan: 0x%02x / Switch: %d° C (%s)", State.FanCtrl, MaxTemp,	templist);

	// display fan speed

	if (fan1speed > 0x1fff)
		fan1speed = lastfan1speed;
	if (fan2speed > 0x1fff)
		fan2speed = lastfan2speed;
	sprintf_s(obuf2, sizeof(obuf2), "%d/%d", this->fan1speed, this->fan2speed);

	sprintf_s(CurrentStatuscsv, sizeof(CurrentStatuscsv), "%s %s; %d; %d; ", templist, obuf2, State.FanCtrl, MaxTemp);

	::SetDlgItemText(this->hwndDialog, 8112, this->CurrentStatus);

	//
	// handle fan control according to mode
	//

	this->CurrentModeFromDialog();
	this->ShowAllFromDialog();

	switch (this->CurrentMode) {

	case 1: // BIOS
		if (this->PreviousMode != this->CurrentMode) {
			sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from ");

			if (this->PreviousMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS->");
			if (this->PreviousMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart->");
			if (this->PreviousMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual->");

			if (this->CurrentMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS, setting fan speed");
			if (this->CurrentMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
			if (this->CurrentMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual, setting fan speed");

			this->Trace(obuf);
		}

		if (this->State.FanCtrl != 0x080)
			ok = this->SetFan("BIOS", 0x80);
		break;

	case 2: // Smart
		this->SmartControl();
		break;

	case 3: // Manual
		if (this->PreviousMode != this->CurrentMode) {
			sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from ");

			if (this->PreviousMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS->");
			if (this->PreviousMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart->");
			if (this->PreviousMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual->");

			if (this->CurrentMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS, setting fan speed");
			if (this->CurrentMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
			if (this->CurrentMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual, setting fan speed");

			this->Trace(obuf);
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

	if (this->CurrentMode == 3 && this->MaxTemp > this->ManModeExitInternal)
		this->CurrentMode = 2;

	return ok;
}

//-------------------------------------------------------------------------
//  smart fan control depending on temperature
//-------------------------------------------------------------------------
void
FANCONTROL::SmartControl(void) {
	char obuf[256] = "";

	if (this->PreviousMode == 1) {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from BIOS->");
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
		this->Trace(obuf);
	}

	if (this->PreviousMode == 3) {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from Manual->");
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
		this->Trace(obuf);
	}

	std::vector<SmartLevel> levels;
	for (int i = 0; this->SmartLevels[i].temp != -1; i++) {
		levels.push_back({ this->SmartLevels[i].temp, this->SmartLevels[i].fan, this->SmartLevels[i].hystUp, this->SmartLevels[i].hystDown });
	}

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

	if (this->FanBeepFreq && this->FanBeepDura)
		::Beep(this->FanBeepFreq, this->FanBeepDura);

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "%s: Set fan control to 0x%02x, ", source, fanctrl);
	if (this->IndSmartLevel == 1 && this->SmartLevels2[0].temp2 != 0 && source == "Smart")
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Mode 2, ");
	if (this->IndSmartLevel == 0 && this->SmartLevels2[0].temp2 != 0 && source == "Smart")
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Mode 1, ");
	sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Result: ");

	if (this->ActiveMode && !this->FinalSeen) {
		if (!this->LockECAccess()) return false;

		// Use modernized FanController
		ok = m_fanController->SetFanLevel(fanctrl, true); // Assuming dual fan as per original code
		
		// Sync back the state
		this->State.FanCtrl = m_fanController->GetCurrentFanCtrl();

		this->FreeECAccess();

		if (ok) {
			sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "OK");
			if (final)
				this->FinalSeen = true;    // prevent further changes when setting final mode

		}
		else {
			sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "FAILED!!");
			ok = false;
		}
	}
	else {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "IGNORED!(passive mode");
	}

	// display result
	sprintf_s(obuf2, sizeof(obuf2), "%s   (%s)", obuf, datebuf);

	::SetDlgItemText(this->hwndDialog, 8113, obuf2);

	this->Trace(this->CurrentStatus);
	this->Trace(obuf);

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

	sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "%s: Set EC register 0x%02x to %d, ", source, HdwOffset, (unsigned char)resultValue);
	sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Result: ");

	if (ok) {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "OK");
	}
	else {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "COULD NOT SET HARDWARE STATE!!!!");
	}

	// display result
	sprintf_s(obuf2, sizeof(obuf2), "%s   (%s)", obuf, datebuf);

	::SetDlgItemText(this->hwndDialog, 8113, obuf2);

	this->Trace(obuf);

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
	if (!m_sensorManager->UpdateSensors(this->ShowBiasedTemps, this->NoExtSensor, this->UseTWR)) {
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
