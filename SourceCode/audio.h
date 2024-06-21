#pragma once

#include <xaudio2.h>
#include <wrl.h>
#include <mmreg.h>

#include <unordered_map>
#include <mutex>
#include <string>
#include <cassert>

#include "misc.h"

class audio
{
	WAVEFORMATEXTENSIBLE wfx = { 0 };
	XAUDIO2_BUFFER buffer = { 0 };
	IXAudio2SourceVoice* source_voice = NULL;

public:
	audio(IXAudio2* xaudio2, const wchar_t* filename);
	~audio();
	audio(const audio&) = delete;
	audio& operator =(const audio&) = delete;
	audio(audio&&) noexcept = delete;
	audio& operator =(audio&&) noexcept = delete;

	void play(int loop_count = 0);
	void stop(bool play_tails = true, size_t after_samples_played = 0);
	void volume(float volume);
	bool queuing();

private:
	class device
	{
		friend class audio;
	public:
		IXAudio2* _xaudio2;
		IXAudio2MasteringVoice* _master_voice;
	private:
		device()
		{
			HRESULT hr;
			hr = XAudio2Create(&_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

#if defined(DEBUG) | defined(_DEBUG)
			XAUDIO2_DEBUG_CONFIGURATION xaudio2_debug_configuration = {};
			xaudio2_debug_configuration.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
			xaudio2_debug_configuration.BreakMask = XAUDIO2_LOG_ERRORS;
			_xaudio2->SetDebugConfiguration(&xaudio2_debug_configuration, NULL);
#endif
			hr = _xaudio2->CreateMasteringVoice(&_master_voice);
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		}
	};

public:
	static const device _device()
	{
		static audio::device singleton;
		return singleton;
	}


private:
	static std::unordered_map<std::wstring, std::shared_ptr<audio>> _audios;
	static std::mutex _mutex;

public:
	static std::shared_ptr<audio> _emplace(const wchar_t* name)
	{
		if (_audios.find(name) == _audios.end())
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_audios.emplace(name, std::make_shared<audio>(audio::_device()._xaudio2, name));
		}
		return _audios.at(name);
	}
#if 0
	static std::shared_ptr<audio> at(const wchar_t* name)
	{
		return _audios.at(name);
	}
	static void _erase(const wchar_t* name)
	{
		assert(_audios.find(name) != _audios.end() && "An audio by 'name' doesn't exist.");
		_audios.erase(name);
	}
#endif
	static void _exterminate()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_audios.clear();
	}
};

