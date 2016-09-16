//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.
//

#define SH_EXPORTING

#include <assert.h>

#include "InitializeDll.h"
#include "../glslang/Include/InitializeGlobals.h"

#include "../glslang/Public/ShaderLang.h"

namespace glslang {

OS_TLSIndex ThreadInitializeIndex = OS_INVALID_TLS_INDEX;

static void DetachThreadLinux(void *)
{
	DetachThread();
}

//
// Registers cleanup handler, sets cancel type and state, and excecutes the thread specific
// cleanup handler.  This function will be called in the Standalone.cpp for regression
// testing.  When OpenGL applications are run with the driver code, Linux OS does the
// thread cleanup.
//
void OS_CleanupThreadData(void)
{
#ifdef __ANDROID__
  DetachThread();
#else
	int old_cancel_state, old_cancel_type;
	void *cleanupArg = NULL;

	//
	// Set thread cancel state and push cleanup handler.
	//
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_cancel_state);
	pthread_cleanup_push(DetachThreadLinux, (void *) cleanupArg);

	//
	// Put the thread in deferred cancellation mode.
	//
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old_cancel_type);

	//
	// Pop cleanup handler and execute it prior to unregistering the cleanup handler.
	//
	pthread_cleanup_pop(1);

	//
	// Restore the thread's previous cancellation mode.
	//
	pthread_setcanceltype(old_cancel_state, NULL);
#endif
}


//
// Thread Local Storage Operations
//
inline OS_TLSIndex PthreadKeyToTLSIndex(pthread_key_t key)
{
	return (OS_TLSIndex)((uintptr_t)key + 1);
}

inline pthread_key_t TLSIndexToPthreadKey(OS_TLSIndex nIndex)
{
	return (pthread_key_t)((uintptr_t)nIndex - 1);
}

OS_TLSIndex OS_AllocTLSIndex()
{
	pthread_key_t pPoolIndex;

	//
	// Create global pool key.
	//
	if ((pthread_key_create(&pPoolIndex, NULL)) != 0) {
		assert(0 && "OS_AllocTLSIndex(): Unable to allocate Thread Local Storage");
		return OS_INVALID_TLS_INDEX;
	}
	else
		return PthreadKeyToTLSIndex(pPoolIndex);
}


bool OS_SetTLSValue(OS_TLSIndex nIndex, void *lpvValue)
{
	if (nIndex == OS_INVALID_TLS_INDEX) {
		assert(0 && "OS_SetTLSValue(): Invalid TLS Index");
		return false;
	}

	if (pthread_setspecific(TLSIndexToPthreadKey(nIndex), lpvValue) == 0)
		return true;
	else
		return false;
}

void* OS_GetTLSValue(OS_TLSIndex nIndex)
{
	//
	// This function should return 0 if nIndex is invalid.
	//
	assert(nIndex != OS_INVALID_TLS_INDEX);
	return pthread_getspecific(TLSIndexToPthreadKey(nIndex));
}

bool OS_FreeTLSIndex(OS_TLSIndex nIndex)
{
	if (nIndex == OS_INVALID_TLS_INDEX) {
		assert(0 && "OS_SetTLSValue(): Invalid TLS Index");
		return false;
	}

	//
	// Delete the global pool key.
	//
	if (pthread_key_delete(TLSIndexToPthreadKey(nIndex)) == 0)
		return true;
	else
		return false;
}

// TODO: non-windows: if we need these on linux, flesh them out
void InitGlobalLock()
{
}

void GetGlobalLock()
{
}

void ReleaseGlobalLock()
{
}

void* OS_CreateThread(TThreadEntrypoint entry)
{
    return 0;
}

void OS_WaitForAllThreads(void* threads, int numThreads)
{
}

void OS_Sleep(int milliseconds)
{
}

void OS_DumpMemoryCounters()
{
}

bool InitProcess()
{
    glslang::GetGlobalLock();

    if (ThreadInitializeIndex != OS_INVALID_TLS_INDEX) {
        //
        // Function is re-entrant.
        //

        glslang::ReleaseGlobalLock();
        return true;
    }

    ThreadInitializeIndex = OS_AllocTLSIndex();

    if (ThreadInitializeIndex == OS_INVALID_TLS_INDEX) {
        assert(0 && "InitProcess(): Failed to allocate TLS area for init flag");

        glslang::ReleaseGlobalLock();
        return false;
    }

    if (! InitializePoolIndex()) {
        assert(0 && "InitProcess(): Failed to initialize global pool");

        glslang::ReleaseGlobalLock();
        return false;
    }

    if (! InitThread()) {
        assert(0 && "InitProcess(): Failed to initialize thread");

        glslang::ReleaseGlobalLock();
        return false;
    }

    glslang::ReleaseGlobalLock();
    return true;
}


bool InitThread()
{
    //
    // This function is re-entrant
    //
    if (ThreadInitializeIndex == OS_INVALID_TLS_INDEX) {
        assert(0 && "InitThread(): Process hasn't been initalised.");
        return false;
    }

    if (OS_GetTLSValue(ThreadInitializeIndex) != 0)
        return true;

    InitializeMemoryPools();

    if (! OS_SetTLSValue(ThreadInitializeIndex, (void *)1)) {
        assert(0 && "InitThread(): Unable to set init flag.");
        return false;
    }

    return true;
}


bool DetachThread()
{
    bool success = true;

    if (ThreadInitializeIndex == OS_INVALID_TLS_INDEX)
        return true;

    //
    // Function is re-entrant and this thread may not have been initialized.
    //
    if (OS_GetTLSValue(ThreadInitializeIndex) != 0) {
        if (!OS_SetTLSValue(ThreadInitializeIndex, (void *)0)) {
            assert(0 && "DetachThread(): Unable to clear init flag.");
            success = false;
        }

        FreeGlobalPools();

    }

    return success;
}

bool DetachProcess()
{
    bool success = true;

    if (ThreadInitializeIndex == OS_INVALID_TLS_INDEX)
        return true;

    ShFinalize();

    success = DetachThread();

    FreePoolIndex();

    OS_FreeTLSIndex(ThreadInitializeIndex);
    ThreadInitializeIndex = OS_INVALID_TLS_INDEX;

    return success;
}

} // end namespace glslang