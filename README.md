# hyprRiver
River layouts for Hyprland

This plugin implements the river-layout-v3 protocol for Hyprland. 
This was mostly a proof of concept/because I can think. It _is_ usable, with the following limitations(?)

- You cannot have any workspace specific layout settings. Hyrpland and River do not use the same workspace model, so river layout providers have no idea how to track per-workspace settings.
- You cannot resize tiled windows. They will just get forced floating if you try (same as River)

River layouts provide a 'namespace' to the wayland server; this namespace is used to create a layout with the same name in Hyprland.
You can change to that layout by modifying the value of general:layout

All 'layoutmsg' dispatch commands are sent directly to the appropriate river layout provider. This may cause some unexpected lack of functionality in Hyprland.

### River layout providers
- [rivertile](https://github.com/riverwm/river)
- [kile](https://gitlab.com/snakedye/kile)
- [river-luatile](https://github.com/MaxVerevkin/river-luatile)
- [rivercarro](https://sr.ht/~novakane/rivercarro/)


### Installation

## Using hyprpm, Hyprland's official plugin manager (recommended)
1. Run `hyprpm add https://github.com/zakk4223/hyprRiver` and wait for hyprpm to build the plugin.
2. Run `hyprpm enable hyprRiver`

