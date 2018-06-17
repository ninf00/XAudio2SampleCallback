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
#include "voice_callback.h"

//-------------------------------------------------------------------------------------------------
// Helper macros
//-------------------------------------------------------------------------------------------------
#ifndef SAVE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p); (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
#endif


//-------------------------------------------------------------------------------------------------
// main function
//-------------------------------------------------------------------------------------------------
int main()
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