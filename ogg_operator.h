//---------------------------------------------------------------------------------------
// File: ogg_operator.h
//
// OGGファイルを操作するためのクラス
// Author: ninf 
//---------------------------------------------------------------------------------------
#ifndef _H_OGG_OPERATOR
#define _H_OGG_OPERATOR

#include <xaudio2.h>
#include "constants.h"
#include <vorbis\vorbisfile.h>

//-----------------------------------------------------------------------------
// Typing macros 
//-----------------------------------------------------------------------------
#define OGGFILE_READ   1
#define OGGFILE_WRITE  2

namespace OggOperatorNS
{
	const unsigned int requestSize = 4096;	// 読み込み単位
	// 一度にメモリに読み込むファイルサイズ 1M
	const int pcmSize = 1024 * 1000;
}


class OggOperator
{
private:
	OggVorbis_File ovf;
	FILE *fp;
	bool isReady;
	wchar_t fileName[256];
	unsigned int channelNumber;
	unsigned int samplingRate;
	unsigned int bitRate;


public:
	WAVEFORMATEX * format;
	HMMIO mmio;
	MMCKINFO chunk;
	MMCKINFO riffChunk;
	MMIOINFO mmioinfo;
	MMRESULT ret;
	DWORD size;
	DWORD modeFlag;
	BOOL isReadingFromMemory;
	ULONG memoryDataSize;
	BYTE* memoryData;
	BYTE* memoryDataCur;

private:
	// セグメントデータ取得
	virtual bool GetSegment(char* buffer, unsigned int size, int* writeSize, bool* isEnd);
	bool GenerateWaveFormatEx();

protected:
	HRESULT ReadMMIO();
	HRESULT WriteMMIO(WAVEFORMATEX* format);

public:
	// コンストラクター
	OggOperator();
	// デストラクター
	~OggOperator();

	// アクセッサメソッド
	void SetReady(bool value) { isReady = value; }
	bool GetReady() { return isReady; }

	void SetFileName(LPWSTR value);

	void SetChannelNumber(int value) { channelNumber = value; }
	void SetSamplingRate(int value) { samplingRate = value; }
	void SetBitRate(int value) { bitRate = value; }

	// ファイルを開く
	HRESULT Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag, bool isBuffered = false);
	// メモリからファイルを開く
	HRESULT OpenFromMemory(BYTE* data, ULONG dataSize, WAVEFORMATEX* format, DWORD flag);
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
