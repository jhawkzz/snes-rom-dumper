#include <cstring>
#include <cstdint>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

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
		ConfigForHiZ();
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
	
	void ConfigForHiZ()
	{
		if(mDirection != Direction::HiZ)
		{
			mDirection = Direction::HiZ;
			
			Release();
			
			if(!OpenLine())
			{
				LOG("Failed to open line.");
				return;
			}
			
			int32_t result = gpiod_line_request_input_flags(mpLine, gRequestingProgram, GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
			//int32_t result = gpiod_line_request_input(mpLine, gRequestingProgram);
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
		Output,
		HiZ
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
			//printf("lineVal %d is %d\n", i, lineVal);
		}
		
		return value;
	}
	
private:
	GPIOLine mLines[NumLines];
};

//jhm 11-3 gpio21 IS NO LONGER ADDRESS LINE 15 IT IS RESET PIN.
uint8_t gAddressLineIndices[] = {2,3,4,17,27,22,10,9,5,6,13,19,26,16,20};
GPIOLineArray<15> gAddressLines;

uint8_t gDataLineIndices[] = {14,15,18,23,24,25,8,7};
GPIOLineArray<8> gDataLines;

uint8_t gWriteLineIndices[] = {12};
GPIOLineArray<1> gWriteEnable;

uint8_t gResetLineIndices[] = {21};
GPIOLineArray<1> gReset;

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

void WriteSRAM(uint32_t sramSize)
{
	const char* pSramFileName = "./sram.ram";
		
	FILE* pFile = fopen(pSramFileName, "rb");
	if(pFile)
	{
		printf("Read file to gSRAMBuffer from '%s'\n", pSramFileName);
		fread(gSRAMBuffer, sizeof(gSRAMBuffer), 1, pFile);
		
		fclose(pFile);
		pFile = NULL;
	}
	else
	{
		printf("Failed to open file '%s' for write!\n", pSramFileName);
	}
	
	printf("Writing SRAM at range $70:0000 - $70:03FF\n");
	usleep(1);
	for(uint32_t i = 0; i < sramSize; i++ )
	{
		uint32_t address = i;
		
		 //LOG("---Write---");
		 //LOG("Setting write enable high");
		 gWriteEnable.Write(1);
		 usleep(10);
		 
		 //LOG("Setting data hiZ");
		 gDataLines.HiZ();
		 usleep(10);
		
		 //printf("Setting address 0%x\n", address);
		 gAddressLines.Write(address);
		 usleep(10);
		 
		 //LOG("Setting write enable low");
		 gWriteEnable.Write(0);
		 usleep(10);
		
		 printf("%x: %x\n", address, gSRAMBuffer[i]);
		 gDataLines.Write(gSRAMBuffer[i]);
		 usleep(10);
		
		 //LOG("Setting write enable high");
		 gWriteEnable.Write(1);
		 usleep(10);
		 
		 //LOG("Setting data hiZ");
		 gDataLines.HiZ();
		 usleep(10);
	}
}

void ReadSRAM(uint32_t sramSize)
{
	printf("Dumping SRAM at range $70:0000 - $70:03FF\n");
	usleep(1);
	for(uint32_t i = 0; i < sramSize; i++ )
	{
		uint32_t address = i;
		
		 // read
		 //printf("Setting Read Address to: 70:%04x\n", address);
		 gAddressLines.Write(address);
		 usleep(10);
		 
		 //printf("Setting Data Bus to: HiZ\n");
		 gDataLines.HiZ();
		 usleep(10);
		
		 uint8_t value = gDataLines.Read();
		 printf("%x: %x\n", address, value);
		 usleep(10);
		 gSRAMBuffer[i] = value;
		 
		 //printf("Setting Data Bus to: HiZ\n");
		 gDataLines.HiZ();
		 usleep(10);
		 
		 //printf("\n");
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
	gReset.Create(pChip, gResetLineIndices);
	
	if(argc > 2)
	{
		if(!strcmp(argv[1], "--read-address"))
		{
			uint16_t addressVal = atoi(argv[2]);
			
			gAddressLines.HiZ();
			gDataLines.HiZ();
			usleep(10);
		
			gWriteEnable.Write(1);
			usleep(10);
		
			gAddressLines.Write(addressVal % 65536);
			usleep(10);
			gDataLines.HiZ();
			usleep(10);
			uint8_t dataVal = gDataLines.Read();
			printf("Address: $%04x contains data: $%x\n", addressVal, dataVal);
			
			gAddressLines.HiZ();
			gDataLines.HiZ();
			usleep(10);
		
			printf("REMOVE CARTRIDGE NOW. Then press any key to exit.\n");
			while(!getchar())
			{
			}
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
		else if(!strcmp(argv[1], "--hiz"))
		{
			gAddressLines.HiZ();
			gDataLines.HiZ();
			gWriteEnable.HiZ();
		}
		else if (!strcmp(argv[1], "--read") || !strcmp(argv[1], "--write"))
		{
			//11-3-2025: Reading and writing is stable. I put the reset line on a gpio port, set it to low until you insert the game,
			//then set it high, read/write, then set it low and tell you to remove it. This ensures that the game uses the battery until its 
			//actually time to read/write, and avoids power fluctuations that alter sram state on insertion/removal.
			//I tested:
			//SWM - Insert, read, remove, insert, read: identical
			//FF2 - Insert, WRITE, remove, play in snes, read, remove, insert in SNES, remove, read, remove, read.
			//		In this instance, it remained identical through and through except for the times i put it in the snes, and then two bytes 
			//		changed at address  16A3-16A4. But when i just removed/inserted/removed/inserted (no snes) it remained binary identical.
			//		its clear that these bytes are being changed by the game.
			// where we stand:
			// lines 0-14 are address lines 
			// lines 0-7 are data lines 
			// write line 
			// reset line
			// all otheres are hardwired.
			// the trick is the write line and reset lines must be high and low respectively when inserting/removing the cart.
			// also, LEDs do drain power and further hurt data integrity.
			gAddressLines.HiZ();
			gDataLines.HiZ();
			usleep(10);
			
			gWriteEnable.Write(1);
			usleep(10);
			
			gReset.Write(0);
			usleep(10);
			
			bool bReading = true;
			if(!strcmp(argv[1], "--write"))			
			{
				bReading = false;
			}
			
			printf("Preparing to: %s - Insert game and press any key.\n", bReading ? "READ" : "WRITE");
			while(!getchar())
			{
			}
			
			gReset.Write(1);
			usleep(10);
			
			uint32_t ff2Size = 8192;
			uint32_t smwSize = 1024;
			
			if(bReading)
			{
				ReadSRAM(ff2Size);
			}
			else
			{
				WriteSRAM(ff2Size);
			}
			
			// Wrap Up
			gWriteEnable.Write(1);
			usleep(10);
			
			gReset.Write(0);
			usleep(10);
			
			gAddressLines.HiZ();
			gDataLines.HiZ();
			usleep(10);
			
			printf("REMOVE CARTRIDGE NOW. Then press any key to exit.\n");
			while(!getchar())
			{
			}
		}
		else
		{
			LOG("Unknown arg. Did you forget a value for the arg?");
		}
	}
	else
	{		 
		LOG("No args provided. Maybe you wanted --read or --write?");
	}
	
	gAddressLines.Release();
	gDataLines.Release();
	gWriteEnable.Release();
	gReset.Release();
	
	if(pChip)
	{
		gpiod_chip_close(pChip);
		pChip = nullptr;
	}
	
	return 0;
}