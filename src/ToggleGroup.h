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

#include <string>
#include <unordered_set>

#include "CDataFile.h"
#include "KeyData.h"

namespace ShaderToggler
{
	class ToggleGroup
	{
	public:
		ToggleGroup(std::string name);

		void setToggleKey(uint8_t newKeyValue, bool shiftRequired, bool altRequired, bool ctrlRequired);
		void setToggleKey(KeyData newData);
		void setName(std::string newName);
		/// <summary>
		/// Writes the shader hashes, name and toggle key to the ini file specified, using a Group + groupCounter section.
		/// </summary>
		/// <param name="iniFile"></param>
		/// <param name="groupCounter"></param>
		void saveState(CDataFile& iniFile, int groupCounter) const;
		/// <summary>
		/// Loads the shader hashes, name and toggle key from the ini file specified, using a Group + groupCounter section.
		/// </summary>
		/// <param name="iniFile"></param>
		/// <param name="groupCounter">if -1, the ini file is in the pre-1.0 format</param>
		void loadState(CDataFile& iniFile, int groupCounter);

		/// <summary>
		/// Copies the shader hashes contained in this group to the two passed in set objects. 
		/// </summary>
		/// <param name="vertexShaderHashes"></param>
		/// <param name="pixelShaderHashes"></param>
		///	<remarks>This method won't clear the sets passed in.</remarks>
		void collectHashes(std::unordered_set<uint32_t>& vertexShaderHashes, std::unordered_set<uint32_t>& pixelShaderHashes);
		bool isBlockedVertexShader(uint32_t shaderHash);
		bool isBlockedPixelShader(uint32_t shaderHash);

		std::string getToggleKeyAsString() { return _keyData.getKeyAsString();};
		uint8_t getToggleKey() { return _keyData.getKeyCode();}
		std::string getName() { return _name;}
		void toggleActive() { _isActive = !_isActive;}
		void setEditing(bool isEditing) { _isEditing = isEditing;}
		bool isActive() { return _isActive;}
		bool isEditing() { return _isEditing;}

	private:
		std::string	_name;
		KeyData _keyData;
		std::unordered_set<uint32_t> _vertexShaderHashes;
		std::unordered_set<uint32_t> _pixelShaderHashes;
		bool _isActive;			// true means the group is actively toggled (so the hashes have to be hidden.
		bool _isEditing;		// true means the group is actively edited and the shaders are modified.
	};
}
