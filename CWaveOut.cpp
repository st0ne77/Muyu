#include "CWaveOut.h"

#pragma comment(lib, "winmm.lib")


static void CALLBACK WaveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
    (void)hWaveOut; (void)dwParam1; (void)dwParam2;
	switch (uMsg)
	{
	case WOM_OPEN:
	case WOM_CLOSE:
		break;

	case WOM_DONE:
		CFreeBlockManager* poFreeBlockManager = reinterpret_cast<CFreeBlockManager*>(dwInstance);
		poFreeBlockManager->Return();
		break;
	}
}

CWaveOut::CWaveOut(uint32_t unBlockSize, uint32_t unNumOfBlocks)
    : m_hWaveOut(0), m_stWaveFormat(), m_oWaveBlocks()
    , m_nCurrentBlockIndex(-1), m_oFreeBlockManager()
    , m_nBlockSize(unBlockSize)
    , m_isOpened(false)
{
	Initialize(unBlockSize, unNumOfBlocks);
}

CWaveOut::~CWaveOut()
{
    Close();
	Finalize();
}

uint32_t CWaveOut::Open(int32_t nSampleRate, int32_t nBitsPerSample, int32_t nChannels)
{
    // circle queue: (0 -> 1 -> 2 -> ... -> N -> 0 -> ...)
	m_nCurrentBlockIndex = 0;

	m_stWaveFormat.nSamplesPerSec = nSampleRate;
	m_stWaveFormat.wBitsPerSample = nBitsPerSample;
	m_stWaveFormat.nChannels = nChannels;
	m_stWaveFormat.cbSize = 0;
	m_stWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	m_stWaveFormat.nBlockAlign = (m_stWaveFormat.wBitsPerSample * m_stWaveFormat.nChannels) >> 3;
	m_stWaveFormat.nAvgBytesPerSec = m_stWaveFormat.nBlockAlign * m_stWaveFormat.nSamplesPerSec;


    // --优先采用 uDeviceID 0 号设备打开，若失败，则用 -1  第二个参数 --
    MMRESULT rt = waveOutOpen(&m_hWaveOut, 0, &m_stWaveFormat, (DWORD_PTR)WaveOutProc,
                              (DWORD_PTR)&m_oFreeBlockManager, CALLBACK_FUNCTION);

    if(rt != MMSYSERR_NOERROR) {
        rt = waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &m_stWaveFormat, (DWORD_PTR)WaveOutProc,
                         (DWORD_PTR)&m_oFreeBlockManager, CALLBACK_FUNCTION);
        if(rt != MMSYSERR_NOERROR) {
            DEBG_I_Print(("CWaveOut::Open - Failed !\n"));
            fprintf(stderr, "CWaveOut::Open - Failed  %d\n", rt);  fflush(stderr);
            return -1;
        }
    }
    m_isOpened = true;
	return 0;
}

uint32_t CWaveOut::Close(void)
{
    if(!m_isOpened)  return 0;
    m_isOpened = false;

    this->Wait();   // !!! 太重要了,否则会挂...

    for (uint32_t i = 0; i < m_oWaveBlocks.size(); i++)
	{
		if (m_oWaveBlocks[i]->dwFlags & WHDR_PREPARED)
		{
			waveOutUnprepareHeader(m_hWaveOut, m_oWaveBlocks[i], sizeof(WAVEHDR));
		}
	}

    return waveOutClose(m_hWaveOut);
}

void CWaveOut::Write(char* pszBuffer, int32_t nSize)
{
    if(nSize <= 0)  return;
    assert(nSize>=0 && nSize <= m_nBlockSize); // 保证内存块大小足够

    WAVEHDR* pstCurrentBlock = m_oWaveBlocks[m_nCurrentBlockIndex];

    // prepared
	if (pstCurrentBlock->dwFlags & WHDR_PREPARED)
	{
		waveOutUnprepareHeader(m_hWaveOut, pstCurrentBlock, sizeof(WAVEHDR));
	} // not exist else statement

	memcpy(pstCurrentBlock->lpData, pszBuffer, nSize);
	pstCurrentBlock->dwBufferLength = nSize;
	waveOutPrepareHeader(m_hWaveOut, pstCurrentBlock, sizeof(WAVEHDR));
	waveOutWrite(m_hWaveOut, pstCurrentBlock, sizeof(WAVEHDR));

	m_oFreeBlockManager.Use();

    ExchangeWaveBlock();
}

void CWaveOut::Wait(void)
{
	m_oFreeBlockManager.WaitForAllFree();
}

void CWaveOut::Initialize(uint32_t unBlockSize, uint32_t unNumOfBlocks)
{
	InitializeBlocks(unBlockSize, unNumOfBlocks);
	m_oFreeBlockManager.Initialize(unNumOfBlocks);
}

void CWaveOut::Finalize(void)
{
	FinalizeBlocks();
	m_oFreeBlockManager.Finalize();
}

void CWaveOut::InitializeBlocks(int32_t nBlockSize, int32_t nNumOfBlocks)
{
	m_oWaveBlocks.resize(nNumOfBlocks);
    m_nBlockSize = nBlockSize;

	for (int32_t i = 0; i < nNumOfBlocks; i++)
	{
		m_oWaveBlocks[i] = new WAVEHDR;
		memset(m_oWaveBlocks[i], 0, sizeof(WAVEHDR));

		m_oWaveBlocks[i]->lpData = new char[nBlockSize];
		memset(m_oWaveBlocks[i]->lpData, 0, sizeof(char) * nBlockSize);
	}
}

void CWaveOut::FinalizeBlocks(void)
{
	for (uint32_t i = 0; i < m_oWaveBlocks.size(); i++)
	{
		delete[] m_oWaveBlocks[i]->lpData;
        m_oWaveBlocks[i]->lpData = 0;

		delete m_oWaveBlocks[i];
        m_oWaveBlocks[i] = 0;
	}

	m_oWaveBlocks.clear();
}

void CWaveOut::ExchangeWaveBlock(void)
{
	m_nCurrentBlockIndex++;
	m_nCurrentBlockIndex %= m_oWaveBlocks.size();
}
