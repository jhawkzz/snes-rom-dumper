#include <stdio.h>
#include <gpiod.h>

int main(int argc, const char* argv)
{
	const char chipName[] = "gpiochip0";
	int lineNum = 12;
	int value = 0;
	
	gpiod_chip* pChip = nullptr;
	gpiod_line* pLine = nullptr;
	
	pChip = gpiod_chip_open_by_name(chipName);
	if(!pChip)
	{
		printf("open chip failed");
		return 0;
	}
	
	return 0;
}