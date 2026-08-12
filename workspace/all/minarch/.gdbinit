target remote 192.168.1.186:9999
handle SIG32 nostop noprint
tui enable
b PLAT_getScreenGame
b video_refresh_callback_resize
continue