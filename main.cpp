#include <cstring>
#include <cstdint>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

static const char gRequestingProgram[] = "CopyRom";

#define LOG(a) printf("%s\n", (a))

class GPIOLine
{
public:
	GPIOLine() 
	{}
	
	GPIOLine(uint32_t gpioLineNum) : 
		mGPIOLineNum(gpioLineNum) 
	{}
	
	void Create(uint32_t gpioLineNum)
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
		
		if(mGPIOLineNum < 0)
		{
			LOG("GPIOLineNum not set");
			return false;
		}
		
		mpLine = gpiod_chip_get_line(pChip, mGPIOLineNum);
		return mpLine != nullptr;
	}
	
	int32_t RequestLineOutput(uint32_t highLowState) const
	{
		if(!mpLine)
		{
			LOG("Line not open");
			return -1;
		}
		
		// Take the lowest bit so we don't send a value other than 0 or 1
		return gpiod_line_request_output(mpLine, gRequestingProgram, (highLowState & 0x1));
	}
	
	int32_t SetValue(uint32_t highLowState) const
	{
		if(!mpLine)
		{
			LOG("Line not open");
			return -1;
		}
		
		return gpiod_line_set_value(mpLine, (highLowState & 0x1));
	}
	
	void Destroy()
	{
		if(mpLine)
		{
			gpiod_line_release(mpLine);
			mpLine = nullptr;
		}
	}
	
	~GPIOLine()
	{
		if(mpLine)
		{
			LOG("ERROR! ~Line() had to release! Call Destroy() yourself!");
			Destroy();
		}
	}
	
private:
	int32_t mGPIOLineNum = -1;
	gpiod_line* mpLine = nullptr;
};

template<int NumLines>
class GPIOLineArray
{
public:
	GPIOLineArray(uint8_t* pGPIOVals)
	{
		for(uint32_t i = 0; i < NumLines; i++)
		{
			mLines[i].Create(pGPIOVals[i]);
		}
	}
	
	~GPIOLineArray()
	{
		Destroy();
	}
	
	void Create(gpiod_chip* pChip)
	{
		if(!pChip)
		{
			LOG("Invalid chip passed!");
			return;
		}
		
		for(int32_t i = 0; i < NumLines; i++)
		{
			mLines[i].OpenLine(pChip);
		}
	}
	
	void Destroy()
	{
		for(int32_t i = 0; i < NumLines; i++)
		{
			mLines[i].Destroy();
		}
	}
	
	void RequestLinesOutput(uint8_t value)
	{
		for(int32_t i = 0; i < NumLines; i++)
		{
			mLines[i].RequestLineOutput(value & 0x1);
		}
	}
	
	void SetValue(uint32_t value)
	{
		for(int32_t i = 0; i < NumLines; i++)
		{
			mLines[i].SetValue((value & (0x1 << i)) != 0 ? 1 : 0);
		}
	}
	
private:
	GPIOLine mLines[NumLines];
};

uint8_t gAddressLineIndices[] = {2,3,4,17,27,22,10,9};
GPIOLineArray<8> gAddressLines(gAddressLineIndices);

uint8_t gDataLineIndices[] = {14,15,18,23,24,25,8,7};
GPIOLineArray<8> gDataLines(gDataLineIndices);

uint8_t gWriteLineIndices[] = {2};
GPIOLineArray<1> gWriteEnable(gWriteLineIndices);

//GPIOLine gWriteEnable(2);

int main(int argc, const char** argv)
{
	const char chipName[] = "gpiochip0";
	
	gpiod_chip* pChip = gpiod_chip_open_by_name(chipName);
	if(!pChip)
	{
		printf("open chip failed\n");
		return 0;
	}
	printf("Chip opened!");
	
	gAddressLines.Create(pChip);
	gDataLines.Create(pChip);
	//gWriteEnable.Create(pChip);
	
	gAddressLines.RequestLinesOutput(0);
	gDataLines.RequestLinesOutput(0);
	//gWriteEnable.RequestLinesOutput(0);
	
	if(argc > 1)
	{
		if(!strcmp(argv[1], "--on"))
		{
			gAddressLines.SetValue(0xFF);
			gDataLines.SetValue(0xFF);
			//gWriteEnable.SetValue(1);
		}
		else if(!strcmp(argv[1], "--off"))
		{
			gAddressLines.SetValue(0);
			gDataLines.SetValue(0);
			//gWriteEnable.SetValue(0);
		}
		else if(!strcmp(argv[1], "--value"))
		{
			int16_t value = atoi(argv[2]);
			gAddressLines.SetValue(value);
			gDataLines.SetValue(value);
			//gWriteEnable.SetValue(0);
		}
		
		sleep(1);
	}
	else
	{
		//gWriteEnable.SetValue(1);
		
		for(int32_t i = 0; i < 256; i++ )
		{
			gAddressLines.SetValue(i);
			gDataLines.SetValue(i);
			sleep(1);
		}
		
		gAddressLines.SetValue(0);
		gDataLines.SetValue(0);
		//gWriteEnable.SetValue(0);
	}
	
	gAddressLines.Destroy();
	gDataLines.Destroy();
	//gWriteEnable.Destroy();
	
	if(pChip)
	{
		gpiod_chip_close(pChip);
		pChip = nullptr;
	}
	
	return 0;
}