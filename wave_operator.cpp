//---------------------------------------------------------------------------------------
// File: wave_operator.cpp
//
// WAVE�t�@�C���𑀍삷�邭�炷�̎���
// Author: ninf
//---------------------------------------------------------------------------------------
#define STRICT
#include "wave_operator.h"
#undef min
#undef max

//---------------------------------------------------------------------------------------
// Name: WaveOperator::WaveOperator()
// Desc: �R���X�g���N�^�[
// �߂�΁[�̏��������s��
//---------------------------------------------------------------------------------------
WaveOperator::WaveOperator()
{
	format = NULL;
	mmio = NULL;
	size = 0;
	isReadingFromMemory = FALSE;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::~WaveOperator()
// Desc: �f�X�g���N�^�[
// �Q�Ƃ̉���Ȃǂ��s��
//---------------------------------------------------------------------------------------
WaveOperator::~WaveOperator()
{
	Close();
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Open()
// Desc: �ǂݍ��ݐ�p�Ńt�@�C�����J��
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::Open(LPWSTR strFileName, WAVEFORMATEX* format, DWORD flag, bool isBuffered)
{
	HRESULT hr;
	modeFlag = flag;

	// ���[�h�ɉ����ăt�@�C�����J��
	if (modeFlag == WAVEFILE_READ)
	{
		// �t�@�C�����`�F�b�N
		if (strFileName == NULL)
			return E_INVALIDARG;
		SAFE_DELETE_ARRAY( format );

		mmioinfo = { 0 };
		// �o�b�t�@�[�t���O�ɉ����ĊJ�����؂�ւ�
		if(isBuffered)
			mmio = mmioOpen(strFileName, &mmioinfo, MMIO_ALLOCBUF | MMIO_READ);
		else
			mmio = mmioOpen(strFileName, &mmioinfo, MMIO_READ);
		if (!mmio)
			return E_FAIL;

		// �t�@�C�����e�̓ǂݍ���
		if (FAILED(hr = ReadMMIO()))
		{
			mmioClose(mmio, 0);
			return E_FAIL;
		}
		// �t�@�C���ʒu�̃��Z�b�g
		if (FAILED(hr = ResetFile()))
			return E_FAIL;

		// �t�@�C���T�C�Y���Z�b�g����
		size = chunk.cksize;

	}
	else
	{
		return E_FAIL;
	}

	return hr;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::OpenFromMemory()
// Desc: �������[���烁���o�[�ϐ���ǂݍ���
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::OpenFromMemory(BYTE* data, ULONG dataSize, WAVEFORMATEX* format, DWORD flag)
{
	// �f�[�^���R�s�[����
	format = format;
	memoryData = data;
	memoryDataSize = dataSize;
	memoryDataCur = memoryData;
	isReadingFromMemory = TRUE;

	if (flag != WAVEFILE_READ)
		return E_NOTIMPL;
	
	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::ReadMMIO()
// Desc: multimedia I/O stream����ǂݍ��ނ��߂̊֐�
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::ReadMMIO()
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
// Name: WaveOperator::GetSize()
// Desc: wave�t�@�C���̃T�C�Y��Ԃ�
//---------------------------------------------------------------------------------------
DWORD WaveOperator::GetSize()
{
	return size;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::ResetFile()
// Desc: �t�@�C���̓ǂݎ��|�C���^�[�����Z�b�g����
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::ResetFile()
{
	if (isReadingFromMemory)
	{
		memoryDataCur = memoryData;
	}
	else
	{
		if (mmio == NULL)
			return E_FAIL;

		if (modeFlag == WAVEFILE_READ)
		{
			// �f�[�^���V�[�N����
			if (-1 == mmioSeek(mmio, riffChunk.dwDataOffset + sizeof(FOURCC), SEEK_SET))
				return E_FAIL;

			// 'data'�`�����N��T��
			chunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
			if (0 != mmioDescend(mmio, &chunk, &riffChunk, MMIO_FINDCHUNK))
				return E_FAIL;
		}
		else
		{
			return E_FAIL;
		}
	}

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Read()
// Desc: �t�@�C�����e�����ׂăo�b�t�@�[�ɓǂݍ���
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::Read(BYTE* buffer, DWORD size, DWORD* readSize)
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
		MMIOINFO mmioinfoIn; // mmio�̌��݂̃X�e�[�^�X���i�[

		if (mmio == NULL)
			return CO_E_NOTINITIALIZED;
		// �����`�F�b�N
		if (buffer == NULL || readSize == NULL)
			return E_INVALIDARG;

		*readSize = 0;

		// mmio�̏����擾
		if (0 != mmioGetInfo(mmio, &mmioinfoIn, 0))
			return E_FAIL;

		// �ǂݎ��f�[�^�̃T�C�Y�m�F
		UINT cbDataIn = size;
		if (cbDataIn > chunk.cksize)
			cbDataIn = chunk.cksize;

		chunk.cksize -= cbDataIn;

		// �t�@�C�����e��ǂݍ���
		for (DWORD cT = 0; cT < cbDataIn; cT++)
		{
			// �t�@�C���̏I��肩�`�F�b�N
			if (mmioinfoIn.pchNext == mmioinfoIn.pchEndRead)
			{
				// ���o�͗p�o�b�t�@��i�߂�
				if (0 != mmioAdvance(mmio, &mmioinfoIn, MMIO_READ))
					return E_FAIL;
				if (mmioinfoIn.pchNext == mmioinfoIn.pchEndRead)
					return E_FAIL;
			}

			// ���ۂɃt�@�C�����e���o�b�t�@�ɃR�s�[
			*((BYTE*)buffer + cT) = *((BYTE*)mmioinfoIn.pchNext);
			mmioinfoIn.pchNext++;

		}

		// �t�@�C��������������
		if (0 != mmioSetInfo(mmio, &mmioinfoIn, 0))
			return E_FAIL;

		// �ǂݍ��񂾃T�C�Y���Z�b�g
		*readSize = cbDataIn;

		return S_OK;
	}
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::ReadChunk()
// Desc: �t�@�C�����e���w�肳�ꂽ�����œǂݍ���Ńo�b�t�@�[�ɓ����
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::ReadChunk(BYTE** buffer, int bufferCount, const int len, int* readSize)
{
	if (readSize != NULL)
		*readSize = 0;

	// �t�@�C������w�肳�ꂽ�T�C�Y�����f�[�^��ǂݎ��
	*readSize = mmioRead(mmio, (HPSTR)buffer[bufferCount], len);
	if (*readSize == -1)
		return E_FAIL;

	return S_OK;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Close()
// Desc: ���\�[�X���������
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
// Desc: multimedia I/O stream�̃T�|�[�g�֐�
//       �V����wave file���쐬����
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::WriteMMIO(WAVEFORMATEX* format)
{
	return E_FAIL;
}

//---------------------------------------------------------------------------------------
// Name: WaveOperator::Write()
// Desc: �J����wave�t�@�C���Ƀf�[�^����������
//---------------------------------------------------------------------------------------
HRESULT WaveOperator::Write(UINT writeSize, BYTE* data, UINT wroteSize)
{
	return E_FAIL;
}
