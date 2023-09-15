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

#include "stdafx.h"
#include "ToggleGroup.h"
#include "KeyData.h"

namespace ShaderToggler
{
	ToggleGroup::ToggleGroup(std::string name, int id): _id(id), _isActive(false), _isEditing(false), _isActiveAtStartup(false)
	{
		_name = name.size() > 0 ? name : "Default";
	}


	int ToggleGroup::getNewGroupId()
	{
		static atomic_int s_groupId = 0;

		++s_groupId;
		return s_groupId;
	}


	void ToggleGroup::setToggleKey(uint8_t newKeyValue, bool shiftRequired, bool altRequired, bool ctrlRequired)
	{
		_keyData.setKey(newKeyValue, shiftRequired, altRequired, ctrlRequired);
	}


	void ToggleGroup::setToggleKey(KeyData newData)
	{
		if(newData.isValid())
		{
			_keyData = newData;
		}
	}


	void ToggleGroup::storeCollectedHashes(const std::unordered_set<uint32_t> pixelShaderHashes, const std::unordered_set<uint32_t> vertexShaderHashes, const std::unordered_set<uint32_t> computeShaderHashes)
	{
		clearHashes();

		for(const auto hash : vertexShaderHashes)
		{
			_vertexShaderHashes.emplace(hash);
		}
		for(const auto hash : pixelShaderHashes)
		{
			_pixelShaderHashes.emplace(hash);
		}
		for(const auto hash : computeShaderHashes)
		{
			_computeShaderHashes.emplace(hash);
		}
	}


	bool ToggleGroup::isBlockedPixelShader(uint32_t shaderHash)
	{
		return _isActive && (_pixelShaderHashes.count(shaderHash)==1);
	}


	bool ToggleGroup::isBlockedVertexShader(uint32_t shaderHash)
	{
		return _isActive && (_vertexShaderHashes.count(shaderHash) == 1);
	}


	bool ToggleGroup::isBlockedComputeShader(uint32_t shaderHash)
	{
		return _isActive && (_computeShaderHashes.count(shaderHash) == 1);
	}


	void ToggleGroup::clearHashes()
	{
		_pixelShaderHashes.clear();
		_vertexShaderHashes.clear();
		_computeShaderHashes.clear();
	}


	void ToggleGroup::setName(std::string newName)
	{
		if(newName.size()<=0)
		{
			return;
		}
		_name = newName;
	}


	void ToggleGroup::saveState(CDataFile& iniFile, int groupCounter) const
	{
		const std::string sectionRoot = "Group" + std::to_string(groupCounter);
		const std::string vertexHashesCategory = sectionRoot + "_VertexShaders";
		const std::string pixelHashesCategory = sectionRoot + "_PixelShaders";
		const std::string computeHashesCategory = sectionRoot + "_ComputeShaders";

		int counter = 0;
		for(const auto hash: _vertexShaderHashes)
		{
			iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", vertexHashesCategory);
			counter++;
		}
		iniFile.SetUInt("AmountHashes", counter, "", vertexHashesCategory);

		counter=0;
		for(const auto hash: _pixelShaderHashes)
		{
			iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", pixelHashesCategory);
			counter++;
		}
		iniFile.SetUInt("AmountHashes", counter, "", pixelHashesCategory);

		counter = 0;
		for(const auto hash : _computeShaderHashes)
		{
			iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", computeHashesCategory);
			counter++;
		}
		iniFile.SetUInt("AmountHashes", counter, "", computeHashesCategory);

		iniFile.SetValue("Name", _name, "", sectionRoot);
		iniFile.SetUInt("ToggleKey", _keyData.getKeyForIniFile(), "", sectionRoot);
		iniFile.SetBool("IsActiveAtStartup", _isActiveAtStartup, "", sectionRoot);
	}


	void ToggleGroup::loadState(CDataFile& iniFile, int groupCounter)
	{
		if(groupCounter<0)
		{
			int amount = iniFile.GetInt("AmountHashes", "PixelShaders");
			for(int i = 0;i< amount;i++)
			{
				uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "PixelShaders");
				if(hash!=UINT_MAX)
				{
					_pixelShaderHashes.emplace(hash);
				}
			}
			amount = iniFile.GetInt("AmountHashes", "VertexShaders");
			for(int i = 0;i< amount;i++)
			{
				uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "VertexShaders");
				if(hash!=UINT_MAX)
				{
					_vertexShaderHashes.emplace(hash);
				}
			}
			amount = iniFile.GetInt("AmountHashes", "ComputeShaders");
			for(int i = 0; i < amount; i++)
			{
				uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "ComputeShaders");
				if(hash != UINT_MAX)
				{
					_computeShaderHashes.emplace(hash);
				}
			}

			// done
			return;
		}

		const std::string sectionRoot = "Group" + std::to_string(groupCounter);
		const std::string vertexHashesCategory = sectionRoot + "_VertexShaders";
		const std::string pixelHashesCategory = sectionRoot + "_PixelShaders";
		const std::string computeHashesCategory = sectionRoot + "_ComputeShaders";

		int amountShaders = iniFile.GetInt("AmountHashes", vertexHashesCategory);
		for(int i = 0;i< amountShaders;i++)
		{
			uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), vertexHashesCategory);
			if(hash!=UINT_MAX)
			{
				_vertexShaderHashes.emplace(hash);
			}
		}

		amountShaders = iniFile.GetInt("AmountHashes", pixelHashesCategory);
		for(int i = 0;i< amountShaders;i++)
		{
			uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), pixelHashesCategory);
			if(hash!=UINT_MAX)
			{
				_pixelShaderHashes.emplace(hash);
			}
		}

		amountShaders = iniFile.GetInt("AmountHashes", computeHashesCategory);
		for(int i = 0; i < amountShaders; i++)
		{
			uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), computeHashesCategory);
			if(hash != UINT_MAX)
			{
				_computeShaderHashes.emplace(hash);
			}
		}

		_name = iniFile.GetValue("Name", sectionRoot);
		if(_name.size()<=0)
		{
			_name = "Default";
		}
		const uint32_t toggleKeyValue = iniFile.GetUInt("ToggleKey", sectionRoot);
		if(toggleKeyValue == UINT_MAX)
		{
			_keyData.setKey(VK_CAPITAL, false, false, false);
		}
		else
		{
			_keyData.setKeyFromIniFile(toggleKeyValue);
		}
		_isActiveAtStartup = iniFile.GetBool("IsActiveAtStartup", sectionRoot);
		_isActive = _isActiveAtStartup;
	}
}
