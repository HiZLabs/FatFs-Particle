# FatFs (Particle library)

FatFs includes the FatFs library from ChaN (v0.12) and a C++ driver interface. Up to 4 drivers can be loaded simultaneously.

**Goals of this project**

- Filesystem I/O on Particle with a license compatible with proprietary projects
- Separate out device drivers from filesystem driver in an extensible fashion
- Dynamically attach drivers

**Quickstart** (using FatFsSD driver included in library)

1. Include FatFs library
2. Include a driver library
3. Declare and set up instance of driver
4. Attach (mount) driver

```c++
#include <FatFs/FatFs.h>

//If using another driver, include the library
//#include <OtherDriver/OtherDriver.h>

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
        Serial.printlnf("Drive attach failed: %s", FatFs::fileResultMessage(result));
    }
}

void loop() 
{
    //do stuff
}
```

**Using the filesystem** | 
After attaching the drive, you will use the FatFs API to use the drive. For reference and examples for interacting with the filesystem, see the [FatFs documentation](http://elm-chan.org/fsw/ff/00index_e.html). 

The drive is attached using the drive number you supply. In the example code, the SD card is attached on drive number 0, so the file `\test.txt` on the SD card would be accessed at the path `0:/test.txt`.

The included example program [01_FatFs_Examples.ino](https://github.com/HiZLabs/FatFs-Particle/tree/master/firmware/examples/01_FatFs_Examples.ino) demonstrates performing several file operations including reading and writing files, getting file info such as size and attributes, and traversal of the directory structure. Setup info and sample output is in the code file.

API Reference
=============

**`FatFs::` function reference** | attaching and detaching drivers and interpreting error messages

| function      | description          |
| ------------- | -------------------- |
| `FRESULT FatFs::attach(FatFsDriver& driver, BYTE driveNumber)` | attach a driver to a drive number |
|`void FatFs::detach(BYTE driveNumber)`| detach a driver (does not close open files) |
|`const char* FatFs::fileResultMessage(FRESULT fileResult)`| returns a user-readable status message for FRESULT error codes|

**`FatFsSD` member function reference** | configuring and using an instance of the driver

| function      | description          |
| ------------- | -------------------- |
| `void begin(SPIClass& spi, const uint16_t cs)` | Set up instance with an SPI interface and CS pin. |
|`void begin(SPIClass& spi, const uint16_t cs, std::mutex& mutex)`| Set up instance with SPI interface, CS pin, and a mutex for sharing access to the SPI interface. *Available only on threaded platforms.*|
|`void enableCardDetect(const uint16_t cdPin, uint8_t activeState)`| Set up a pin to signal whether or not card is present. Pass `HIGH` or `LOW` for `activeState`. |
| `void enableWriteProtectDetect(const Pin& wpPin, bool activeState)` | Set up a pin to signal whether or not card is write protected. Pass `HIGH` or `LOW` for `activeState`. |
| `uint32_t highSpeedClock()` | Returns the current high-speed clock limit in Hz. High speed is used after the card interface has been initialized. |
| `void highSpeedClock(uint32_t clock)` | Sets the high-speed clock maximum speed in Hz. The initial value is `15000000`. |
| `uint32_t lowSpeedClock()` | Returns the current low-speed clock limit in Hz. Low speed is used for initialization of the SD card. |
| `void lowSpeedClock(uint32_t clock)` | Sets the low-speed clock maximum speed in Hz. The initial value is `400000`. |
| `uint32_t activeClock()` | Returns the active clock speed limit in Hz. |
| `bool wasBusySinceLastCheck()` | Returns true if the disk was read or written since the last call. (For use in a UI loop to update the status of an LED) |


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
