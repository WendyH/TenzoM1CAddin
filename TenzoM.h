#pragma once
#if defined(_WIN64) || defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WINDOWS__) || defined(__TOS_WIN__) || defined(__CYGWIN__)
	#define ISWINDOWS  1
#elif defined(unix) || defined(__unix) || defined(__unix__)
	#define ISLINUX 1
#endif

#if defined(ISWINDOWS)
#include <windows.h>
#else
#include <com.h>
#endif

using namespace std;

#define READ_TIMEOUT       2000     // milliseconds
#define RECV_BUFFER_LENGHT 1024 * 4 // bytes

struct TenzoMSTATUS {
	bool Reset;
	bool Error;
	bool Netto;
	bool KeyPressed;
	bool EndDosing;
	bool WeightFixed;
	bool ADCCalibration;
	bool Dosing;
};

class TenzoM
{
private:
#ifdef ISWINDOWS
	HANDLE port = INVALID_HANDLE_VALUE;
#else
	long fd;
#endif
	BYTE   readBuffer[RECV_BUFFER_LENGHT] = { 0 };

	bool  Send(BYTE* message, DWORD msgSize);
	bool  SendCommand(BYTE command);
	DWORD Receive();
	void  SetCrcOfMessage(BYTE* buffer, long bufSize);
	int   ExtractWeight();
	int   RandomWeight();
	void  CheckLastError();

public:
	enum ProtocolType
	{
		eProtocolTenzoM = 0,
		eProtocol643,
		eProtocolNet,
		eProtocolWeb
	};

	int  emulMaxOffset     = 0;
	int  emulTotalSteps    = 4;
	int  emulTargetWeight  = 0;
	int  emulCurrentWeight = 0;

	ProtocolType Protocol  = eProtocolTenzoM; // Протокол обмена с весами
	
	unsigned char Adr = 1; // Адрес устройства

	bool Calm     = false; // Вес стабилен
	bool Overload = false; // Флаг перегрузки
	bool Emulate  = false; // Режим эмуляции

	wstring IP      = L"Проверка!"; // IP-адрес для протоколов web или net
	int     NetPort = 5001;  // Порт для протокола net
	int     WebPort = 8080;  // Порт для протокола web

	unsigned long LastError	 = 0;
	wstring Error = { 0 };

	bool  OpenPort(wstring comName, long boud, int deviceAddress);
	bool  PortOpened();
	void  ClosePort();

	TenzoMSTATUS GetStatus();
	bool         SetZero();
	int          GetWeight();
	void         SwitchToWeighing();
	wstring      GetFreeComPorts();
};
