### Here a quick note about all three multidisc supported methods:

1. single pbp file created with psxpackager (recommended method)
2. m3u folder using bin+cue
3. m3u folder using chd

starting from bin+cue files.

chd files created with:
<pre>
chdman createcd -i "My Game Test (Disc 1).cue" -o "My Game Test (USA) (Disc 1).chd"
chdman createcd -i "My Game Test (Disc 2).cue" -o "My Game Test (USA) (Disc 2).chd"
</pre>
pbp file created with:
<pre>
psxpackager -i "My Game Test.m3u" -o "My Game Test Single.pbp"
</pre>

folder structures:
<pre>
Sony Playstation (PS)
├── My Game Test
│   ├── My Game Test (Disc 1).bin
│   ├── My Game Test (Disc 1).cue
│   ├── My Game Test (Disc 2).bin
│   ├── My Game Test (Disc 2).cue
│   └── My Game Test.m3u
├── My Game Test (USA)
|   ├── My Game Test (USA) (Disc 1).chd
|   ├── My Game Test (USA) (Disc 2).chd
|   └── My Game Test (USA).m3u
└── My Game Test Single.pbp
</pre>

My Game Test.m3u contains:
<pre>
My Game Test (Disc 1).cue
My Game Test (Disc 2).cue
</pre>

My Game Test (USA).m3u contains:
<pre>
My Game Test (USA) (Disc 1).chd
My Game Test (USA) (Disc 2).chd
</pre>

