#pragma once

#include <Windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput9_1_0.lib")

#include <memory>
#include "keyboard.h"


class gamepad
{
private:
	enum class side { l, r };
	enum class axis { x, y };
	enum class deadzone_mode { independent_axes, circular, none };
public:
	enum class button { a, b, x, y, left_thumb, right_thumb, left_shoulder, right_shoulder, start, back, up, down, left, right, end };
private:
	static const size_t _button_count = static_cast<size_t>(button::end);

private:
	int user_id = 0;
	struct state
	{
		DWORD packet;
		bool connected = false;
		bool buttons[_button_count];
		float triggers[2]; //[side]
		float thumb_sticks[2][2]; //[side][axis]
	};
	state current_state;
	state previous_state;


	bool emulation_mode = false; //ゲームパッドが接続されていない場合、キーボードが使用されるが、user_id = 0に限定する
	std::unique_ptr<keybutton> button_keys[_button_count];
	std::unique_ptr<keybutton> thumb_stick_keys[2][4]; 
	std::unique_ptr<keybutton> trigger_keys[2]; 

public:
	gamepad(int user_id = 0, float deadzonex = 0.05f, float deadzoney = 0.02f, deadzone_mode deadzone_mode = deadzone_mode::independent_axes);
	virtual ~gamepad() = default;
	gamepad(gamepad const&) = delete;
	gamepad& operator=(gamepad const&) = delete;


	bool acquire();

	bool button_state(button button, trigger_mode trigger_mode = trigger_mode::rising_edge) const;
	
	float thumb_state_lx() const { return thumb_state(side::l, axis::x); }
	float thumb_state_ly() const { return thumb_state(side::l, axis::y); }
	float thumb_state_rx() const { return thumb_state(side::r, axis::x); }
	float thumb_state_ry() const { return thumb_state(side::r, axis::y); }

	float trigger_state_l()const { return trigger_state(side::l); }
	float trigger_state_r()const { return trigger_state(side::r); }

private:
	float thumb_state(side side, axis axis) const;
	float trigger_state(side side)const;

	float deadzonex = 0.05f;
	float deadzoney = 0.02f;
	deadzone_mode _deadzone_mode = deadzone_mode::none;
	void apply_stick_deadzone(float x, float y, deadzone_mode deadzone_mode, float max_value, float deadzone_size, _Out_ float& result_x, _Out_ float& result_y);
};

