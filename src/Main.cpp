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
#define HASH_FILE_NAME	"ShaderToggler.ini"

#include <imgui.h>
#include <reshade.hpp>
#include "crc32_hash.hpp"
#include "ShaderManager.h"
#include "CDataFile.h"
#include "ToggleGroup.h"

using namespace reshade::api;
using namespace ShaderToggler;

extern "C" __declspec(dllexport) const char *NAME = "Shader Toggler";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Add-on which allows you to define groups of shaders to toggle on/off with one key press.";

struct __declspec(uuid("038B03AA-4C75-443B-A695-752D80797037")) CommandListDataContainer {
    uint64_t activePixelShaderPipeline;
    uint64_t activeVertexShaderPipeline;
};

static ShaderToggler::ShaderManager g_pixelShaderManager;
static ShaderToggler::ShaderManager g_vertexShaderManager;
static bool g_shaderHuntingModeActive = false;
static atomic_uint32_t g_activeCollectorFrameCounter = 0;
static std::vector<ToggleGroup> g_toggleGroups;
static atomic_int g_actionKeyBindingEditing = -1;
static KeyData g_keyCollector;

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


/// <summary>
/// Adds a default group with VK_CAPITAL as toggle key. Only used if there aren't any groups defined in the ini file.
/// </summary>
void addDefaultGroup()
{
	ToggleGroup toAdd("Default");
	toAdd.setToggleKey(VK_CAPITAL, false, false, false);
	g_toggleGroups.push_back(toAdd);
}


void loadHashFile()
{
	// Will assume it's started at the start of the application and therefore no groups are present.

	CDataFile iniFile;
	if(!iniFile.Load(HASH_FILE_NAME))
	{
		// not there
		addDefaultGroup();
		return;
	}
	int groupCounter = 0;
	const int numberOfGroups = iniFile.GetInt("AmountGroups", "General");
	if(numberOfGroups==INT_MIN)
	{
		// old format file?
		addDefaultGroup();
		groupCounter=-1;	// enforce old format read for pre 1.0 ini file.
	}
	else
	{
		for(int i=0;i<numberOfGroups;i++)
		{
			g_toggleGroups.push_back(ToggleGroup(""));
		}
	}
	for(auto& group: g_toggleGroups)
	{
		group.loadState(iniFile, groupCounter);		// groupCounter is normally 0 or greater. For when the old format is detected, it's -1 (and there's 1 group).
		groupCounter++;
	}
}


void saveHashFile()
{
	// format: first section with # of groups, then per group a section with pixel and vertex shaders, as well as their name and key value.
	// groups are stored with "Group" + group counter, starting with 0.
	CDataFile iniFile;
	iniFile.SetInt("AmountGroups", g_toggleGroups.size(), "General");

	int groupCounter = 0;
	for(const auto& group: g_toggleGroups)
	{
		group.saveState(iniFile, groupCounter);
		groupCounter++;
	}
	iniFile.SetFileName(HASH_FILE_NAME);
	iniFile.Save();
}


static void onInitCommandList(command_list *commandList)
{
	commandList->create_private_data<CommandListDataContainer>();
}


static void onDestroyCommandList(command_list *commandList)
{
	commandList->destroy_private_data<CommandListDataContainer>();
}

static void onResetCommandList(command_list *commandList)
{
	CommandListDataContainer &commandListData = commandList->get_private_data<CommandListDataContainer>();
	commandListData.activePixelShaderPipeline = -1;
	commandListData.activeVertexShaderPipeline = -1;
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
			uint32_t counterValue = g_activeCollectorFrameCounter;
			ImGui::Text("Collecting active shaders... frames to go: %d", counterValue);
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
				const auto isMarkedText = g_vertexShaderManager.isHuntedShaderMarked() ? " Shader is part of toggle group." : "";
				ImGui::Text("Current selected vertex shader: %d / %d. %s", g_vertexShaderManager.getActiveHuntedShaderIndex(), amountCollected, isMarkedText);
			}
			if(g_pixelShaderManager.isInHuntingMode())
			{
				const uint32_t amountCollected = g_pixelShaderManager.getAmountShaderHashesCollected();
				ImGui::Text("# of pixel shaders active: %d", g_pixelShaderManager.getAmountShaderHashesCollected());
				const auto isMarkedText = g_pixelShaderManager.isHuntedShaderMarked() ? " Shader is part of toggle group." : "";
				ImGui::Text("Current selected pixel shader: %d / %d. %s", g_pixelShaderManager.getActiveHuntedShaderIndex(), amountCollected, isMarkedText);
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
		const bool handleHasPixelShaderAttached = g_pixelShaderManager.isKnownHandle(pipelineHandle.handle);
		const bool handleHasVertexShaderAttached = g_vertexShaderManager.isKnownHandle(pipelineHandle.handle);
		if(!handleHasPixelShaderAttached && !handleHasVertexShaderAttached)
		{
			// draw call with unknown handle, don't collect it
			return;
		}
		CommandListDataContainer &commandListData = commandList->get_private_data<CommandListDataContainer>();
		switch(stages)
		{
			case pipeline_stage::all:
				if(g_activeCollectorFrameCounter>0)
				{
					// in collection mode
					if(handleHasPixelShaderAttached)
					{
						g_pixelShaderManager.addActivePipelineHandle(pipelineHandle.handle);
					}
					if(handleHasVertexShaderAttached)
					{
						g_vertexShaderManager.addActivePipelineHandle(pipelineHandle.handle);
					}
				}
				else
				{
					commandListData.activePixelShaderPipeline = handleHasPixelShaderAttached ? pipelineHandle.handle : -1;
					commandListData.activeVertexShaderPipeline = handleHasVertexShaderAttached ? pipelineHandle.handle : -1;
				}
				break;	
			case pipeline_stage::pixel_shader:
				if(handleHasPixelShaderAttached)
				{
					if(g_activeCollectorFrameCounter>0)
					{
						// in collection mode
						g_pixelShaderManager.addActivePipelineHandle(pipelineHandle.handle);
					}
					commandListData.activePixelShaderPipeline = pipelineHandle.handle;
				}
				break;
			case pipeline_stage::vertex_shader:
				if(handleHasVertexShaderAttached)
				{
					if(g_activeCollectorFrameCounter>0)
					{
						// in collection mode
						g_vertexShaderManager.addActivePipelineHandle(pipelineHandle.handle);
					}
					commandListData.activeVertexShaderPipeline = pipelineHandle.handle;
				}
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
	uint32_t shaderHash = g_pixelShaderManager.getShaderHash(commandListData.activePixelShaderPipeline);
	bool blockCall = g_pixelShaderManager.isBlockedShader(shaderHash);
	for(auto& group : g_toggleGroups)
	{
		blockCall |= group.isBlockedPixelShader(shaderHash);
	}
	shaderHash = g_vertexShaderManager.getShaderHash(commandListData.activePixelShaderPipeline);
	blockCall |= g_vertexShaderManager.isBlockedShader(shaderHash);
	for(auto& group : g_toggleGroups)
	{
		blockCall |= group.isBlockedVertexShader(shaderHash);
	}
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
		--g_activeCollectorFrameCounter;
	}

	for(auto& group: g_toggleGroups)
	{
		if(runtime->is_key_pressed(group.getToggleKey()))
		{
			group.toggleActive();
			if(group.isEditing())
			{
				g_vertexShaderManager.toggleHideMarkedShaders();
				g_pixelShaderManager.toggleHideMarkedShaders();
			}
		}
	}

	// hardcoded hunting keys. 
	// Numpad 1: previous pixel shader
	// Numpad 2: next pixel shader
	// Numpad 3: mark current pixel shader as part of the toggle group
	// Numpad 4: previous vertex shader
	// Numpad 5: next vertex shader
	// Numpad 6: mark current vertex shader as part of the toggle group
	if(runtime->is_key_pressed(VK_NUMPAD1))
	{
		g_pixelShaderManager.huntPreviousShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD2))
	{
		g_pixelShaderManager.huntNextShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD3))
	{
		g_pixelShaderManager.toggleMarkOnHuntedShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD4))
	{
		g_vertexShaderManager.huntPreviousShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD5))
	{
		g_vertexShaderManager.huntNextShader();
	}
	if(runtime->is_key_pressed(VK_NUMPAD6))
	{
		g_vertexShaderManager.toggleMarkOnHuntedShader();
	}
}


void endKeyBindingCapturing(bool acceptCollectedBinding, ToggleGroup& groupEditing)
{
	if (acceptCollectedBinding && g_actionKeyBindingEditing >= 0 && g_keyCollector.isValid())
	{
		groupEditing.setToggleKey(g_keyCollector);
	}
	g_actionKeyBindingEditing = -1;
	g_keyCollector.clear();
}


void startKeyBindingCapturing(int groupNumber, ToggleGroup& groupEditing)
{
	if (g_actionKeyBindingEditing == groupNumber)
	{
		return;
	}
	if (g_actionKeyBindingEditing >= 0)
	{
		endKeyBindingCapturing(false, groupEditing);
	}
	g_actionKeyBindingEditing = groupNumber;
}


static void displaySettings(reshade::api::effect_runtime *runtime)
{
	if (g_actionKeyBindingEditing >= 0)
	{
		// a keybinding is being edited. Read current pressed keys into the collector, cumulatively;
		g_keyCollector.collectKeysPressed(runtime);
	}

	ImGui::AlignTextToFramePadding();
	ImGui::Text("List of Toggle Groups ");
	ImGui::SameLine();
	if(ImGui::Button(" New "))
	{
		addDefaultGroup();
	}
	ImGui::Separator();

	int groupCounter = 1;
	std::vector<ToggleGroup> toRemove;
	for(auto& group : g_toggleGroups)
	{
		ImGui::PushID(groupCounter);
		ImGui::AlignTextToFramePadding();
		if(ImGui::Button("X"))
		{
			toRemove.push_back(group);
		}
		ImGui::SameLine();
		ImGui::Text(" %d ", groupCounter);
		ImGui::SameLine();
		if(ImGui::Button("Edit"))
		{
			group.setEditing(true);
		}

		ImGui::SameLine();
		if(ImGui::Button("Change shaders"))
		{
			// set group in the hunting mode.
		}
		ImGui::SameLine();
		ImGui::Text(" %s (%s)", group.getName().c_str() , group.getToggleKeyAsString().c_str());
		if(group.isEditing())
		{
			ImGui::Separator();
			ImGui::Text("Edit group %d", groupCounter);

			// Name of group
			char tmpBuffer[150];
			const string& name = group.getName();
			strncpy_s(tmpBuffer, 150, name.c_str(), name.size());
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.7f);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Name");
				ImGui::SameLine(ImGui::GetWindowWidth() * 0.2f);
				ImGui::InputText("##Name", tmpBuffer, 149);
				group.setName(tmpBuffer);
			ImGui::PopItemWidth();

			// Key binding of group
			bool isKeyEditing = false;
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Key shortcut");
				ImGui::SameLine(ImGui::GetWindowWidth() * 0.2f);
				string textBoxContents = (g_actionKeyBindingEditing == groupCounter) ? g_keyCollector.getKeyAsString() : group.getToggleKeyAsString();	// The 'press a key' is inside keycollector
				string toggleKeyName = group.getToggleKeyAsString();
				ImGui::InputText("##Key shortcut", (char*)textBoxContents.c_str(), textBoxContents.size(), ImGuiInputTextFlags_ReadOnly);
				if(ImGui::IsItemClicked())
				{
					startKeyBindingCapturing(groupCounter, group);
				}
				if(g_actionKeyBindingEditing==groupCounter)
				{
					isKeyEditing = true;
					ImGui::SameLine();
					if (ImGui::Button("OK"))
					{
						endKeyBindingCapturing(true, group);
					}
					ImGui::SameLine();
					if (ImGui::Button("Cancel"))
					{
						endKeyBindingCapturing(false, group);
					}
				}
			ImGui::PopItemWidth();

			if(!isKeyEditing)
			{
				if(ImGui::Button("OK"))
				{
					group.setEditing(false);
					g_actionKeyBindingEditing = -1;
					g_keyCollector.clear();
				}
			}
			ImGui::Separator();
		}
				
		ImGui::PopID();
		groupCounter++;
	}

	ImGui::Separator();
	if(g_toggleGroups.size()>0)
	{
		if(ImGui::Button("Save all Toggle Groups"))
		{
			saveHashFile();
		}
	}


	if(0)
	{
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
		reshade::register_event<reshade::addon_event::reset_command_list>(onResetCommandList);
		reshade::register_event<reshade::addon_event::destroy_pipeline>(onDestroyPipeline);
		reshade::register_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
		reshade::register_event<reshade::addon_event::reshade_present>(onReshadePresent);
		reshade::register_event<reshade::addon_event::bind_pipeline>(onBindPipeline);
		reshade::register_event<reshade::addon_event::draw>(onDraw);
		reshade::register_event<reshade::addon_event::draw_indexed>(onDrawIndexed);
		reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(onDrawOrDispatchIndirect);
		reshade::register_overlay(nullptr, &displaySettings);
		loadHashFile();
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
		reshade::unregister_event<reshade::addon_event::reset_command_list>(onResetCommandList);
		reshade::unregister_overlay(nullptr, &displaySettings);
		reshade::unregister_addon(hModule);
		break;
	}

	return TRUE;
}
