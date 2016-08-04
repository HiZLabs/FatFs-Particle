/* The MIT License (MIT)
 *
 * Copyright (c) 2016 Hi-Z Labs, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FATFS_PARTICLE_TRAMPOLINE_H_
#define FATFS_PARTICLE_TRAMPOLINE_H_

#ifndef _INVOKE_TRAMPOLINE_DEFINED
#define _INVOKE_TRAMPOLINE_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*trampoline_callback_t)();

/*! \brief A tool to allow the execution of a lambda with captures (or a C function pointer with a state argument) via
 *         a parameterless C function pointer
 *
 *  Uses a nested function to obtain a parameterless function pointer that can be invoked with a state. This allows, in
 *  particular cases, a function pointer with no parameters to be supplied in a case when state is required.
 *
 *  The particular scenario this was created for was a DMA callback in a multithreaded environment.
 *
 *  	void sendBuffer(SPIClass& spi, uint8_t* buffer, size_t count) {
 *  		std::mutex signal;												//use a mutex to signal between threads
 *  		signal.lock(); 													//signal the transfer has started
 *  		auto transferCompleteCallback = [&](){ signal.unlock() }; 		//set up the callback for transfer complete
 *  		spi.transfer(buffer, nullptr, count, transferCompleteCallback);	//set up and enable DMA send
 *  		signal.lock();													//block thread until DMA callback unlocks signal
 *  		signal.unlock();												//release signal
 *  	}
 *
 *  The idea is to allow ready threads of any priority to run while waiting for the DMA transfer to complete. However, this
 *  code won't compile. The lambda used for the callback cannot be invoked as a function pointer, because it captures.
 *  If it can't capture, a global variable would have to be used, which would require a separate signal instance for each
 *  possible SPIClass passed in. (Which you might have one set of already if you are sharing buses between multiple
 *  devices and threads.) Additionally, a separate function for each signal is going to be needed then to use for the
 *  callback to reference the appropriate signal name.
 *
 *  There is no solution to this in standard C/C++. However, GCC has a C-only extension that allows nested functions (not
 *  allowed in C++, however). Nested functions are allowed to reference variables from the enclosing function, which allows
 *  a parameterless function to access some state instance information. This approach has one caveat: the nested function
 *  becomes invalid when the enclosing function returns. (https://gcc.gnu.org/onlinedocs/gcc/Nested-Functions.html)
 *  In the scenario in question, that caveat is not a problem, since the enclosing function won't return until after the
 *  nested function has returned.
 *
 *  The realization is pretty simple (see trampoline.c), but it requires some slight rethinking of the problem. The code
 *  that uses the parameterless function pointer must be put into a function that takes the pointer as an argument, and
 *  the code that the parameterless function pointer invokes is in another one. Both functions take a void* for state info.
 *
 *  The C++ wrapper makes this very simple to use. The C++ invoke_trampoline(...) takes two std::functions as arguments:
 *  the function that uses the parameterless function pointer and the callback. These functions can be lambdas that capture,
 *  so now the situation presented in the sendBuffer(...) example above is possible, with some slight tweaking...
 *
 *    	void sendBuffer(SPIClass& spi, uint8_t* buffer, size_t count) {
 *  		std::mutex signal;														//use a mutex to signal between threads
 *  		invoke_trampoline(
 *  			[&](trampoline_callback_t transferCompleteCallback){				//the first lambda will receive the callback as a parameter
 *  				signal.lock(); 													//signal the transfer has started
 *  				spi.transfer(buffer, nullptr, count, transferCompleteCallback);	//set up and enable DMA send
 *  				signal.lock();													//block thread until DMA callback unlocks signal
 *  				signal.unlock();												//release signal
 *  			},
 *  			[&](){ signal.unlock() }											//the second lambda will be what is invoked in the callback
 *  		);
 *
 *	It works in this case because we block the first function from returning before the second function does. This method
 *	cannot be used to obtain a pointer to a dynamically created function for general use, because the pointer to the second function
 *	only exists while the first function is executing. For example, this would compile, but would be a bad idea:
 *
 *		trampoline_callback_t function_pointer;
 *		invoke_trampoline(
 *			[&](trampoline_callback_t callback) { function_pointer = callback; },
 *			[](){ Serial.println("It happened"); }
 *		);
 *
 *		function_pointer();
 *
 *	In this example, the trampolined function would probably still exist in memory just above the stack pointer when it was
 *	called, via function_pointer, but as the stack grows from executing the Serial.println, the function would likely end up
 *	clobbering itself. You must make sure that the callback is not used beyond the scope of the first function--no exceptions.
 */
void invoke_trampoline(void(*invoke)(trampoline_callback_t, void*), void* invoke_arg, void(*callback)(void*), void* callback_arg);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <functional>
void invoke_trampoline(const std::function<void(trampoline_callback_t)>& invocation, const std::function<void()>& callback);
#endif

#endif /* _INVOKE_TRAMPOLINE_DEFINED */
#endif /* FATFS_PARTICLE_TRAMPOLINE_H_ */
