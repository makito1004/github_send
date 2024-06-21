#include "scene.h"

#include <chrono>

std::string scene::_current_scene;
std::string scene::_next_scene;

std::unordered_map<std::string, std::unique_ptr<scene>> scene::_scenes;
std::unordered_map<std::string, std::function<std::unique_ptr<scene>()>> scene::_alchemists;
std::unordered_map<std::string, std::string> scene::_props;

std::unordered_map<std::string, std::future<bool>> scene::_futures;

std::mutex scene::_mutex;

#include "event.h"
#include "actor.h"

#include "geometric_substance.h"
#include "texture.h"
#include "shader.h"
#include "audio.h"

struct debris
{
	std::unordered_multimap<std::string, std::string> names;
	template <class T>
	void amass(const T& data, const std::string& scene_name)
	{
		for (T::const_reference it : data)
		{
			names.emplace(scene_name, it.first);
		}
	}
	template <class T>
	void erase(const std::string& scene_name)
	{
		for (std::unordered_multimap<std::string, std::string>::iterator it = names.find(scene_name); it != names.end(); ++it)
		{
			T::_erase(it->second.c_str());
		}
		names.erase(scene_name);
	}
};
debris _actor_names;
debris _event_types;

bool scene::_update(ID3D11DeviceContext* immediate_context, float delta_time)
{
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	immediate_context->GetDevice(device.GetAddressOf());

	D3D11_VIEWPORT viewport;
	UINT num_viewports{ 1 };
	immediate_context->RSGetViewports(&num_viewports, &viewport);

	_ASSERT_EXPR(_current_scene.size() > 0, L"current_scene is always required.");
	_scenes.at(_current_scene)->update(immediate_context, delta_time);

	bool renderable = true;
	if (_next_scene.size() > 0)
	{
		if (_current_scene.size() > 0)
		{
			_scenes.at(_current_scene)->state(scene_state::uninitializing);
			bool completely_regenerate = _scenes.at(_current_scene)->uninitialize(device.Get());
			_scenes.at(_current_scene)->state(scene_state::uninitialized);
			if (completely_regenerate)
			{
#if 1
				_reemplace(_current_scene);
				// exterminate resources
				geometric_substance::_exterminate();
				texture::_exterminate();
				shader<ID3D11VertexShader>::_exterminate();
				shader<ID3D11DomainShader>::_exterminate();
				shader<ID3D11HullShader>::_exterminate();
				shader<ID3D11GeometryShader>::_exterminate();
				shader<ID3D11PixelShader>::_exterminate();
				shader<ID3D11ComputeShader>::_exterminate();
				audio::_exterminate();
#endif
			}
			_scenes.at(_current_scene)->state(scene_state::awaiting);
		}
		if (_scenes.at(_next_scene)->state() < scene_state::initializing)
		{
			_scenes.at(_next_scene)->state(scene_state::initializing);
			_scenes.at(_next_scene)->initialize(device.Get(), static_cast<UINT64>(viewport.Width), static_cast<UINT64>(viewport.Height), _props);
			_scenes.at(_next_scene)->state(scene_state::initialized);
		}
		_scenes.at(_next_scene)->state(scene_state::active);



		_actor_names.erase<actor>(_current_scene);
		_event_types.erase<event>(_current_scene);

		_actor_names.amass(actor::_actors, _next_scene);
		_event_types.amass(event::_handlers, _next_scene);

		_scenes.at(_next_scene)->state(scene_state::active);

		_current_scene = _next_scene;
		_next_scene.clear();
		_props.clear();

		renderable = false;
	}
	return renderable;
}

bool scene::_async_preload_scene(ID3D11Device* device, UINT64 width, UINT height, const std::string& name)
{
	_ASSERT_EXPR(_scenes.find(name) != _scenes.end(), L"Not found scene name in scenes");

	if (!_futures[name].valid())
	{
		if (_scenes.at(name)->state() == scene_state::awaiting)
		{
			_futures.at(name) = std::async(std::launch::async, [device, name, width, height]() {
				_scenes.at(name)->state(scene_state::initializing);
				bool success = _scenes.at(name)->initialize(device, width, height, {});
				_scenes.at(name)->state(scene_state::initialized);
				return success;
				});
		}
	}
	return _scenes.at(name)->state() > scene_state::initializing;
}


