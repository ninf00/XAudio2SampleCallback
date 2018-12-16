//---------------------------------------------------------------------------------------
// File: ogg_operator.cpp
//
// OGGファイルを操作するくらすの実装
// Author: ninf
//---------------------------------------------------------------------------------------
#define STRICT
#include "ogg_operator.h"
#include <memory.h>
#include <crtdbg.h>
#undef min
#undef max

//---------------------------------------------------------------------------------------
// Name: OggOperator::OggOperator()
// Desc: コンストラクター
// めんばーの初期化を行う
//---------------------------------------------------------------------------------------
OggOperator::OggOperator()
{
	format = NULL;
	mmio = NULL;
	size = 0;
	isReadingFromMemory = FALSE;
	isReady = false;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::~OggOperator()
// Desc: デストラクター
// 参照の解放などを行う
//---------------------------------------------------------------------------------------
OggOperator::~OggOperator()
{
	Close();
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::Open()
// Desc: 読み込み専用でファイルを開く
//---------------------------------------------------------------------------------------
HRESULT OggOperator::Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag, bool isBuffered)
{
	HRESULT hr;
	modeFlag = flag;
	errno_t error;

	// モードに応じてファイルを開く
	if (modeFlag == OGGFILE_READ)
	{
		// ファイル名チェック
		if (strFileName == NULL)
			return E_INVALIDARG;
		SAFE_DELETE_ARRAY(format);

		// oggファイルを開く
		error = _wfopen_s(&fp, strFileName, L"rb");
		if (error != 0)
		{
			return E_FAIL;
		}
		// oggファイルを開く
		error = ov_open(fp, &ovf, NULL, 0);
		if (error != 0)
		{
			return E_FAIL;
		}
		// oggファイル情報を取得してセット
		vorbis_info *info = ov_info(&ovf, -1);
		SetFileName(strFileName);
		SetChannelNumber(info->channels);
		// bitrateは16にしておく
		SetBitRate(16);
		SetSamplingRate(info->rate);
		// セットした情報使ってフォーマットファイル作成
		if (!GenerateWaveFormatEx())
			return E_FAIL;
		// サイズは一旦1M固定で暫定対応
		size = OggOperatorNS::pcmSize;
	}
	else
	{
		return E_FAIL;
	}

	// ファイルの準備が出来た
	isReady = true;
	hr = S_OK;
	return hr;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::OpenFromMemory()
// Desc: メモリーからメンバー変数を読み込む
//---------------------------------------------------------------------------------------
HRESULT OggOperator::OpenFromMemory(BYTE* data, ULONG dataSize, WAVEFORMATEX* format, DWORD flag)
{
	// データをコピーする
	format = format;
	memoryData = data;
	memoryDataSize = dataSize;
	memoryDataCur = memoryData;
	isReadingFromMemory = TRUE;

	if (flag != OGGFILE_READ)
		return E_NOTIMPL;

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::ReadMMIO()
// Desc: multimedia I/O streamから読み込むための関数
//---------------------------------------------------------------------------------------
HRESULT OggOperator::ReadMMIO()
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
// Name: OggOperator::GetSize()
// Desc: waveファイルのサイズを返す
//---------------------------------------------------------------------------------------
DWORD OggOperator::GetSize()
{
	return size;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::ResetFile()
// Desc: ファイルの読み取りポインターをリセットする
//---------------------------------------------------------------------------------------
HRESULT OggOperator::ResetFile()
{
	if (isReadingFromMemory)
	{
		memoryDataCur = memoryData;
	}
	else
	{
		if (!GetReady())
			return E_FAIL;

		if (modeFlag == OGGFILE_READ)
		{
			ov_time_seek(&ovf, 0.0);
		}
		else
		{
			return E_FAIL;
		}
	}

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::Read()
// Desc: ファイル内容をすべてバッファーに読み込み
//---------------------------------------------------------------------------------------
HRESULT OggOperator::Read(BYTE* buffer, DWORD size, DWORD* readSize)
{
	if (isReadingFromMemory)
	{
		// メモリ用カーソルチェック
		if (memoryDataCur == NULL)
			return CO_E_NOTINITIALIZED;
		// 読み取ったサイズのポインタ初期化
		if (readSize != NULL)
			*readSize = 0;
		// 読み取るサイズをチェック
		if ((BYTE*)(memoryDataCur + size) > (BYTE*)(memoryData + memoryDataSize))
		{
			size = memoryDataSize - (DWORD)(memoryDataCur - memoryData);
		}

		// メモリから読み取ったファイルデータをバッファーにコピー
#pragma warning( disable: 4616 )
#pragma warning( diable: 22104 )
		CopyMemory(buffer, memoryDataCur, size);
#pragma warning( default: 22104 )
#pragma warning( default: 4616 )

		// 読み取ったサイズを更新
		if (readSize != NULL)
			*readSize = size;

		return S_OK;
	}
	else
	{
		// 読み込むバッファーをメモリに割り当て
		memset(buffer, 0, this->size);
		char *tmpBuffer = new char[this->size];
		int bitStream = 0;
		int tmpReadSize = 0;
		int comSize = 0;

		*readSize = 0;
		while (1)
		{
			tmpReadSize = ov_read(&ovf, (char*)tmpBuffer, 4096, 0, 2, 1, &bitStream);
			// 読み込みサイズ超えてたり、ファイル末尾まで読み込みしてたらbreak
			if (comSize + tmpReadSize >= this->size || tmpReadSize == 0 || tmpReadSize == EOF)
				break;
			// メモリーのバッファーにポインター位置ずらしながら書き込み
			memcpy(buffer + comSize, tmpBuffer, tmpReadSize);
			comSize += tmpReadSize;
		}


		// 読み込んだサイズをセット
		*readSize = comSize;
		// 後始末
		delete[] tmpBuffer;

		return S_OK;
	}
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::GetSegment()
// Desc: 指定のサイズまで埋めたセグメントデータを取得する
//---------------------------------------------------------------------------------------
bool OggOperator::GetSegment(char* buffer, unsigned int size, int* writeSize, bool* isEnd)
{
	// ファイルの準備が終わってなければ終了
	if (GetReady() == false)
		return false;

	// バッファーの指定チェック
	if (buffer == 0)
	{
		if (isEnd) *isEnd = true;
		if (writeSize) *writeSize = 0;
		return false;
	}

	if (isEnd) *isEnd = false;

	// メモリ上にバッファー領域を確保
	memset(buffer, 0, size);
	unsigned int requestSize = OggOperatorNS::requestSize;
	int bitStream = 0;
	int readSize = 0;
	unsigned int comSize = 0;
	bool isAdjust = false; // 端数のデータの調整中フラグ

	// 指定サイズが予定サイズより小さい場合は調整中とみなす
	if (size < requestSize)
	{
		requestSize = size;
		isAdjust = true;
	}

	// バッファーを指定サイズで埋めるまで繰り返す
	while (1)
	{
		readSize = ov_read(&ovf, (char*)(buffer + comSize), requestSize, 0, 2, 1, &bitStream);
		// ファイルエンドに達した
if (readSize == 0)
{
	// 終了
	if (isEnd) *isEnd = true;
	if (writeSize) *writeSize = comSize;
	return true;
}

// 読み取りサイズを加算
comSize += readSize;

_ASSERT(comSize <= size);	// バッファオーバー

// バッファを埋め尽くした
if (comSize >= size)
{
	if (writeSize) *writeSize = comSize;
	return true;
}

// 端数データの調整
if (size - comSize < OggOperatorNS::requestSize)
{
	isAdjust = true;
	requestSize = size - comSize;
}

	}

	// エラー
	if (writeSize) *writeSize = 0;
	return false;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::ReadChunk()
// Desc: ファイル内容を指定された長さで読み込んでバッファーに入れる
//---------------------------------------------------------------------------------------
HRESULT OggOperator::ReadChunk(BYTE** buffer, int bufferCount, const int len, int* readSize)
{
	if (readSize != NULL)
		*readSize = 0;

	// ファイルから指定されたサイズだけデータを読み取る
	bool result = GetSegment((char *)buffer[bufferCount], len, readSize, 0);
	if (result == false)
		return E_FAIL;

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::Close()
// Desc: リソースを解放する
//---------------------------------------------------------------------------------------
HRESULT OggOperator::Close()
{
	if (modeFlag == OGGFILE_READ)
	{
		ov_clear(&ovf);
		if (fp)
		{
			fclose(fp);
			fp = NULL;
		}
	}

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::WriteMMIO()
// Desc: multimedia I/O streamのサポート関数
//       新しいwave fileを作成する
//---------------------------------------------------------------------------------------
HRESULT OggOperator::WriteMMIO(WAVEFORMATEX* format)
{
	return E_FAIL;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::Write()
// Desc: 開いたwaveファイルにデータを書き込む
//---------------------------------------------------------------------------------------
HRESULT OggOperator::Write(UINT writeSize, BYTE* data, UINT wroteSize)
{
	return E_FAIL;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::SetFileName()
// Desc: 取り扱うoggファイル名をセットする
//---------------------------------------------------------------------------------------
void OggOperator::SetFileName(LPWSTR value)
{
	wcscpy_s(fileName, value);
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::GenerateWaveFormatEx()
// Desc: 開いたoggファイルの情報からwaveヘッダー生成
//---------------------------------------------------------------------------------------
bool OggOperator::GenerateWaveFormatEx()
{
	format = NULL;
	format = (WAVEFORMATEX*)new CHAR[sizeof(WAVEFORMATEX)];
	if (NULL == format)
		return false;

	format->wFormatTag = WAVE_FORMAT_PCM;
	format->nChannels = channelNumber;
	format->nSamplesPerSec = samplingRate;
	format->wBitsPerSample = bitRate;
	format->nBlockAlign = channelNumber * bitRate / 8;
	format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
	format->cbSize = 0;

	return true;
}
