# ShaderToggler
Reshade 5+ addin to toggle shaders on/off in groups based on a key press. It allows you to configure these groups from within the addin as well.

## Current state
The current state is a proof-of-work addon which can build a single group and toggle it using a hard-coded key (`CAPS_LOCK`).

## How to use

To create a toggle for a set of shaders, open the reshade gui and go to the addon's tab -> Shader Toggler area. Then check the
`Show OSD Info` checkbox. This will display the actual # of shaders it has found.

To create a group of shaders to toggle, be sure the elements you want to toggle are visible, e.g. a menu or a hud or dof. Then open the
reshade gui and check the `Enable shader hunting mode` checkbox in the Shader Toggler area on the addons tab. After 250 frames it has enough
information to allow you to browse the shaders. The next section will describe how to do that.

### Marking shaders

To walk the available pixel shaders, use the `Numpad 1` and `Numpad 2` keys. If an element disappears, the shader that's currently 'active' is 
rendering these elements, so if you want to hide these elements, press `Numpad 3`. The shader is then *marked*, and part of the toggle group. Press `Numpad 3` again
to remove it from the group. 

To walk the available vertex shaders, instead use the `Numpad 4` and `Numpad 5` keys. To mark a vertex shader to be part of the toggle group, press `Numpad 6`. 

To test your current group, press `CAPS_LOCK`. When you're done, click the `Save toggle group` button. This will write an ini file with the information
to create the set of shaders to toggle next time you start the game. It's in the file `ShaderToggler.ini`. 