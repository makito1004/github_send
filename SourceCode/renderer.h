#pragma once

#include <d3d11.h>
#include <unordered_map>
#include "geometric_substance.h"

class renderer
{
	std::unordered_map<std::string, std::unique_ptr<geometric_substance>> geometric_substances;

public:
	void emplace(ID3D11Device* device, const std::string& name)
	{
		geometric_substances.emplace(name, std::make_unique<geometric_substance>(device, name.c_str()));
	}

	void drawcall(ID3D11DeviceContext* immediate_context);
};
