#pragma once

#include <d3d11_1.h>
#include <wrl.h>
#include <d2d1_1.h>
#include <dwrite.h>

#include <unordered_map>
#include <string>
#include <future>

#include <functional>

enum class scene_state { awaiting, initializing, initialized, active, uninitializing, uninitialized };
class scene
{
public:
	scene() = default;
	virtual ~scene() = default;
	scene(const scene&) = delete;
	scene& operator =(const scene&) = delete;
	scene(scene&&) noexcept = delete;
	scene& operator =(scene&&) noexcept = delete;

	scene_state state() const
	{
		return state_;
	}

private:
	virtual bool initialize(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props) = 0;
	virtual void update(ID3D11DeviceContext* immediate_context, float delta_time) = 0;
	virtual void render(ID3D11DeviceContext* immediate_context, float delta_time) = 0;
	virtual bool uninitialize(ID3D11Device* device)
	{
		bool completely_regenerate = true;
		return completely_regenerate;
	}
	virtual bool on_size_changed(ID3D11Device* device, UINT64 width, UINT height)
	{
		return true;

	}

	std::atomic<scene_state> state_ = scene_state::awaiting;
	void state(scene_state state)
	{
		state_ = state;
	}

public:
	template<class _boot_scene>
	static bool _boot(ID3D11Device* device, UINT64 width, UINT height, const std::unordered_map<std::string, std::string>& props)
	{
		_current_scene = _emplace<_boot_scene>();
		_scenes.at(_current_scene)->state(scene_state::initializing);
		_scenes.at(_current_scene)->initialize(device, width, height, props);
		_scenes.at(_current_scene)->state(scene_state::initialized);
		_scenes.at(_current_scene)->state(scene_state::active);
		return true;
	}
	static bool _update(ID3D11DeviceContext* immediate_context, float delta_time);
	static void _render(ID3D11DeviceContext* immediate_context, float delta_time)
	{
		_scenes.at(_current_scene)->render(immediate_context, delta_time);
	}
	static bool _uninitialize(ID3D11Device* device)
	{
		for (decltype(_futures)::reference future : _futures)
		{
			if (future.second.valid())
			{
				future.second.wait();
			}
		}
		_scenes.at(_current_scene)->uninitialize(device);
		_scenes.clear();
		return true;
	}
	static bool _on_size_changed(ID3D11Device* device, UINT64 width, UINT height)
	{
		return _scenes.at(_current_scene)->on_size_changed(device, width, height);
	}
	template<class T>
	static std::string _emplace()
	{
		std::string class_name = typeid(T).name();
		class_name = class_name.substr(class_name.find_last_of(" ") + 1, class_name.length());

		_ASSERT_EXPR(_scenes.find(class_name) == _scenes.end(), L"'scenes' already has a scene with 'class_name'");
		_scenes.emplace(std::make_pair(class_name, std::make_unique<T>()));
		_alchemists.emplace(std::make_pair(class_name, []() { return std::make_unique<T>(); }));

		return class_name;
	}

protected:
	static bool _has_finished_preloading(const std::string& name)
	{
		if (_futures[name].valid())
		{
			if (_futures.at(name).wait_for(std::chrono::seconds(0)) == std::future_status::ready)
			{
				_futures.at(name).get();
				return name.size() > 0 ? _scenes.at(name)->state() >= scene_state::initialized : true;
			}
			return false;
		}
		return true;
	}

	static bool _transition(const std::string& next_scene, const std::unordered_map<std::string, std::string>& props)
	{
		_ASSERT_EXPR(_scenes.find(next_scene) != _scenes.end(), L"Not found scene name in scenes");

		if (!_async_wait(next_scene))
		{
			return false;
		}

		_ASSERT_EXPR(_next_scene.size() == 0, L"");
		_next_scene = next_scene;
		_props = props;

		return true;
	}

	template<class T>
	static T* _scene()
	{
		std::string class_name = typeid(T).name();
		class_name = class_name.substr(class_name.find_last_of(" ") + 1, class_name.length());
		_ASSERT_EXPR(_scenes.find(class_name) != _scenes.end(), L"Not found scene name in scenes");

		return dynamic_cast<T*>(_scenes.at(class_name).get());
	}

	static bool _async_preload_scene(ID3D11Device* device, UINT64 width, UINT height, const std::string& name);
	static bool _async_wait(const std::string& name)
	{
		if (_futures[name].valid())
		{
			if (_futures.at(name).wait_for(std::chrono::seconds(0)) == std::future_status::ready)
			{
				_futures.at(name).get();
			}
			else
			{
				return false;
			}
		}
		return true;
	}

private:
	static std::unordered_map<std::string, std::unique_ptr<scene>> _scenes;
	static std::unordered_map<std::string, std::function<std::unique_ptr<scene>()>> _alchemists;
	static std::unordered_map<std::string, std::string> _props;

	static std::string _next_scene;
	static std::string _current_scene;

	static std::unordered_map<std::string, std::future<bool>> _futures;

	static void _reemplace(std::string name)
	{
		_ASSERT_EXPR(_scenes.find(name) != _scenes.end(), L"Not found scene name in scenes");
		_scenes.erase(name);
		_scenes.emplace(std::make_pair(name, _alchemists.at(name)()));
	}
	static std::mutex _mutex;
};





