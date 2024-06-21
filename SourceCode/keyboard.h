#pragma once
#include <windows.h>
#include <cstdint>

enum class trigger_mode { none, rising_edge, falling_edge };
class keybutton
{
	int vkey;
	int current_state;
	int previous_state;
public:
	keybutton(int vkey) : vkey(vkey), current_state(0), previous_state(0)
	{

	}
	virtual ~keybutton() = default;
	keybutton(keybutton&) = delete;
	keybutton& operator=(keybutton&) = delete;

	// Signal edge
	bool state(trigger_mode trigger_mode)
	{
		previous_state = current_state;
		if (static_cast<USHORT>(GetAsyncKeyState(vkey)) & 0x8000)
		{
			current_state++;
		}
		else
		{
			current_state = 0;
		}
		if (trigger_mode == trigger_mode::rising_edge)
		{
			return previous_state == 0 && current_state > 0;
		}
		else if (trigger_mode == trigger_mode::falling_edge)
		{
			return previous_state > 0 && current_state == 0;
		}
		else
		{
			return current_state > 0;
		}
	}
};