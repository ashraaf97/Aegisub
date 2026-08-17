// Copyright (c) 2019, Qirui Wang
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file audio_player_xaudio2.cpp
/// @brief XAudio2-based audio output
/// @ingroup audio_output
///
/// XAudio2 ships with the Windows SDK, so unlike the DirectSound backends it
/// needs no external SDK to build. It supersedes DirectSound, which Microsoft
/// deprecated; the DirectSound players are kept as a fallback.

#ifdef WITH_XAUDIO2
#include "include/aegisub/audio_player.h"

#include "options.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/log.h>
#include <libaegisub/scoped_ptr.h>

#include <algorithm>
#include <memory>
#include <process.h>
#include <vector>
#include <xaudio2.h>

namespace {
class XAudio2Thread;

/// @class XAudio2Player
/// @brief XAudio2-based audio player
///
/// The core design idea is to have a playback thread that performs all
/// playback operations, and use the player object as a proxy to send commands
/// to the playback thread.
class XAudio2Player final : public AudioPlayer {
	/// The playback thread
	std::unique_ptr<XAudio2Thread> thread;

	/// Desired length in milliseconds to write ahead of the playback cursor
	int WantedLatency;

	/// Multiplier for WantedLatency to get total buffer length
	int BufferLength;

	/// @brief Tell whether playback thread is alive
	/// @return True if there is a playback thread and it's ready
	bool IsThreadAlive();

public:
	XAudio2Player(agi::AudioProvider *provider);
	~XAudio2Player() = default;

	void Play(int64_t start, int64_t count) override;
	void Stop() override;
	bool IsPlaying() override;

	int64_t GetEndPosition() override;
	int64_t GetCurrentPosition() override;
	void SetEndPosition(int64_t pos) override;
	void SetVolume(double vol) override;
};

/// @brief RAII support class to init and de-init the COM library
struct COMInitialization {
	bool inited = false;

	~COMInitialization() {
		if (inited) CoUninitialize();
	}

	/// @brief Initialise COM as a single-threaded apartment if we have not already
	bool Init() {
		if (!inited && SUCCEEDED(CoInitialize(nullptr)))
			inited = true;
		return inited;
	}
};

/// @brief RAII wrapper around Win32 HANDLE type
struct Win32KernelHandle final : public agi::scoped_holder<HANDLE, BOOL(__stdcall *)(HANDLE)> {
	Win32KernelHandle(HANDLE handle = 0) : scoped_holder(handle, CloseHandle) {}

	Win32KernelHandle& operator=(HANDLE new_handle) {
		scoped_holder::operator=(new_handle);
		return *this;
	}
};

/// @class XAudio2Thread
/// @brief Playback thread class for XAudio2Player
///
/// Not based on wxThread, but uses Win32 threads directly
class XAudio2Thread : public IXAudio2VoiceCallback {
	/// @brief Win32 thread entry point
	static unsigned int __stdcall ThreadProc(void *parameter);
	/// @brief Thread entry point
	void Run();

	/// @brief Check for error state and throw exception if one occurred
	void CheckError();

	/// Win32 handle to the thread
	Win32KernelHandle thread_handle;

	/// Event object, world to thread, set to start playback
	Win32KernelHandle event_start_playback;

	/// Event object, world to thread, set to stop playback
	Win32KernelHandle event_stop_playback;

	/// Event object, world to thread, set if playback end time was updated
	Win32KernelHandle event_update_end_time;

	/// Event object, world to thread, set if the volume was changed
	Win32KernelHandle event_set_volume;

	/// Event object, thread to itself, set when a buffer has been consumed
	Win32KernelHandle event_buffer_end;

	/// Event object, world to thread, set if the thread should end as soon as possible
	Win32KernelHandle event_kill_self;

	/// Event object, thread to world, set when the thread has entered its main loop
	Win32KernelHandle thread_running;

	/// Event object, thread to world, set when playback is ongoing
	Win32KernelHandle is_playing;

	/// Event object, thread to world, set if an error state has occurred
	Win32KernelHandle error_happened;

	/// Statically allocated error message text describing reason for error_happened being set
	const char *error_message = nullptr;

	/// Playback volume, 1.0 is "unchanged"
	double volume = 1.0;

	/// Audio frame to start playback at
	int64_t start_frame = 0;

	/// Audio frame to end playback at
	int64_t end_frame = 0;

	/// Desired length in milliseconds to write ahead of the playback cursor
	int wanted_latency;

	/// Multiplier for wanted_latency to get total buffer length
	int buffer_length;

	/// System millisecond timestamp of last playback start, used to calculate playback position
	ULONGLONG last_playback_restart = 0;

	/// Audio provider to take sample data from
	agi::AudioProvider *provider;

	/// Buffer occupied indicator. Written by the XAudio2 callback thread and
	/// read by the playback thread; std::vector<bool> is not used here since
	/// its proxy references are not safe to write concurrently.
	std::vector<char> buffer_occupied;

public:
	/// @brief Constructor, creates and starts playback thread
	/// @param provider      Audio provider to take sample data from
	/// @param WantedLatency Desired length in milliseconds to write ahead of the playback cursor
	/// @param BufferLength  Multiplier for WantedLatency to get total buffer length
	XAudio2Thread(agi::AudioProvider *provider, int WantedLatency, int BufferLength);
	~XAudio2Thread();

	// IXAudio2VoiceCallback
	void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 BytesRequired) override {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
	void STDMETHODCALLTYPE OnStreamEnd() override {}
	void STDMETHODCALLTYPE OnBufferStart(void *pBufferContext) override {}
	void STDMETHODCALLTYPE OnBufferEnd(void *pBufferContext) override {
		intptr_t i = reinterpret_cast<intptr_t>(pBufferContext);
		buffer_occupied[i] = 0;
		SetEvent(event_buffer_end);
	}
	void STDMETHODCALLTYPE OnLoopEnd(void *pBufferContext) override {}
	void STDMETHODCALLTYPE OnVoiceError(void *pBufferContext, HRESULT Error) override {}

	void Play(int64_t start, int64_t count);
	void Stop();

	/// @brief Change audio playback end point
	///
	/// Playback stops instantly if new_end_frame is before the current playback position
	void SetEndFrame(int64_t new_end_frame);

	void SetVolume(double new_volume);
	bool IsPlaying();

	/// @brief Get approximate current audio frame being heard by the user
	/// @return Audio frame index, or 0 if not playing
	int64_t GetCurrentFrame();

	int64_t GetEndFrame();

	/// @brief Tell whether playback thread has died
	bool IsDead();
};

unsigned int __stdcall XAudio2Thread::ThreadProc(void *parameter) {
	static_cast<XAudio2Thread *>(parameter)->Run();
	return 0;
}

/// Macro used to set error_message, error_happened and end the thread
#define REPORT_ERROR(msg) \
	{ \
		ResetEvent(is_playing); \
		error_message = "XAudio2Thread: " msg; \
		SetEvent(error_happened); \
		return; \
	}

void XAudio2Thread::Run() {
	COMInitialization COM_library;
	if (!COM_library.Init())
		REPORT_ERROR("Could not initialise COM")

	IXAudio2 *pXAudio2 = nullptr;
	IXAudio2SourceVoice *pSourceVoice = nullptr;
	HRESULT hr;
	if (FAILED(hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
		REPORT_ERROR("Failed initializing XAudio2")

	IXAudio2MasteringVoice *pMasterVoice = nullptr;
	if (FAILED(hr = pXAudio2->CreateMasteringVoice(&pMasterVoice)))
		REPORT_ERROR("Failed initializing XAudio2 MasteringVoice")

	// Describe the wave format. AudioProviderFactory guarantees that anything
	// reaching a player has already been converted to 16 bit signed mono PCM
	// at 32kHz or above, so no format fallback is needed here.
	WAVEFORMATEX wfx;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nSamplesPerSec = provider->GetSampleRate();
	wfx.nChannels = provider->GetChannels();
	wfx.wBitsPerSample = provider->GetBytesPerSample() * 8;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize = 0;

	if (FAILED(hr = pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx, 0, 2.0f, this)))
		REPORT_ERROR("Failed initializing XAudio2 SourceVoice")

	// Now we're ready to roll!
	SetEvent(thread_running);
	bool running = true;

	HANDLE events_to_wait[] = {
		event_start_playback,
		event_stop_playback,
		event_update_end_time,
		event_set_volume,
		event_buffer_end,
		event_kill_self
	};

	int64_t next_input_frame = 0;
	bool playback_should_be_running = false;
	const int wanted_frames = wanted_latency * wfx.nSamplesPerSec / 1000;
	const DWORD wanted_latency_bytes = wanted_frames * wfx.nBlockAlign;
	std::vector<std::vector<BYTE>> buff(buffer_length);
	for (auto &i : buff)
		i.resize(wanted_latency_bytes);

	while (running) {
		DWORD wait_result = WaitForMultipleObjects(sizeof(events_to_wait) / sizeof(HANDLE), events_to_wait, FALSE, INFINITE);

		switch (wait_result) {
		case WAIT_OBJECT_0 + 0:
			// Start or restart playback
			pSourceVoice->Stop();
			pSourceVoice->FlushSourceBuffers();

			next_input_frame = start_frame;
			playback_should_be_running = true;
			pSourceVoice->Start();
			SetEvent(is_playing);
			goto do_fill_buffer;

		case WAIT_OBJECT_0 + 1:
		stop_playback:
			// Stop playing
			ResetEvent(is_playing);
			pSourceVoice->Stop();
			pSourceVoice->FlushSourceBuffers();
			playback_should_be_running = false;
			break;

		case WAIT_OBJECT_0 + 2:
			// Set end frame
			if (end_frame <= next_input_frame)
				goto stop_playback;
			goto do_fill_buffer;

		case WAIT_OBJECT_0 + 3:
			// Change volume
			pSourceVoice->SetVolume(static_cast<float>(volume));
			break;

		case WAIT_OBJECT_0 + 4:
			// A buffer was consumed; top the queue back up
		do_fill_buffer:
			if (!playback_should_be_running)
				break;

			for (int i = 0; i < buffer_length; ++i) {
				if (buffer_occupied[i])
					continue;

				int fill_len = static_cast<int>(std::min<int64_t>(end_frame - next_input_frame, wanted_frames));
				if (fill_len <= 0)
					break;

				buffer_occupied[i] = 1;
				provider->GetAudio(buff[i].data(), next_input_frame, fill_len);
				next_input_frame += fill_len;

				XAUDIO2_BUFFER xbf;
				xbf.Flags = next_input_frame == end_frame ? XAUDIO2_END_OF_STREAM : 0;
				xbf.AudioBytes = fill_len * wfx.nBlockAlign;
				xbf.pAudioData = buff[i].data();
				xbf.PlayBegin = 0;
				xbf.PlayLength = 0;
				xbf.LoopBegin = 0;
				xbf.LoopLength = 0;
				xbf.LoopCount = 0;
				xbf.pContext = reinterpret_cast<void *>(static_cast<intptr_t>(i));
				if (FAILED(hr = pSourceVoice->SubmitSourceBuffer(&xbf)))
					REPORT_ERROR("Failed submitting sample buffer")
			}
			break;

		case WAIT_OBJECT_0 + 5:
			// Perform suicide
			running = false;
			pSourceVoice->Stop();
			pSourceVoice->FlushSourceBuffers();
			pSourceVoice->DestroyVoice();
			pMasterVoice->DestroyVoice();
			pXAudio2->Release();
			ResetEvent(is_playing);
			playback_should_be_running = false;
			break;

		default:
			REPORT_ERROR("Something bad happened while waiting on events in playback loop, either the wait failed or an event object was abandoned.")
			break;
		}
	}
}

#undef REPORT_ERROR

void XAudio2Thread::CheckError() {
	try {
		switch (WaitForSingleObject(error_happened, 0)) {
		case WAIT_OBJECT_0:
			throw error_message;

		case WAIT_ABANDONED:
			throw "The XAudio2Thread error signal event was abandoned, somehow. This should not happen.";

		case WAIT_FAILED:
			throw "Failed checking state of XAudio2Thread error signal event.";

		case WAIT_TIMEOUT:
		default:
			return;
		}
	}
	catch (...) {
		ResetEvent(is_playing);
		ResetEvent(thread_running);
		throw;
	}
}

XAudio2Thread::XAudio2Thread(agi::AudioProvider *provider, int WantedLatency, int BufferLength)
: event_start_playback(CreateEvent(0, FALSE, FALSE, 0))
, event_stop_playback(CreateEvent(0, FALSE, FALSE, 0))
, event_update_end_time(CreateEvent(0, FALSE, FALSE, 0))
, event_set_volume(CreateEvent(0, FALSE, FALSE, 0))
, event_buffer_end(CreateEvent(0, FALSE, FALSE, 0))
, event_kill_self(CreateEvent(0, FALSE, FALSE, 0))
, thread_running(CreateEvent(0, TRUE, FALSE, 0))
, is_playing(CreateEvent(0, TRUE, FALSE, 0))
, error_happened(CreateEvent(0, FALSE, FALSE, 0))
, wanted_latency(WantedLatency)
, buffer_length(std::min(BufferLength, static_cast<int>(XAUDIO2_MAX_QUEUED_BUFFERS)))
, provider(provider)
, buffer_occupied(std::min(BufferLength, static_cast<int>(XAUDIO2_MAX_QUEUED_BUFFERS)), 0)
{
	if (!(thread_handle = (HANDLE)_beginthreadex(0, 0, ThreadProc, this, 0, 0)))
		throw AudioPlayerOpenError("Failed creating playback thread in XAudio2Player. This is bad.");

	HANDLE running_or_error[] = { thread_running, error_happened };
	switch (WaitForMultipleObjects(2, running_or_error, FALSE, INFINITE)) {
	case WAIT_OBJECT_0:
		// running, all good
		return;

	case WAIT_OBJECT_0 + 1:
		// error happened, we fail
		throw AudioPlayerOpenError(error_message ? error_message : "Failed wait for thread start or thread error in XAudio2Player. This is bad.");

	default:
		throw AudioPlayerOpenError("Failed wait for thread start or thread error in XAudio2Player. This is bad.");
	}
}

XAudio2Thread::~XAudio2Thread() {
	SetEvent(event_kill_self);
	WaitForSingleObject(thread_handle, INFINITE);
}

void XAudio2Thread::Play(int64_t start, int64_t count) {
	CheckError();

	start_frame = start;
	end_frame = start + count;
	SetEvent(event_start_playback);

	last_playback_restart = GetTickCount64();

	// Block until playback actually begins to avoid race conditions with
	// checking if playback is in progress
	HANDLE events_to_wait[] = { is_playing, error_happened };
	switch (WaitForMultipleObjects(2, events_to_wait, FALSE, INFINITE)) {
	case WAIT_OBJECT_0 + 0: // Playing
		LOG_D("audio/player/xaudio2") << "Playback begun";
		break;
	case WAIT_OBJECT_0 + 1: // Error
		throw error_message;
	default:
		throw agi::InternalError("Unexpected result from WaitForMultipleObjects in XAudio2Thread::Play");
	}
}

void XAudio2Thread::Stop() {
	CheckError();
	SetEvent(event_stop_playback);
}

void XAudio2Thread::SetEndFrame(int64_t new_end_frame) {
	CheckError();
	end_frame = new_end_frame;
	SetEvent(event_update_end_time);
}

void XAudio2Thread::SetVolume(double new_volume) {
	CheckError();
	volume = new_volume;
	SetEvent(event_set_volume);
}

bool XAudio2Thread::IsPlaying() {
	CheckError();

	switch (WaitForSingleObject(is_playing, 0)) {
	case WAIT_ABANDONED:
		throw "The XAudio2Thread playback state event was abandoned, somehow. This should not happen.";

	case WAIT_FAILED:
		throw "Failed checking state of XAudio2Thread playback state event.";

	case WAIT_OBJECT_0:
		return true;

	case WAIT_TIMEOUT:
	default:
		return false;
	}
}

int64_t XAudio2Thread::GetCurrentFrame() {
	CheckError();
	if (!IsPlaying()) return 0;
	ULONGLONG milliseconds_elapsed = GetTickCount64() - last_playback_restart;
	return start_frame + milliseconds_elapsed * provider->GetSampleRate() / 1000;
}

int64_t XAudio2Thread::GetEndFrame() {
	CheckError();
	return end_frame;
}

bool XAudio2Thread::IsDead() {
	switch (WaitForSingleObject(thread_running, 0)) {
	case WAIT_OBJECT_0:
		return false;
	default:
		return true;
	}
}

XAudio2Player::XAudio2Player(agi::AudioProvider *provider) : AudioPlayer(provider) {
	// The buffer will hold BufferLength times WantedLatency milliseconds of audio
	WantedLatency = OPT_GET("Player/Audio/XAudio2/Buffer Latency")->GetInt();
	BufferLength = OPT_GET("Player/Audio/XAudio2/Buffer Length")->GetInt();

	// sanity checking
	if (WantedLatency <= 0)
		WantedLatency = 100;
	if (BufferLength <= 0)
		BufferLength = 5;

	try {
		thread = std::make_unique<XAudio2Thread>(provider, WantedLatency, BufferLength);
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
		throw AudioPlayerOpenError(msg);
	}
}

bool XAudio2Player::IsThreadAlive() {
	if (thread && thread->IsDead())
		thread.reset();
	return static_cast<bool>(thread);
}

void XAudio2Player::Play(int64_t start, int64_t count) {
	try {
		thread->Play(start, count);
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
	}
}

void XAudio2Player::Stop() {
	try {
		if (IsThreadAlive()) thread->Stop();
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
	}
}

bool XAudio2Player::IsPlaying() {
	try {
		if (!IsThreadAlive()) return false;
		return thread->IsPlaying();
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
		return false;
	}
}

int64_t XAudio2Player::GetEndPosition() {
	try {
		if (!IsThreadAlive()) return 0;
		return thread->GetEndFrame();
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
		return 0;
	}
}

int64_t XAudio2Player::GetCurrentPosition() {
	try {
		if (!IsThreadAlive()) return 0;
		return thread->GetCurrentFrame();
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
		return 0;
	}
}

void XAudio2Player::SetEndPosition(int64_t pos) {
	try {
		if (IsThreadAlive()) thread->SetEndFrame(pos);
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
	}
}

void XAudio2Player::SetVolume(double vol) {
	try {
		if (IsThreadAlive()) thread->SetVolume(vol);
	}
	catch (const char *msg) {
		LOG_E("audio/player/xaudio2") << msg;
	}
}
}

std::unique_ptr<AudioPlayer> CreateXAudio2Player(agi::AudioProvider *provider, wxWindow *) {
	return std::make_unique<XAudio2Player>(provider);
}

#endif // WITH_XAUDIO2
