//-------------------------------------------------------------------------------------------------
// File: XAudio2SampleCallback.cpp
//
// XAudio2をCallbackありで使うサンプル
// Author: ninf
//
#include <windows.h>
#include <xaudio2.h>
#include <strsafe.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <conio.h>
#include "voice_callback.h"
#include "constants.h"
#include "wave_operator.h"

//-------------------------------------------------------------------------------------------------
// define
//-------------------------------------------------------------------------------------------------
#define PLAY_MODE 1

//-------------------------------------------------------------------------------------------------
// function prototype
//-------------------------------------------------------------------------------------------------
int playMediaDirect();
int playMediaWaveOperator();

//-------------------------------------------------------------------------------------------------
// main function
//-------------------------------------------------------------------------------------------------
int main()
{
	if (PLAY_MODE == 0)
		return playMediaDirect();
	else
		return playMediaWaveOperator();
}

//---------------------------------------------------------------------------------------
// Name: playMediaDirect()
// Desc: 直接MMIO使ってWaveファイル開いて再生する
//---------------------------------------------------------------------------------------
int playMediaDirect()
{
	HRESULT ret;

	// COMコンポーネントの初期化 - XAudio2で使うため
	ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		printf("error CoInitializeEx ret=%d\n", ret);
		return -1;
	}

	// XAudio2エンジン生成
	IXAudio2 *audio = NULL;
	ret = XAudio2Create(
		&audio
	);
	if (FAILED(ret)) {
		printf("error XAudio2Create ret=%d\n", ret);
		return -1;
	}

	// マスターボイス生成
	IXAudio2MasteringVoice *master = NULL;
	ret = audio->CreateMasteringVoice(
		&master
	);
	if (FAILED(ret)) {
		printf("error CreateMasteringVoice ret=%d\n", ret);
		return -1;
	}

	//
	// 再生する音楽ファイルの読み込み
	//
	// ファイルを開く
	wchar_t file[] = { L"test.wav" };
	HMMIO mmio = NULL;
	MMIOINFO info = { 0 };
	mmio = mmioOpen(file, &info, MMIO_READ);
	if (!mmio) {
		printf("error mmioOpen\n");
		return -1;
	}

	// チャンクを下りる
	// まずはWAVEチャンクに進む
	MMRESULT mret;
	MMCKINFO riff_chunk;
	riff_chunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	mret = mmioDescend(mmio, &riff_chunk, NULL, MMIO_FINDRIFF);
	if (mret != MMSYSERR_BASE) {
		printf("error mmioDescend(wave) ret=%d\n", mret);
		return -1;
	}
	// 次にfmtチャンクに進む
	MMCKINFO chunk;
	chunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mret = mmioDescend(mmio, &chunk, &riff_chunk, MMIO_FINDCHUNK);
	if (mret != MMSYSERR_BASE)
	{
		printf("error mmioDescend(fmt) ret=%d\n", mret);
		return -1;
	}

	// WAVEFORMATEXを生成
	WAVEFORMATEX format = { 0 };
	{
		// mmioOpen関数で開いたファイルから、指定されたバイト数を読み取る
		DWORD size = mmioRead(mmio, (HPSTR)&format, chunk.cksize);
		if (size != chunk.cksize) {
			printf("error mmioRead(fmt) ret=%d\n", mret);
			return -1;
		}

		printf("format =%d\n", format.wFormatTag);
		printf("channel =%d\n", format.nChannels);
		printf("sampling =%dHz\n", format.nSamplesPerSec);
		printf("bit/sample =%d\n", format.wBitsPerSample);
		printf("byte/sec =%d\n", format.nAvgBytesPerSec);
	}

	// ソースボイスを生成
	IXAudio2SourceVoice *voice = NULL;
	VoiceCallback callback;
	ret = audio->CreateSourceVoice(
		&voice,
		&format,
		0,
		XAUDIO2_DEFAULT_FREQ_RATIO,
		&callback
	);
	if (FAILED(ret))
	{
		printf("error CreateSourceVoice ret=%d\n", ret);
		return -1;
	}

	// dataチャンクに進める
	chunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	mret = mmioDescend(mmio, &chunk, &riff_chunk, MMIO_FINDCHUNK);
	if (mret != MMSYSERR_BASE)
	{
		printf("error mmioDescend(data) ret=%d\n", mret);
		return -1;
	}

	// ボイスによるオーディオの使用および処理を開始
	voice->Start();

	// オーディオバッファーを用意
	XAUDIO2_BUFFER buffer = { 0 };
	const int buf_len = 2;
	BYTE** buf = new BYTE*[buf_len];
	const int len = format.nAvgBytesPerSec;
	for (int i = 0; i < buf_len; i++) {
		buf[i] = new BYTE[len];
	}
	int buf_cnt = 0;
	int size;
	size = mmioRead(mmio, (HPSTR)buf[buf_cnt], len);
	buffer.AudioBytes = size;
	buffer.pAudioData = buf[buf_cnt];
	if (0 < size)
	{
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", mret);
			return -1;
		}
	}
	if (buf_len <= ++buf_cnt) buf_cnt = 0;

	// 再生ループ
	do {
		// ファイルからデータを読み取り
		size = mmioRead(mmio, (HPSTR)buf[buf_cnt], len);
		// なければループ抜ける
		if (size <= 0) {
			break;
		}
		// バッファー生成
		buffer.AudioBytes = size;
		buffer.pAudioData = buf[buf_cnt];
		// バッファーをキューに追加
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", mret);
			return -1;
		}
		// 2つ確保しているバッファーを交互に使うようにカウンタチェックとリセット
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// 再生すべきデータをすべて読み取ったのでボイスによるオーディオの使用を停止
	// Stop呼ばない限り再生状態を続ける
	voice->Stop();

	// 終了メッセージ
	printf("play end!");

	// ユーザーの入力を待つ
	getchar();

	// 後処理
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// ファイルを閉じる
	mmioClose(mmio, MMIO_FHOPEN);
	// ソースボイスの解放
	voice->DestroyVoice(); voice = NULL;
	// マスターボイスの解放
	master->DestroyVoice(); master = NULL;
	// オーディオエンジンの解放
	audio->Release(); audio = NULL;
	// COMコンポーネントの解放
	CoUninitialize();

	return 0;
}

//---------------------------------------------------------------------------------------
// Name: playeMediaWaveOperator()
// Desc: WaveOperatorクラスを利用してWaveファイルを開いて再生する
//---------------------------------------------------------------------------------------
int playMediaWaveOperator()
{
	HRESULT ret;

	// COMコンポーネントの初期化 - XAudio2で使うため
	ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		printf("error CoInitializeEx ret=%d\n", ret);
		return -1;
	}

	// XAudio2エンジン生成
	IXAudio2 *audio = NULL;
	ret = XAudio2Create(
		&audio
	);
	if (FAILED(ret)) {
		printf("error XAudio2Create ret=%d\n", ret);
		return -1;
	}

	// マスターボイス生成
	IXAudio2MasteringVoice *master = NULL;
	ret = audio->CreateMasteringVoice(
		&master
	);
	if (FAILED(ret)) {
		printf("error CreateMasteringVoice ret=%d\n", ret);
		return -1;
	}

	//
	// 再生する音楽ファイルの読み込み
	//
	// ファイルを開く
	wchar_t file[] = { L"test.wav" };
	WaveOperator audio_file;
	if (FAILED(ret = audio_file.Open(file, NULL, WAVEFILE_READ)))
	{
		printf("error wave file open ret=%d\n", ret);
		return -1;
	}

	// 開いたファイル情報表示
	WAVEFORMATEX* pFormat = audio_file.GetFormat();
	printf("format =%d\n", pFormat->wFormatTag);
	printf("channel =%d\n", pFormat->nChannels);
	printf("sampling =%dHz\n", pFormat->nSamplesPerSec);
	printf("bit/sample =%d\n", pFormat->wBitsPerSample);
	printf("byte/sec =%d\n", pFormat->nAvgBytesPerSec);


	// ソースボイスを生成
	IXAudio2SourceVoice *voice = NULL;
	VoiceCallback callback;
	ret = audio->CreateSourceVoice(
		&voice,
		pFormat,
		0,
		XAUDIO2_DEFAULT_FREQ_RATIO,
		&callback
	);
	if (FAILED(ret))
	{
		printf("error CreateSourceVoice ret=%d\n", ret);
		return -1;
	}

	// ボイスによるオーディオの使用および処理を開始
	voice->Start();

	// オーディオバッファーを用意
	XAUDIO2_BUFFER buffer = { 0 };
	const int buf_len = 2;
	BYTE** buf = new BYTE*[buf_len];
	const int len = pFormat->nAvgBytesPerSec;
	for (int i = 0; i < buf_len; i++) {
		buf[i] = new BYTE[len];
	}
	int buf_cnt = 0;
	int size;
	// 最初のバッファーへデータを読み込む
	ret = audio_file.ReadChunk(buf, buf_cnt, len, &size);
	if (FAILED(ret))
	{
		printf("error Read File ret=%d\n", ret);
		return -1;
	}

	buffer.AudioBytes = size;
	buffer.pAudioData = buf[buf_cnt];
	if (0 < size)
	{
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", ret);
			return -1;
		}
	}
	if (buf_len <= ++buf_cnt) buf_cnt = 0;

	// 再生ループ
	do {
		// ファイルからデータを読み取り
		audio_file.ReadChunk(buf, buf_cnt, len, &size);

		// なければループ抜ける
		if (size <= 0) {
			break;
		}
		// バッファー生成
		buffer.AudioBytes = size;
		buffer.pAudioData = buf[buf_cnt];
		// バッファーをキューに追加
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", ret);
			return -1;
		}
		// 2つ確保しているバッファーを交互に使うようにカウンタチェックとリセット
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// 再生すべきデータをすべて読み取ったのでボイスによるオーディオの使用を停止
	// Stop呼ばない限り再生状態を続ける
	voice->Stop();

	// 終了メッセージ
	printf("play end!");

	// ユーザーの入力を待つ
	getchar();

	// 後処理
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// ファイルを閉じる
	audio_file.Close();
	// ソースボイスの解放
	voice->DestroyVoice(); voice = NULL;
	// マスターボイスの解放
	master->DestroyVoice(); master = NULL;
	// オーディオエンジンの解放
	audio->Release(); audio = NULL;
	// COMコンポーネントの解放
	CoUninitialize();

	return 0;
}