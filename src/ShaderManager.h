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

#pragma once

#include <map>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <shared_mutex>
#include <unordered_set>

#include "CDataFile.h"
#include "ToggleGroup.h"


namespace ShaderToggler
{
	class ShaderManager
	{
	public:
		ShaderManager();

		void addHashHandlePair(uint32_t shaderHash, uint64_t pipelineHandle);
		void removeHandle(uint64_t handle);
		/// <summary>
		/// Switches on the hunting mode for the shader manager. It will copy the passed in hashes to the set of marked hashes
		/// </summary>
		/// <param name="currentMarkedHashes"></param>
		void startHuntingMode(const std::unordered_set<uint32_t>& currentMarkedHashes);
		/// <summary>
		/// Switches off the hunting mode for the shader manager. If cancel is false, it'll copy the set of marked shader hashes to the passed in set.
		/// </summary>
		/// <param name="cancel">if true, it'll simply ignore the marked set of hashes. If false it'll copy the marked set of hashes to the passed in set</param>
		/// <param name="markedHashesDestination">if cancel is false, it'll copy all hashes marked to this set, after clearing it first. </param>
		void stopHuntingMode(bool cancel, std::unordered_set<uint32_t>& markedHashesDestination);
		void huntNextShader();
		void huntPreviousShader();
		/// <summary>
		/// Returns true if the shader hash passed in is the currently hunted shader or it's part of the marked shader hashes
		/// </summary>
		/// <param name="shaderHash"></param>
		/// <returns></returns>
		bool isBlockedShader(uint32_t shaderHash);
		/// <summary>
		/// Returns the shader hash for the passed in pipeline handle, if found. 0 otherwise.
		/// </summary>
		/// <param name="handle"></param>
		/// <returns></returns>
		uint32_t getShaderHash(uint64_t handle);
		void addActivePipelineHandle(uint64_t handle);
		void toggleMarkOnHuntedShader();

		uint32_t getPipelineCount() {return _handleToShaderHash.size();}
		uint32_t getShaderCount() { return _shaderHashes.size();}
		uint32_t getAmountShaderHashesCollected() { return _collectedActiveShaderHashes.size(); }
		bool isInHuntingMode() { return _isInHuntingMode;}
		uint64_t getActiveHuntedShaderHash() { return _activeHuntedShaderHash;}
		int getActiveHuntedShaderIndex() { return _activeHuntedShaderIndex; }
		void toggleHideMarkedShaders() { _hideMarkedShaders=!_hideMarkedShaders;}

		bool isHuntedShaderMarked()
		{
			std::shared_lock lock(_markedShaderHashMutex);
			return _markedShaderHashes.count(_activeHuntedShaderHash)==1;
		}

		bool isKnownHandle(uint64_t pipelineHandle)
		{
			std::shared_lock lock(_hashHandlesMutex);
			return _handleToShaderHash.count(pipelineHandle)==1;
		}
		
	private:
		void setActiveHuntedShaderHandle();
		
		std::unordered_set<uint32_t> _shaderHashes;				// all shader hashes added through init pipeline
		std::map<uint64_t, uint32_t> _handleToShaderHash;		// pipeline handle per shader hash. Handle is removed when a pipeline is destroyed.
		std::unordered_set<uint32_t> _collectedActiveShaderHashes;	// shader hashes bound to pipeline handles which were collected during the collection phase after hunting was enabled, which are the pipeline handles active during the last X frames
		std::unordered_set<uint32_t> _markedShaderHashes;		// the hashes for shaders which are currently marked.

		bool _isInHuntingMode = false;
		int _activeHuntedShaderIndex = -1;
		uint32_t _activeHuntedShaderHash;
		std::shared_mutex _collectedActiveHandlesMutex;
		std::shared_mutex _hashHandlesMutex;
		std::shared_mutex _markedShaderHashMutex;
		bool _hideMarkedShaders = false;
	};
}

