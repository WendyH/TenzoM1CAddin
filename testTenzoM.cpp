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

int main()
{
	setlocale(LC_ALL, "ru-RU.UTF-8");
	wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> UTF8Convert;

	printf("Проверка.\n");

	CAddInNative addin;

	auto index = addin.FindProp(u"СетевойАдрес");
	printf("index: %d  \n", index);

	TenzoM tenzom;
	tenzom.IP = u"Проверка русского!";
	tenzom.Protocol = TenzoM::eProtocol643;
	
	tenzom.WriteLog = true;
	tenzom.LogFile  = u"D:\\tenzom.log";

	auto comports = tenzom.GetFreeComPorts();

#ifdef  __linux__
	printf("comports: %s  IP:%s\n", UTF8Convert.to_bytes(comports).c_str(), UTF8Convert.to_bytes(tenzom.IP).c_str());
#else
	printf("comports: %ls  IP:%ls\n", comports.c_str(), tenzom.IP.c_str());
#endif
	bool success = tenzom.OpenPort(u"COM4", 9600, 1);

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
#endif
	}

	return 0;
}
