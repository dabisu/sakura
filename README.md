# sakura

**sakura** is a simple [gtk](http://www.gtk.org) and [vte](https://gitlab.gnome.org/GNOME/vte) based terminal emulator. It uses tabs to provide several terminals in one window and allows to change configuration options via a contextual menu. No more no less.

## Installation

How to compile and install this beast ?

```bash
$ cmake .
$ make
$ sudo make install
```
**sakura** now uses the CMake building system (RIP our old system MOBS, we'll remember you ;)).

To install **sakura** with a different prefix, cmake needs to be invoked with the proper environment
variables, so for example, to install sakura in `/usr`, you must type:

```bash
$ cmake -DCMAKE_INSTALL_PREFIX=/usr .
```

Use CMAKE_BUILD_TYPE=Debug if you need debug symbols. Default type is "Release".


## Usage

**sakura** has several command line options. Run `sakura --help` for a full list.

## Keybindings

**sakura** supports keyboard bindings in its config file (`~/.config/sakura/sakura.conf`), but there's no GUI to edit them, so please use your favourite editor to change the following values. Keybindings are a combination of an accelerator+key.

### Accelerators

Accelerators can be set to any _GdkModifierType_ mask value. The full list of _GdkModifierType_ values is available [here](http://gtk.php.net/manual/en/html/gdk/gdk.enum.modifiertype.html)

Mask values can be combined by ORing them. For example, to set the delete tab accelerator to Ctrl+Shift, change the option "del_tab_accelerator" value to "5". This number comes from ORing GDK_SHIFT_MASK and GDK_CONTROL_MASK.

I realise that this configuration is not user-friendly, but...  :-P

Quick reference: Shift(1), Cps-Lock(2), Ctrl(4), Alt(8), Ctrl-S(5), Ctrl-A(12), Ctrl-A-S(13)

### Keys

To change default keys, set the key value you want to modify to your desired key. For example, if you want to use the "D" key instead of the "W" key to delete a tab, set "del_tab_key" to "D" in the config file.

### Default keybindings

	Ctrl + Shift + T                 -> New tab
	Ctrl + Shift + W                 -> Close current tab
	Ctrl + Shift + C                 -> Copy selected text
	Ctrl + Shift + V                 -> Paste selected text
	Ctrl + Shift + N                 -> Set tab name

	Alt  + Left cursor               -> Previous tab
	Alt  + Right cursor              -> Next tab
	Alt  + Shift + Left cursor       -> Move tab to the left
	Alt  + Shift + Right cursor      -> Move tab to the right
	Ctrl + [1-9]                     -> Switch to tab N (1-9)

	Ctrl + Shift + S                 -> Toggle/Untoggle scrollbar
	Ctrl + Shift + Mouse left button -> Open link
	F11                              -> Fullscreen
	Shift + PageUp                   -> Move up through scrollback by page
	Shift + PageDown                 -> Move down through scrollback by page
	Ctrl + Shift + Up                -> Move up through scrollback by line
	Ctrl + Shift + Down              -> Move down through scrollback by line
	Ctrl + Shift + [F1-F6]           -> Select the colorset for the current tab

You can also increase and decrease the font size in the GTK standard way:

	Ctrl + '+'                                -> Increase font size
	Ctrl + '-'                                -> Decrease font size

By default, mouse buttons are bound to the following:

	Button1                          -> No action
	Button2                          -> Paste
	Button3                          -> Context menu

Behavior can be changed with the following config settings:

	copy_on_select                   -> set to true to automatically copy selected text
	paste_button                     -> set to desired mouse button (default: 2)
	menu_button                      -> set to desired mouse button (default: 3)

## Contributing
Pull requests are welcome. But please, create first a bug report in [Launchpad](https://bugs.launchpad.net/sakura), particularly if you plan to make major changes, to make sure your patch will be merged into **sakura**. If you'd like to contribute with translations, use the translations framework in [Launchpad](https://translations.launchpad.net/sakura) or send [me](mailto:dabisu@gmail.com) directly the translated po file.

## License
[GPL 2.0](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)

\
Enjoy **sakura**!
