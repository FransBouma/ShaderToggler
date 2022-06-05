# ShaderToggler
Reshade 5.1+ addin to toggle game shaders on/off in groups based on a key press. It allows you to configure these groups from within the addin as well. 
This addon is meant to toggle a game's shaders on/off, not reshade effects. 

It's mainly for 64bit reshade. There's a 32bit version in the releases, but it's not actively maintained/tested. 

## How to use
Place the `ShaderToggler.addon` in the same folder as where the game exe is located. This is in most cases the same folder as where the Reshade 5.1+ dll
is located. Only for games which use Vulkan, the Reshade dll is likely elsewhere. For Unreal Engine powered games there might be two
game exe's: one in the game's installation folder, and one in a folder deeper into that folder, e.g. 
`GameName\Binaries\Win64\GameName-Win64-Shipping.exe`; the shader toggler addon has to be in that second folder, in our example:
`GameName\Binaries\Win64`. Reshade has to be placed in that folder as well.

Be sure to use the Reshade version which supports Addons (so the unsigned version). When you start your game, the `Addons` tab in 
the Reshade gui should show the Shader Toggler information and controls. 

To obtain the unsigned Reshade version, go to <https://reshade.me> and click on *Download*, then on the grey button to download the ReShade version with Add-on support. 

To create a toggle for a set of shaders, open the reshade gui and go to the addon's tab -> Shader Toggler area. Then click 
the `New` button to create a new *Toggle group*. By default the new toggle group has as name `Default` and as toggle key 
`Caps Lock`. To change these, click the `Edit` button of the Toggle Group. You can then change the name of the toggle group
and also the keyboard shortcut. The keyboard shortcut nor the name have to be unique. The keyboard shortcut can be 
any key on your keyboard, optionally in combination of using `Alt`, `Control`, and `Shift`. 

A toggle group needs shaders assigned to it, that's done with marking shaders below.

## Marking Shaders
To successfully be able to mark a set of shaders to toggle, be sure the elements you want to toggle, so the elements
using the shaders, are visible, e.g. a menu or a hud or a certain effect. After you've made sure the elements to toggle are
visible, click the `Change Shaders` button of the toggle group. This will start the 'Shader hunting' phase for the particular 
toggle group. 

You should see the shader overlay in the top left corner with the information you need. By default it waits a certain amount
of frames (which you can configure in the reshade overlay) to see which shaders are currently active. This is to avoid having
to walk through potentially thousands of shaders which aren't currently used. 

After the frames have been collected, it has enough information to allow you to browse the shaders. 

To walk the available pixel shaders, use the `Numpad 1` and `Numpad 2` keys. If an element disappears, the shader that's 
currently 'active' is rendering these elements, so if you want to hide these elements, press `Numpad 3`. The shader is then 
marked, and part of the toggle group. Press `Numpad 3` again to remove it from the group.

To walk all shaders you already marked in the current group, you can hold down `Ctrl` and press `Numpad 1` and `Numpad 2` to quickly
move back/forth through the shaders in a group, e.g. when you made a mistake and you want to unmark a shader. 

To walk the available vertex shaders, instead use the `Numpad 4` and `Numpad 5` keys. To mark a vertex shader to be 
part of the toggle group, press `Numpad 6`.

Same for vertex shaders: to walk all shaders you already marked in the current group, you can hold down `Ctrl` and press `Numpad 4` and `Numpad 5` 
to quickly move back/forth through the shaders in a group. 

To test your current group, press the toggle key you assigned to the group. When you're done, click the 'Done' button in the 
reshade overlay for the particular toggle group. 

To re-use this information the next time you run the game, click the Save toggle group button. This will write an ini file 
(`ShaderToggler.ini`) with the information to create the set of shaders to toggle next time you start the game. This file is
located in the same folder as `ShaderToggler.addon`.
