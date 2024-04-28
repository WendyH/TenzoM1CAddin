#define CE_SERIAL_IMPLEMENTATION

#include <stdio.h>
#include "TenzoM.h"


using namespace std;

int main()
{
	TenzoM tenzom;

	tenzom.Protocol = TenzoM::eProtocol643;

	bool success = tenzom.OpenPort("COM4", 9600, 1);

	if (success)
	{
		auto ves = tenzom.GetBrutto();
		printf("ves: %d Calm: %s", ves, tenzom.Calm ? "true" : "false");
	}
	else
	{
		printf("Error code: %d", tenzom.LastError);
	}

	return 0;
}