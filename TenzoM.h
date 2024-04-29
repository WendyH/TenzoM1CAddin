#pragma once
#include <vector>
#include "ceSerial.h"

using namespace std;

//#define RECV_BUFFER_LENGHT 1024 * 4 // bytes

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
	ceSerial com;

	bool SendCommand(char command);
	long Receive(vector<char>& readBuffer);
	void SetCrcOfMessage(vector<char>& buffer, long bufSize);
	int  ExtractWeight(vector<char>& readBuffer);
	int  GetWeight643();
	int  RandomWeight();

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
	
	unsigned char Adr = 1;  // Адрес устройства

	bool Calm     = false;  // Вес стабилен
	bool Overload = false;  // Флаг перегрузки
	bool Emulate  = false;  // Режим эмуляции

	string IP      = { 0 }; // IP-адрес для протоколов web или net
	int    NetPort = 5001;  // Порт для протокола net
	int    WebPort = 8080;  // Порт для протокола web

	unsigned long LastError	 = 0;

	bool OpenPort(string comName, long boud, int deviceAddress);
	void ClosePort();
	bool PortOpened();

	TenzoMSTATUS GetStatus();
	int    GetFixedBrutto();
	bool   SetZero();
	int    GetNetto();
	int    GetBrutto();
	void   SwitchToWeighing();
	string GetFreeComPorts();
};
