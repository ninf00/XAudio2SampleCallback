//---------------------------------------------------------------------------------------
// File: ogg_operator.h
//
// OGG�t�@�C���𑀍삷�邽�߂̃N���X
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
	const unsigned int requestSize = 4096;	// �ǂݍ��ݒP��
	// ��x�Ƀ������ɓǂݍ��ރt�@�C���T�C�Y 1M
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
	// �Z�O�����g�f�[�^�擾
	virtual bool GetSegment(char* buffer, unsigned int size, int* writeSize, bool* isEnd);
	bool GenerateWaveFormatEx();

protected:
	HRESULT ReadMMIO();
	HRESULT WriteMMIO(WAVEFORMATEX* format);

public:
	// �R���X�g���N�^�[
	OggOperator();
	// �f�X�g���N�^�[
	~OggOperator();

	// �A�N�Z�b�T���\�b�h
	void SetReady(bool value) { isReady = value; }
	bool GetReady() { return isReady; }

	void SetFileName(LPWSTR value);

	void SetChannelNumber(int value) { channelNumber = value; }
	void SetSamplingRate(int value) { samplingRate = value; }
	void SetBitRate(int value) { bitRate = value; }

	// �t�@�C�����J��
	HRESULT Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag, bool isBuffered = false);
	// ����������t�@�C�����J��
	HRESULT OpenFromMemory(BYTE* data, ULONG dataSize, WAVEFORMATEX* format, DWORD flag);
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
