#include <cstring>
#include <cstdint>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

static const char gRequestingProgram[] = "CopyRom";

#define LOG(a) printf("%s: %s\n", __FUNCTION__, (a))

class GPIOLine
{
public:
	void Create(gpiod_chip* pChip, uint32_t gpioLineNum)
	{
		if(!pChip)
		{
			LOG("Invalid chip passed.");
			return;
		}
		
		mpChip = pChip;
		mGPIOLineNum = gpioLineNum;
	}
	
	void HiZ()
	{
		ConfigForInput();
	}
	
	void Write(uint8_t bit)
	{
		ConfigForOutput();
		
		int32_t result = gpiod_line_set_value(mpLine, (bit & 0x1));
		if(result == -1)
		{
			LOG("Failed to write to line.");
		}
	}
	
	int8_t Read()
	{
		ConfigForInput();
		
		int8_t value = gpiod_line_get_value(mpLine);
		if(value > -1)
		{
			return value & 0x1;
		}
		else
		{
			LOG("Failed to read value for line.");
			return -1;
		}
	}
	
	void Release()
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
			gpiod_line_release(mpLine);
		}
		
		mpLine = nullptr;
		mpChip = nullptr;
	}
	
private:
	bool OpenLine()
	{
		if(!mpChip)
		{
			LOG("Chip invalid");
			return false;
		}
		
		if(mGPIOLineNum < 0)
		{
			LOG("GPIOLineNum not set");
			return false;
		}
		
		mpLine = gpiod_chip_get_line(mpChip, mGPIOLineNum);
		return mpLine != nullptr;
	}
	
	void ConfigForOutput()
	{
		if(mDirection != Direction::Output)
		{
			mDirection = Direction::Output;
			
			Release();
			
			if(!OpenLine())
			{
				LOG("Failed to open line.");
				return;
			}
			
			int32_t result = gpiod_line_request_output(mpLine, gRequestingProgram, 0);
			if(result == -1)
			{
				LOG("Failed to set line to output.");
			}
		}
	}
	
	void ConfigForInput()
	{
		if(mDirection != Direction::Input)
		{
			mDirection = Direction::Input;
			
			Release();
			
			if(!OpenLine())
			{
				LOG("Failed to open line.");
				return;
			}
			
			int32_t result = gpiod_line_request_input_flags(mpLine, gRequestingProgram, GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN);
			if(result == -1)
			{
				LOG("Failed to set line to input.");
			}
		}
	}
	
private:
	int32_t mGPIOLineNum = -1;
	gpiod_line* mpLine = nullptr;
	gpiod_chip* mpChip = nullptr;
	
	enum class Direction
	{
		None,
		Input,
		Output
	};
	
	Direction mDirection = Direction::None;
};

template<int NumLines>
class GPIOLineArray
{
public:
	~GPIOLineArray()
	{
		Release();
	}
	
	void Create(gpiod_chip* pChip, uint8_t* pGPIOVals)
	{
		if(!pChip)
		{
			LOG("Invalid chip passed!");
			return;
		}
		
		for(uint32_t i = 0; i < NumLines; i++)
		{
			mLines[i].Create(pChip, pGPIOVals[i]);
		}
	}
	
	void Release()
	{
		for(uint32_t i = 0; i < NumLines; i++)
		{
			mLines[i].Release();
		}
	}
	
	void HiZ()
	{
		for(int32_t i = 0; i < NumLines; i++)
		{
			mLines[i].HiZ();
		}
	}
	
	void Write(uint32_t value)
	{
		for(int32_t i = 0; i < NumLines; i++)
		{
			mLines[i].Write((value & (0x1 << i)) != 0 ? 1 : 0);
		}
	}
	
	uint8_t Read()
	{
		uint8_t value = 0;
		
		for(int32_t i = 0; i < NumLines; i++)
		{
			uint8_t lineVal = mLines[i].Read();
			
			value |= ((lineVal != 0 ? 0x1 : 0) << i);
			printf("lineVal %d is %d\n", i, lineVal);
		}
		
		return value;
	}
	
private:
	GPIOLine mLines[NumLines];
};

uint8_t gAddressLineIndices[] = {2,3,4,17,27,22,10,9,5,6,13,19,26,16,20,21};
GPIOLineArray<16> gAddressLines;

uint8_t gDataLineIndices[] = {14,15,18,23,24,25,8,7};
GPIOLineArray<8> gDataLines;

uint8_t gWriteLineIndices[] = {12};
GPIOLineArray<1> gWriteEnable;

uint8_t gSRAMBuffer[65536] = {0};

void WriteReadRAMTest()
{
	// Use with the 15bit addressable ram chip.
	
	for(uint32_t i = 0; i < 65536; i++)
	{
		uint32_t address = i;
		uint32_t writeValue = i % 256;
		
		// write
		 LOG("---Write---");
		 LOG("Setting write enable high");
		 gWriteEnable.Write(1);
		 usleep(10);
		 
		 LOG("Setting data hiZ");
		 gDataLines.HiZ();
		 usleep(10);
		
		 printf("Setting address 0%x\n", address);
		 gAddressLines.Write(address);
		 usleep(10);
		 
		 LOG("Setting write enable low");
		 gWriteEnable.Write(0);
		 usleep(10);
		
		 printf("Writing value: 0%x\n", writeValue);
		 gDataLines.Write(writeValue);
		 usleep(10);
		
		 LOG("Setting write enable high");
		 gWriteEnable.Write(1);
		 usleep(10);
		 
		 LOG("Setting data hiZ");
		 gDataLines.HiZ();
		 usleep(10);
	}
	 
	 for(uint32_t i = 0; i < 65536; i++)
	 {
		uint32_t address = i;
		
		 // read
		 LOG("---Read---");
		 LOG("Setting write enable high");
		 gWriteEnable.Write(1);
		 usleep(10);
		
		 printf("Setting address 0%x\n", address);
		 gAddressLines.Write(address);
		 usleep(10);
		 
		 LOG("Setting data hiZ");
		 gDataLines.HiZ();
		 usleep(10);
		
		 LOG("Reading");
		 uint8_t value = gDataLines.Read();
		 printf("Value at dataline: %x\n", value);
		 usleep(10);
		 
		 if(value != i % 256)
		 {
			 printf("*****THIS DOES NOT MATCH******\n");
			 return;
		 }
		 
		 LOG("Setting data hiZ");
		 gDataLines.HiZ();
		 usleep(10);
	 }
	 
	 printf("*****ALL VALUES MATCH******\n");
}

int main(int argc, const char** argv)
{
	const char chipName[] = "gpiochip0";
	
	gpiod_chip* pChip = gpiod_chip_open_by_name(chipName);
	if(!pChip)
	{
		printf("open chip failed\n");
		return 0;
	}
	
	gAddressLines.Create(pChip, gAddressLineIndices);
	gDataLines.Create(pChip, gDataLineIndices);
	gWriteEnable.Create(pChip, gWriteLineIndices);
	
	if(argc > 2)
	{
		if(!strcmp(argv[1], "--address"))
		{
			uint16_t value = atoi(argv[2]);
			gAddressLines.Write(value % 65536);
		}
		else if(!strcmp(argv[1], "--data"))
		{
			uint8_t value = atoi(argv[2]);
			gDataLines.Write(value % 256);
		}
		else if(!strcmp(argv[1], "--write"))
		{
			uint8_t value = atoi(argv[2]);
			gWriteEnable.Write(value % 2);
		}
		else if(!strcmp(argv[1], "--address-line-on"))
		{
			uint8_t value = atoi(argv[2]);
			gAddressLines.Write(0x1 << value);
		}
		else if(!strcmp(argv[1], "--address-line-on"))
		{
			uint8_t value = atoi(argv[2]);
			gAddressLines.Write(0x1 << value);
		}
		else if(!strcmp(argv[1], "--address-line-off"))
		{
			gAddressLines.Write(0);
		}
		else if(!strcmp(argv[1], "--data-line-on"))
		{
			uint8_t value = atoi(argv[2]);
			gDataLines.Write(0x1 << value);
		}
	}
	else if(argc > 1)
	{
		if(!strcmp(argv[1], "--on"))
		{
			gAddressLines.Write(65535);
			gDataLines.Write(0xFF);
			gWriteEnable.Write(1);
		}
		else if(!strcmp(argv[1], "--off"))
		{
			gAddressLines.Write(0);
			gDataLines.Write(0);
			gWriteEnable.Write(0);
		}
		else if(!strcmp(argv[1], "--data-line-off"))
		{
			gDataLines.Write(0);
		}
		else if(!strcmp(argv[1], "--dance"))
		{
			for(int32_t i = 0; i < 65536; i++ )
			{
				gAddressLines.Write(i);
				gDataLines.Write(i % 256);
				gWriteEnable.Write(i % 2);
				sleep(1);
			}
		}
		else
		{
			LOG("Unknown arg. Did you forget a value for the arg?");
		}
	}
	else
	{		 
		printf("Dumping SRAM at range $70:0000 - $70:FFFF\n");
		sleep(1);
			
		printf("Setting write enable high\n");
		gWriteEnable.Write(1);
		usleep(10);
		
		for(uint32_t i = 0; i < 65536; i++ )
		{
			uint32_t address = i;
			
			 // read
			 printf("Setting Read Address to: 70:%04x\n", address);
			 gAddressLines.Write(address);
			 usleep(10);
			 
			 printf("Setting Data Bus to: HiZ\n");
			 gDataLines.HiZ();
			 usleep(10);
			
			 uint8_t value = gDataLines.Read();
			 printf("Data Bus Received Value: %x\n", value);
			 usleep(10);
			 gSRAMBuffer[i] = value;
			 
			 printf("Setting Data Bus to: HiZ\n");
			 gDataLines.HiZ();
			 usleep(10);
			 
			 printf("\n");
		}
		
		const char* pSramFileName = "./sram.ram";
		
		FILE* pFile = fopen(pSramFileName, "wb");
		if(pFile)
		{
			printf("Wrote gSRAMBuffer to file '%s'\n", pSramFileName);
			fwrite(gSRAMBuffer, sizeof(gSRAMBuffer), 1, pFile);
			
			fclose(pFile);
			pFile = NULL;
		}
		else
		{
			printf("Failed to open file '%s' for write!\n", pSramFileName);
		}
	}
	
	gAddressLines.Release();
	gDataLines.Release();
	gWriteEnable.Release();
	
	if(pChip)
	{
		gpiod_chip_close(pChip);
		pChip = nullptr;
	}
	
	return 0;
}