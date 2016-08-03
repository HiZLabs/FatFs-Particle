# FatFs-Particle

FatFs-Particle is a FAT disk access library with linkage for Particle

  - Includes SD card (SPI mode) driver
  - Simple driver interface for extending to any storage medium
  - Currently only supports multi-thread configurations
  - MIT license for use in proprietary closed-source projects

**Quickstart: SD card**

1. Include FatFs-Particle library
2. Delare instance of driver
3. Setup driver
4. Attach (mount) driver

```
/* FatFsSdExample.ino */
#include "application.h"
#include <FatFs-Particle/FatFs-Particle.h>

FatFsSd SD0;
void setup() 
{
    SD0.begin(SPI, A2);
    FRESULT result = FatFs::attach(SD0, 0);
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

After attaching the drive, you will use the FatFs API to use the drive. For more info see the [FatFs documentation](http://elm-chan.org/fsw/ff/00index_e.html). The drive is attached using the drive number you supply. In the example code, the SD card is attached on drive number 0, so the file `\test.txt` would be accessed at the path `0:/test.txt`.

**`FatFs::` static functions** -- attaching and detaching drivers and interpresting error messages

| function name | function description |
| ------------- | -------------------- |
| `FRESULT FatFs::attach(FatFsDriver& driver, BYTE driveNumber)` | attach a driver to a drive number |
|`void FatFs::detach(BYTE driveNumber)`| detach a driver (does not close open files) |
|`const char* FatFs::fileResultMessage(FRESULT fileResult) { return FR_string(fileResult)`| returns a user-readable status message for FRESULT error codes

**
