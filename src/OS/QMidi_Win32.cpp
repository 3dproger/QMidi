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

QMap<QString, QString> QMidiOut::devices()
{
	QMap<QString, QString> ret;

	int numDevs = midiOutGetNumDevs();
	if (numDevs == 0)
		return ret;

	for (int i = 0; i < numDevs; i++) {
		MIDIOUTCAPSW devCaps;
        MMRESULT result = midiOutGetDevCapsW(i, &devCaps, sizeof(MIDIOUTCAPSW));

        if (result == MMSYSERR_BADDEVICEID)
        {
            qWarning("%s: %s", Q_FUNC_INFO, "The specified device identifier is out of range.");
        }
        else if (result == MMSYSERR_INVALPARAM)
        {
            qWarning("%s: %s", Q_FUNC_INFO, "The specified pointer or structure is invalid.");
        }
        else if (result == MMSYSERR_NODRIVER)
        {
            qWarning("%s: %s", Q_FUNC_INFO, "The driver is not installed.");
        }
        else if (result == MMSYSERR_NOMEM)
        {
            qWarning("%s: %s", Q_FUNC_INFO, "The system is unable to load mapper string description.");
        }
        else if (result != MMSYSERR_NOERROR)
        {
            qWarning("%s: Unknown error. Result:  %d", Q_FUNC_INFO, result);
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

    MMRESULT result = midiOutOpen(&fMidiPtrs->midiOut, outDeviceId.toInt(), 0, 0, CALLBACK_NULL);

    if (result == MIDIERR_NODEVICE) {
        qWarning("%s: %s", Q_FUNC_INFO, "No MIDI port was found. This error occurs only when the mapper is opened.");
    }
    else if (result == MMSYSERR_ALLOCATED) {
        qWarning("%s: %s", Q_FUNC_INFO, "The specified resource is already allocated.");
    }
    else if (result == MMSYSERR_BADDEVICEID) {
        qWarning("%s: %s", Q_FUNC_INFO, "The specified device identifier is out of range.");
    }
    else if (result == MMSYSERR_INVALPARAM) {
        qWarning("%s: %s", Q_FUNC_INFO, "The specified pointer or structure is invalid.");
    }
    else if (result == MMSYSERR_NOMEM) {
        qWarning("%s: %s", Q_FUNC_INFO, "The system is unable to allocate or lock memory.");
    }
    else if (result != MMSYSERR_NOERROR)
    {
        qWarning("%s: Unknown error. Result:  %d", Q_FUNC_INFO, result);
    }

	fDeviceId = outDeviceId;
    fConnected = true;

    return true;
}

void QMidiOut::disconnect()
{
	if (!fConnected)
		return;

    MMRESULT result = midiOutClose(fMidiPtrs->midiOut);

    if (result == MIDIERR_STILLPLAYING)
    {
        qWarning("%s: %s", Q_FUNC_INFO, "Buffers are still in the queue.");
    }
    else if (result == MMSYSERR_INVALHANDLE)
    {
        qWarning("%s: %s", Q_FUNC_INFO, "The specified device handle is invalid.");
    }
    else if (result == MMSYSERR_NOMEM)
    {
        qWarning("%s: %s", Q_FUNC_INFO, "The system is unable to load mapper string description.");
    }
    else if (result != MMSYSERR_NOERROR)
    {
        qWarning("%s: Unknown error. Result:  %d", Q_FUNC_INFO, result);
    }

	fConnected = false;

	delete fMidiPtrs;
	fMidiPtrs = NULL;
}

void QMidiOut::sendMsg(qint32 msg)
{
	if (!fConnected)
		return;

    MMRESULT result = midiOutShortMsg(fMidiPtrs->midiOut, (DWORD)msg);

    if (result == MIDIERR_BADOPENMODE)
    {
        qWarning("%s: %s", Q_FUNC_INFO, "The application sent a message without a status byte to a stream handle.");
    }
    else if (result == MIDIERR_NOTREADY)
    {
        qWarning("%s: %s", Q_FUNC_INFO, "The hardware is busy with other data.");
    }
    else if (result == MMSYSERR_INVALHANDLE)
    {
        qWarning("%s: %s", Q_FUNC_INFO, "The specified device handle is invalid.");
    }
    else if (result != MMSYSERR_NOERROR)
    {
        qWarning("%s: Unknown error. Result:  %d", Q_FUNC_INFO, result);
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

	// TODO: check for retval of midiOutPrepareHeader
	midiOutPrepareHeader(fMidiPtrs->midiOut, &header, sizeof(MIDIHDR));

	midiOutLongMsg(fMidiPtrs->midiOut, &header, sizeof(MIDIHDR));

	while (midiOutUnprepareHeader(fMidiPtrs->midiOut, &header, sizeof(MIDIHDR)) == MIDIERR_STILLPLAYING);
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
		midiInGetDevCapsW(i, &devCaps, sizeof(MIDIINCAPSW));
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

		// Prepare the midi header to be reused -- what's the worst that could happen?
		midiInUnprepareHeader(hMidiIn, midiHeader, sizeof(MIDIHDR));
		midiInPrepareHeader(hMidiIn, midiHeader, sizeof(MIDIHDR));
		midiInAddBuffer(hMidiIn, midiHeader, sizeof(MIDIHDR));
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

	fDeviceId = inDeviceId;
	midiInOpen(&fMidiPtrs->midiIn,
		inDeviceId.toInt(),
		reinterpret_cast<DWORD_PTR>(&QMidiInProc),
		reinterpret_cast<DWORD_PTR>(this),
		CALLBACK_FUNCTION | MIDI_IO_STATUS);

	memset(&fMidiPtrs->header, 0, sizeof(MIDIHDR));
	fMidiPtrs->header.lpData = new char[512];  // 512 bytes ought to be enough for everyone
	fMidiPtrs->header.dwBufferLength = 512;
	midiInPrepareHeader(fMidiPtrs->midiIn, &fMidiPtrs->header, sizeof(MIDIHDR));
	midiInAddBuffer(fMidiPtrs->midiIn, &fMidiPtrs->header, sizeof(MIDIHDR));

	fConnected = true;
	return true;
}

void QMidiIn::disconnect()
{
	if (!fConnected)
		return;

	delete fMidiPtrs->header.lpData;
	midiInClose(fMidiPtrs->midiIn);
	fConnected = false;
	delete fMidiPtrs;
	fMidiPtrs = nullptr;
}

void QMidiIn::start()
{
	if (!fConnected)
		return;

	midiInStart(fMidiPtrs->midiIn);
}

void QMidiIn::stop()
{
	if (!fConnected)
		return;

	midiInStop(fMidiPtrs->midiIn);
}
