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
	wstring_convert<codecvt_utf8<char16_t>, char16_t> UTF8Converter;
	wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t> convert;

	printf("Проверка.\n");

	CAddInNative addin;

	auto index = addin.FindProp(u"СетевойАдрес");
	printf("index: %d  \n", index);

	TenzoM tenzom;
	tenzom.IP = u"Проверка русского!";
	tenzom.Protocol = TenzoM::eProtocol643;

	auto comports = tenzom.GetFreeComPorts();

	printf("comports: %s  IP:%s\n", UTF8Converter.to_bytes(comports).c_str(), UTF8Converter.to_bytes(tenzom.IP).c_str());

	bool success = tenzom.OpenPort(u"COM4", 9600, 1);

	if (success)
	{
		int ves = tenzom.GetWeight();
		printf("ves: %d Calm: %s\n", ves, tenzom.Calm ? L"1" : L"0");
	}
	else
	{
		auto zzz = UTF8Converter.to_bytes(tenzom.Error);
		printf("Error code: %d Message: %s\n", (int)tenzom.LastError, zzz.c_str());
	}

	return 0;
}
