#define CE_SERIAL_IMPLEMENTATION

#include <stdio.h>
#include "TenzoM.h"

#if defined( __linux__ )
#include "TenzoM.cpp"
#endif

#pragma execution_character_set("utf-8")

using namespace std;

int main()
{
	printf("Проверка.");

	#if defined( __linux__ )
	setlocale(LC_ALL, "ru_RU.UTF-8");
	#else
	SetConsoleOutputCP(65001);
	#endif

	TenzoM tenzom;

	auto comports = tenzom.GetFreeComPorts();

	printf("comports: %s\n", comports.c_str());

	tenzom.Protocol = TenzoM::eProtocol643;

	tenzom.IP = "Проврка русского!";

	bool success = tenzom.OpenPort("COM4", 9600, 1);

	auto text = tenzom.Error;

	if (success)
	{
		auto ves = tenzom.GetWeight();
		printf("ves: %d Calm: %s\n", ves, tenzom.Calm ? "true" : "false");
	}
	else
	{
		printf("Error code: %d Message: %s\n", (int)tenzom.LastError, tenzom.Error.c_str());
	}

	return 0;
}