Thanks to [hong19860320](https://github.com/hong19860320) who made a PR to MinUI (rejected by MinUI dev) that allowed me to add cheats support with a minimal effort:

This pull request migrates code from https://github.com/LoveRetro/NextUI/blob/b0ed86d1a4cf1bfec247ee6ab78347541483eb1a/workspace/all/minarch/minarch.c#L384

Copy Your cheats file to the Cheats/System folders, the cht file must match the rom name including brackets in the file name.

As an example for the NES "1942 (Japan, USA).nes" You can download the cheat file from https://github.com/libretro/libretro-database/blob/master/cht/Nintendo%20-%20Nintendo%20Entertainment%20System/1942%20(Japan,%20USA).cht, and place in the /Cheats/FC/ directory.
Then a new menu Cheats is available in minarch in game menu where You can activate the cheats You like.

Cheats are not saved in the setting file so at every time a game starts it must be manually activated the desired cheat.