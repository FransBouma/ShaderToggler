///////////////////////////////////////////////////////////////////////
//
// Part of ShaderToggler, a shader toggler add on for Reshade 5+ which allows you
// to define groups of shaders to toggle them on/off with one key press
// 
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/ShaderToggler
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////

#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define ImTextureID unsigned long long // Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle
#define FRAMECOUNT_COLLECTION_PHASE 250;

#include <imgui.h>
#include <reshade.hpp>
#include "crc32_hash.hpp"
#include <map>
#include "ShaderManager.h"

using namespace reshade::api;

extern "C" __declspec(dllexport) const char *NAME = "Shader Toggler";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Add-on which allows you to define groups of shaders to toggle on/off with one key press.";

struct __declspec(uuid("038B03AA-4C75-443B-A695-752D80797037")) CommandListDataContainer {
    uint64_t activePixelShaderPipeline;
    uint64_t activeVertexShaderPipeline;
};

static ShaderToggler::ShaderManager g_pixelShaderManager;
static ShaderToggler::ShaderManager g_vertexShaderManager;
static bool g_shaderHuntingModeActive = false;
static uint32_t g_activeCollectorFrameCounter = 0;

#ifdef _DEBUG
static bool g_osdInfoVisible = true;
#else
static bool g_osdInfoVisible = false;
#endif


static uint32_t calculateShaderHash(void* shaderData)
{
	if(nullptr==shaderData)
	{
		return 0;
	}

	const auto shaderDesc = *static_cast<shader_desc *>(shaderData);
	return compute_crc32(static_cast<const uint8_t *>(shaderDesc.code), shaderDesc.code_size);
}


static void onInitCommandList(command_list *commandList)
{
	commandList->create_private_data<CommandListDataContainer>();
}


static void onDestroyCommandList(command_list *commandList)
{
	commandList->destroy_private_data<CommandListDataContainer>();
}



static void onInitPipeline(device *device, pipeline_layout, uint32_t subobjectCount, const pipeline_subobject *subobjects, pipeline pipelineHandle)
{
	// shader has been created, we will now create a hash and store it with the handle we got.
	for (uint32_t i = 0; i < subobjectCount; ++i)
	{
		switch (subobjects[i].type)
		{
		case pipeline_subobject_type::vertex_shader:
			{
				g_vertexShaderManager.addHashHandlePair(calculateShaderHash(subobjects[i].data), pipelineHandle.handle);
			}
			break;
		case pipeline_subobject_type::pixel_shader:
			{
				g_pixelShaderManager.addHashHandlePair(calculateShaderHash(subobjects[i].data), pipelineHandle.handle);
			}
			break;
		}
	}
}


static void onDestroyPipeline(device *device, pipeline pipelineHandle)
{
	g_pixelShaderManager.removeHandle(pipelineHandle.handle);
	g_vertexShaderManager.removeHandle(pipelineHandle.handle);
}


static void onReshadeOverlay(reshade::api::effect_runtime *runtime)
{
	if(g_osdInfoVisible)
	{
		ImGui::SetNextWindowPos(ImVec2(10, 10));
		if (!ImGui::Begin("ShaderTogglerInfo", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | 
												  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::End();
			return;
		}
		const ImVec4 foregroundColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, foregroundColor);
		ImGui::Text("# of pipelines with vertex shaders: %d. # of different vertex shaders gathered: %d.", g_vertexShaderManager.getPipelineCount(), g_vertexShaderManager.getShaderCount());
		ImGui::Text("# of pipelines with pixel shaders: %d. # of different pixel shaders gathered: %d.", g_pixelShaderManager.getPipelineCount(), g_pixelShaderManager.getShaderCount());
		if(g_activeCollectorFrameCounter > 0)
		{
			ImGui::Text("Collecting active shaders... frames to go: %d", g_activeCollectorFrameCounter);
		}
		else
		{
			if(g_vertexShaderManager.isInHuntingMode() || g_pixelShaderManager.isInHuntingMode())
			{
				ImGui::Text("In hunting mode");
			}
			if(g_vertexShaderManager.isInHuntingMode())
			{
				const uint32_t amountCollected = g_vertexShaderManager.getAmountShaderHashesCollected();
				ImGui::Text("# of vertex shaders active: %d", g_vertexShaderManager.getAmountShaderHashesCollected());
				ImGui::Text("Current selected vertex shader: %d / %d", g_vertexShaderManager.getActiveHuntedShaderIndex(), amountCollected);
			}
			if(g_pixelShaderManager.isInHuntingMode())
			{
				const uint32_t amountCollected = g_pixelShaderManager.getAmountShaderHashesCollected();
				ImGui::Text("# of pixel shaders active: %d", g_pixelShaderManager.getAmountShaderHashesCollected());
				ImGui::Text("Current selected pixel shader: %d / %d", g_pixelShaderManager.getActiveHuntedShaderIndex(), amountCollected);
			}
		}
		ImGui::PopStyleColor();
		ImGui::End();
	}
}


static void onBindPipeline(command_list* commandList, pipeline_stage stages, pipeline pipelineHandle)
{
	if(nullptr!=commandList && pipelineHandle.handle!=0)
	{
		CommandListDataContainer &commandListData = commandList->get_private_data<CommandListDataContainer>();
		switch(stages)
		{
			case pipeline_stage::all:
			case pipeline_stage::all_graphics:
				if(g_activeCollectorFrameCounter>0)
				{
					// in collection mode
					g_pixelShaderManager.addActivePipelineHandle(pipelineHandle.handle);
					g_vertexShaderManager.addActivePipelineHandle(pipelineHandle.handle);
				}
				commandListData.activePixelShaderPipeline = pipelineHandle.handle;
				commandListData.activeVertexShaderPipeline = pipelineHandle.handle;
				break;	
			case pipeline_stage::pixel_shader:
				if(g_activeCollectorFrameCounter>0)
				{
					// in collection mode
					g_pixelShaderManager.addActivePipelineHandle(pipelineHandle.handle);
				}
				commandListData.activePixelShaderPipeline = pipelineHandle.handle;
				break;
			case pipeline_stage::vertex_shader:
				if(g_activeCollectorFrameCounter>0)
				{
					// in collection mode
					g_vertexShaderManager.addActivePipelineHandle(pipelineHandle.handle);
				}
				commandListData.activeVertexShaderPipeline = pipelineHandle.handle;
				break;
		}
	}
}


bool blockDrawCallForCommandList(command_list* commandList)
{
	if(nullptr==commandList)
	{
		return false;
	}

	const CommandListDataContainer &commandListData = commandList->get_private_data<CommandListDataContainer>();
	bool blockCall = g_pixelShaderManager.isBlockedShader(commandListData.activePixelShaderPipeline);
	blockCall |= g_vertexShaderManager.isBlockedShader(commandListData.activeVertexShaderPipeline);
	return blockCall;
}


static bool onDraw(command_list* commandList, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
{
	// check if for this command list the active shader handles are part of the blocked set. If so, return true
	return blockDrawCallForCommandList(commandList);
}


static bool onDrawIndexed(command_list* commandList, uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance)
{
	// same as onDraw
	return blockDrawCallForCommandList(commandList);
}


static bool onDrawOrDispatchIndirect(command_list* commandList, indirect_command type, resource buffer, uint64_t offset, uint32_t draw_count, uint32_t stride)
{
	switch(type)
	{
		case indirect_command::unknown:
		case indirect_command::draw:
		case indirect_command::draw_indexed: 
			// same as OnDraw
			return blockDrawCallForCommandList(commandList);
		// the rest aren't blocked processed
	}
	// no blocking for compute shaders.
	return false;
}


static void onReshadePresent(effect_runtime* runtime)
{
	if(g_activeCollectorFrameCounter>0)
	{
		g_activeCollectorFrameCounter--;
	}

	// handle key presses. For now hardcoded, pixel shaders only
	// Keys:
	// Numpad 1: previous pixel shader
	// Numpad 2: next pixel shader
	// Numpad 4: previous vertex shader
	// Numpad 5: next vertex shader
	if(runtime->is_key_pressed(VK_NUMPAD1))
	{
		g_pixelShaderManager.huntPreviousShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD2))
	{
		g_pixelShaderManager.huntNextShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD4))
	{
		g_vertexShaderManager.huntPreviousShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD5))
	{
		g_vertexShaderManager.huntNextShader();
	}
}


static void displaySettings(reshade::api::effect_runtime *runtime)
{
	ImGui::Checkbox("Show OSD Info", &g_osdInfoVisible);
	if(ImGui::Checkbox("Enable shader hunting mode", &g_shaderHuntingModeActive))
	{
		g_pixelShaderManager.toggleHuntingMode();
		g_vertexShaderManager.toggleHuntingMode();
		if(g_activeCollectorFrameCounter<=0 && (g_pixelShaderManager.isInHuntingMode() || g_vertexShaderManager.isInHuntingMode()))
		{
			// set the counter, so it'll run down to 0 before hunting can start.
			g_activeCollectorFrameCounter = FRAMECOUNT_COLLECTION_PHASE;
		}
	}
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!reshade::register_addon(hModule))
		{
			return FALSE;
		}
		reshade::register_event<reshade::addon_event::init_pipeline>(onInitPipeline);
		reshade::register_event<reshade::addon_event::init_command_list>(onInitCommandList);
		reshade::register_event<reshade::addon_event::destroy_command_list>(onDestroyCommandList);
		reshade::register_event<reshade::addon_event::destroy_pipeline>(onDestroyPipeline);
		reshade::register_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
		reshade::register_event<reshade::addon_event::reshade_present>(onReshadePresent);
		reshade::register_event<reshade::addon_event::bind_pipeline>(onBindPipeline);
		reshade::register_event<reshade::addon_event::draw>(onDraw);
		reshade::register_event<reshade::addon_event::draw_indexed>(onDrawIndexed);
		reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(onDrawOrDispatchIndirect);
		reshade::register_overlay(nullptr, &displaySettings);
		break;
	case DLL_PROCESS_DETACH:
		reshade::unregister_event<reshade::addon_event::reshade_present>(onReshadePresent);
		reshade::unregister_event<reshade::addon_event::destroy_pipeline>(onDestroyPipeline);
		reshade::unregister_event<reshade::addon_event::init_pipeline>(onInitPipeline);
		reshade::unregister_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
		reshade::unregister_event<reshade::addon_event::bind_pipeline>(onBindPipeline);
		reshade::unregister_event<reshade::addon_event::draw>(onDraw);
		reshade::unregister_event<reshade::addon_event::draw_indexed>(onDrawIndexed);
		reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(onDrawOrDispatchIndirect);
		reshade::unregister_event<reshade::addon_event::init_command_list>(onInitCommandList);
		reshade::unregister_event<reshade::addon_event::destroy_command_list>(onDestroyCommandList);
		reshade::unregister_overlay(nullptr, &displaySettings);
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
