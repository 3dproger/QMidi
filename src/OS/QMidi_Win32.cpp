/*
 * Copyright 2012-2016 Augustin Cavalier <waddlesplash>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "QMidiOut.h"
#include "QMidiIn.h"
#include "QMidiFile.h"

#include <QStringList>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

// # pragma mark - QMidiOut

struct NativeMidiOutInstances {
	HMIDIOUT midiOut;
};

// TODO: error reporting
inline void showWinApiError(const char* tag, long long int code)
{
    const DWORD size = 1024;
    WCHAR buffer[size];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), buffer, size, NULL);

    if (strlen(tag) > 0)
    {
        qWarning(QString("%1: ERROR (%2): %3")
                 .arg(tag).arg(code).arg(buffer).toUtf8());
    }
    else
    {
        qWarning(QString("%1: ERROR (%2): %3")
                 .arg(__FILE__).arg(code).arg(buffer).toUtf8());
    }
}

QMap<QString, QString> QMidiOut::devices()
{
	QMap<QString, QString> ret;

	int numDevs = midiOutGetNumDevs();
	if (numDevs == 0)
		return ret;

	for (int i = 0; i < numDevs; i++) {
		MIDIOUTCAPSW devCaps;

        MMRESULT winApiResult = midiOutGetDevCapsW(i, &devCaps, sizeof(MIDIOUTCAPSW));
        if (MMSYSERR_NOERROR != winApiResult) {
            showWinApiError(Q_FUNC_INFO, winApiResult);
        }

		ret.insert(QString::number(i), QString::fromWCharArray(devCaps.szPname));
	}

	return ret;
}

bool QMidiOut::connect(QString outDeviceId)
{
	if (fConnected)
		disconnect();
	fMidiPtrs = new NativeMidiOutInstances;

    MMRESULT winApiResult = midiOutOpen(&fMidiPtrs->midiOut, outDeviceId.toInt(), 0, 0, CALLBACK_NULL);
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

	fDeviceId = outDeviceId;
    fConnected = true;

    return true;
}

void QMidiOut::disconnect()
{
	if (!fConnected)
		return;

    MMRESULT winApiResult = midiOutClose(fMidiPtrs->midiOut);
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

	fConnected = false;

	delete fMidiPtrs;
	fMidiPtrs = NULL;
}

void QMidiOut::sendMsg(qint32 msg)
{
	if (!fConnected)
		return;

    MMRESULT winApiResult = midiOutShortMsg(fMidiPtrs->midiOut, (DWORD)msg);
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }
}

void QMidiOut::sendSysEx(const QByteArray &data)
{
	if (!fConnected)
		return;

	MIDIHDR header;
	memset(&header, 0, sizeof(MIDIHDR));

	header.lpData = (LPSTR) data.data();
	header.dwBufferLength = data.length();

    MMRESULT winApiResult;

	// TODO: check for retval of midiOutPrepareHeader
    winApiResult = midiOutPrepareHeader(fMidiPtrs->midiOut, &header, sizeof(MIDIHDR));
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

    winApiResult = midiOutLongMsg(fMidiPtrs->midiOut, &header, sizeof(MIDIHDR));
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

    winApiResult = midiOutUnprepareHeader(fMidiPtrs->midiOut, &header, sizeof(MIDIHDR));
    while (winApiResult == MIDIERR_STILLPLAYING);
    if (winApiResult != MIDIERR_STILLPLAYING && winApiResult != MMSYSERR_NOERROR) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }
}

// # pragma mark - QMidiIn

struct NativeMidiInInstances {
	//! \brief midiIn is a reference to the MIDI input device
	HMIDIIN midiIn;
	//! \brief header is a prepared MIDI header, used for receiving
	//! MIM_LONGDATA (System Exclusive) messages.
	MIDIHDR header;
};

QMap<QString, QString> QMidiIn::devices()
{
	QMap<QString, QString> ret;

	unsigned int numDevs = midiInGetNumDevs();
	if (numDevs == 0)
		return ret;

	for (unsigned int i = 0; i < numDevs; i++) {
		MIDIINCAPSW devCaps;
        MMRESULT winApiResult = midiInGetDevCapsW(i, &devCaps, sizeof(MIDIINCAPSW));
        if (MMSYSERR_NOERROR != winApiResult) {
            showWinApiError(Q_FUNC_INFO, winApiResult);
        }
		ret.insert(QString::number(i), QString::fromWCharArray(devCaps.szPname));
	}

	return ret;
}

static void CALLBACK QMidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	QMidiIn* self = reinterpret_cast<QMidiIn*>(dwInstance);
	switch (wMsg)
	{
	case MIM_OPEN:
	case MIM_CLOSE:
		break;
	case MIM_DATA:
		emit(self->midiEvent(static_cast<quint32>(dwParam1), static_cast<quint32>(dwParam2)));
		break;
	case MIM_LONGDATA:
	{
		auto midiHeader = reinterpret_cast<MIDIHDR*>(dwParam1);
		auto rawData = QByteArray(midiHeader->lpData, static_cast<int>(midiHeader->dwBytesRecorded));
		emit(self->midiSysExEvent(rawData));

        MMRESULT winApiResult;

		// Prepare the midi header to be reused -- what's the worst that could happen?
        winApiResult = midiInUnprepareHeader(hMidiIn, midiHeader, sizeof(MIDIHDR));
        if (MMSYSERR_NOERROR != winApiResult) {
            showWinApiError(Q_FUNC_INFO, winApiResult);
        }

        winApiResult = midiInPrepareHeader(hMidiIn, midiHeader, sizeof(MIDIHDR));
        if (MMSYSERR_NOERROR != winApiResult) {
            showWinApiError(Q_FUNC_INFO, winApiResult);
        }

        winApiResult = midiInAddBuffer(hMidiIn, midiHeader, sizeof(MIDIHDR));
        if (MMSYSERR_NOERROR != winApiResult) {
            showWinApiError(Q_FUNC_INFO, winApiResult);
        }
		break;
	}
	default:
        qWarning("%s: no handler for message %d", Q_FUNC_INFO, wMsg);
	}
}

bool QMidiIn::connect(QString inDeviceId)
{
	if (fConnected)
		disconnect();
	fMidiPtrs = new NativeMidiInInstances;

    MMRESULT winApiResult;

	fDeviceId = inDeviceId;
    winApiResult = midiInOpen(&fMidiPtrs->midiIn,
		inDeviceId.toInt(),
		reinterpret_cast<DWORD_PTR>(&QMidiInProc),
		reinterpret_cast<DWORD_PTR>(this),
		CALLBACK_FUNCTION | MIDI_IO_STATUS);
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

	memset(&fMidiPtrs->header, 0, sizeof(MIDIHDR));
	fMidiPtrs->header.lpData = new char[512];  // 512 bytes ought to be enough for everyone
	fMidiPtrs->header.dwBufferLength = 512;
    winApiResult = midiInPrepareHeader(fMidiPtrs->midiIn, &fMidiPtrs->header, sizeof(MIDIHDR));
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

    winApiResult = midiInAddBuffer(fMidiPtrs->midiIn, &fMidiPtrs->header, sizeof(MIDIHDR));
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

	fConnected = true;
	return true;
}

void QMidiIn::disconnect()
{
	if (!fConnected)
		return;

	delete fMidiPtrs->header.lpData;
    MMRESULT winApiResult = midiInClose(fMidiPtrs->midiIn);
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }

	fConnected = false;
	delete fMidiPtrs;
	fMidiPtrs = nullptr;
}

void QMidiIn::start()
{
	if (!fConnected)
		return;

    MMRESULT winApiResult = midiInStart(fMidiPtrs->midiIn);
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }
}

void QMidiIn::stop()
{
	if (!fConnected)
		return;

    MMRESULT winApiResult = midiInStop(fMidiPtrs->midiIn);
    if (MMSYSERR_NOERROR != winApiResult) {
        showWinApiError(Q_FUNC_INFO, winApiResult);
    }
}
