/*
 * FatFs-SD.cpp
 *
 *  Created on: Aug 6, 2016
 *      Author: aaron
 */

#include "FatFs-SD.h"

extern "C" void __ff_spi_send_dma(SPIClass& spi, const BYTE* buff, const UINT btx) {
#if PLATFORM_THREADING
 #ifdef SYSTEM_VERSION_060
		//use a queue to signal because the firmware implementation at the time of writing
		//checks to use the ISR version of put when appropriate
		os_queue_t signal;
		os_queue_create(&signal, sizeof(void*), 1, nullptr);
		void* result;
 #else
		std::mutex signal;
 #endif

		invoke_trampoline([&](HAL_SPI_DMA_UserCallback callback){

 #ifndef SYSTEM_VERSION_060
			signal.lock();
 #endif
			spi.transfer((BYTE*)buff, nullptr, btx, callback);

 #ifdef SYSTEM_VERSION_060
			os_queue_take(signal, &result, CONCURRENT_WAIT_FOREVER, nullptr);
			os_queue_destroy(signal, nullptr);
			signal = nullptr;
 #else
			signal.lock();
			signal.unlock(); //superfluous, but...but...
 #endif
		}, [&]() {
 #ifdef SYSTEM_VERSION_060
			os_queue_put(signal, result, 0, nullptr);
 #else
			signal.unlock();
 #endif
		});
#else
		//DMA not working on core
		for(size_t i = 0; i < btx; i++)
			spi.transfer((BYTE)buff[i]);
#endif
}


extern "C" void __ff_spi_receive_dma(SPIClass& spi, BYTE* buff, const UINT btr, const BYTE sendByte) {
	/* Read multiple bytes, send 0xFF as dummy */
	memset(buff, sendByte, btr);

#if PLATFORM_THREADING
#ifdef SYSTEM_VERSION_060
	//use a queue to signal because the firmware implementation at the time of writing
	//checks to use the ISR version of put when appropriate
	os_queue_t signal;
	os_queue_create(&signal, sizeof(void*), 1, nullptr);
	void* result;
#else
	std::mutex signal;
#endif

	invoke_trampoline([&](HAL_SPI_DMA_UserCallback callback){

#ifndef SYSTEM_VERSION_060
		signal.lock();
#endif
		spi.transfer(buff, buff, btr, callback);

#ifdef SYSTEM_VERSION_060
		os_queue_take(signal, &result, CONCURRENT_WAIT_FOREVER, nullptr);
		os_queue_destroy(signal, nullptr);
		signal = nullptr;
#else
		signal.lock();
		signal.unlock(); //superfluous, but...but...
#endif
	}, [&]() {
#ifdef SYSTEM_VERSION_060
		os_queue_put(signal, result, 0, nullptr);
#else
		signal.unlock();
#endif
	});
#else
	//DMA not working on core
	for(size_t i = 0; i < btr; i++)
		buff[i] = spi.transfer(0xff);
#endif
}


