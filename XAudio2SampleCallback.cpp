//-------------------------------------------------------------------------------------------------
// File: XAudio2SampleCallback.cpp
//
// XAudio2��Callback����Ŏg���T���v��
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
// Desc: ����MMIO�g����Wave�t�@�C���J���čĐ�����
//---------------------------------------------------------------------------------------
int playMediaDirect()
{
	HRESULT ret;

	// COM�R���|�[�l���g�̏����� - XAudio2�Ŏg������
	ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		printf("error CoInitializeEx ret=%d\n", ret);
		return -1;
	}

	// XAudio2�G���W������
	IXAudio2 *audio = NULL;
	ret = XAudio2Create(
		&audio
	);
	if (FAILED(ret)) {
		printf("error XAudio2Create ret=%d\n", ret);
		return -1;
	}

	// �}�X�^�[�{�C�X����
	IXAudio2MasteringVoice *master = NULL;
	ret = audio->CreateMasteringVoice(
		&master
	);
	if (FAILED(ret)) {
		printf("error CreateMasteringVoice ret=%d\n", ret);
		return -1;
	}

	//
	// �Đ����鉹�y�t�@�C���̓ǂݍ���
	//
	// �t�@�C�����J��
	wchar_t file[] = { L"test.wav" };
	HMMIO mmio = NULL;
	MMIOINFO info = { 0 };
	mmio = mmioOpen(file, &info, MMIO_READ);
	if (!mmio) {
		printf("error mmioOpen\n");
		return -1;
	}

	// �`�����N�������
	// �܂���WAVE�`�����N�ɐi��
	MMRESULT mret;
	MMCKINFO riff_chunk;
	riff_chunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	mret = mmioDescend(mmio, &riff_chunk, NULL, MMIO_FINDRIFF);
	if (mret != MMSYSERR_BASE) {
		printf("error mmioDescend(wave) ret=%d\n", mret);
		return -1;
	}
	// ����fmt�`�����N�ɐi��
	MMCKINFO chunk;
	chunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
	mret = mmioDescend(mmio, &chunk, &riff_chunk, MMIO_FINDCHUNK);
	if (mret != MMSYSERR_BASE)
	{
		printf("error mmioDescend(fmt) ret=%d\n", mret);
		return -1;
	}

	// WAVEFORMATEX�𐶐�
	WAVEFORMATEX format = { 0 };
	{
		// mmioOpen�֐��ŊJ�����t�@�C������A�w�肳�ꂽ�o�C�g����ǂݎ��
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

	// �\�[�X�{�C�X�𐶐�
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

	// data�`�����N�ɐi�߂�
	chunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
	mret = mmioDescend(mmio, &chunk, &riff_chunk, MMIO_FINDCHUNK);
	if (mret != MMSYSERR_BASE)
	{
		printf("error mmioDescend(data) ret=%d\n", mret);
		return -1;
	}

	// �{�C�X�ɂ��I�[�f�B�I�̎g�p����я������J�n
	voice->Start();

	// �I�[�f�B�I�o�b�t�@�[��p��
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

	// �Đ����[�v
	do {
		// �t�@�C������f�[�^��ǂݎ��
		size = mmioRead(mmio, (HPSTR)buf[buf_cnt], len);
		// �Ȃ���΃��[�v������
		if (size <= 0) {
			break;
		}
		// �o�b�t�@�[����
		buffer.AudioBytes = size;
		buffer.pAudioData = buf[buf_cnt];
		// �o�b�t�@�[���L���[�ɒǉ�
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", mret);
			return -1;
		}
		// 2�m�ۂ��Ă���o�b�t�@�[�����݂Ɏg���悤�ɃJ�E���^�`�F�b�N�ƃ��Z�b�g
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// �Đ����ׂ��f�[�^�����ׂēǂݎ�����̂Ń{�C�X�ɂ��I�[�f�B�I�̎g�p���~
	// Stop�Ă΂Ȃ�����Đ���Ԃ𑱂���
	voice->Stop();

	// �I�����b�Z�[�W
	printf("play end!");

	// ���[�U�[�̓��͂�҂�
	getchar();

	// �㏈��
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// �t�@�C�������
	mmioClose(mmio, MMIO_FHOPEN);
	// �\�[�X�{�C�X�̉��
	voice->DestroyVoice(); voice = NULL;
	// �}�X�^�[�{�C�X�̉��
	master->DestroyVoice(); master = NULL;
	// �I�[�f�B�I�G���W���̉��
	audio->Release(); audio = NULL;
	// COM�R���|�[�l���g�̉��
	CoUninitialize();

	return 0;
}

//---------------------------------------------------------------------------------------
// Name: playeMediaWaveOperator()
// Desc: WaveOperator�N���X�𗘗p����Wave�t�@�C�����J���čĐ�����
//---------------------------------------------------------------------------------------
int playMediaWaveOperator()
{
	HRESULT ret;

	// COM�R���|�[�l���g�̏����� - XAudio2�Ŏg������
	ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		printf("error CoInitializeEx ret=%d\n", ret);
		return -1;
	}

	// XAudio2�G���W������
	IXAudio2 *audio = NULL;
	ret = XAudio2Create(
		&audio
	);
	if (FAILED(ret)) {
		printf("error XAudio2Create ret=%d\n", ret);
		return -1;
	}

	// �}�X�^�[�{�C�X����
	IXAudio2MasteringVoice *master = NULL;
	ret = audio->CreateMasteringVoice(
		&master
	);
	if (FAILED(ret)) {
		printf("error CreateMasteringVoice ret=%d\n", ret);
		return -1;
	}

	//
	// �Đ����鉹�y�t�@�C���̓ǂݍ���
	//
	// �t�@�C�����J��
	wchar_t file[] = { L"test.wav" };
	WaveOperator audio_file;
	if (FAILED(ret = audio_file.Open(file, NULL, WAVEFILE_READ)))
	{
		printf("error wave file open ret=%d\n", ret);
		return -1;
	}

	// �J�����t�@�C�����\��
	WAVEFORMATEX* pFormat = audio_file.GetFormat();
	printf("format =%d\n", pFormat->wFormatTag);
	printf("channel =%d\n", pFormat->nChannels);
	printf("sampling =%dHz\n", pFormat->nSamplesPerSec);
	printf("bit/sample =%d\n", pFormat->wBitsPerSample);
	printf("byte/sec =%d\n", pFormat->nAvgBytesPerSec);


	// �\�[�X�{�C�X�𐶐�
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

	// �{�C�X�ɂ��I�[�f�B�I�̎g�p����я������J�n
	voice->Start();

	// �I�[�f�B�I�o�b�t�@�[��p��
	XAUDIO2_BUFFER buffer = { 0 };
	const int buf_len = 2;
	BYTE** buf = new BYTE*[buf_len];
	const int len = pFormat->nAvgBytesPerSec;
	for (int i = 0; i < buf_len; i++) {
		buf[i] = new BYTE[len];
	}
	int buf_cnt = 0;
	int size;
	// �ŏ��̃o�b�t�@�[�փf�[�^��ǂݍ���
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

	// �Đ����[�v
	do {
		// �t�@�C������f�[�^��ǂݎ��
		audio_file.ReadChunk(buf, buf_cnt, len, &size);

		// �Ȃ���΃��[�v������
		if (size <= 0) {
			break;
		}
		// �o�b�t�@�[����
		buffer.AudioBytes = size;
		buffer.pAudioData = buf[buf_cnt];
		// �o�b�t�@�[���L���[�ɒǉ�
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", ret);
			return -1;
		}
		// 2�m�ۂ��Ă���o�b�t�@�[�����݂Ɏg���悤�ɃJ�E���^�`�F�b�N�ƃ��Z�b�g
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// �Đ����ׂ��f�[�^�����ׂēǂݎ�����̂Ń{�C�X�ɂ��I�[�f�B�I�̎g�p���~
	// Stop�Ă΂Ȃ�����Đ���Ԃ𑱂���
	voice->Stop();

	// �I�����b�Z�[�W
	printf("play end!");

	// ���[�U�[�̓��͂�҂�
	getchar();

	// �㏈��
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// �t�@�C�������
	audio_file.Close();
	// �\�[�X�{�C�X�̉��
	voice->DestroyVoice(); voice = NULL;
	// �}�X�^�[�{�C�X�̉��
	master->DestroyVoice(); master = NULL;
	// �I�[�f�B�I�G���W���̉��
	audio->Release(); audio = NULL;
	// COM�R���|�[�l���g�̉��
	CoUninitialize();

	return 0;
}

//---------------------------------------------------------------------------------------
// Name: playMediaWaveOperatorWithThread()
// Desc: WaveOperator�N���X�𗘗p����Wave�t�@�C�����J���čĐ�����
//       �����͕ʓr���������X���b�h���ōĐ������
//---------------------------------------------------------------------------------------
int playMediaWaveOperatorWithThread()
{
	HRESULT ret;

	// COM�R���|�[�l���g�̏����� - XAudio2�Ŏg������
	ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		printf("error CoInitializeEx ret=%d\n", ret);
		return -1;
	}

	// XAudio2�G���W������
	IXAudio2 *audio = NULL;
	ret = XAudio2Create(
		&audio
	);
	if (FAILED(ret)) {
		printf("error XAudio2Create ret=%d\n", ret);
		return -1;
	}

	// �}�X�^�[�{�C�X����
	IXAudio2MasteringVoice *master = NULL;
	ret = audio->CreateMasteringVoice(
		&master
	);
	if (FAILED(ret)) {
		printf("error CreateMasteringVoice ret=%d\n", ret);
		return -1;
	}

	//-------------------------------------------
	// SE�p�I�[�f�B�I�̓ǂݍ���
	//-------------------------------------------
	wchar_t se_file[] = { L"se01.wav" };
	WaveOperator seOperator;
	if (FAILED(ret = seOperator.Open(se_file, NULL, WAVEFILE_READ, true))) {
		printf("error SE File can not open ret=%d\n", ret);
		return -1;
	}
	// SE�t�@�C���̍\����
	WAVEFORMATEX* seWfx = seOperator.GetFormat();
	// SE�t�@�C���̃T�C�Y
	DWORD seFileSize = seOperator.GetSize();
	// SE�t�@�C���̓ǂݍ��ݗp�o�b�t�@�[
	BYTE* seWaveBuffer = new BYTE[seFileSize];
	
	// SE�t�@�C���̃f�[�^���o�b�t�@�[�ɓǂݍ���
	if (FAILED(ret = seOperator.Read(seWaveBuffer, seFileSize, &seFileSize))) {
		printf("error read SE File %#X\n", ret);
		return -1;
	}

	// SE�p�̃\�[�X�{�C�X����
	IXAudio2SourceVoice* seSourceVoice;
	if (FAILED(ret = audio->CreateSourceVoice(&seSourceVoice, seWfx)))
	{
		printf("error %#X creating se source voice\n", ret);
		SAFE_DELETE_ARRAY(seWaveBuffer);
		return -1;
	}
	// �{�����[���𔼕��ɃZ�b�g
	seSourceVoice->SetVolume(0.5f);

	// SE�p��structure�\�z
	PlaySeData seAudioData = { 0 };
	seAudioData.isPlayed = false;
	seAudioData.buffer = seWaveBuffer;
	seAudioData.size = seFileSize;
	seAudioData.sourceVoice = seSourceVoice;


	// �I�[�f�B�I�Đ��p��structure�\�z
	AudioData* audioData = new AudioData();
	audioData->isPlayed = false;
	audioData->audioEngine = audio;

	// ���[�U�[�̓��͂�҂�
	getchar();

	int loopCount = 0;
	// �Q�[���p���[�v��z��
	while (!audioData->isPlayed)
	{
		// ���񃋁[�v���ɃX���b�h���s
		if (loopCount == 0)
		{
			// AudioStream�����̃X���b�h�𐶐�
			std::thread t(PlayAudioStream, audioData);
			t.detach();
		}
		// ���[�v100000�������SE�炷�^�C�~���O�؂�ւ�
		// 100000��ȉ� -> 1000��
		// 100000��ȏ� -> 25000��
		if (loopCount <= 100000)
		{
			// ���[�v1000�񂲂Ƃ�SE�炷
			if (loopCount % 1000 == 0)
				playSe(&seAudioData);
		}
		else
		{
			// ���[�v25000�񂲂Ƃ�SE�炷
			if (loopCount % 25000 == 0)
				playSe(&seAudioData);
		}

		printf("count: %d\r", loopCount);
		loopCount++;
	}
	// �g�p�ςݍ\���̂����(BGM)
	audioData->isPlayed = false;
	audioData->audioEngine = NULL;
	SAFE_DELETE(audioData);
	// �\�[�X�{�C�X���~�߂�
	seSourceVoice->Stop();
	// �g�p�ςݍ\���̂����(SE)
	seAudioData = { 0 };
	

	// ���s
	printf("\n");

	// �I�����b�Z�[�W
	printf("play end!");

	// ���[�U�[�̓��͂�҂�
	getchar();

	// SE�p�\�[�X�{�C�X���
	seSourceVoice->DestroyVoice();
	// SE�p�o�b�t�@�[���
	SAFE_DELETE_ARRAY(seWaveBuffer);

	// �}�X�^�[�{�C�X�̉��
	master->DestroyVoice(); master = NULL;
	// �I�[�f�B�I�G���W���̉��
	audio->Release(); audio = NULL;
	// COM�R���|�[�l���g�̉��
	CoUninitialize();

	return 0;
}

//---------------------------------------------------------------------------------------
// Name: playMediaOggOperatorWithThread()
// Desc: OggOperator�N���X�𗘗p����Ogg�t�@�C�����J���čĐ�����
//       �����͕ʓr���������X���b�h���ōĐ������
//---------------------------------------------------------------------------------------
int playMediaOggOperatorWithThread()
{
	HRESULT ret;

	// COM�R���|�[�l���g�̏����� - XAudio2�Ŏg������
	ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(ret)) {
		printf("error CoInitializeEx ret=%d\n", ret);
		return -1;
	}

	// XAudio2�G���W������
	IXAudio2 *audio = NULL;
	ret = XAudio2Create(
		&audio
	);
	if (FAILED(ret)) {
		printf("error XAudio2Create ret=%d\n", ret);
		return -1;
	}

	// �}�X�^�[�{�C�X����
	IXAudio2MasteringVoice *master = NULL;
	ret = audio->CreateMasteringVoice(
		&master
	);
	if (FAILED(ret)) {
		printf("error CreateMasteringVoice ret=%d\n", ret);
		return -1;
	}

	//-------------------------------------------
	// SE�p�I�[�f�B�I�̓ǂݍ���
	//-------------------------------------------
	wchar_t se_file[] = { L"se01.ogg" };
	OggOperator seOperator;
	if (FAILED(ret = seOperator.Open(se_file, NULL, WAVEFILE_READ, true))) {
		printf("error SE File can not open ret=%d\n", ret);
		return -1;
	}
	// SE�t�@�C���̍\����
	WAVEFORMATEX* seWfx = seOperator.GetFormat();
	// SE�t�@�C���̃T�C�Y
	DWORD seFileSize = seOperator.GetSize();
	// SE�t�@�C���̓ǂݍ��ݗp�o�b�t�@�[
	BYTE* seWaveBuffer = new BYTE[seFileSize];

	// SE�t�@�C���̃f�[�^���o�b�t�@�[�ɓǂݍ���
	if (FAILED(ret = seOperator.Read(seWaveBuffer, seFileSize, &seFileSize))) {
		printf("error read SE File %#X\n", ret);
		return -1;
	}

	// SE�p�̃\�[�X�{�C�X����
	IXAudio2SourceVoice* seSourceVoice;
	if (FAILED(ret = audio->CreateSourceVoice(&seSourceVoice, seWfx)))
	{
		printf("error %#X creating se source voice\n", ret);
		SAFE_DELETE_ARRAY(seWaveBuffer);
		return -1;
	}
	// �{�����[���𔼕��ɃZ�b�g
	seSourceVoice->SetVolume(1.00f);

	// SE�p��structure�\�z
	PlaySeData seAudioData = { 0 };
	seAudioData.isPlayed = false;
	seAudioData.buffer = seWaveBuffer;
	seAudioData.size = seFileSize;
	seAudioData.sourceVoice = seSourceVoice;


	// �I�[�f�B�I�Đ��p��structure�\�z
	AudioData* audioData = new AudioData();
	audioData->isPlayed = false;
	audioData->audioEngine = audio;

	// ���[�U�[�̓��͂�҂�
	getchar();

	int loopCount = 0;
	// �Q�[���p���[�v��z��
	while (!audioData->isPlayed)
	{
		// ���񃋁[�v���ɃX���b�h���s
		if (loopCount == 0)
		{
			// AudioStream�����̃X���b�h�𐶐�
			std::thread t(PlayAudioStreamByOgg, audioData);
			t.detach();
		}
		// ���[�v100000�������SE�炷�^�C�~���O�؂�ւ�
		// 100000��ȉ� -> 1000��
		// 100000��ȏ� -> 25000��
		if (loopCount <= 100000)
		{
			// ���[�v1000�񂲂Ƃ�SE�炷
			if (loopCount % 1000 == 0)
				playSe(&seAudioData);
		}
		else
		{
			// ���[�v25000�񂲂Ƃ�SE�炷
			if (loopCount % 25000 == 0)
				playSe(&seAudioData);
		}

		printf("count: %d\r", loopCount);
		loopCount++;
	}
	// �g�p�ςݍ\���̂����(BGM)
	audioData->isPlayed = false;
	audioData->audioEngine = NULL;
	SAFE_DELETE(audioData);
	// �\�[�X�{�C�X���~�߂�
	seSourceVoice->Stop();
	// �g�p�ςݍ\���̂����(SE)
	seAudioData = { 0 };


	// ���s
	printf("\n");

	// �I�����b�Z�[�W
	printf("play end!");

	// ���[�U�[�̓��͂�҂�
	getchar();

	// SE�p�\�[�X�{�C�X���
	seSourceVoice->DestroyVoice();
	// SE�p�o�b�t�@�[���
	SAFE_DELETE_ARRAY(seWaveBuffer);

	// �}�X�^�[�{�C�X�̉��
	master->DestroyVoice(); master = NULL;
	// �I�[�f�B�I�G���W���̉��
	audio->Release(); audio = NULL;
	// COM�R���|�[�l���g�̉��
	CoUninitialize();

	return 0;
}

//---------------------------------------------------------------------------------------
// Name: PlayAudioStream()
// Desc: �X���b�h�ŌĂяo�����֐�
//       �����t�@�C���̓ǂݍ��݂ƃ\�[�X�{�C�X�̐����A�X�g���[�~���O���s��
//---------------------------------------------------------------------------------------
void PlayAudioStream(AudioData* audioData)
{
	HRESULT ret;
	audioData->isPlayed = false;
	//
	// �Đ����鉹�y�t�@�C���̓ǂݍ���
	//
	// �t�@�C�����J��
	wchar_t file[] = { L"test.wav" };
	WaveOperator audio_file;
	if (FAILED(ret = audio_file.Open(file, NULL, WAVEFILE_READ)))
	{
		printf("error wave file open ret=%d\n", ret);
		return;
	}

	// �J�����t�@�C�����\��
	WAVEFORMATEX* pFormat = audio_file.GetFormat();
	printf("format =%d\n", pFormat->wFormatTag);
	printf("channel =%d\n", pFormat->nChannels);
	printf("sampling =%dHz\n", pFormat->nSamplesPerSec);
	printf("bit/sample =%d\n", pFormat->wBitsPerSample);
	printf("byte/sec =%d\n", pFormat->nAvgBytesPerSec);


	// �\�[�X�{�C�X�𐶐�
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

	// �{�C�X�ɂ��I�[�f�B�I�̎g�p����я������J�n
	voice->Start();

	// �I�[�f�B�I�o�b�t�@�[��p��
	XAUDIO2_BUFFER buffer = { 0 };
	const int buf_len = 2;
	BYTE** buf = new BYTE*[buf_len];
	const int len = pFormat->nAvgBytesPerSec;
	for (int i = 0; i < buf_len; i++) {
		buf[i] = new BYTE[len];
	}
	int buf_cnt = 0;
	int size;
	// �ŏ��̃o�b�t�@�[�փf�[�^��ǂݍ���
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

	// �Đ����[�v
	do {
		// �t�@�C������f�[�^��ǂݎ��
		audio_file.ReadChunk(buf, buf_cnt, len, &size);

		// �Ȃ���΃��[�v������
		if (size <= 0) {
			break;
		}
		// �o�b�t�@�[����
		buffer.AudioBytes = size;
		buffer.pAudioData = buf[buf_cnt];
		// �o�b�t�@�[���L���[�ɒǉ�
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", ret);
			return;
		}
		// 2�m�ۂ��Ă���o�b�t�@�[�����݂Ɏg���悤�ɃJ�E���^�`�F�b�N�ƃ��Z�b�g
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// �Đ����ׂ��f�[�^�����ׂēǂݎ�����̂Ń{�C�X�ɂ��I�[�f�B�I�̎g�p���~
	// Stop�Ă΂Ȃ�����Đ���Ԃ𑱂���
	voice->Stop();

	// �㏈��
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// �t�@�C�������
	audio_file.Close();
	// �\�[�X�{�C�X�̉��
	voice->DestroyVoice(); voice = NULL;
	// �t���O���Ă�
	audioData->isPlayed = true;

}

//---------------------------------------------------------------------------------------
// Name: PlayAudioStreamByOgg()
// Desc: �X���b�h�ŌĂяo�����֐�
//       �����t�@�C���̓ǂݍ��݂ƃ\�[�X�{�C�X�̐����A�X�g���[�~���O���s��
//---------------------------------------------------------------------------------------
void PlayAudioStreamByOgg(AudioData* audioData)
{
	HRESULT ret;
	audioData->isPlayed = false;
	//
	// �Đ����鉹�y�t�@�C���̓ǂݍ���
	//
	// �t�@�C�����J��
	wchar_t file[] = { L"test.ogg" };
	OggOperator audio_file;
	if (FAILED(ret = audio_file.Open(file, NULL, WAVEFILE_READ)))
	{
		printf("error ogg file open ret=%d\n", ret);
		return;
	}

	// �J�����t�@�C�����\��
	WAVEFORMATEX* pFormat = audio_file.GetFormat();
	printf("format =%d\n", pFormat->wFormatTag);
	printf("channel =%d\n", pFormat->nChannels);
	printf("sampling =%dHz\n", pFormat->nSamplesPerSec);
	printf("bit/sample =%d\n", pFormat->wBitsPerSample);
	printf("byte/sec =%d\n", pFormat->nAvgBytesPerSec);


	// �\�[�X�{�C�X�𐶐�
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

	// �{�C�X�ɂ��I�[�f�B�I�̎g�p����я������J�n
	voice->Start();

	// �I�[�f�B�I�o�b�t�@�[��p��
	XAUDIO2_BUFFER buffer = { 0 };
	const int buf_len = 2;
	BYTE** buf = new BYTE*[buf_len];
	const int len = pFormat->nAvgBytesPerSec;
	for (int i = 0; i < buf_len; i++) {
		buf[i] = new BYTE[len];
	}
	int buf_cnt = 0;
	int size;
	// �ŏ��̃o�b�t�@�[�փf�[�^��ǂݍ���
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

	// �Đ����[�v
	do {
		// �t�@�C������f�[�^��ǂݎ��
		audio_file.ReadChunk(buf, buf_cnt, len, &size);

		// �Ȃ���΃��[�v������
		if (size <= 0) {
			break;
		}
		// �o�b�t�@�[����
		buffer.AudioBytes = size;
		buffer.pAudioData = buf[buf_cnt];
		// �o�b�t�@�[���L���[�ɒǉ�
		ret = voice->SubmitSourceBuffer(&buffer);
		if (FAILED(ret)) {
			printf("error SubmitSourceBuffer ret=%d\n", ret);
			return;
		}
		// 2�m�ۂ��Ă���o�b�t�@�[�����݂Ɏg���悤�ɃJ�E���^�`�F�b�N�ƃ��Z�b�g
		if (buf_len <= ++buf_cnt) buf_cnt = 0;
	} while (WaitForSingleObject(callback.event, INFINITE) == WAIT_OBJECT_0);

	// �Đ����ׂ��f�[�^�����ׂēǂݎ�����̂Ń{�C�X�ɂ��I�[�f�B�I�̎g�p���~
	// Stop�Ă΂Ȃ�����Đ���Ԃ𑱂���
	voice->Stop();

	// �㏈��
	for (int i = 0; i < buf_len; i++) {
		delete[] buf[i];
	}
	delete[] buf;
	// �t�@�C�������
	audio_file.Close();
	// �\�[�X�{�C�X�̉��
	voice->DestroyVoice(); voice = NULL;
	// �t���O���Ă�
	audioData->isPlayed = true;

}

//---------------------------------------------------------------------------------------
// Name: playSe()
// Desc: ���O�Ƀ������Ƀ��[�h���Ă����������Đ�����
//---------------------------------------------------------------------------------------
void playSe(PlaySeData* audioData)
{
	HRESULT hr;
	// �t���O�`�F�b�N
	// ��x���J�n����ĂȂ��Ȃ�\�[�X�{�C�X���J�n
	if (audioData->isPlayed == false)
	{
		audioData->isPlayed = true;
		audioData->sourceVoice->Start();
	}

	//----------------------------------------------
	// �L���[�̒��g���`�F�b�N���Ă܂��c���Ă��ꍇ
	// 1. �{�C�X���~����
	// 2. �o�b�t�@�[���t���b�V�����ċ�ɂ���
	// 3. �{�C�X���Đ�����
	//----------------------------------------------
	XAUDIO2_VOICE_STATE state;
	audioData->sourceVoice->GetState(&state);
	if (state.BuffersQueued > 0)
	{
		audioData->sourceVoice->Stop();
		audioData->sourceVoice->FlushSourceBuffers();
		audioData->sourceVoice->Start();
	}

	// �o�b�t�@�[�𐶐�����
	XAUDIO2_BUFFER seBuffer = { 0 };
	seBuffer.pAudioData = audioData->buffer;
	// ���̃t���O�����ƃ\�[�X�{�C�X�̍ė��p�ł��Ȃ��Ȃ���ۂ�
	//seBuffer.Flags = XAUDIO2_END_OF_STREAM;
	seBuffer.AudioBytes = audioData->size;
	
	// �L���[�Ƀo�b�t�@�[�𓊓�����
	if (FAILED(hr = audioData->sourceVoice->SubmitSourceBuffer(&seBuffer)))
	{
		printf("error %#X submitting source buffer\n", hr);
		return;
	}
}