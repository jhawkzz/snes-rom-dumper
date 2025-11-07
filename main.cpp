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
	
#include <cstring>
#include <cstdint>
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

//todo: put in RomManager.h
#define MAX_ROM_INFOS (2)
struct RomInfo
{
public:
	RomInfo() : mSRAMSize(0)
	{
		memset(mRomName, 0, sizeof(mRomName));
	}
	
	char mRomName[256];
	uint32_t mSRAMSize;	
};

uint32_t gNumRomInfos = 0;
RomInfo gRomInfos[MAX_ROM_INFOS];

bool AddRomInfo(const char* pRomName, uint32_t sramSize)
{
	if(gNumRomInfos + 1 > MAX_ROM_INFOS)
	{
		printf("AddRomInfo: Cannot add more roms. Increase MAX_ROM_INFOS which is size: %d\n", MAX_ROM_INFOS);
		return false;
	}
	
	strcpy(gRomInfos[gNumRomInfos].mRomName, pRomName);
	gRomInfos[gNumRomInfos].mSRAMSize = sramSize;
	
	gNumRomInfos++;
	return true;
}

bool CreateRomInfos()
{
	// Final Fantasy 2 (ff2)
	if(!AddRomInfo("ff2", 8192)) return false;
	
	// Super Mario World
	if(!AddRomInfo("smw", 1024)) return false;
	
	return true;
}

RomInfo* GetRomInfo(const char* pRomName)
{
	for(uint32_t i = 0; i < gNumRomInfos; i++)
	{
		if(!strcmp(gRomInfos[i].mRomName, pRomName))
		{
			return &gRomInfos[i];
		}
	}
	
	return nullptr;
}
//

// todo: put in GPIOManager.h
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
		}
		
		return value;
	}
	
private:
	GPIOLine mLines[NumLines];
};
//

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

void WriteSRAM(RomInfo* pRomInfo)
{
	char sramFileName[300] = { 0 };
	snprintf(sramFileName, sizeof(sramFileName) - 1, "./%s.srm", pRomInfo->mRomName);
		
	FILE* pFile = fopen(sramFileName, "rb");
	if(pFile)
	{
		fread(gSRAMBuffer, pRomInfo->mSRAMSize, 1, pFile);
		
		fclose(pFile);
		pFile = NULL;
	}
	else
	{
		printf("Failed to open file '%s' for write!\n", pRomInfo->mRomName);
		return;
	}
	
	usleep(1);
	for(uint32_t i = 0; i < pRomInfo->mSRAMSize; i++ )
	{
		uint32_t address = i;
		
		 gWriteEnable.Write(1);
		 usleep(10);
		 
		 gDataLines.HiZ();
		 usleep(10);
		
		 gAddressLines.Write(address);
		 usleep(10);
		 
		 gWriteEnable.Write(0);
		 usleep(10);
		
		 printf("%x: %x\n", address, gSRAMBuffer[i]);
		 gDataLines.Write(gSRAMBuffer[i]);
		 usleep(10);
		
		 gWriteEnable.Write(1);
		 usleep(10);
		 
		 gDataLines.HiZ();
		 usleep(10);
	}
	
	printf("WriteSRAM: Uploaded contents of file '%s' to Cart SRAM\n", sramFileName);
}

void ReadSRAM(RomInfo* pRomInfo)
{
	usleep(1);
	for(uint32_t i = 0; i < pRomInfo->mSRAMSize; i++ )
	{
		uint32_t address = i;
		
		 // read
		 gAddressLines.Write(address);
		 usleep(10);
		 
		 gDataLines.HiZ();
		 usleep(10);
		
		 uint8_t value = gDataLines.Read();
		 usleep(10);
		 gSRAMBuffer[i] = value;
		 printf("%x: %x\n", address, value);
		 
		 gDataLines.HiZ();
		 usleep(10);
	}
	
	char sramFileName[300] = { 0 };
	snprintf(sramFileName, sizeof(sramFileName) - 1, "./%s.srm", pRomInfo->mRomName);
	
	FILE* pFile = fopen(sramFileName, "wb");
	if(pFile)
	{
		printf("ReadSRAM: Wrote contents to file '%s'\n", sramFileName);
		fwrite(gSRAMBuffer, pRomInfo->mSRAMSize, 1, pFile);
		
		fclose(pFile);
		pFile = NULL;
	}
	else
	{
		printf("Failed to open file '%s' for write!\n", sramFileName);
	}
}

void RunMain(const char* pRomName)
{
	RomInfo* pRomInfo = GetRomInfo(pRomName);
	if(!pRomInfo)
	{
		printf("Could not find rom info for rom '%s'\n", pRomName);
		return;
	}
	
	// Configure lines for cartridge insertion
	gWriteEnable.Write(1);
	usleep(100);
	
	gReset.Write(0);
	usleep(100);
	
	gAddressLines.HiZ();
	gDataLines.HiZ();
	usleep(100);
	//
	
	printf("INSERT GAME and press any key.\n");
	while(!getchar())
	{
	}
	
	// Set Reset High so we send sram 5v and can read/write.
	gReset.Write(1);
	usleep(100);

	bool shouldExit = false;	
	while(!shouldExit)
	{
		printf("[R]ead from SRAM\n");
		printf("[W]rite to SRAM\n");
		printf("E[x]it\n");
		printf("Your Selection: ");

		uint32_t inputChoice = 0;
		uint32_t c = 0;
		do
		{
			c = getchar();
			if(c != '\n')
			{
				inputChoice = c;
			}
		}
		while(c != '\n');
			
		switch(inputChoice)
		{
			case 'r':
			{
				ReadSRAM(pRomInfo);
				break;
			}
			
			case 'w':
			{
				WriteSRAM(pRomInfo);
				break;
			}
			
			case 'x':
			{
				shouldExit = true;
				break;
			}
			
			default:
			{
				printf("Unknown choice '%c'\n", inputChoice);
				break;
			}
		}
	}
	
	
	// Configure lines for removing cartridge
	gWriteEnable.Write(1);
	usleep(100);
	
	gReset.Write(0);
	usleep(100);
	
	gAddressLines.HiZ();
	gDataLines.HiZ();
	usleep(100);
	
	printf("REMOVE CARTRIDGE and press any key.\n");
	while(!getchar())
	{
	}
}

int main(int argc, const char** argv)
{
	if(argc == 1)
	{
		printf("Not enough arguments supplied!\n");
		printf("Try --game [gamename]\n");
		return 0;
	}
	
	const char chipName[] = "gpiochip0";
	
	gpiod_chip* pChip = gpiod_chip_open_by_name(chipName);
	if(!pChip)
	{
		printf("open chip failed\n");
		return 0;
	}
	
	if(!CreateRomInfos())
	{
		printf("Failed to create Rom Infos\n");
		return 0;
	}
	
	// setup bus lines
	gAddressLines.Create(pChip, gAddressLineIndices);
	gDataLines.Create(pChip, gDataLineIndices);
	gWriteEnable.Create(pChip, gWriteLineIndices);
	gReset.Create(pChip, gResetLineIndices);
	
	if(!strcmp(argv[1], "--game"))
	{
		if(argc == 3)
		{
			RunMain(argv[2]);
		}
		else
		{
			printf("Not enough args for '--game'. Expected '--game [gamename]'\n");
		}
	}
	// Random Unit Test stuff
	else
	{
		if(argc > 2)
		{
			if(!strcmp(argv[1], "--game"))
			{
				RunMain(argv[2]);
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
				
			}
			else
			{
				LOG("Unknown arg. Did you forget a value for the arg?");
			}
		}
	}
	//
	
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