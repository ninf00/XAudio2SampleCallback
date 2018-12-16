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
#include <thread>
#include "voice_callback.h"
#include "constants.h"
#include "wave_operator.h"
#include "ogg_operator.h"

//-------------------------------------------------------------------------------------------------
// define
//-------------------------------------------------------------------------------------------------
#define PLAY_MODE 3

struct AudioData {
	bool isPlayed;
	IXAudio2 * audioEngine;
};

struct PlaySeData {
	bool isPlayed;
	BYTE* buffer;
	DWORD size;
	IXAudio2SourceVoice* sourceVoice;
};

//-------------------------------------------------------------------------------------------------
// function prototype
//-------------------------------------------------------------------------------------------------
int playMediaDirect();
int playMediaWaveOperator();
int playMediaWaveOperatorWithThread();
int playMediaOggOperatorWithThread();
void PlayAudioStream(AudioData* audioData);
void PlayAudioStreamByOgg(AudioData* audioData);
void playSe(PlaySeData* audioData);

//-------------------------------------------------------------------------------------------------
// main function
//-------------------------------------------------------------------------------------------------
int main()
{
	if (PLAY_MODE == 0)
		return playMediaDirect();
	else if (PLAY_MODE == 1)
		return playMediaWaveOperator();
	else if (PLAY_MODE == 2)
		return playMediaWaveOperatorWithThread();
	else
		return playMediaOggOperatorWithThread();
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

//---------------------------------------------------------------------------------------
// Name: playMediaWaveOperatorWithThread()
// Desc: WaveOperatorクラスを利用してWaveファイルを開いて再生する
//       音声は別途生成したスレッド内で再生される
//---------------------------------------------------------------------------------------
int playMediaWaveOperatorWithThread()
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

	//-------------------------------------------
	// SE用オーディオの読み込み
	//-------------------------------------------
	wchar_t se_file[] = { L"se01.wav" };
	WaveOperator seOperator;
	if (FAILED(ret = seOperator.Open(se_file, NULL, WAVEFILE_READ, true))) {
		printf("error SE File can not open ret=%d\n", ret);
		return -1;
	}
	// SEファイルの構造体
	WAVEFORMATEX* seWfx = seOperator.GetFormat();
	// SEファイルのサイズ
	DWORD seFileSize = seOperator.GetSize();
	// SEファイルの読み込み用バッファー
	BYTE* seWaveBuffer = new BYTE[seFileSize];
	
	// SEファイルのデータをバッファーに読み込み
	if (FAILED(ret = seOperator.Read(seWaveBuffer, seFileSize, &seFileSize))) {
		printf("error read SE File %#X\n", ret);
		return -1;
	}

	// SE用のソースボイス生成
	IXAudio2SourceVoice* seSourceVoice;
	if (FAILED(ret = audio->CreateSourceVoice(&seSourceVoice, seWfx)))
	{
		printf("error %#X creating se source voice\n", ret);
		SAFE_DELETE_ARRAY(seWaveBuffer);
		return -1;
	}
	// ボリュームを半分にセット
	seSourceVoice->SetVolume(0.5f);

	// SE用のstructure構築
	PlaySeData seAudioData = { 0 };
	seAudioData.isPlayed = false;
	seAudioData.buffer = seWaveBuffer;
	seAudioData.size = seFileSize;
	seAudioData.sourceVoice = seSourceVoice;


	// オーディオ再生用のstructure構築
	AudioData* audioData = new AudioData();
	audioData->isPlayed = false;
	audioData->audioEngine = audio;

	// ユーザーの入力を待つ
	getchar();

	int loopCount = 0;
	// ゲーム用ループを想定
	while (!audioData->isPlayed)
	{
		// 初回ループ時にスレッド実行
		if (loopCount == 0)
		{
			// AudioStream処理のスレッドを生成
			std::thread t(PlayAudioStream, audioData);
			t.detach();
		}
		// ループ100000回を境にSE鳴らすタイミング切り替え
		// 100000回以下 -> 1000回毎
		// 100000回以上 -> 25000回毎
		if (loopCount <= 100000)
		{
			// ループ1000回ごとにSE鳴らす
			if (loopCount % 1000 == 0)
				playSe(&seAudioData);
		}
		else
		{
			// ループ25000回ごとにSE鳴らす
			if (loopCount % 25000 == 0)
				playSe(&seAudioData);
		}

		printf("count: %d\r", loopCount);
		loopCount++;
	}
	// 使用済み構造体を解放(BGM)
	audioData->isPlayed = false;
	audioData->audioEngine = NULL;
	SAFE_DELETE(audioData);
	// ソースボイスを止める
	seSourceVoice->Stop();
	// 使用済み構造体を解放(SE)
	seAudioData = { 0 };
	

	// 改行
	printf("\n");

	// 終了メッセージ
	printf("play end!");

	// ユーザーの入力を待つ
	getchar();

	// SE用ソースボイス解放
	seSourceVoice->DestroyVoice();
	// SE用バッファー解放
	SAFE_DELETE_ARRAY(seWaveBuffer);

	// マスターボイスの解放
	master->DestroyVoice(); master = NULL;
	// オーディオエンジンの解放
	audio->Release(); audio = NULL;
	// COMコンポーネントの解放
	CoUninitialize();

	return 0;
}

//---------------------------------------------------------------------------------------
// Name: playMediaOggOperatorWithThread()
// Desc: OggOperatorクラスを利用してOggファイルを開いて再生する
//       音声は別途生成したスレッド内で再生される
//---------------------------------------------------------------------------------------
int playMediaOggOperatorWithThread()
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

	//-------------------------------------------
	// SE用オーディオの読み込み
	//-------------------------------------------
	wchar_t se_file[] = { L"se01.ogg" };
	OggOperator seOperator;
	if (FAILED(ret = seOperator.Open(se_file, NULL, WAVEFILE_READ, true))) {
		printf("error SE File can not open ret=%d\n", ret);
		return -1;
	}
	// SEファイルの構造体
	WAVEFORMATEX* seWfx = seOperator.GetFormat();
	// SEファイルのサイズ
	DWORD seFileSize = seOperator.GetSize();
	// SEファイルの読み込み用バッファー
	BYTE* seWaveBuffer = new BYTE[seFileSize];

	// SEファイルのデータをバッファーに読み込み
	if (FAILED(ret = seOperator.Read(seWaveBuffer, seFileSize, &seFileSize))) {
		printf("error read SE File %#X\n", ret);
		return -1;
	}

	// SE用のソースボイス生成
	IXAudio2SourceVoice* seSourceVoice;
	if (FAILED(ret = audio->CreateSourceVoice(&seSourceVoice, seWfx)))
	{
		printf("error %#X creating se source voice\n", ret);
		SAFE_DELETE_ARRAY(seWaveBuffer);
		return -1;
	}
	// ボリュームを半分にセット
	seSourceVoice->SetVolume(1.00f);

	// SE用のstructure構築
	PlaySeData seAudioData = { 0 };
	seAudioData.isPlayed = false;
	seAudioData.buffer = seWaveBuffer;
	seAudioData.size = seFileSize;
	seAudioData.sourceVoice = seSourceVoice;


	// オーディオ再生用のstructure構築
	AudioData* audioData = new AudioData();
	audioData->isPlayed = false;
	audioData->audioEngine = audio;

	// ユーザーの入力を待つ
	getchar();

	int loopCount = 0;
	// ゲーム用ループを想定
	while (!audioData->isPlayed)
	{
		// 初回ループ時にスレッド実行
		if (loopCount == 0)
		{
			// AudioStream処理のスレッドを生成
			std::thread t(PlayAudioStreamByOgg, audioData);
			t.detach();
		}
		// ループ100000回を境にSE鳴らすタイミング切り替え
		// 100000回以下 -> 1000回毎
		// 100000回以上 -> 25000回毎
		if (loopCount <= 100000)
		{
			// ループ1000回ごとにSE鳴らす
			if (loopCount % 1000 == 0)
				playSe(&seAudioData);
		}
		else
		{
			// ループ25000回ごとにSE鳴らす
			if (loopCount % 25000 == 0)
				playSe(&seAudioData);
		}

		printf("count: %d\r", loopCount);
		loopCount++;
	}
	// 使用済み構造体を解放(BGM)
	audioData->isPlayed = false;
	audioData->audioEngine = NULL;
	SAFE_DELETE(audioData);
	// ソースボイスを止める
	seSourceVoice->Stop();
	// 使用済み構造体を解放(SE)
	seAudioData = { 0 };


	// 改行
	printf("\n");

	// 終了メッセージ
	printf("play end!");

	// ユーザーの入力を待つ
	getchar();

	// SE用ソースボイス解放
	seSourceVoice->DestroyVoice();
	// SE用バッファー解放
	SAFE_DELETE_ARRAY(seWaveBuffer);

	// マスターボイスの解放
	master->DestroyVoice(); master = NULL;
	// オーディオエンジンの解放
	audio->Release(); audio = NULL;
	// COMコンポーネントの解放
	CoUninitialize();

	return 0;
}

//---------------------------------------------------------------------------------------
// Name: PlayAudioStream()
// Desc: スレッドで呼び出される関数
//       音声ファイルの読み込みとソースボイスの生成、ストリーミングを行う
//---------------------------------------------------------------------------------------
void PlayAudioStream(AudioData* audioData)
{
	HRESULT ret;
	audioData->isPlayed = false;
	//
	// 再生する音楽ファイルの読み込み
	//
	// ファイルを開く
	wchar_t file[] = { L"test.wav" };
	WaveOperator audio_file;
	if (FAILED(ret = audio_file.Open(file, NULL, WAVEFILE_READ)))
	{
		printf("error wave file open ret=%d\n", ret);
		return;
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
	ret = audioData->audioEngine->CreateSourceVoice(
		&voice,
		pFormat,
		0,
		XAUDIO2_DEFAULT_FREQ_RATIO,
		&callback
	);
	if (FAILED(ret))
	{
		printf("error CreateSourceVoice ret=%d\n", ret);
		return;
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
		return;
	}

	buffer.AudioBytes = size;
	buffer.pAudioData = buf[buf_cnt];
	if (0 < size)
	{
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", ret);
			return;
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
			return;
		}
		// 2つ確保しているバッファーを交互に使うようにカウンタチェックとリセット
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// 再生すべきデータをすべて読み取ったのでボイスによるオーディオの使用を停止
	// Stop呼ばない限り再生状態を続ける
	voice->Stop();

	// 後処理
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// ファイルを閉じる
	audio_file.Close();
	// ソースボイスの解放
	voice->DestroyVoice(); voice = NULL;
	// フラグ立てる
	audioData->isPlayed = true;

}

//---------------------------------------------------------------------------------------
// Name: PlayAudioStreamByOgg()
// Desc: スレッドで呼び出される関数
//       音声ファイルの読み込みとソースボイスの生成、ストリーミングを行う
//---------------------------------------------------------------------------------------
void PlayAudioStreamByOgg(AudioData* audioData)
{
	HRESULT ret;
	audioData->isPlayed = false;
	//
	// 再生する音楽ファイルの読み込み
	//
	// ファイルを開く
	wchar_t file[] = { L"test.ogg" };
	OggOperator audio_file;
	if (FAILED(ret = audio_file.Open(file, NULL, WAVEFILE_READ)))
	{
		printf("error ogg file open ret=%d\n", ret);
		return;
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
	ret = audioData->audioEngine->CreateSourceVoice(
		&voice,
		pFormat,
		0,
		XAUDIO2_DEFAULT_FREQ_RATIO,
		&callback
	);
	if (FAILED(ret))
	{
		printf("error CreateSourceVoice ret=%d\n", ret);
		return;
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
		return;
	}

	buffer.AudioBytes = size;
	buffer.pAudioData = buf[buf_cnt];
	if (0 < size)
	{
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", ret);
			return;
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
			return;
		}
		// 2つ確保しているバッファーを交互に使うようにカウンタチェックとリセット
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// 再生すべきデータをすべて読み取ったのでボイスによるオーディオの使用を停止
	// Stop呼ばない限り再生状態を続ける
	voice->Stop();

	// 後処理
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// ファイルを閉じる
	audio_file.Close();
	// ソースボイスの解放
	voice->DestroyVoice(); voice = NULL;
	// フラグ立てる
	audioData->isPlayed = true;

}

//---------------------------------------------------------------------------------------
// Name: playSe()
// Desc: 事前にメモリにロードしていた音声を再生する
//---------------------------------------------------------------------------------------
void playSe(PlaySeData* audioData)
{
	HRESULT hr;
	// フラグチェック
	// 一度も開始されてないならソースボイスも開始
	if (audioData->isPlayed == false)
	{
		audioData->isPlayed = true;
		audioData->sourceVoice->Start();
	}

	//----------------------------------------------
	// キューの中身をチェックしてまだ残ってた場合
	// 1. ボイスを停止する
	// 2. バッファーをフラッシュして空にする
	// 3. ボイスを再生する
	//----------------------------------------------
	XAUDIO2_VOICE_STATE state;
	audioData->sourceVoice->GetState(&state);
	if (state.BuffersQueued > 0)
	{
		audioData->sourceVoice->Stop();
		audioData->sourceVoice->FlushSourceBuffers();
		audioData->sourceVoice->Start();
	}

	// バッファーを生成する
	XAUDIO2_BUFFER seBuffer = { 0 };
	seBuffer.pAudioData = audioData->buffer;
	// このフラグ入れるとソースボイスの再利用できなくなるっぽい
	//seBuffer.Flags = XAUDIO2_END_OF_STREAM;
	seBuffer.AudioBytes = audioData->size;
	
	// キューにバッファーを投入する
	if (FAILED(hr = audioData->sourceVoice->SubmitSourceBuffer(&seBuffer)))
	{
		printf("error %#X submitting source buffer\n", hr);
		return;
	}
}