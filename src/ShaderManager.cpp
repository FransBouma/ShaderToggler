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

#include "ShaderManager.h"

using namespace reshade::api;

namespace ShaderToggler
{
	ShaderManager::ShaderManager(): _activeHuntedShaderHandle(0)
	{
	}


	void ShaderManager::addHashHandlePair(uint32_t shaderHash, uint64_t handle)
	{
		if(handle>0)
		{
			_handleToShaderHash[handle] = shaderHash;
			_shaderHashToHandle[shaderHash] = handle;
		}
	}


	void ShaderManager::removeHandle(uint64_t handle)
	{
		const auto it = _handleToShaderHash.find(handle);
		_handleToShaderHash.erase(it, _handleToShaderHash.end());
	}


	uint64_t ShaderManager::getHandle(uint32_t shaderHash)
	{
		if(_shaderHashToHandle.count(shaderHash)!=1)
		{
			return 0;
		}
		return _shaderHashToHandle.at(shaderHash);
	}


	uint32_t ShaderManager::getShaderHash(uint64_t handle)
	{
		if(_handleToShaderHash.count(handle)!=1)
		{
			return 0;
		}
		return _handleToShaderHash.at(handle);
	}


	void ShaderManager::toggleHuntingMode()
	{
		if(_isInHuntingMode)
		{
			_isInHuntingMode = false;
			_activeHuntedShaderIndex = -1;
			_activeHuntedShaderHandle = 0;
		}
		else
		{
			_isInHuntingMode = true;
			if(_handleToShaderHash.size() > 0)
			{
				_activeHuntedShaderIndex = 0;
				_activeHuntedShaderHandle = _handleToShaderHash.begin()->first;
			}
			else
			{
				_activeHuntedShaderIndex = -1;
			}
		}
	}


	void ShaderManager::setActiveHuntedShaderHandle()
	{
		auto it = _handleToShaderHash.begin();
		std::advance(it, _activeHuntedShaderIndex);
		_activeHuntedShaderHandle = it->first;
	}


	void ShaderManager::huntNextShader()
	{
		if(!_isInHuntingMode)
		{
			return;
		}
		if(_handleToShaderHash.size()<=0)
		{
			return;
		}
		if(_activeHuntedShaderIndex<_handleToShaderHash.size()-1)
		{
			_activeHuntedShaderIndex++;
		}
		else
		{
			_activeHuntedShaderIndex = 0;
		}
		setActiveHuntedShaderHandle();
	}


	void ShaderManager::huntPreviousShader()
	{
		if(!_isInHuntingMode)
		{
			return;
		}
		if(_handleToShaderHash.size()<=0)
		{
			return;
		}
		if(_activeHuntedShaderIndex<=0)
		{
			_activeHuntedShaderIndex = _handleToShaderHash.size()-1;
		}
		else
		{
			--_activeHuntedShaderIndex;
		}
		setActiveHuntedShaderHandle();
	}


	bool ShaderManager::isBlockedShader(uint64_t handle)
	{
		return _activeHuntedShaderHandle == handle;
	}
}
