### Here some quick notes about all three multidisc supported methods:

1. m3u folder using bin+cue -> is the most common format for roms needs a subfolder precise structure
2. single pbp file created with psxpackager -> it generates a single file which is easier to handle.
3. m3u folder using chd     -> as bin+cue needs subfolder but it reduce files size 

starting from these bin+cue files:
<pre>
My Game Test (Disc 1).bin
My Game Test (Disc 1).cue
My Game Test (Disc 2).bin
My Game Test (Disc 2).cue
</pre>

My Game Test.m3u is a text file placed in the same folder as bin+cue files and contains:
<pre>
My Game Test (Disc 1).cue
My Game Test (Disc 2).cue
</pre>

this is how the folder structure must be: 
<pre>
My Emulator (XX)
├──Imgs
│   └── My Game Test.png
└──My Game Test
    ├── My Game Test (Disc 1).bin
    ├── My Game Test (Disc 1).cue
    ├── My Game Test (Disc 2).bin
    ├── My Game Test (Disc 2).cue
    └── My Game Test.m3u
</pre>

PBP file created with:
<pre>
psxpackager -i "My Game Test.m3u" -o "My Game Test.pbp"
</pre>

this is how the folder structure must be: 
<pre>
My Emulator (XX)
├──Imgs
│   └── My Game Test.png
└──My Game Test.pbp
</pre>


CHD files created with:
<pre>
chdman createcd -i "My Game Test (Disc 1).cue" -o "My Game Test (Disc 1).chd"
chdman createcd -i "My Game Test (Disc 2).cue" -o "My Game Test (Disc 2).chd"
</pre>

My Game Test.m3u when using CHD contains:
<pre>
My Game Test (Disc 1).chd
My Game Test (Disc 2).chd
</pre>
this is how the folder structure must be: 
<pre>
My Emulator (XX)
├──Imgs
│   └── My Game Test.png
└──My Game Test
    ├── My Game Test (Disc 1).chd
    ├── My Game Test (Disc 2).chd
    └── My Game Test.m3u
</pre>

in all three cases the boxart image is stored in the Imgs folder and named as the folder or file in case of pbp 

