#define CE_SERIAL_IMPLEMENTATION

#include <locale>
#include <stdio.h>
#include "TenzoM.h"

#if defined( __linux__ )
#include "TenzoM.cpp"
#endif

#pragma execution_character_set("utf-8")

using namespace std;

int main()
{
	setlocale(LC_ALL, "ru-RU.UTF-8");

	wprintf(L"Проверка.\n");

	TenzoM tenzom;
	tenzom.IP = L"Проверка русского!";
	tenzom.Protocol = TenzoM::eProtocol643;

	auto comports = tenzom.GetFreeComPorts();

	wprintf(L"comports: %s  IP:%ls\n", comports.c_str(), tenzom.IP.c_str());

	bool success = tenzom.OpenPort(L"COM4", 9600, 1);

	if (success)
	{
		int ves = tenzom.GetWeight();
		wprintf(L"ves: %d Calm: %s\n", ves, tenzom.Calm ? L"1" : L"0");
	}
	else
	{
		wprintf(L"Error code: %d Message: %s\n", (int)tenzom.LastError, tenzom.Error.c_str());
	}

	return 0;
}
