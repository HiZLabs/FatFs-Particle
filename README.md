# FatFs (Particle library)

FatFs includes the FatFs library from ChaN (v0.12) and a C++ wrapper and driver interface. Up to 4 drivers can be loaded simultaneously.

**Quickstart** (using FatFs-SD driver for SD cards)

1. Include FatFs library
2. Include a driver library
3. Declare and set up instance of driver
4. Attach (mount) driver

```c++
SYSTEM_THREAD(ENABLED); //required for FatFs-SD
#include <FatFs/FatFs.h>

//Include a driver library
#include <FatFs-SD/FatFs-SD.h>
//Declare an instance of the driver
FatFsSD SD0;

void setup() 
{
    //Set up driver instance
    SD0.begin(SPI, A2);
    //Attach driver instance to drive 0:
    FRESULT result = FatFs::attach(SD0, 0);
    //FatFs operations return FRESULT codes. FR_OK indicates success.
    if(result != FR_OK)
    {
        Serial.print("Drive attach failed: ");
        Serial.println(FatFs::fileResultMessage(result));
    }
}

void loop() 
{
    //do stuff
}
```

After attaching the drive, you will use the FatFs API to use the drive. For reference and examples for interacting with the filesystem, see the [FatFs documentation](http://elm-chan.org/fsw/ff/00index_e.html). 

The drive is attached using the drive number you supply. In the example code, the SD card is attached on drive number 0, so the file `\test.txt` on the SD card would be accessed at the path `0:/test.txt`.

**`FatFs::` function reference** | attaching and detaching drivers and interpreting error messages

| function      | description          |
| ------------- | -------------------- |
| `FRESULT FatFs::attach(FatFsDriver& driver, BYTE driveNumber)` | attach a driver to a drive number |
|`void FatFs::detach(BYTE driveNumber)`| detach a driver (does not close open files) |
|`const char* FatFs::fileResultMessage(FRESULT fileResult)`| returns a user-readable status message for FRESULT error codes|

**Custom Drivers** | A driver for any storage device can be created by extending the abstract class `FatFsDriver`. This driver can then be attached using `FatFs::attach()`. For more information, see the `disk_` functions in the [FatFs documentation](http://elm-chan.org/fsw/ff/00index_e.html) under the section *Device Control Interface*.

```c++
class MyCustomFatFsDriver : public FatFsDriver {
public:
	virtual DSTATUS initialize();
	virtual DSTATUS status();
	virtual DRESULT read(BYTE* buff, DWORD sector, UINT count);
	virtual DRESULT write(const BYTE* buff, DWORD sector, UINT count);
	virtual DRESULT ioctl(BYTE cmd, void* buff);
	virtual ~FatFsDriver();
};
```

*Note on the read/write commands:* the `sector` parameter the driver uses a sector size of 512 bytes, so your driver will need to translate from 512 byte sector indexes to the appropriate addresses for your medium.