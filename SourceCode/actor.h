#pragma once

#include <directxmath.h>
#include <mutex>
#include <string>
#include <cassert>
#include <unordered_map>

class actor
{
public:
	const std::string name;
	actor(const char* name) : name(name) {};
	virtual ~actor() = default;
	actor(const actor&) = delete;
	actor& operator =(const actor&) = delete;
	actor(actor&&) noexcept = delete;
	actor& operator =(actor&&) noexcept = delete;

	const DirectX::XMFLOAT4& position() const { return _position; };
	const DirectX::XMFLOAT4& rotation() const { return _rotation; };
	const DirectX::XMFLOAT4X4& transform() const { return _composed_transform; };
	const DirectX::XMFLOAT4& scale() const { return _scale; };
protected:
	DirectX::XMFLOAT4 _scale = { 1, 1, 1, 1 };
	DirectX::XMFLOAT4 _position = { 0, 0, 0, 1 };
	DirectX::XMFLOAT4 _rotation = { 0, 0, 0, 1 };
#if 1
	void uniform_scaling(float scale)
	{
		_scale.x = _scale.y = _scale.z = scale;
	}
#endif
	virtual DirectX::XMFLOAT4X4 _compose_transform(float to_meters_scale = 0.01f, size_t coordinate_system_transform = 0)
	{
		const DirectX::XMFLOAT4X4 coordinate_system_transforms[] = {
		{ -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },	// 0:RHS Y-UP
		{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },		// 1:LHS Y-UP
		{ 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },		// 2:RHS Z-UP
		{ -1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },	// 3:LHS Z-UP
		};
		DirectX::XMMATRIX C = DirectX::XMLoadFloat4x4(&coordinate_system_transforms[coordinate_system_transform]);
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(_scale.x * to_meters_scale, _scale.y * to_meters_scale, _scale.z * to_meters_scale);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(_rotation.x, _rotation.y, _rotation.z);
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(_position.x, _position.y, _position.z);
		DirectX::XMStoreFloat4x4(&_composed_transform, C * S * R * T);
		return _composed_transform;
	}
private:
	DirectX::XMFLOAT4X4 _composed_transform;

private:
	friend class scene;
	static std::unordered_map<std::string, std::shared_ptr<actor>> _actors;
	static std::mutex _mutex;
public:
	static std::shared_ptr<actor> _at(const char* name)
	{
		assert(_actors.find(name) != _actors.end() && "An actor by 'name' doesn't exist.");
		return _actors.at(name);
	}

	template <class T>
	static std::shared_ptr<T> _at(const char* name)
	{
		assert(_actors.find(name) != _actors.end() && "An actor by 'name' doesn't exist.");
		return std::dynamic_pointer_cast<T>(_actors.at(name));
	}

	template <class T, class... A>
	static std::shared_ptr<T> _emplace(const char* name, A... args)
	{
		assert(_actors.find(name) == _actors.end() && "An actor by 'name' already exists.");

		std::lock_guard<std::mutex> lock(_mutex);
		_actors.emplace(std::make_pair(name, std::make_shared<T>(name, args...)));
		return std::dynamic_pointer_cast<T>(_actors.at(name));
	}

	static void _erase(const char* name)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_actors.erase(name);
	}

#if 0
	static void _exterminate()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_actors.clear();
	}
	static void _erase_if_orphan()
	{
		for (std::unordered_map<std::string, std::shared_ptr<actor>>::iterator it = _actors.begin(); it != _actors.end();) {
			int x = it->second.use_count();
			if (it->second.use_count() == 1)
			{
				std::lock_guard<std::mutex> lock(_mutex);
				it = _actors.erase(it);
			}
			else {
				++it;
			}
		}
	}
#endif
};
