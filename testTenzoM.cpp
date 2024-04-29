#define CE_SERIAL_IMPLEMENTATION

#include <stdio.h>
#include "TenzoM.h"

#if defined( __linux__ )
#include "TenzoM.cpp"
#endif


using namespace std;

int main()
{
	TenzoM tenzom;

	auto comports = tenzom.GetFreeComPorts();

	printf("comports: %s\n", comports.c_str());

	tenzom.Protocol = TenzoM::eProtocol643;

	bool success = tenzom.OpenPort(u"COM4", 9600, 1);

	if (success)
	{
		auto ves = tenzom.GetWeight();
		printf("ves: %d Calm: %s\n", ves, tenzom.Calm ? "true" : "false");
	}
	else
	{
		printf("Error code: %d\n", (int)tenzom.LastError);
	}

	return 0;
}