#pragma once
#include <Windows.h>

#define READ_TIMEOUT       2000     // milliseconds
#define RECV_BUFFER_LENGHT 1024 * 4 // bytes

struct TenzoMSTATUS {
	BOOL Reset;
	BOOL Error;
	BOOL Netto;
	BOOL KeyPressed;
	BOOL EndDosing;
	BOOL WeightFixed;
	BOOL ADCCalibration;
	BOOL Dosing;
};

class TenzoM
{
	private:
		int  randomWeight = 0;
		HANDLE port = 0;
		BYTE readBuffer[RECV_BUFFER_LENGHT] = { 0 };

		BOOL Send(unsigned char* lpBuf, DWORD bufSize);
		BOOL SendCommand(unsigned char command);
		BOOL SendCommand(BYTE command, BYTE data);
		BOOL SendCommand(unsigned char command, BYTE* lpData, size_t dataLenght);
		DWORD Receive();
		void SetCrcOfMessage(BYTE* buffer, int bufSize);
		int  ExtractWeight();
		int  RandomWeight();

	public:
		BOOL PortOpened = FALSE;
		BYTE Adr      = 1;
		BOOL Calm     = FALSE;
		BOOL Overload = FALSE;
		BOOL Emulate  = FALSE;

		DWORD LastError = 0;

		// destructor
		~TenzoM()
		{
			ClosePort();
		}

		BOOL  OpenPort(int portNumber, DWORD boud, BYTE deviceAddr);
		void  ClosePort();

		void  SetDeviceAddress(BYTE deviceAddress);
		DWORD GetDeviceSN();

		int   GetFixedBrutto();
		TenzoMSTATUS GetStatus();
		void  SetZero();
		int   GetNetto();
		int   GetBrutto();
		LPSTR GetIndicatorText();
		int   GetCounter(BYTE numCounter);
		void  SwitchToWeighing();
};

