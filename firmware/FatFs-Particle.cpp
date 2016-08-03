/*----------------------------------------------------------------------------/
/  FatFs Driver Linkage for Particle                                          /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2016, ChaN, Aaron Heise, all right reserved.
/
/ FatFs module is an open source software. Redistribution and use of FatFs in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:

/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/

#include "FatFs-Particle.h"

extern "C" DSTATUS disk_initialize(BYTE pdrv)
{
	if(FatFs::_drivers[pdrv] == nullptr)
		return STA_NOINIT;
	return FatFs::_drivers[pdrv]->initialize();
}

extern "C" DSTATUS disk_status(BYTE pdrv)
{
	if(FatFs::_drivers[pdrv] == nullptr)
		return STA_NOINIT;
	return FatFs::_drivers[pdrv]->status();
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
	if(FatFs::_drivers[pdrv] == nullptr)
		return RES_PARERR;
	return FatFs::_drivers[pdrv]->read(buff, sector, count);
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
	if(FatFs::_drivers[pdrv] == nullptr)
		return RES_PARERR;
	return FatFs::_drivers[pdrv]->write(buff, sector, count);
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
	if(FatFs::_drivers[pdrv] == nullptr)
		return RES_PARERR;
	return FatFs::_drivers[pdrv]->ioctl(cmd, buff);
}

extern "C" DWORD get_fattime(void) {
	/* Returns current time packed into a DWORD variable */
	return	  ((DWORD)(Time.year() - 1980) << 25)	/* Year 2013 */
			| ((DWORD)Time.month() << 21)				/* Month 7 */
			| ((DWORD)Time.day() << 16)				/* Mday 28 */
			| ((DWORD)Time.hour() << 11)				/* Hour 0 */
			| ((DWORD)Time.minute() << 5)				/* Min 0 */
			| ((DWORD)Time.second() >> 1);				/* Sec 0 */
}

std::vector<FatFsDriver*> FatFs::_drivers;

FRESULT FatFs::attach(FatFsDriver& driver, BYTE driveNumber)
{
	while(driveNumber >= _drivers.size())
		_drivers.push_back(nullptr);

	if(_drivers[driveNumber] != nullptr)
		return FR_INVALID_DRIVE;

	_drivers[driveNumber] = &driver;

	char path[3];
	path[0] = '0' + driveNumber;
	path[1] = ':';
	path[2] = 0;

	FRESULT result = f_mount(&_drivers[driveNumber]->fs, path, 1);
	_drivers[driveNumber]->_attached = result == 0;

	if(!_drivers[driveNumber]->_attached) {
		_drivers[driveNumber] = nullptr;
		return FR_DISK_ERR;
	}

	return result;
}

void FatFs::detach(BYTE driveNumber)
{
	FatFsDriver* driver = _drivers[driveNumber];
	if(driver != nullptr && driver->_attached)
	{
		char path[3];
		path[0] = driveNumber;
		path[1] = ':';
		path[2] = 0;
		f_mount(nullptr, path, 0);
		_drivers[driveNumber] = nullptr;
	}
}

const char* FR_strings[] = {
		/*FR_OK = 0*/				"(0) Succeeded",
		/*FR_DISK_ERR*/				"(1) A hard error occurred in the low level disk I/O layer",
		/*FR_INT_ERR*/				"(2) Assertion failed",
		/*FR_NOT_READY*/			"(3) The physical drive cannot work",
		/*FR_NO_FILE*/				"(4) Could not find the file",
		/*FR_NO_PATH*/				"(5) Could not find the path",
		/*FR_INVALID_NAME*/			"(6) The path name format is invalid",
		/*FR_DENIED*/				"(7) Access denied due to prohibited access or directory full",
		/*FR_EXIST*/				"(8) Access denied due to prohibited access",
		/*FR_INVALID_OBJECT*/		"(9) The file/directory object is invalid",
		/*FR_WRITE_PROTECTED*/		"(10) The physical drive is write protected",
		/*FR_INVALID_DRIVE*/		"(11) The logical drive number is invalid",
		/*FR_NOT_ENABLED*/			"(12) The volume has no work area",
		/*FR_NO_FILESYSTEM*/		"(13) There is no valid FAT volume",
		/*FR_MKFS_ABORTED*/			"(14) The f_mkfs() aborted due to any problem",
		/*FR_TIMEOUT*/				"(15) Could not get a grant to access the volume within defined period",
		/*FR_LOCKED*/				"(16) The operation is rejected according to the file sharing policy",
		/*FR_NOT_ENOUGH_CORE*/		"(17) LFN working buffer could not be allocated",
		/*FR_TOO_MANY_OPEN_FILES*/	"(18) Number of open files > _FS_LOCK",
		/*FR_INVALID_PARAMETER*/	"(19) Given parameter is invalid",
};
