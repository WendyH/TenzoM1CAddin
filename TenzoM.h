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

class TenzoM
{
private:
#ifdef ISWINDOWS
#else
	locale_t locale;
#endif

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
	sockaddr* saddr;
	int saddrlen;
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
	bool FindTenzoMPacket(long bytesRead, char command);
	bool FindTenzoMPacket(long bytesRead, char command, char*& data, long& dataSize);
	bool CheckErrorPacket(long bytesRead);
	int  ExtractWeight(char* data, long dataSize);
	int  RandomWeight();
	void SetErrorText(unsigned long errorCode);
	void CheckLastError();
	void Log(u16string txt, char* buf, int i);
	void Log(u16string txt);
	void SendKey(unsigned short keycode);
	void SetUnsupportedComadrError();

public:
	enum ProtocolType
	{
		eProtocolTenzoM = 0,
		eProtocol643
	};

	// Constructor
	TenzoM() {     
		DecimalPoint = stof("0,5") > 0.1 ? ',' : '.'; // detect decimal separator
#ifdef ISWINDOWS
#else
		locale = newlocale(LC_ALL_MASK, "", (locale_t)0);
#endif
	}
	
	// Destructor
	~TenzoM() {
		ClosePort();
	}

	u16string    Name = u"";
	ProtocolType Protocol = eProtocolTenzoM; // Протокол обмена с весами
	char Adr   = 1;     // Адрес устройства
	bool NScal = false; // Флаг использования второго тезнодатчика
	bool Event = false; // Флаг присутствия события (введён код с клиавиатуры)

	bool Calm     = false; // Вес стабилен
	bool Overload = false; // Флаг перегрузки
	bool Emulate  = false; // Режим эмуляции
	bool NetMode  = false; // Режим работы по TCP/IP вместо COM-порта

	char DecimalPoint = '.';

	bool SendKeys   = false; // Cобытие нажатия на нумпаде терминала посылает в систему нажатие клавиши
	bool NumpadKeys = false; // При посылке нажатия кнопок - использовать цифры нумпада клавиатуры

	int  EmulMinKg = 9;    // Минимальный вес в килограммах при эмуляции веса
	int  EmulMaxKg = 80;   // Минимальный вес в килограммах при эмуляции веса

	bool WriteLog = false; // Писать в лог все отправленные и полученные байты
	u16string LogFile = u"tenzom.log";

	u16string IP      = u"127.0.0.1"; // IP-адрес для протоколов web или net
	int       NetPort = 4001;         // Порт для протокола net

	unsigned long LastError = 0;
	u16string     Error     = { 0 };

	void Delay(unsigned long ms);
	bool OpenPort(u16string comName, long boud, int deviceAddress);
	bool PortOpened();
	void ClosePort();

	u16string    GetFreeComPorts();

	unsigned char GetStatus();
	bool          SetZero();
	int           GetWeight();
	bool          SwitchToWeighing();
	u16string     GetIndicatorText(unsigned char num);
	bool          SetIndicatorText(u16string text, unsigned char num);
	char          GetEnteredCode();
	bool          SetInputChannel(int channelNum);
	bool		  Calibrate();
	int			  GetSerialNum();
	u16string     Version();
};