#include <locale>
#include <stdio.h>
#include <codecvt>
#include "TenzoM.h"
#include "AddInNative.h"
#include "AddInNative.cpp"


#if defined( __linux__ )
#include "TenzoM.cpp"
#endif

#pragma execution_character_set("utf-8")

using namespace std;

int main(int argc, char** argv)
{
	setlocale(LC_ALL, "ru-RU.UTF-8");
	wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> UTF8Convert;

	u16string comPort = u"COM4";

	if (argc == 1) {
		string coms(argv[0]);
		comPort.assign(coms.begin(), coms.end());
	}


	TenzoM tenzom;
	tenzom.IP		= u"192.168.93.21";
	tenzom.IP		= u"kompur-final01";
	tenzom.Protocol = TenzoM::eProtocolNet;
	
	tenzom.WriteLog = true;
#ifdef ISWINDOWS
	tenzom.LogFile  = u"D:\\tenzom.log";
#else
	tenzom.LogFile  = u"/tmp/tenzom.log";
#endif
	auto comports = tenzom.GetFreeComPorts();
	//tenzom.Emulate = true;

#ifdef  __linux__
	printf("comports: %s\n", UTF8Convert.to_bytes(comports).c_str());
#else
	printf("comports: %ls\n", comports.c_str());
#endif


	bool success = tenzom.OpenPort(comPort, 9600, 1);
	if (success)
	{
		int ves = tenzom.GetWeight();
		printf("ves: %d Calm: %s\n", ves, tenzom.Calm ? "1" : "0");
	}
	else
	{
#ifdef  __linux__
		printf("Error code: %d Message: %s\n", (int)tenzom.LastError, UTF8Convert.to_bytes(tenzom.Error).c_str());
#else
		printf("Error code: %d Message: %ls\n", (int)tenzom.LastError, tenzom.Error.c_str());
		//system("pause");
#endif
	}

	return 0;
}
