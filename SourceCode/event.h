#pragma once

#include <vector>
#include <unordered_map>
#include <map>
#include <string>
#include <functional>
#include <mutex>

#include <variant>
#include <cassert>

#include "scene.h"

using arguments = std::vector<std::variant<std::string, float, void*>>;

class event
{
	friend class scene;
	static std::mutex _mutex;
	static std::unordered_multimap<std::string, std::function<void(const arguments&)>> _handlers;

public:
	//イベントの登録
	static void _bind(const char* type, const std::function<void(const arguments&)>& handler)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_handlers.emplace(std::make_pair(type, handler));
	}
	//イベントの発行
	static void _dispatch(const char* type, const arguments& args)
	{
		assert(_handlers.find(type) != _handlers.end() && "No handler is registered for this event type.");
		std::pair<decltype(_handlers)::iterator, decltype(_handlers)::iterator> paired_it = _handlers.equal_range(type);
		for (decltype(_handlers)::iterator it = paired_it.first; it != paired_it.second; ++it)
		{
			it->second(args);
		}
	}
	//イベントの削除
	static void _erase(const char* type)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_handlers.erase(type);
	}
#if 0
	static void _exterminate()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_handlers.clear();
	}
#endif
};

