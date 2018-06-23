//---------------------------------------------------------------------------------------
// File: wave_operator.cpp
//
// WAVEファイルを操作するくらすの実装
// Author: ninf
//---------------------------------------------------------------------------------------
#define STRICT
#include "wave_operator.h"
#undef min
#undef max

//---------------------------------------------------------------------------------------
// Name: WaveOperator::WaveOperator()
// Desc: コンストラクター
// めんばーの初期化を行う
//---------------------------------------------------------------------------------------
WaveOperator::WaveOperator()
{
	format = NULL;
	mmio = NULL;
	size = 0;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::~WaveOperator()
// Desc: デストラクター
// 参照の解放などを行う
//---------------------------------------------------------------------------------------
WaveOperator::~WaveOperator()
{
	Close();
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Open()
// Desc: 読み込み専用でファイルを開く
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag)
{
	HRESULT hr;
	modeFlag = flag;

	// モードに応じてファイルを開く
	if (modeFlag == WAVEFILE_READ)
	{
		// ファイル名チェック
		if (strFileName == NULL)
			return E_INVALIDARG;
		SAFE_DELETE_ARRAY( format );

		mmioinfo = { 0 };
		mmio = mmioOpen(strFileName, &mmioinfo, MMIO_READ);
		if (!mmio)
			return E_FAIL;

		// ファイル内容の読み込み
		if (FAILED(hr = ReadMMIO()))
		{
			mmioClose(mmio, 0);
			return E_FAIL;
		}
		// ファイル位置のリセット
		if (FAILED(hr = ResetFile()))
			return E_FAIL;

		// ファイルサイズをセットする
		size = chunk.cksize;

	}
	else
	{
		return E_FAIL;
	}

	return hr;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::ReadMMIO()
// Desc: multimedia I/O streamから読み込むための関数
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::ReadMMIO()
{
	MMCKINFO ckIn; // 汎用的に使う、 チャンクの情報
	PCMWAVEFORMAT pcmWaveFormat; // 一時的につかうPCM構造体

	memset(&ckIn, 0, sizeof(ckIn)); // ckInに0をセット

	format = NULL;

	// チャンクを下りる
	if ((0 != mmioDescend(mmio, &riffChunk, NULL, 0)))
		return E_FAIL;

	// ファイル構造チェック、WAVEファイルの構造になっているか？
	if ((riffChunk.ckid != FOURCC_RIFF) ||
		(riffChunk.fccType != mmioFOURCC('W', 'A', 'V', 'E')))
		return E_FAIL;

	// fmtチャンクを探す
	ckIn.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (0 != mmioDescend(mmio, &ckIn, &riffChunk, MMIO_FINDCHUNK))
		return E_FAIL;

	// 'fmt'チャンクは少なくともPCMWAVEFORMATより大きい
	if (ckIn.cksize < (LONG)sizeof(PCMWAVEFORMAT))
		return E_FAIL;

	// 'fmt'チャンクをPCMWAVEFORMATに読み込む
	if (mmioRead(mmio, (HPSTR)&pcmWaveFormat, sizeof(pcmWaveFormat)) != sizeof(pcmWaveFormat))
		return E_FAIL;

	// waveformatexを割り当てる
	// pcmフォーマットでなければ次のwordを読み込み、割り当てるためにバイトを拡張する
	if (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM)
	{
		format = (WAVEFORMATEX*)new CHAR[sizeof(WAVEFORMATEX)];
		if (NULL == format)
			return E_FAIL;

		// pcm構造体からwaveformatexにバイトをコピーする
		memcpy(format, &pcmWaveFormat, sizeof(pcmWaveFormat));
		format->cbSize = 0;
	}
	else
	{
		// extra bytes の長さを読み込む
		WORD cbExtraBytes = 0L;
		if (mmioRead(mmio, (CHAR*)&cbExtraBytes, sizeof(WORD)) != sizeof(WORD))
			return E_FAIL;

		format = (WAVEFORMATEX*)new CHAR[sizeof(WAVEFORMATEX) + cbExtraBytes];
		if (NULL == format)
			return E_FAIL;

		// PCM構造体からwaveformatexにバイトをコピーする
		memcpy(format, &pcmWaveFormat, sizeof(pcmWaveFormat));
		format->cbSize = cbExtraBytes;

		// cbExtraAllocが0じゃなければ構造体に拡張したバイトを読み込む
		if (mmioRead(mmio, (CHAR*)(((BYTE*)&(format->cbSize)) + sizeof(WORD)), cbExtraBytes) != cbExtraBytes)
		{
			SAFE_DELETE(format);
			return E_FAIL;
		}
	}

	// 'fmt 'チャンクの外に戻す
	if (0 != mmioAscend(mmio, &ckIn, 0))
	{
		SAFE_DELETE(format);
		return E_FAIL;
	}

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::GetSize()
// Desc: waveファイルのサイズを返す
//---------------------------------------------------------------------------------------
DWORD WaveOperator::GetSize()
{
	return size;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::ResetFile()
// Desc: ファイルの読み取りポインターをリセットする
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::ResetFile()
{
	if (mmio == NULL)
		return E_FAIL;

	if (modeFlag == WAVEFILE_READ)
	{
		// データをシークする
		if (-1 == mmioSeek(mmio, riffChunk.dwDataOffset + sizeof(FOURCC), SEEK_SET))
			return E_FAIL;

		// 'data'チャンクを探す
		chunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
		if (0 != mmioDescend(mmio, &chunk, &riffChunk, MMIO_FINDCHUNK))
			return E_FAIL;
	}
	else
	{
		return E_FAIL;
	}

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Read()
// Desc: ファイル内容をすべてバッファーに読み込み
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::Read(BYTE* buffer, DWORD size, DWORD* readSize)
{
	return E_FAIL;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::ReadChunk()
// Desc: ファイル内容を指定された長さで読み込んでバッファーに入れる
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::ReadChunk(BYTE** buffer, int bufferCount, const int len, int* readSize)
{
	if (readSize != NULL)
		*readSize = 0;

	// ファイルから指定されたサイズだけデータを読み取る
	*readSize = mmioRead(mmio, (HPSTR)buffer[bufferCount], len);
	if (*readSize == -1)
		return E_FAIL;

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Close()
// Desc: リソースを解放する
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::Close()
{
	if (modeFlag == WAVEFILE_READ)
	{
		if (mmio != NULL)
		{
			mmioClose(mmio, 0);
			mmio = NULL;
		}
	}

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::WriteMMIO()
// Desc: multimedia I/O streamのサポート関数
//       新しいwave fileを作成する
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::WriteMMIO(WAVEFORMATEX* format)
{
	return E_FAIL;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Write()
// Desc: 開いたwaveファイルにデータを書き込む
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::Write(UINT writeSize, BYTE* data, UINT wroteSize)
{
	return E_FAIL;
}
