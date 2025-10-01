#include <cstdint>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, const char** argv)
{
	const char chipName[] = "gpiochip0";
	uint32_t lineNum = 12;
	int32_t result = 0;
	
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
	
	result = gpiod_line_request_output(pLine, "CopyRom", 0);
	if(result < 0)
	{
		printf("Request line as output failed\n");
		return 0;
	}
	
	uint32_t value = 0;
	int32_t numTimes = 3;
	while(numTimes > 0)
	{
		value = !value;
		
		result = gpiod_line_set_value(pLine, value);
		if(result < 0)
		{
			printf("Set line value failed\n");
			return 0;
		}
		
		numTimes--;
		sleep(5);
	}
	
	result = gpiod_line_set_value(pLine, 0);
		
	if(pLine)
	{
		gpiod_line_release(pLine);
		pLine = nullptr;
	}
	
	if(pChip)
	{
		gpiod_chip_close(pChip);
		pChip = nullptr;
	}
	
	return 0;
}