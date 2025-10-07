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
	
	void Release()
	{
		if(mpLine)
		{
			gpiod_line_release(mpLine);
			mpLine = nullptr;
		}
	}
	
	~Line()
	{
		if(mpLine)
		{
			LOG("ERROR! ~Line() had to release! Call Release() yourself!");
			Release();
		}
	}
	
private:
	int32_t mGPIOLineNum = 0;
	int32_t mHigh = 0;
	gpiod_line* mpLine = nullptr;
};

class AddressIO
{
public:
	AddressIO(uint8_t pGPIOVals[8]) 
	: mAddressLines{{pGPIOVals[0]}, 
					{pGPIOVals[1]},
					{pGPIOVals[2]},
					{pGPIOVals[3]},
					{pGPIOVals[4]},
					{pGPIOVals[5]},
					{pGPIOVals[6]},
					{pGPIOVals[7]}}
	{}
	
	~AddressIO()
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
		
		for(int32_t i = 0; i < 8; i++)
		{
			mAddressLines[i].OpenLine(pChip);
		}
	}
	
	void Destroy()
	{
		for(int32_t i = 0; i < 8; i++)
		{
			mAddressLines[i].Release();
		}
	}
	
	void RequestLines(uint8_t value)
	{
		for(int32_t i = 0; i < 8; i++)
		{
			mAddressLines[i].RequestLine(value & 0x1);
		}
	}
	
	void SetValue(uint8_t value)
	{
		for(int32_t i = 0; i < 8; i++)
		{
			mAddressLines[i].SetHighLow((value & (0x1 << i)) != 0 ? 1 : 0);
		}
	}
	
private:
	Line mAddressLines[8];
};


//Line gAddressLines[8] = { {4}, {17}, {27}, {22}, {5}, {6}, {13}, {19} };
//AddressIO gAddressBus(uint8_t[] = {4,17,27,22,5,6,13,19});

int main(int argc, const char** argv)
{
	//todo: fix this, i want to pass globally
	uint8_t array[] = {4,17,27,22,5,6,13,19};
	AddressIO gAddressBus(array);
	
	const char chipName[] = "gpiochip0";
	int32_t result = 0;
	
	gpiod_chip* pChip = gpiod_chip_open_by_name(chipName);
	if(!pChip)
	{
		printf("open chip failed\n");
		return 0;
	}
	printf("Chip opened!");
	
	//todo: make this work!
	gAddressBus.Create(pChip);
	
	gAddressBus.RequestLines(0);
	
	for(int32_t i = 0; i < 255; i++ )
	{
		gAddressBus.SetValue(i);
		sleep(1);
	}
	
	gAddressBus.SetValue(0);
	
	gAddressBus.Destroy();
	
	/*if(!gAddressLines[0].OpenLine(pChip))
	{
		LOG("Couldn't open line");
		return 0;
	}
	
	result = gAddressLines[0].RequestLine(0);
	if(result < 0)
	{
		LOG("Couldn't open line");
		return 0;
	}
		
	uint32_t value = 0;
	int32_t numTimes = 3;
	while(numTimes > 0)
	{
		value = !value;
				
		result = gAddressLines[0].SetHighLow(value);
		if(result < 0)
		{
			LOG("Set line value failed");
			return 0;
		}
		
		numTimes--;
		sleep(5);
	}
	
	result = gAddressLines[0].SetHighLow(0);
		
	gAddressLines[0].Release();*/
	
	if(pChip)
	{
		gpiod_chip_close(pChip);
		pChip = nullptr;
	}
	
	return 0;
}