//---------------------------------------------------------------------------------------
// File: wave_operator.h
//
// Waveファイルを操作するためのクラス
// Author: ninf 
//---------------------------------------------------------------------------------------
#ifndef _H_WAVE_OPERATOR
#define _H_WAVE_OPERATOR

#include <xaudio2.h>
#include "constants.h"

//-----------------------------------------------------------------------------
// Typing macros 
//-----------------------------------------------------------------------------
#define WAVEFILE_READ   1
#define WAVEFILE_WRITE  2


class WaveOperator
{
public:
	WAVEFORMATEX* format;
	HMMIO mmio;
	MMCKINFO chunk;
	MMCKINFO riffChunk;
	MMIOINFO mmioinfo;
	MMRESULT ret;
	DWORD size;
	DWORD modeFlag;

protected:
	HRESULT ReadMMIO();
	HRESULT WriteMMIO(WAVEFORMATEX* format);

public:
	// コンストラクター
	WaveOperator();
	// デストラクター
	~WaveOperator();
	// ファイルを開く
	HRESULT Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag);
    // ファイルを閉じる
	HRESULT Close();
	// ファイルを読み込む
	HRESULT Read(BYTE* buffer, DWORD size, DWORD* readSize);
	// ファイルに書き込む
	HRESULT Write(UINT writeSize, BYTE* data, UINT wroteSize);
	// チャンクデータを読み込み(細切れにでーたを読みこみ)
	HRESULT ReadChunk(BYTE** buffer, int bufferCount, const int len, int* readSize);
	// サイズを取得
	DWORD GetSize();
	// ファイルの位置をリセット
	HRESULT ResetFile();
	WAVEFORMATEX* GetFormat() { return format; }
};

#endif
