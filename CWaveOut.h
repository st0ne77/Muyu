#pragma once
#ifndef CWAVEOUT_H
#define CWAVEOUT_H

#include <Windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <vector>

// For Code Compatibility
#define DEBG_I_Print(x) printf x

class CFreeBlockManager
{
	int32_t nMaxBlockCount;
	volatile int32_t nFreeBlockCount;
    CRITICAL_SECTION stCriticalSection;

public:
	void Initialize(int32_t nValue)
	{
		nMaxBlockCount = nValue;
        nFreeBlockCount = nMaxBlockCount;
		InitializeCriticalSection(&stCriticalSection);
	}

	void Finalize(void)
    {
		DeleteCriticalSection(&stCriticalSection);
	}

	void Return(void)
	{
		EnterCriticalSection(&stCriticalSection);
		nFreeBlockCount++;
        //fprintf(stderr, "----- free = %d\n", nFreeBlockCount);
		LeaveCriticalSection(&stCriticalSection);
	}

	void Use(void)
	{
		EnterCriticalSection(&stCriticalSection);
		nFreeBlockCount--;
		LeaveCriticalSection(&stCriticalSection);

		while (nFreeBlockCount == 0)
		{
            //fprintf(stderr, "------#------- full @@@@\n");
			Sleep(10);
		}
	}

	void WaitForAllFree(void)
	{
		while (nFreeBlockCount < nMaxBlockCount)
		{
			Sleep(10);
		}
	}
};

class CWaveOut
{
	HWAVEOUT m_hWaveOut; ///< WaveOut Handle
	WAVEFORMATEX m_stWaveFormat; ///< WaveOut Format

	std::vector<WAVEHDR*> m_oWaveBlocks; ///< WaveHeader List, Used Sequentially & Cyclically
	int32_t m_nCurrentBlockIndex; ///< Current Playback WaveHeader Index

	CFreeBlockManager m_oFreeBlockManager; ///< Managing Number of Free Blocks & Critical Section
    int  m_nBlockSize;
    bool m_isOpened;

public:
	CWaveOut(uint32_t unBlockSize = 1024, uint32_t unNumOfBlocks = 2);
	~CWaveOut();

	uint32_t Open(int32_t nSampleRate, int32_t nBitsPerSample, int32_t nChannels);
	uint32_t Close(void);

	void Write(char* pszBuffer, int32_t nSize);
	void Wait(void);

    int  maxBlockSize() { return m_nBlockSize; }

private:
	void Initialize(uint32_t unBlockSize, uint32_t unNumOfBlocks);
	void Finalize(void);

	void InitializeBlocks(int32_t nBlockSize, int32_t nNumOfBlocks);
	void FinalizeBlocks(void);

	void ExchangeWaveBlock(void);
};

#endif //CWAVEOUT_H
