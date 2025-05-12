# DinguxCommander

DinguxCommander is a file manager for RetroFW.
It uses two vertical panels side by side, one being the source and the other the
destination, like many 'commander-style' file managers such as Norton Commander
or Midnight Commander.
DinguxCommander allows to:

* Copy, move and delete multiple files.
* View a file
* Execute a file
* Rename a file or directory
* Create a new directory
* Display disk space used by a list of selected files/dirs
* Display disk information (used, available, total)

## Building

To build for RetroFW, run:

```bash
BUILDROOT=<path to buildroot> ./build.sh retrofw
```

To build for RG350/GKD350h, run:

```bash
BUILDROOT=<path to buildroot> ./build.sh rg350
```

The package will be built at `build-retrofw/commander.ipk`.

To build for the host system, run:

```bash
./build-host.sh
```

## Controls


* D-pad         Move
* L/R           Page up/page down
* A             For a directory: open
                For a file: view or execute
* B             Go to parent directory / cancel
* Y             System actions:
                  - Select all items
                  - Select no items
                  - Create new directory
                  - Display disk information
                  - Quit program
* X             Actions on selected items:
                  - Copy to destination directory
                  - Move to destination directory
                  - Rename (appears only if 1 item is selected)
                  - Delete
                  - Display disk used
* SELECT        Select highlighted item.
                Selected items are displayed in red.
* START         Open highlighted directory in destination panel.
                If a file is highlighted, open current directory in destination panel.


## Credits

Homepage:      http://beyondds.free.fr/
Development:   Mia
Font:          Beycan Çetin
