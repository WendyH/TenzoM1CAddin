#pragma once
#if defined(_WIN64) || defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WINDOWS__) || defined(__TOS_WIN__) || defined(__CYGWIN__)
	#define ISWINDOWS  1
#elif defined(unix) || defined(__unix) || defined(__unix__)
	#define ISLINUX 1
#endif

#ifdef ISWINDOWS
#include <windows.h>
#else
//#include <com.h>
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
	int  emulMaxOffset     = 0;
	int  emulTotalSteps    = 4;
	int  emulTargetWeight  = 0;
	int  emulCurrentWeight = 0;
	int  emulCalmSteps     = 4;
	int  emulCalmStep      = 0;

#ifdef ISWINDOWS
	HANDLE port = INVALID_HANDLE_VALUE;
#else
	long fd;
#endif
	char readBuffer[RECV_BUFFER_LENGHT] = { 0 };

	bool Send(char* message, long msgSize);
	bool SendCommand(char command);
	unsigned long Receive();
	void SetCrcOfMessage(char* buffer, long bufSize);
	int  ExtractWeight();
	int  RandomWeight();
	void CheckLastError();

public:
	enum ProtocolType
	{
		eProtocolTenzoM = 0,
		eProtocol643,
		eProtocolNet,
		eProtocolWeb
	};

	ProtocolType Protocol  = eProtocolTenzoM; // Протокол обмена с весами
	
	char Adr      = 1;     // Адрес устройства
	bool Calm     = false; // Вес стабилен
	bool Overload = false; // Флаг перегрузки
	bool Emulate  = false; // Режим эмуляции

	u16string IP    = u"Проверка!"; // IP-адрес для протоколов web или net
	int     NetPort = 5001;  // Порт для протокола net
	int     WebPort = 8080;  // Порт для протокола web

	unsigned long LastError = 0;
	u16string     Error     = { 0 };

	void Delay(unsigned long ms);
	bool OpenPort(u16string comName, long boud, int deviceAddress);
	bool PortOpened();
	void ClosePort();

	TenzoMSTATUS GetStatus();
	bool         SetZero();
	int          GetWeight();
	void         SwitchToWeighing();
	u16string    GetFreeComPorts();
};
