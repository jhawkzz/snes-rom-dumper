#include <stdio.h>
#include <gpiod.h>

int main(int argc, const char** argv)
{
	const char chipName[] = "gpiochip0";
	int lineNum = 12;
	int value = 0;
	int result = 0;
	
	gpiod_chip* pChip = nullptr;
	gpiod_line* pLine = nullptr;
	
	pChip = gpiod_chip_open_by_name(chipName);
	if(!pChip)
	{
		printf("open chip failed\n");
		return 0;
	}
	printf("Chip opened!");
	
	pLine = gpiod_chip_get_line(pChip, lineNum);
	if(!pLine)
	{
		printf("get line failed\n");
		return 0;
	}
	printf("get line worked!");
	
	result = gpiod_line_request_output(line, "CopyRom", 0);
	if(result < 0)
	{
		printf("Request line as output failed\n");
		return 0;
	}
	
	result = gpiod_line_set_value(pLine, 1);
	if(result < 0)
	{
		printf("Set line value failed\n");
		return 0;
	}
	
	gpiod_line_release(pLine);
	pLine = nullptr;
	
	gpiod_chip_close(pChip);
	pChip = nullptr;
	
	return 0;
}