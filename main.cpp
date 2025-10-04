#include <cstdint>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

static const char gRequestingProgram[] = "CopyRom";

#define LOG(a) printf("%s\n", (a))

class Line
{
public:
	Line(int32_t gpioLineNum)
	{
		mGPIOLineNum = gpioLineNum;
	}
	
	bool OpenLine(gpiod_chip* pChip)
	{
		if(!pChip)
		{
			LOG("Chip invalid");
			return false;
		}
		
		mpLine = gpiod_chip_get_line(pChip, mGPIOLineNum);
		return mpLine != nullptr;
	}
	
	int32_t RequestLine(uint32_t highLowState) const
	{
		if(!mpLine)
		{
			LOG("Line not open");
			return -1;
		}
		
		// Take the lowest bit so we don't send a value other than 0 or 1
		return gpiod_line_request_output(mpLine, gRequestingProgram, (highLowState & 0x1));
	}
	
	int32_t SetHighLow(uint32_t highLowState) const
	{
		if(!mpLine)
		{
			LOG("Line not open");
			return -1;
		}
		
		return gpiod_line_set_value(mpLine, (highLowState & 0x1));
	}
	
private:
	int32_t mGPIOLineNum = 0;
	int32_t mHigh = 0;
	gpiod_line* mpLine = nullptr;
};

int main(int argc, const char** argv)
{
	const char chipName[] = "gpiochip0";
	uint32_t lineNum = 4;
	int32_t result = 0;
	
	Line line17(17);
	
	gpiod_chip* pChip = nullptr;
	gpiod_line* pLine = nullptr;
	
	pChip = gpiod_chip_open_by_name(chipName);
	if(!pChip)
	{
		printf("open chip failed\n");
		return 0;
	}
	printf("Chip opened!");
	
	if(!line17.OpenLine(pChip))
	{
		LOG("Couldn't open line 17");
		return 0;
	}
	
	
	pLine = gpiod_chip_get_line(pChip, lineNum);
	if(!pLine)
	{
		printf("get line failed\n");
		return 0;
	}
	printf("get line worked!");
	
	result = line17.RequestLine(0);
	if(result < 0)
	{
		LOG("Couldn't open line");
		return 0;
	}
	
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
		
		result = line17.SetHighLow(value);
		if(result < 0)
		{
			LOG("Set line value failed");
			return 0;
		}
		
		numTimes--;
		sleep(5);
	}
	
	result = gpiod_line_set_value(pLine, 0);
	result = line17.SetHighLow(0);
		
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