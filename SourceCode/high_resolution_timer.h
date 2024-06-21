#pragma once

#include <windows.h>

class high_resolution_timer
{
public:
	high_resolution_timer()
	{
		LONGLONG counts_per_sec;
		QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&counts_per_sec));
		seconds_per_count = 1.0 / static_cast<double>(counts_per_sec);

		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		base_time = this_time;
		last_time = this_time;
	}
	~high_resolution_timer() = default;
	high_resolution_timer(const high_resolution_timer&) = delete;
	high_resolution_timer& operator=(const high_resolution_timer&) = delete;
	high_resolution_timer(high_resolution_timer&&) noexcept = delete;
	high_resolution_timer& operator=(high_resolution_timer&&) noexcept = delete;

	//Reset()が呼び出されてからの経過時間の合計を返す
	float time_stamp() const 
	{
		if (stopped)
		{
			return static_cast<float>(((stop_time - paused_time) - base_time) * seconds_per_count);
		}
		else
		{
			return static_cast<float>(((this_time - paused_time) - base_time) * seconds_per_count);
		}
	}

	float time_interval() const  
	{
		return static_cast<float>(delta_time);
	}

	void reset() //
	{
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		base_time = this_time;
		last_time = this_time;

		stop_time = 0;
		stopped = false;
	}

	void start() // 未使用時の呼び出し。
	{
		LONGLONG start_time;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_time));
		//ストップとスタートのペア間の経過時間を累積する。
		if (stopped)
		{
			paused_time += (start_time - stop_time);
			last_time = start_time;
			stop_time = 0;
			stopped = false;
		}
	}

	void stop() 
	{
		if (!stopped)
		{
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&stop_time));
			stopped = true;
		}
	}

	void tick() 
	{
		if (stopped)
		{
			delta_time = 0.0;
			return;
		}

		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time));
		// フレームと前のフレームとの時間差
		delta_time = (this_time - last_time) * seconds_per_count;

		// 次のフレームに備える。
		last_time = this_time;

		if (delta_time < 0.0)
		{
			delta_time = 0.0;
		}
	}

private:
	double seconds_per_count{ 0.0 };
	double delta_time{ 0.0 };

	LONGLONG base_time{ 0LL };
	LONGLONG paused_time{ 0LL };
	LONGLONG stop_time{ 0LL };
	LONGLONG last_time{ 0LL };
	LONGLONG this_time{ 0LL };

	bool stopped{ false };
};
