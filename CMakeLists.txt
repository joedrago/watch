cmake_minimum_required(VERSION 3.5)
project(watch)

include_directories(pdcurses)
add_executable(watch
    watch.c

    getopt.c

    pdcurses/pdcurses/addch.c
    pdcurses/pdcurses/addchstr.c
    pdcurses/pdcurses/addstr.c
    pdcurses/pdcurses/attr.c
    pdcurses/pdcurses/beep.c
    pdcurses/pdcurses/bkgd.c
    pdcurses/pdcurses/border.c
    pdcurses/pdcurses/clear.c
    pdcurses/pdcurses/color.c
    pdcurses/pdcurses/debug.c
    pdcurses/pdcurses/delch.c
    pdcurses/pdcurses/deleteln.c
    pdcurses/pdcurses/getch.c
    pdcurses/pdcurses/getstr.c
    pdcurses/pdcurses/getyx.c
    pdcurses/pdcurses/inch.c
    pdcurses/pdcurses/inchstr.c
    pdcurses/pdcurses/initscr.c
    pdcurses/pdcurses/inopts.c
    pdcurses/pdcurses/insch.c
    pdcurses/pdcurses/insstr.c
    pdcurses/pdcurses/instr.c
    pdcurses/pdcurses/kernel.c
    pdcurses/pdcurses/keyname.c
    pdcurses/pdcurses/mouse.c
    pdcurses/pdcurses/move.c
    pdcurses/pdcurses/outopts.c
    pdcurses/pdcurses/overlay.c
    pdcurses/pdcurses/pad.c
    pdcurses/pdcurses/panel.c
    pdcurses/pdcurses/printw.c
    pdcurses/pdcurses/refresh.c
    pdcurses/pdcurses/scanw.c
    pdcurses/pdcurses/scroll.c
    pdcurses/pdcurses/scr_dump.c
    pdcurses/pdcurses/slk.c
    pdcurses/pdcurses/termattr.c
    pdcurses/pdcurses/touch.c
    pdcurses/pdcurses/util.c
    pdcurses/pdcurses/window.c

    pdcurses/wincon/pdcclip.c
    pdcurses/wincon/pdcdisp.c
    pdcurses/wincon/pdcgetsc.c
    pdcurses/wincon/pdckbd.c
    pdcurses/wincon/pdcscrn.c
    pdcurses/wincon/pdcsetsc.c
    pdcurses/wincon/pdcutil.c
)
