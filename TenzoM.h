#pragma once
#if defined(_WIN64) || defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WINDOWS__) || defined(__TOS_WIN__) || defined(__CYGWIN__)
	#define ISWINDOWS  1
#elif defined(unix) || defined(__unix) || defined(__unix__)
	#define ISLINUX 1
#endif

#include <string>

#ifdef ISWINDOWS
#include <windows.h>
#else
//#include <com.h>
	#define INVALID_SOCKET -1
#endif

using namespace std;

#define READ_TIMEOUT       2000     // milliseconds
#define RECV_BUFFER_LENGHT 1024 * 4 // bytes
#define TCP_TIMEOUT_SEC    2

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
	WSADATA wsaData;
	HANDLE  comPort		 = INVALID_HANDLE_VALUE;
	DWORD   tcpTimeout   = TCP_TIMEOUT_SEC * 1000;
	bool    wsaStarted	 = false;
	int     clientSocket = INVALID_SOCKET;
#else
	long fd;
	int  tcpTimeout   = TCP_TIMEOUT_SEC;
	int  clientSocket = INVALID_SOCKET;
#endif
	char readBuffer[RECV_BUFFER_LENGHT] = { 0 };
	bool TryConnectTo();
	bool Send(char* message, long msgSize);
	bool SendCommand(char command);
	unsigned long Receive();
	void SetCrcOfMessage(char* buffer, long bufSize);
	char* FindTenzoMPacket(long bytesRead);
	int  ExtractWeight(char* lpBuf);
	int  RandomWeight();
	void CheckTenzoNetCodeError(string sCode);
	void SetErrorText(unsigned long errorCode);
	void CheckLastError();
	void Log(u16string txt, char* buf, int i);
	void Log(u16string txt);

public:
	enum ProtocolType
	{
		eProtocolTenzoM = 0,
		eProtocol643,
		eProtocolNet,
		eProtocolWeb
	};

	// Constructor
	TenzoM() {     
		try {
			// detect decimal separator
			DecimalPoint = stof("0,5") > 0.1 ? ',' : '.';
		}
		catch (...)
		{
		}
	}
	
	// Destructor
	~TenzoM() {
		ClosePort();
	}

	ProtocolType Protocol  = eProtocolTenzoM; // Протокол обмена с весами
	
	char Adr      = 1;     // Адрес устройства
	bool Calm     = false; // Вес стабилен
	bool Overload = false; // Флаг перегрузки
	bool Emulate  = false; // Режим эмуляции
	char DecimalPoint = '.';

	int  EmulMinKg = 9;    // Минимальный вес в килограммах при эмуляции веса
	int  EmulMaxKg = 80;   // Минимальный вес в килограммах при эмуляции веса

	bool WriteLog = false; // Писать в лог все отправленные и полученные байты
	u16string LogFile = u"tenzom.log";

	u16string IP    = u"127.0.0.1"; // IP-адрес для протоколов web или net
	int     NetPort = 5001;         // Порт для протокола net
	int     WebPort = 8080;         // Порт для протокола web
	u16string Name  = u"Весы1";     // Имя весов для работы по web или net

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