#include "gamepad.h"
#include <math.h>
#include <algorithm>
#include <assert.h>

static float apply_linear_deadzone(float value, float max_value, float deadzone_size)
{
	if (value < -deadzone_size)
	{
		// デッドゾーンの不連続性をなくすため、負の値を大きくする
		value += deadzone_size;
	}
	else if (value > deadzone_size)
	{
		//デッドゾーンの不連続性をなくすため、正の値を小さくする
		value -= deadzone_size;
	}
	else
	{
		return 0;
	}

	// 0～1の範囲にスケーリング
	float scaled_value = value / (max_value - deadzone_size);
	return std::max<float>(-1.f, std::min<float>(scaled_value, 1.f));
}

void gamepad::apply_stick_deadzone(float x, float y, deadzone_mode deadzone_mode, float max_value, float deadzone_size, _Out_ float& result_x, _Out_ float& result_y)
{
	switch (deadzone_mode)
	{
	case deadzone_mode::independent_axes:
		result_x = apply_linear_deadzone(x, max_value, deadzone_size);
		result_y = apply_linear_deadzone(y, max_value, deadzone_size);
		break;
	case deadzone_mode::circular:
	{
		float dist = sqrtf(x * x + y * y);
		float wanted = apply_linear_deadzone(dist, max_value, deadzone_size);

		float scale = (wanted > 0.f) ? (wanted / dist) : 0.f;

		result_x = std::max <float>(-1.f, std::min<float>(x * scale, 1.f));
		result_y = std::max<float>(-1.f, std::min<float>(y * scale, 1.f));
		break;
	}
	default: //デッドゾーン・モード::なし
		result_x = apply_linear_deadzone(x, max_value, 0);
		result_y = apply_linear_deadzone(y, max_value, 0);
		break;
	}
}

gamepad::gamepad(int user_id, float deadzonex, float deadzoney, deadzone_mode deadzone_mode) : user_id(user_id), emulation_mode(user_id == 0), deadzonex(deadzonex), deadzoney(deadzoney), _deadzone_mode(deadzone_mode)
{
	memset(&current_state, 0, sizeof(state));
	memset(&previous_state, 0, sizeof(state));

	if (emulation_mode)
	{
		const int button_keymap[_button_count] = {
			'Z'/*A*/, 'X'/*B*/, 'C'/*X*/, 'V'/*Y*/,
			VK_HOME/*LEFT_THUMB*/, VK_END/*RIGHT_THUMB*/, VK_LCONTROL/*LEFT_SHOULDER*/, VK_RCONTROL/*RIGHT_SHOULDER*/,
			VK_SPACE/*START*/, VK_BACK/*BACK*/, VK_F1/*UP*/, VK_F2/*DOWN*/, VK_F3/*LEFT*/, VK_F4/*RIGHT*/};

		for (size_t button_index = 0; button_index < _button_count; ++button_index)
		{
			button_keys[button_index] = std::make_unique<keybutton>(button_keymap[button_index]);
		}
#if 0
		thumb_stick_keys[static_cast<size_t>(side::l)][0] = std::make_unique<keybutton>('D');
		thumb_stick_keys[static_cast<size_t>(side::l)][1] = std::make_unique<keybutton>('A');
		thumb_stick_keys[static_cast<size_t>(side::l)][2] = std::make_unique<keybutton>('W');
		thumb_stick_keys[static_cast<size_t>(side::l)][3] = std::make_unique<keybutton>('S');
		thumb_stick_keys[static_cast<size_t>(side::r)][0] = std::make_unique<keybutton>(VK_RIGHT);
		thumb_stick_keys[static_cast<size_t>(side::r)][1] = std::make_unique<keybutton>(VK_LEFT);
		thumb_stick_keys[static_cast<size_t>(side::r)][2] = std::make_unique<keybutton>(VK_UP);
		thumb_stick_keys[static_cast<size_t>(side::r)][3] = std::make_unique<keybutton>(VK_DOWN);
#else
		thumb_stick_keys[static_cast<size_t>(side::r)][0] = std::make_unique<keybutton>('D');
		thumb_stick_keys[static_cast<size_t>(side::r)][1] = std::make_unique<keybutton>('A');
		thumb_stick_keys[static_cast<size_t>(side::r)][2] = std::make_unique<keybutton>('W');
		thumb_stick_keys[static_cast<size_t>(side::r)][3] = std::make_unique<keybutton>('S');
		thumb_stick_keys[static_cast<size_t>(side::l)][0] = std::make_unique<keybutton>(VK_RIGHT);
		thumb_stick_keys[static_cast<size_t>(side::l)][1] = std::make_unique<keybutton>(VK_LEFT);
		thumb_stick_keys[static_cast<size_t>(side::l)][2] = std::make_unique<keybutton>(VK_UP);
		thumb_stick_keys[static_cast<size_t>(side::l)][3] = std::make_unique<keybutton>(VK_DOWN);
#endif

		trigger_keys[static_cast<size_t>(side::l)] = std::make_unique<keybutton>(VK_LSHIFT);
		trigger_keys[static_cast<size_t>(side::r)] = std::make_unique<keybutton>(VK_RSHIFT);

	}
}
bool gamepad::acquire()
{
	XINPUT_STATE state;
	DWORD result = XInputGetState(static_cast<DWORD>(user_id), &state);
	if (result == ERROR_DEVICE_NOT_CONNECTED)
	{
		current_state.connected = false;
	}
	else
	{
		previous_state = current_state;
		memset(&current_state, 0, sizeof(current_state));
		current_state.connected = true;

		current_state.packet = state.dwPacketNumber;

		WORD button_states = state.Gamepad.wButtons;
		current_state.buttons[static_cast<size_t>(button::a)] = (button_states & XINPUT_GAMEPAD_A) != 0;
		current_state.buttons[static_cast<size_t>(button::b)] = (button_states & XINPUT_GAMEPAD_B) != 0;
		current_state.buttons[static_cast<size_t>(button::x)] = (button_states & XINPUT_GAMEPAD_X) != 0;
		current_state.buttons[static_cast<size_t>(button::y)] = (button_states & XINPUT_GAMEPAD_Y) != 0;
		current_state.buttons[static_cast<size_t>(button::left_thumb)] = (button_states & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
		current_state.buttons[static_cast<size_t>(button::right_thumb)] = (button_states & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
		current_state.buttons[static_cast<size_t>(button::left_shoulder)] = (button_states & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
		current_state.buttons[static_cast<size_t>(button::right_shoulder)] = (button_states & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
		current_state.buttons[static_cast<size_t>(button::back)] = (button_states & XINPUT_GAMEPAD_BACK) != 0;
		current_state.buttons[static_cast<size_t>(button::start)] = (button_states & XINPUT_GAMEPAD_START) != 0;

		current_state.buttons[static_cast<size_t>(button::up)] = (button_states & XINPUT_GAMEPAD_DPAD_UP) != 0;
		current_state.buttons[static_cast<size_t>(button::down)] = (button_states & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
		current_state.buttons[static_cast<size_t>(button::right)] = (button_states & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
		current_state.buttons[static_cast<size_t>(button::left)] = (button_states & XINPUT_GAMEPAD_DPAD_LEFT) != 0;

		if (_deadzone_mode == deadzone_mode::none)
		{
			current_state.triggers[static_cast<size_t>(side::l)] = apply_linear_deadzone(static_cast<float>(state.Gamepad.bLeftTrigger), 255.f, 0.f);
			current_state.triggers[static_cast<size_t>(side::r)] = apply_linear_deadzone(static_cast<float>(state.Gamepad.bRightTrigger), 255.f, 0.f);
		}
		else
		{
			current_state.triggers[static_cast<size_t>(side::l)] = apply_linear_deadzone(static_cast<float>(state.Gamepad.bLeftTrigger), 255.f, static_cast<float>(XINPUT_GAMEPAD_TRIGGER_THRESHOLD));
			current_state.triggers[static_cast<size_t>(side::r)] = apply_linear_deadzone(static_cast<float>(state.Gamepad.bRightTrigger), 255.f, static_cast<float>(XINPUT_GAMEPAD_TRIGGER_THRESHOLD));
		}

		apply_stick_deadzone(static_cast<float>(state.Gamepad.sThumbLX), static_cast<float>(state.Gamepad.sThumbLY),
			_deadzone_mode, 32767.f, static_cast<float>(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE),
			current_state.thumb_sticks[static_cast<size_t>(side::l)][static_cast<size_t>(axis::x)], current_state.thumb_sticks[static_cast<size_t>(side::l)][static_cast<size_t>(axis::y)]);

		apply_stick_deadzone(static_cast<float>(state.Gamepad.sThumbRX), static_cast<float>(state.Gamepad.sThumbRY),
			_deadzone_mode, 32767.f, static_cast<float>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE),
			current_state.thumb_sticks[static_cast<size_t>(side::r)][static_cast<size_t>(axis::x)], current_state.thumb_sticks[static_cast<size_t>(side::r)][static_cast<size_t>(axis::y)]);
	}
	return true;
}

bool gamepad::button_state(button button, trigger_mode trigger_mode) const
{
	bool button_state = false;
	if (current_state.connected)
	{
		switch (trigger_mode)
		{
		case trigger_mode::none:
			button_state = current_state.buttons[static_cast<size_t>(button)];
			break;
		case trigger_mode::rising_edge:
			button_state = !previous_state.buttons[static_cast<size_t>(button)] && current_state.buttons[static_cast<size_t>(button)];
			break;
		case trigger_mode::falling_edge:
			button_state = previous_state.buttons[static_cast<size_t>(button)] && !current_state.buttons[static_cast<size_t>(button)];
			break;
		}
	}
	if (emulation_mode && !button_state)
	{
		button_state = button_keys[static_cast<size_t>(button)]->state(trigger_mode);
	}
	return button_state;
}

float gamepad::thumb_state(side side, axis axis) const
{
	float stick_state = 0.0f;
	if (current_state.connected)
	{
		stick_state = current_state.thumb_sticks[static_cast<size_t>(side)][static_cast<size_t>(axis)];
	}
	if (emulation_mode && stick_state == 0.0f)
	{
		stick_state = axis == axis::x ? static_cast<float>(std::min<size_t>(1, thumb_stick_keys[static_cast<size_t>(side)][0]->state(trigger_mode::none))) - (std::min<size_t>(1, thumb_stick_keys[static_cast<size_t>(side)][1]->state(trigger_mode::none))) :
			static_cast<float>(std::min<size_t>(1, thumb_stick_keys[static_cast<size_t>(side)][2]->state(trigger_mode::none))) - (std::min<size_t>(1, thumb_stick_keys[static_cast<size_t>(side)][3]->state(trigger_mode::none)));
	}
	return stick_state;
}
float gamepad::trigger_state(side side) const
{
	float trigger_state = 0.0f;
	if (current_state.connected)
	{
		trigger_state = current_state.triggers[static_cast<size_t>(side)];
	}
	if (emulation_mode && trigger_state == 0)
	{
		trigger_state = trigger_keys[static_cast<size_t>(side)]->state(trigger_mode::none) ? 1.0f : 0.0f;
	}
	return trigger_state;
}
