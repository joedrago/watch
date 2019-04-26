/* watch -- execute a program repeatedly, displaying output fullscreen
 *
 * Based on the original 1991 'watch' by Tony Rems <rembo@unisoft.com>
 * (with mods and corrections by Francois Pinard).
 *
 * Substantially reworked, new features (differences option, SIGWINCH
 * handling, unlimited command length, long line handling) added Apr 1999 by
 * Mike Coleman <mkc@acm.org>.
 *
 * Changes by Albert Cahalan, 2002-2003.
 */

#define _CRT_SECURE_NO_WARNINGS

#define VERSION "0.2.0"

#include <windows.h>

#include <ctype.h>
#include "getopt.h"
#include <signal.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <sys/ioctl.h>
#include <time.h>
// #include <unistd.h>
// #include <termios.h>
#include <locale.h>
// #include "proc/procps.h"

#ifdef FORCE_8BIT
#undef isprint
#define isprint(x) ( (x >= ' ' && x <= '~') || (x >= 0xa0) )
#endif

#define NORETURN

static struct option longopts[] = {
    { "differences", optional_argument, 0, 'd' },
    { "help", no_argument, 0, 'h' },
    { "interval", required_argument, 0, 'n' },
    { "no-title", no_argument, 0, 't' },
    { "version", no_argument, 0, 'v' },
    { 0, 0, 0, 0 }
};

static char usage[] =
    "Usage: %s [-dhntv] [--differences[=cumulative]] [--help] [--interval=<n>] [--no-title] [--version] <command>\n";

static char * progname;

#define MAX_ANSIBUF 100

static int curses_started = 0;
static int height = 24, width = 80;
static int screen_size_changed = 0;
static int first_screen = 1;
static int show_title = 2; // number of lines used, 2 or 0

// #define min(x, y) ((x) > (y) ? (y) : (x))

static void do_usage(void) NORETURN;
static void do_usage(void)
{
    fprintf(stderr, usage, progname);
    exit(1);
}

static void do_exit(int status) NORETURN;
static void do_exit(int status)
{
    if (curses_started)
        endwin();
    exit(status);
}

/* signal handler */
static void die(int notused) NORETURN;
static void die(int notused)
{
    (void)notused;
    do_exit(0);
}

static void winch_handler(int notused)
{
    (void)notused;
    screen_size_changed = 1;
}

static char env_col_buf[24];
static char env_row_buf[24];
static int incoming_cols;
static int incoming_rows;

static void get_terminal_size(void)
{
    int newHeight = PDC_get_rows();
    int newWidth = PDC_get_columns();
    if ((newWidth != width) || (newHeight != height)) {
        width = newWidth;
        height = newHeight;
        screen_size_changed = TRUE;
    }
}

int vasprintf(char ** str, const char * fmt, va_list args)
{
    int size = 0;
    va_list tmpa;

    // copy
    va_copy(tmpa, args);

    // apply variadic arguments to
    // sprintf with format to get size
    size = vsnprintf(NULL, size, fmt, tmpa);

    // toss args
    va_end(tmpa);

    // return -1 to be compliant if
    // size is less than 0
    if (size < 0) { return -1; }

    // alloc with size plus 1 for `\0'
    *str = (char *)malloc(size + 1);

    // return -1 to be compliant
    // if pointer is `NULL'
    if (NULL == *str) { return -1; }

    // format string with original
    // variadic arguments and set new size
    size = vsprintf(*str, fmt, args);
    return size;
}

int asprintf(char ** str, const char * fmt, ...)
{
    int size = 0;
    va_list args;

    // init variadic argumens
    va_start(args, fmt);

    // format and get size
    size = vasprintf(str, fmt, args);

    // toss args
    va_end(args);

    return size;
}

void usleep(__int64 usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

static int nr_of_colors;
static int attributes;
static int fg_col;
static int bg_col;

static void reset_ansi(void)
{
    attributes = A_NORMAL;
    fg_col = 0;
    bg_col = 0;
}

static void init_ansi_colors(void)
{
    short ncurses_colors[] = {
        -1, COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
        COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
    };

    nr_of_colors = sizeof(ncurses_colors) / sizeof(short);

    for (bg_col = 0; bg_col < nr_of_colors; bg_col++)
        for (fg_col = 0; fg_col < nr_of_colors; fg_col++)
            init_pair(bg_col * nr_of_colors + fg_col + 1, ncurses_colors[fg_col], ncurses_colors[bg_col]);
    reset_ansi();
}

static int set_ansi_attribute(const int attrib)
{
    switch (attrib) {
        case -1: /* restore last settings */
            break;
        case 0: /* restore default settings */
            reset_ansi();
            break;
        case 1: /* set bold / increased intensity */
            attributes |= A_BOLD;
            break;
        case 2: /* set decreased intensity (if supported) */
            attributes |= A_DIM;
            break;
#ifdef A_ITALIC
        case 3: /* set italic (if supported) */
            attributes |= A_ITALIC;
            break;
#endif
        case 4: /* set underline */
            attributes |= A_UNDERLINE;
            break;
        case 5: /* set blinking */
            attributes |= A_BLINK;
            break;
        case 7: /* set inversed */
            attributes |= A_REVERSE;
            break;
        case 21: /* unset bold / increased intensity */
            attributes &= ~A_BOLD;
            break;
        case 22: /* unset bold / any intensity modifier */
            attributes &= ~(A_BOLD | A_DIM);
            break;
#ifdef A_ITALIC
        case 23: /* unset italic */
            attributes &= ~A_ITALIC;
            break;
#endif
        case 24: /* unset underline */
            attributes &= ~A_UNDERLINE;
            break;
        case 25: /* unset blinking */
            attributes &= ~A_BLINK;
            break;
        case 27: /* unset inversed */
            attributes &= ~A_REVERSE;
            break;
        case 39:
            fg_col = 0;
            break;
        case 49:
            bg_col = 0;
            break;
        default:
            if (( attrib >= 30) && ( attrib <= 37) ) { /* set foreground color */
                fg_col = attrib - 30 + 1;
            } else if (( attrib >= 40) && ( attrib <= 47) ) { /* set background color */
                bg_col = attrib - 40 + 1;
            } else {
                return 0; /* Not understood */
            }
    }
    attrset(attributes | COLOR_PAIR(bg_col * nr_of_colors + fg_col + 1));
    return 1;
}

static void process_ansi(FILE * fp)
{
    int i, c;
    char buf[MAX_ANSIBUF];
    char * numstart, * endptr;

    c = getc(fp);
    if (c != '[') {
        ungetc(c, fp);
        return;
    }
    for (i = 0; i < MAX_ANSIBUF; i++) {
        c = getc(fp);
        /* COLOUR SEQUENCE ENDS in 'm' */
        if (c == 'm') {
            buf[i] = '\0';
            break;
        }
        if (((c < '0') || (c > '9')) && (c != ';')) {
            return;
        }
        buf[i] = (char)c;
    }
    /*
     * buf now contains a semicolon-separated list of decimal integers,
     * each indicating an attribute to apply.
     * For example, buf might contain "0;1;31", derived from the color
     * escape sequence "<ESC>[0;1;31m". There can be 1 or more
     * attributes to apply, but typically there are between 1 and 3.
     */

    /* Special case of <ESC>[m */
    if (buf[0] == '\0')
        set_ansi_attribute(0);

    for (endptr = numstart = buf; *endptr != '\0'; numstart = endptr + 1) {
        if (!set_ansi_attribute(strtol(numstart, &endptr, 10)))
            break;
    }
}

int main(int argc, char * argv[])
{
    int optc;
    int option_differences = 0,
        option_differences_cumulative = 0,
        option_help = 0, option_version = 0;
    double interval = 2;
    char * command;
    int command_length = 0; /* not including final \0 */

    setlocale(LC_ALL, "");
    progname = argv[0];

    while ((optc = getopt_long(argc, argv, "+d::hn:vt", longopts, (int *)0))
           != EOF)
    {
        switch (optc) {
            case 'd':
                option_differences = 1;
                if (optarg)
                    option_differences_cumulative = 1;
                break;
            case 'h':
                option_help = 1;
                break;
            case 't':
                show_title = 0;
                break;
            case 'n':
            {
                char * str;
                interval = strtod(optarg, &str);
                if (!*optarg || *str)
                    do_usage();
                if (interval < 0.1)
                    interval = 0.1;
                if (interval > ~0u / 1000000)
                    interval = ~0u / 1000000;
                break;
            }
            case 'v':
                option_version = 1;
                break;
            default:
                do_usage();
                break;
        }
    }

    if (option_version) {
        fprintf(stderr, "%s\n", VERSION);
        if (!option_help)
            exit(0);
    }

    if (option_help) {
        fprintf(stderr, usage, progname);
        fputs("  -d, --differences[=cumulative]\thighlight changes between updates\n", stderr);
        fputs("\t\t(cumulative means highlighting is cumulative)\n", stderr);
        fputs("  -h, --help\t\t\t\tprint a summary of the options\n", stderr);
        fputs("  -n, --interval=<seconds>\t\tseconds to wait between updates\n", stderr);
        fputs("  -v, --version\t\t\t\tprint the version number\n", stderr);
        fputs("  -t, --no-title\t\t\tturns off showing the header\n", stderr);
        exit(0);
    }

    if (optind >= argc)
        do_usage();

    command = strdup(argv[optind++]);
    command_length = strlen(command);
    for (; optind < argc; optind++) {
        char * endp;
        int s = strlen(argv[optind]);
        command = realloc(command, command_length + s + 2); /* space and \0 */
        endp = command + command_length;
        *endp = ' ';
        memcpy(endp + 1, argv[optind], s);
        command_length += 1 + s; /* space then string length */
        command[command_length] = '\0';
    }

    PDC_scr_open(argc, argv);
    get_terminal_size();

    /* Catch keyboard interrupts so we can put tty back in a sane state.  */
    signal(SIGINT, die);
    signal(SIGTERM, die);
    // signal(SIGHUP, die);
    // signal(SIGWINCH, winch_handler);

    /* Set up tty for curses use.  */
    curses_started = 1;
    initscr();
    nonl();
    noecho();
    cbreak();

    start_color();
    use_default_colors();
    init_ansi_colors();

    for (;;) {
        time_t t = time(NULL);
        char * ts = ctime(&t);
        int tsl = strlen(ts);
        char * header;
        FILE * p;
        int x, y;
        int oldeolseen = 1;

        get_terminal_size();
        if (screen_size_changed) {
            PDC_resize_screen(height, width);
            resize_term(height, width);
            clear();
            redrawwin(stdscr);
            screen_size_changed = 0;
            first_screen = 1;
        }

        if (show_title) {
            // left justify interval and command,
            // right justify time, clipping all to fit window width
            asprintf(&header, "Every %.1fs: %.*s",
                interval, min(width - 1, command_length), command);
            mvaddstr(0, 0, header);
            if (strlen(header) > (size_t)(width - tsl - 1))
                mvaddstr(0, width - tsl - 4, "...  ");
            mvaddstr(0, width - tsl + 1, ts);
            free(header);
        }

        if (!(p = _popen(command, "r"))) {
            perror("popen");
            do_exit(2);
        }

        reset_ansi();
        for (y = show_title; y < height; y++) {
            int eolseen = 0, tabpending = 0, tabwaspending = 0;
            for (x = 0; x < width; x++) {
                int c = ' ';
                int attr = 0;

                if (tabwaspending)
                    set_ansi_attribute(-1);
                tabwaspending = 0;

                if (!eolseen) {
                    /* if there is a tab pending, just spit spaces until the
                       next stop instead of reading characters */
                    if (!tabpending)
                        do
                            c = getc(p);
                        while (c != EOF && !isprint(c)
                               && c != '\n'
                               && c != '\t'
                               && c != '\033');

                    if (( c == L'\033')) {
                        x--;
                        process_ansi(p);
                        continue;
                    }

                    if (c == '\n')
                        if (!oldeolseen && (x == 0)) {
                            x = -1;
                            continue;
                        } else
                            eolseen = 1;
                    else if (c == '\t')
                        tabpending = 1;
                    if ((c == EOF) || (c == '\n') || (c == '\t')) {
                        c = ' ';
                        attrset(A_NORMAL);
                    }
                    if (tabpending && (((x + 1) % 8) == 0)) {
                        tabpending = 0;
                        tabwaspending = 1;
                    }
                }
                move(y, x);
                if (option_differences) {
                    chtype oldch = inch();
                    char oldc = oldch & A_CHARTEXT;
                    attr = !first_screen
                           && ((char)c != oldc
                               ||
                               (option_differences_cumulative
                                && (oldch & A_ATTRIBUTES)));
                }
                if (attr)
                    standout();
                addch(c);
                if (attr)
                    standend();
            }
            oldeolseen = eolseen;
        }

        _pclose(p);

        first_screen = 0;
        refresh();
        usleep((__int64)(interval * 1000000));
    }

    endwin();

    return 0;
}
