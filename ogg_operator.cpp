//---------------------------------------------------------------------------------------
// File: ogg_operator.cpp
//
// OGG�t�@�C���𑀍삷�邭�炷�̎���
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
// Desc: �R���X�g���N�^�[
// �߂�΁[�̏��������s��
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
// Desc: �f�X�g���N�^�[
// �Q�Ƃ̉���Ȃǂ��s��
//---------------------------------------------------------------------------------------
OggOperator::~OggOperator()
{
	Close();
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::Open()
// Desc: �ǂݍ��ݐ�p�Ńt�@�C�����J��
//---------------------------------------------------------------------------------------
HRESULT OggOperator::Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag, bool isBuffered)
{
	HRESULT hr;
	modeFlag = flag;
	errno_t error;

	// ���[�h�ɉ����ăt�@�C�����J��
	if (modeFlag == OGGFILE_READ)
	{
		// �t�@�C�����`�F�b�N
		if (strFileName == NULL)
			return E_INVALIDARG;
		SAFE_DELETE_ARRAY(format);

		// ogg�t�@�C�����J��
		error = _wfopen_s(&fp, strFileName, L"rb");
		if (error != 0)
		{
			return E_FAIL;
		}
		// ogg�t�@�C�����J��
		error = ov_open(fp, &ovf, NULL, 0);
		if (error != 0)
		{
			return E_FAIL;
		}
		// ogg�t�@�C�������擾���ăZ�b�g
		vorbis_info *info = ov_info(&ovf, -1);
		SetFileName(strFileName);
		SetChannelNumber(info->channels);
		// bitrate��16�ɂ��Ă���
		SetBitRate(16);
		SetSamplingRate(info->rate);
		// �Z�b�g�������g���ăt�H�[�}�b�g�t�@�C���쐬
		if (!GenerateWaveFormatEx())
			return E_FAIL;
		// �T�C�Y�͈�U1M�Œ�Ŏb��Ή�
		size = OggOperatorNS::pcmSize;
	}
	else
	{
		return E_FAIL;
	}

	// �t�@�C���̏������o����
	isReady = true;
	hr = S_OK;
	return hr;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::OpenFromMemory()
// Desc: �������[���烁���o�[�ϐ���ǂݍ���
//---------------------------------------------------------------------------------------
HRESULT OggOperator::OpenFromMemory(BYTE* data, ULONG dataSize, WAVEFORMATEX* format, DWORD flag)
{
	// �f�[�^���R�s�[����
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
// Desc: multimedia I/O stream����ǂݍ��ނ��߂̊֐�
//---------------------------------------------------------------------------------------
HRESULT OggOperator::ReadMMIO()
{
	MMCKINFO ckIn; // �ėp�I�Ɏg���A �`�����N�̏��
	PCMWAVEFORMAT pcmWaveFormat; // �ꎞ�I�ɂ���PCM�\����

	memset(&ckIn, 0, sizeof(ckIn)); // ckIn��0���Z�b�g

	format = NULL;

	// �`�����N�������
	if ((0 != mmioDescend(mmio, &riffChunk, NULL, 0)))
		return E_FAIL;

	// �t�@�C���\���`�F�b�N�AWAVE�t�@�C���̍\���ɂȂ��Ă��邩�H
	if ((riffChunk.ckid != FOURCC_RIFF) ||
		(riffChunk.fccType != mmioFOURCC('W', 'A', 'V', 'E')))
		return E_FAIL;

	// fmt�`�����N��T��
	ckIn.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (0 != mmioDescend(mmio, &ckIn, &riffChunk, MMIO_FINDCHUNK))
		return E_FAIL;

	// 'fmt'�`�����N�͏��Ȃ��Ƃ�PCMWAVEFORMAT���傫��
	if (ckIn.cksize < (LONG)sizeof(PCMWAVEFORMAT))
		return E_FAIL;

	// 'fmt'�`�����N��PCMWAVEFORMAT�ɓǂݍ���
	if (mmioRead(mmio, (HPSTR)&pcmWaveFormat, sizeof(pcmWaveFormat)) != sizeof(pcmWaveFormat))
		return E_FAIL;

	// waveformatex�����蓖�Ă�
	// pcm�t�H�[�}�b�g�łȂ���Ύ���word��ǂݍ��݁A���蓖�Ă邽�߂Ƀo�C�g���g������
	if (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM)
	{
		format = (WAVEFORMATEX*)new CHAR[sizeof(WAVEFORMATEX)];
		if (NULL == format)
			return E_FAIL;

		// pcm�\���̂���waveformatex�Ƀo�C�g���R�s�[����
		memcpy(format, &pcmWaveFormat, sizeof(pcmWaveFormat));
		format->cbSize = 0;
	}
	else
	{
		// extra bytes �̒�����ǂݍ���
		WORD cbExtraBytes = 0L;
		if (mmioRead(mmio, (CHAR*)&cbExtraBytes, sizeof(WORD)) != sizeof(WORD))
			return E_FAIL;

		format = (WAVEFORMATEX*)new CHAR[sizeof(WAVEFORMATEX) + cbExtraBytes];
		if (NULL == format)
			return E_FAIL;

		// PCM�\���̂���waveformatex�Ƀo�C�g���R�s�[����
		memcpy(format, &pcmWaveFormat, sizeof(pcmWaveFormat));
		format->cbSize = cbExtraBytes;

		// cbExtraAlloc��0����Ȃ���΍\���̂Ɋg�������o�C�g��ǂݍ���
		if (mmioRead(mmio, (CHAR*)(((BYTE*)&(format->cbSize)) + sizeof(WORD)), cbExtraBytes) != cbExtraBytes)
		{
			SAFE_DELETE(format);
			return E_FAIL;
		}
	}

	// 'fmt '�`�����N�̊O�ɖ߂�
	if (0 != mmioAscend(mmio, &ckIn, 0))
	{
		SAFE_DELETE(format);
		return E_FAIL;
	}

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::GetSize()
// Desc: wave�t�@�C���̃T�C�Y��Ԃ�
//---------------------------------------------------------------------------------------
DWORD OggOperator::GetSize()
{
	return size;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::ResetFile()
// Desc: �t�@�C���̓ǂݎ��|�C���^�[�����Z�b�g����
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
// Desc: �t�@�C�����e�����ׂăo�b�t�@�[�ɓǂݍ���
//---------------------------------------------------------------------------------------
HRESULT OggOperator::Read(BYTE* buffer, DWORD size, DWORD* readSize)
{
	if (isReadingFromMemory)
	{
		// �������p�J�[�\���`�F�b�N
		if (memoryDataCur == NULL)
			return CO_E_NOTINITIALIZED;
		// �ǂݎ�����T�C�Y�̃|�C���^������
		if (readSize != NULL)
			*readSize = 0;
		// �ǂݎ��T�C�Y���`�F�b�N
		if ((BYTE*)(memoryDataCur + size) > (BYTE*)(memoryData + memoryDataSize))
		{
			size = memoryDataSize - (DWORD)(memoryDataCur - memoryData);
		}

		// ����������ǂݎ�����t�@�C���f�[�^���o�b�t�@�[�ɃR�s�[
#pragma warning( disable: 4616 )
#pragma warning( diable: 22104 )
		CopyMemory(buffer, memoryDataCur, size);
#pragma warning( default: 22104 )
#pragma warning( default: 4616 )

		// �ǂݎ�����T�C�Y���X�V
		if (readSize != NULL)
			*readSize = size;

		return S_OK;
	}
	else
	{
		// �ǂݍ��ރo�b�t�@�[���������Ɋ��蓖��
		memset(buffer, 0, this->size);
		char *tmpBuffer = new char[this->size];
		int bitStream = 0;
		int tmpReadSize = 0;
		int comSize = 0;

		*readSize = 0;
		while (1)
		{
			tmpReadSize = ov_read(&ovf, (char*)tmpBuffer, 4096, 0, 2, 1, &bitStream);
			// �ǂݍ��݃T�C�Y�����Ă���A�t�@�C�������܂œǂݍ��݂��Ă���break
			if (comSize + tmpReadSize >= this->size || tmpReadSize == 0 || tmpReadSize == EOF)
				break;
			// �������[�̃o�b�t�@�[�Ƀ|�C���^�[�ʒu���炵�Ȃ��珑������
			memcpy(buffer + comSize, tmpBuffer, tmpReadSize);
			comSize += tmpReadSize;
		}


		// �ǂݍ��񂾃T�C�Y���Z�b�g
		*readSize = comSize;
		// ��n��
		delete[] tmpBuffer;

		return S_OK;
	}
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::GetSegment()
// Desc: �w��̃T�C�Y�܂Ŗ��߂��Z�O�����g�f�[�^���擾����
//---------------------------------------------------------------------------------------
bool OggOperator::GetSegment(char* buffer, unsigned int size, int* writeSize, bool* isEnd)
{
	// �t�@�C���̏������I����ĂȂ���ΏI��
	if (GetReady() == false)
		return false;

	// �o�b�t�@�[�̎w��`�F�b�N
	if (buffer == 0)
	{
		if (isEnd) *isEnd = true;
		if (writeSize) *writeSize = 0;
		return false;
	}

	if (isEnd) *isEnd = false;

	// ��������Ƀo�b�t�@�[�̈���m��
	memset(buffer, 0, size);
	unsigned int requestSize = OggOperatorNS::requestSize;
	int bitStream = 0;
	int readSize = 0;
	unsigned int comSize = 0;
	bool isAdjust = false; // �[���̃f�[�^�̒������t���O

	// �w��T�C�Y���\��T�C�Y��菬�����ꍇ�͒������Ƃ݂Ȃ�
	if (size < requestSize)
	{
		requestSize = size;
		isAdjust = true;
	}

	// �o�b�t�@�[���w��T�C�Y�Ŗ��߂�܂ŌJ��Ԃ�
	while (1)
	{
		readSize = ov_read(&ovf, (char*)(buffer + comSize), requestSize, 0, 2, 1, &bitStream);
		// �t�@�C���G���h�ɒB����
if (readSize == 0)
{
	// �I��
	if (isEnd) *isEnd = true;
	if (writeSize) *writeSize = comSize;
	return true;
}

// �ǂݎ��T�C�Y�����Z
comSize += readSize;

_ASSERT(comSize <= size);	// �o�b�t�@�I�[�o�[

// �o�b�t�@�𖄂ߐs������
if (comSize >= size)
{
	if (writeSize) *writeSize = comSize;
	return true;
}

// �[���f�[�^�̒���
if (size - comSize < OggOperatorNS::requestSize)
{
	isAdjust = true;
	requestSize = size - comSize;
}

	}

	// �G���[
	if (writeSize) *writeSize = 0;
	return false;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::ReadChunk()
// Desc: �t�@�C�����e���w�肳�ꂽ�����œǂݍ���Ńo�b�t�@�[�ɓ����
//---------------------------------------------------------------------------------------
HRESULT OggOperator::ReadChunk(BYTE** buffer, int bufferCount, const int len, int* readSize)
{
	if (readSize != NULL)
		*readSize = 0;

	// �t�@�C������w�肳�ꂽ�T�C�Y�����f�[�^��ǂݎ��
	bool result = GetSegment((char *)buffer[bufferCount], len, readSize, 0);
	if (result == false)
		return E_FAIL;

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::Close()
// Desc: ���\�[�X���������
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
// Desc: multimedia I/O stream�̃T�|�[�g�֐�
//       �V����wave file���쐬����
//---------------------------------------------------------------------------------------
HRESULT OggOperator::WriteMMIO(WAVEFORMATEX* format)
{
	return E_FAIL;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::Write()
// Desc: �J����wave�t�@�C���Ƀf�[�^����������
//---------------------------------------------------------------------------------------
HRESULT OggOperator::Write(UINT writeSize, BYTE* data, UINT wroteSize)
{
	return E_FAIL;
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::SetFileName()
// Desc: ��舵��ogg�t�@�C�������Z�b�g����
//---------------------------------------------------------------------------------------
void OggOperator::SetFileName(LPWSTR value)
{
	wcscpy_s(fileName, value);
}

//---------------------------------------------------------------------------------------
// Name: OggOperator::GenerateWaveFormatEx()
// Desc: �J����ogg�t�@�C���̏�񂩂�wave�w�b�_�[����
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
