//---------------------------------------------------------------------------------------
// File: wave_operator.h
//
// Wave�t�@�C���𑀍삷�邽�߂̃N���X
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
	// �R���X�g���N�^�[
	WaveOperator();
	// �f�X�g���N�^�[
	~WaveOperator();
	// �t�@�C�����J��
	HRESULT Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag);
    // �t�@�C�������
	HRESULT Close();
	// �t�@�C����ǂݍ���
	HRESULT Read(BYTE* buffer, DWORD size, DWORD* readSize);
	// �t�@�C���ɏ�������
	HRESULT Write(UINT writeSize, BYTE* data, UINT wroteSize);
	// �`�����N�f�[�^��ǂݍ���(�א؂�ɂŁ[����ǂ݂���)
	HRESULT ReadChunk(BYTE** buffer, int bufferCount, const int len, int* readSize);
	// �T�C�Y���擾
	DWORD GetSize();
	// �t�@�C���̈ʒu�����Z�b�g
	HRESULT ResetFile();
	WAVEFORMATEX* GetFormat() { return format; }
};

#endif
