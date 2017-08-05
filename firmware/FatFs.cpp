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

#include "FatFs.h"

LOG_SOURCE_CATEGORY("fatfs.diskio");

extern "C" DSTATUS disk_initialize(BYTE pdrv)
{
	LOG(TRACE, "initialize drive %d", pdrv);
	if(FatFs::_drivers[pdrv] == nullptr)
		return STA_NOINIT;
	return FatFs::_drivers[pdrv]->initialize();
}

extern "C" DSTATUS disk_status(BYTE pdrv)
{
//	LOG(TRACE, "status drive %d", pdrv);
	if(FatFs::_drivers[pdrv] == nullptr)
		return STA_NOINIT;
	return FatFs::_drivers[pdrv]->status();
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
//	LOG(TRACE, "read drive %d", pdrv);
	if(FatFs::_drivers[pdrv] == nullptr)
		return RES_PARERR;
	return FatFs::_drivers[pdrv]->read(buff, sector, count);
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
//	LOG(TRACE, "write drive %d", pdrv);
	if(FatFs::_drivers[pdrv] == nullptr)
		return RES_PARERR;
	return FatFs::_drivers[pdrv]->write(buff, sector, count);
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
//	LOG(TRACE, "ioctl drive %d, cmd %d", pdrv, cmd);
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
	LOG(INFO, "attaching drive %d", driveNumber);
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
	_drivers[driveNumber]->_attached = result == FR_OK;

	if(result != FR_OK) {
		_drivers[driveNumber] = nullptr;
		return result;
	}

	_drivers[driveNumber]->_driveNumber = driveNumber;

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
		driver->_driveNumber = DRIVE_NOT_ATTACHED;
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

FatFsDriver::~FatFsDriver() {
	if(_attached) {
		FatFs::detach(_driveNumber);
	}
}


extern "C" BYTE __get_system_is_threaded() {
    return system_thread_get_state(NULL) != spark::feature::DISABLED &&
      (system_mode()!=SAFE_MODE);
}

extern "C" FRESULT f_copy(const char* src, const char* dst)
{
	FIL srcf;
	FIL dstf;

	FRESULT result = f_open(&srcf, src, FA_READ | FA_OPEN_EXISTING);
	if(result != FR_OK)
		return result;

	result = f_open(&dstf, dst, FA_WRITE | FA_OPEN_ALWAYS);
	if(result != FR_OK)
	{
		f_close(&srcf);
		return result;
	}

	size_t srcBlockSize = disk_ioctl(srcf.obj.fs->drv, GET_BLOCK_SIZE, nullptr);
	size_t dstBlockSize = disk_ioctl(dstf.obj.fs->drv, GET_BLOCK_SIZE, nullptr);
	size_t blockSize = min(srcBlockSize, dstBlockSize);
	uint8_t buf[blockSize];
	UINT br = 0;
	UINT bw = 0;
	FSIZE_t size = f_size(&srcf);

	for(FSIZE_t totalRead = 0; result == FR_OK && totalRead < size; totalRead += br)
	{
		result = f_read(&srcf, buf, blockSize, &br);
		if(result != FR_OK)
			break;
		result = f_write(&dstf, buf, br, &bw);
	}

	f_close(&srcf);
	f_close(&dstf);

	return result;
}

extern "C" FRESULT f_getline(FIL* fp, TCHAR* buf, int len)
{
	buf[0] = 0;
	FSIZE_t startPos = f_tell(fp);
	UINT br;
	FRESULT result = f_read(fp, buf, len, &br);
	buf[len - 1] = 0;
	if(result != FR_OK)
		return result;
	if(br == 0)
		return FR_INVALID_OBJECT;
	int i = 0;
	for(; i < len; i++)
	{
		if(buf[i] == '\n')
		{
			//set the end of the string at before the endl (check for DOS line endings)
			if(i > 0 && buf[i-1] == '\r')
				buf[i-1] = 0;
			else
				buf[i] = 0;
			break;
		}
	}

	if(i >= len)
		return FR_INVALID_PARAMETER; //should we seek back in this case?

	//reposition pointer to position after endl
	result = f_lseek(fp, startPos + i + 1);
	return result;
}

inline uint32_t timeBetween(uint32_t past, uint32_t future) { return past <= future ? future - past : UINT32_MAX - past + future; }

struct __ff_mutexes
{
	std::recursive_mutex m;
	std::recursive_mutex* mp;
};

static __ff_mutexes _mutexes[_VOLUMES];

extern "C" void* __ff_create_mutex(BYTE drv)
{
	__ff_mutexes* m = &_mutexes[drv];
	m->mp = nullptr;
	FatFs::driver(0)->ioctl(drv, &m->mp);
	return m;
}

extern "C" uint8_t  __ff_lock(void* mutex, uint32_t timeout)
{
	__ff_mutexes* m = (__ff_mutexes*)mutex;
	std::recursive_mutex* mp = m->mp? m->mp : &m->m;
	uint32_t begin = millis();
	if(timeout == CONCURRENT_WAIT_FOREVER || timeout == 0)
	{
		mp->lock();
		return true;
	}

	while(timeBetween(begin, millis()) <= timeout)
	{
		if(mp->try_lock())
			return true;
		delay(10);
	}
	return false;
}

extern "C" void __ff_unlock(void* mutex)
{
	__ff_mutexes* m = (__ff_mutexes*)mutex;
	std::recursive_mutex* mp = m->mp? m->mp : &m->m;
	mp->unlock();
}

extern "C" void __ff_destroy(void* m)
{
	((__ff_mutexes*)m)->mp = nullptr;
}
