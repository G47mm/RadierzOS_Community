#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "fat32.h"
#include "string.h"
#include "stdio.h"

#if defined(__linux__)
#error "Use a cross-compiler"
#endif

#if !defined(__i386__)
#error "Must be compiled with an ix86-elf compiler"
#endif

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF
#define VBE_DISPI_INDEX_XRES   1
#define VBE_DISPI_INDEX_YRES   2
#define VBE_DISPI_INDEX_BPP    3
#define VBE_DISPI_INDEX_ENABLE 4
#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40

static volatile uint32_t* fb_ptr = NULL;
static uint32_t fb_width = 640;
static uint32_t fb_height = 480;

#define TERM_MAX_X 420
#define CUBE_MIN_X 440
#define CUBE_MAX_X 640
#define CUBE_MIN_Y 10
#define CUBE_MAX_Y 210
#define CUBE_CX 535
#define CUBE_CY 110

static int term_cursor_x = 0;
static int term_cursor_y = 0;

static char cmd_buf[32];
static int cmd_len = 0;

// Current working directory state for the cd command
static char current_dir[128] = "/";

struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
};

extern char _binary_initrd_tar_start[];
extern char _binary_initrd_tar_end[];

static const uint8_t font8x8[128][8] = {
    ['0'] = {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},
    ['1'] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    ['2'] = {0x3C,0x66,0x0C,0x18,0x30,0x60,0x7E,0x00},
    ['3'] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    ['4'] = {0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x0C,0x00},
    ['5'] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    ['6'] = {0x3C,0x60,0x7C,0x66,0x66,0x66,0x3C,0x00},
    ['7'] = {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00},
    ['8'] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    ['9'] = {0x3C,0x66,0x66,0x3E,0x06,0x60,0x3C,0x00},
    ['A'] = {0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00},
    ['B'] = {0x7C,0x62,0x62,0x7C,0x62,0x62,0x7C,0x00},
    ['C'] = {0x3C,0x62,0x60,0x60,0x60,0x62,0x3C,0x00},
    ['D'] = {0x78,0x64,0x62,0x62,0x62,0x64,0x78,0x00},
    ['E'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
    ['F'] = {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00},
    ['G'] = {0x3C,0x62,0x60,0x6E,0x62,0x62,0x3C,0x00},
    ['H'] = {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00},
    ['I'] = {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    ['J'] = {0x1E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00},
    ['K'] = {0x62,0x64,0x68,0x70,0x68,0x64,0x62,0x00},
    ['L'] = {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    ['M'] = {0x41,0x63,0x55,0x49,0x41,0x41,0x41,0x00},
    ['N'] = {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00},
    ['O'] = {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['P'] = {0x7C,0x62,0x62,0x7C,0x60,0x60,0x60,0x00},
    ['Q'] = {0x3C,0x66,0x66,0x66,0x6A,0x6C,0x3A,0x00},
    ['R'] = {0x7C,0x62,0x62,0x7C,0x68,0x64,0x62,0x00},
    ['S'] = {0x3C,0x60,0x60,0x3C,0x06,0x06,0x3C,0x00},
    ['T'] = {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    ['U'] = {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    ['V'] = {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    ['W'] = {0x41,0x41,0x41,0x49,0x55,0x63,0x41,0x00},
    ['X'] = {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    ['Y'] = {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
    ['Z'] = {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00},
    ['a'] = {0x00,0x00,0x3C,0x04,0x3C,0x44,0x3C,0x00},
    ['b'] = {0x40,0x40,0x5C,0x62,0x42,0x62,0x5C,0x00},
    ['c'] = {0x00,0x00,0x3C,0x40,0x40,0x40,0x3C,0x00},
    ['d'] = {0x04,0x04,0x3C,0x44,0x42,0x42,0x3C,0x00},
    ['e'] = {0x00,0x00,0x3C,0x42,0x7E,0x40,0x3C,0x00},
    ['f'] = {0x1C,0x22,0x20,0x78,0x20,0x20,0x20,0x00},
    ['g'] = {0x00,0x00,0x3C,0x44,0x44,0x3C,0x04,0x38},
    ['h'] = {0x40,0x40,0x5C,0x62,0x42,0x42,0x42,0x00},
    ['i'] = {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00},
    ['j'] = {0x06,0x00,0x0E,0x06,0x06,0x46,0x3C,0x00},
    ['k'] = {0x40,0x40,0x44,0x48,0x70,0x48,0x44,0x00},
    ['l'] = {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    ['m'] = {0x00,0x00,0x66,0x55,0x49,0x49,0x41,0x00},
    ['n'] = {0x00,0x00,0x5C,0x62,0x42,0x42,0x42,0x00},
    ['o'] = {0x00,0x00,0x3C,0x42,0x42,0x42,0x3C,0x00},
    ['p'] = {0x00,0x00,0x5C,0x62,0x62,0x5C,0x40,0x40},
    ['q'] = {0x00,0x00,0x3C,0x42,0x42,0x3C,0x04,0x04},
    ['r'] = {0x00,0x00,0x5C,0x62,0x40,0x40,0x40,0x00},
    ['s'] = {0x00,0x00,0x3C,0x40,0x3C,0x04,0x3C,0x00},
    ['t'] = {0x20,0x20,0x7C,0x20,0x20,0x24,0x18,0x00},
    ['u'] = {0x00,0x00,0x42,0x42,0x42,0x46,0x3A,0x00},
    ['v'] = {0x00,0x00,0x42,0x42,0x42,0x24,0x18,0x00},
    ['w'] = {0x00,0x00,0x41,0x49,0x49,0x55,0x22,0x00},
    ['x'] = {0x00,0x00,0x42,0x24,0x18,0x24,0x42,0x00},
    ['y'] = {0x00,0x00,0x42,0x42,0x42,0x3C,0x04,0x38},
    ['z'] = {0x00,0x00,0x7E,0x08,0x10,0x20,0x7E,0x00},
    [' '] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['.'] = {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    [':'] = {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    ['-'] = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    ['>'] = {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00},
    ['/'] = {0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x00}
};

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static char keyboard_map(uint8_t scancode) {
    static const char ascii[] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };
    if (scancode < sizeof(ascii)) {
        return ascii[scancode];
    }
    return 0;
}

static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) | (offset & 0xFC);
    __asm__ volatile("outl %0, %1" : : "a"(address), "d"((uint16_t)0xCF8));
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a"(result) : "d"((uint16_t)0xCFC));
    return result;
}

static uint32_t get_vbe_framebuffer_address(void) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint32_t vendor_device = pci_read_config(0, slot, 0, 0x00);
        if (vendor_device == 0x11111234) {
            uint32_t bar0 = pci_read_config(0, slot, 0, 0x10);
            return bar0 & 0xFFFFFFF0;
        }
    }
    return 0xFD000000;
}

static void vbe_write(uint16_t index, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(index), "d"((uint16_t)VBE_DISPI_IOPORT_INDEX));
    __asm__ volatile("outw %0, %1" : : "a"(value), "d"((uint16_t)VBE_DISPI_IOPORT_DATA));
}

static void put_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)fb_width && y >= 0 && y < (int)fb_height) {
        fb_ptr[y * fb_width + x] = color;
    }
}

static void terminal_scroll(void) {
    for (int y = 10; y < 430; y++) {
        for (int x = 10; x < TERM_MAX_X - 10; x++) {
            fb_ptr[y * fb_width + x] = fb_ptr[(y + 10) * fb_width + x];
        }
    }
    for (int y = 430; y < 440; y++) {
        for (int x = 10; x < TERM_MAX_X - 10; x++) {
            put_pixel(x, y, 0x000A0A0A);
        }
    }
}

static void terminal_backspace(void) {
    if (cmd_len > 0) {
        cmd_len--;
        if (term_cursor_x > 0) {
            term_cursor_x--;
        } else if (term_cursor_y > 0) {
            term_cursor_y--;
            term_cursor_x = 49;
        }
        int px = 10 + (term_cursor_x * 8);
        int py = 10 + (term_cursor_y * 10);
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                put_pixel(px + col, py + row, 0x000A0A0A);
            }
        }
    }
}

void terminal_putchar(char c, uint32_t fg_color) {
    if (c == '\n') {
        term_cursor_x = 0;
        term_cursor_y++;
        if (term_cursor_y >= 43) {
            terminal_scroll();
            term_cursor_y = 42;
        }
        return;
    }

    int px = 10 + (term_cursor_x * 8);
    int py = 10 + (term_cursor_y * 10);

    if (px + 8 < TERM_MAX_X) {
        unsigned char uc = (unsigned char)c;
        if (uc > 127) uc = ' ';
        const uint8_t* glyph = font8x8[uc];
        for (int row = 0; row < 8; row++) {
            uint8_t line = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (line & (1 << (7 - col))) {
                    put_pixel(px + col, py + row, fg_color);
                }
            }
        }
    }

    term_cursor_x++;
    if (term_cursor_x >= 50) {
        term_cursor_x = 0;
        term_cursor_y++;
        if (term_cursor_y >= 43) {
            terminal_scroll();
            term_cursor_y = 42;
        }
    }
}

void terminal_writestring(const char* str, uint32_t fg_color) {
    while (*str) {
        terminal_putchar(*str++, fg_color);
    }
}

static void system_reboot(void) {
    terminal_writestring("\nRebooting system...\n", 0x00FFFF00);
    uint8_t temp = 0x02;
    while (temp & 0x02) {
        temp = inb(0x64);
    }
    outb(0x64, 0xFE);
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

static void system_shutdown(void) {
    terminal_writestring("\nShutting down system...\n", 0x00FFFF00);
    // QEMU / Bochs ACPI power-off ports
    __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "d"((uint16_t)0x604));
    __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "d"((uint16_t)0xB004));
    __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "d"((uint16_t)0x4004));
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

static void print_prompt(void) {
    terminal_writestring(current_dir, 0x0000FFFF);
    terminal_writestring("> ", 0x0000FF00);
}

static unsigned int parse_octal(const char *s, int maxlen) {
    unsigned int val = 0;
    int i = 0;
    while (i < maxlen && s[i] >= '0' && s[i] <= '7') {
        val = (val * 8) + (s[i] - '0');
        i++;
    }
    return val;
}

static bool streq(const char *a, const char *b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static void list_tar_files(void) {
    char *ptr = _binary_initrd_tar_start;
    terminal_writestring("Files in initrd.tar (USTAR):\n", 0x00FFFF00);
    while (ptr < _binary_initrd_tar_end) {
        struct ustar_header *hdr = (struct ustar_header *)ptr;
        if (hdr->name[0] == '\0') {
            break;
        }
        if (hdr->magic[0] == 'u' && hdr->magic[1] == 's' && hdr->magic[2] == 't' &&
            hdr->magic[3] == 'a' && hdr->magic[4] == 'r') {
            terminal_writestring("  ", 0x00FFFFFF);
            terminal_writestring(hdr->name, 0x00FFFFFF);
            terminal_writestring("\n", 0x00FFFFFF);
        }
        unsigned int size = parse_octal(hdr->size, 12);
        unsigned int blocks = (size + 511) / 512;
        ptr += (1 + blocks) * 512;
    }
}

static void print_file_content(const char *filename) {
    char *ptr = _binary_initrd_tar_start;
    while (ptr < _binary_initrd_tar_end) {
        struct ustar_header *hdr = (struct ustar_header *)ptr;
        if (hdr->name[0] == '\0') {
            break;
        }
        if (hdr->magic[0] == 'u' && hdr->magic[1] == 's' && hdr->magic[2] == 't' &&
            hdr->magic[3] == 'a' && hdr->magic[4] == 'r') {
            if (streq(hdr->name, filename)) {
                unsigned int size = parse_octal(hdr->size, 12);
                char *file_data = ptr + 512;
                terminal_writestring("\n", 0x00FFFFFF);
                for (unsigned int i = 0; i < size; i++) {
                    terminal_putchar(file_data[i], 0x00FFFFFF);
                }
                terminal_writestring("\n", 0x00FFFFFF);
                return;
            }
        }
        unsigned int size = parse_octal(hdr->size, 12);
        unsigned int blocks = (size + 511) / 512;
        ptr += (1 + blocks) * 512;
    }

    terminal_writestring("\nfile not found\n", 0x00FF0000);
}

static void fat_print_adapter(const char* str) {
    terminal_writestring(str, 0x00FFFFFF);
}

static void handle_fat_cat(const char* filename) {
    char buf[2048];
    uint32_t size = 0;
    terminal_writestring("\n", 0x00FFFFFF);
    if (fat32_read_file(filename, buf, sizeof(buf) - 1, &size)) {
        terminal_writestring(buf, 0x00FFFFFF);
        terminal_writestring("\n", 0x00FFFFFF);
    } else {
        terminal_writestring("fat32: file not found\n", 0x00FF0000);
    }
}

static void handle_lsblk(void) {
    terminal_writestring("\nNAME     MAJ:MIN RM  SIZE RO TYPE MOUNTPOINT\n", 0x00FFFF00);
    terminal_writestring("hda        8:0    0  32M   0 disk\n", 0x00FFFFFF);
    terminal_writestring("`-hda1     8:1    0  32M   0 part /fat32\n", 0x00FFFFFF);
    terminal_writestring("initrd     7:0    1  --    1 rom  /tar\n", 0x00FFFFFF);
}

#define LUT_SIZE 64
static const int sina[LUT_SIZE] = {
    0, 98, 195, 290, 382, 471, 555, 634, 707, 773, 831, 881, 923, 956, 980, 995,
    1000, 995, 980, 956, 923, 881, 831, 773, 707, 634, 555, 471, 382, 290, 195, 98,
    0, -98, -195, -290, -382, -471, -555, -634, -707, -773, -831, -881, -923, -956, -980, -995,
    -1000, -995, -980, -956, -923, -881, -831, -773, -707, -634, -555, -471, -382, -290, -195, -98
};

static const int cosa[LUT_SIZE] = {
    1000, 995, 980, 956, 923, 881, 831, 773, 707, 634, 555, 471, 382, 290, 195, 98,
    0, -98, -195, -290, -382, -471, -555, -634, -707, -773, -831, -881, -923, -956, -980, -995,
    -1000, -995, -980, -956, -923, -881, -831, -773, -707, -634, -555, -471, -382, -290, -195, -98,
    0, 98, 195, 290, 382, 471, 555, 634, 707, 773, 831, 881, 923, 956, 980, 995
};

static int vertices[8][3] = {
    {-40, -40, -40}, { 40, -40, -40}, { 40,  40, -40}, {-40,  40, -40},
    {-40, -40,  40}, { 40, -40,  40}, { 40,  40,  40}, {-40,  40,  40}
};

static int faces[6][4] = {
    {0, 1, 2, 3},
    {5, 4, 7, 6},
    {4, 0, 3, 7},
    {1, 5, 6, 2},
    {4, 5, 1, 0},
    {3, 2, 6, 7}
};

static uint32_t face_colors[6] = {
    0x00FF0000,
    0x0000FF00,
    0x000000FF,
    0x00FFFF00,
    0x00FF00FF,
    0x0000FFFF
};

static int edge_func(int x0, int y0, int x1, int y1, int x2, int y2) {
    return (x2 - x0) * (y1 - y0) - (y2 - y0) * (x1 - x0);
}

static void draw_perspective_triangle(int x0, int y0, int u0, int v0, int z0,
                            int x1, int y1, int u1, int v1, int z1,
                            int x2, int y2, int u2, int v2, int z2, uint32_t tint) {
    int min_x = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int max_x = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int min_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int max_y = y0 > y1 ? (y0 > y1 ? y0 : y1) : (y1 > y2 ? y1 : y2);

    if (min_x < CUBE_MIN_X) min_x = CUBE_MIN_X;
    if (max_x > CUBE_MAX_X) max_x = CUBE_MAX_X;
    if (min_y < CUBE_MIN_Y) min_y = CUBE_MIN_Y;
    if (max_y > CUBE_MAX_Y) max_y = CUBE_MAX_Y;

    int area = edge_func(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    int uz0 = (u0 * 1000) / z0, vz0 = (v0 * 1000) / z0, iz0 = 1000 / z0;
    int uz1 = (u1 * 1000) / z1, vz1 = (v1 * 1000) / z1, iz1 = 1000 / z1;
    int uz2 = (u2 * 1000) / z2, vz2 = (v2 * 1000) / z2, iz2 = 1000 / z2;

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            int w0 = edge_func(x1, y1, x2, y2, x, y);
            int w1 = edge_func(x2, y2, x0, y0, x, y);
            int w2 = edge_func(x0, y0, x1, y1, x, y);

            if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                
                int interp_uz = (w0 * uz0 + w1 * uz1 + w2 * uz2) / area;
                int interp_vz = (w0 * vz0 + w1 * vz1 + w2 * vz2) / area;
                int interp_iz = (w0 * iz0 + w1 * iz1 + w2 * iz2) / area;

                if (interp_iz == 0) continue;

                int u = interp_uz / interp_iz;
                int v = interp_vz / interp_iz;

                int checker = ((u >> 4) + (v >> 4)) & 1;
                uint32_t color = checker ? tint : (tint >> 2 & 0x003F3F3F);

                put_pixel(x, y, color);
            }
        }
    }
}

static void frame_delay(void) {
    for (volatile int i = 0; i < 3500000; i++) { }
}

typedef void (*task_func_t)(void);

typedef struct {
    task_func_t func;
    const char* name;
    bool enabled;
} task_t;

static task_t task_list[2];
static int current_task_idx = 0;

static int angleX = 0;
static int angleY = 0;

static void terminal_task(void) {
    if (inb(0x64) & 1) {
        uint8_t scancode = inb(0x60);
        if (!(scancode & 0x80)) { 
            char c = keyboard_map(scancode);
            if (c == '\b') {
                terminal_backspace();
            } else if (c == '\n') {
                cmd_buf[cmd_len] = '\0';
                if (cmd_len > 0) {
                    if (streq(cmd_buf, "ls")) {
                        terminal_writestring("\n", 0x00FFFFFF);
                        list_tar_files();
                    } else if (streq(cmd_buf, "fls")) {
                        terminal_writestring("\nFAT32 Root Directory:\n", 0x00FFFF00);
                        fat32_list_directory(fat_print_adapter);
                    } else if (streq(cmd_buf, "lsblk")) {
                        handle_lsblk();
                    } else if (streq(cmd_buf, "shutdown")) {
                        system_shutdown();
                    } else if (streq(cmd_buf, "restart") || streq(cmd_buf, "reboot")) {
                        system_reboot();
                    } else if (cmd_buf[0] == 'c' && cmd_buf[1] == 'd' && cmd_buf[2] == ' ') {
                        const char *path = &cmd_buf[3];
                        terminal_writestring("\n", 0x00FFFFFF);
                        if (path[0] != '\0') {
                            if (streq(path, "/")) {
                                current_dir[0] = '/';
                                current_dir[1] = '\0';
                            } else if (streq(path, "..")) {
                                current_dir[0] = '/';
                                current_dir[1] = '\0';
                            } else {
                                int i = 0;
                                while (path[i] && i < 126) {
                                    current_dir[i] = path[i];
                                    i++;
                                }
                                current_dir[i] = '\0';
                            }
                        }
                    } else if (cmd_buf[0] == 'c' && cmd_buf[1] == 'a' && cmd_buf[2] == 't' && cmd_buf[3] == ' ') {
                        print_file_content(&cmd_buf[4]);
                    } else if (cmd_buf[0] == 'f' && cmd_buf[1] == 'c' && cmd_buf[2] == 'a' && cmd_buf[3] == 't' && cmd_buf[4] == ' ') {
                        handle_fat_cat(&cmd_buf[5]);
                    } else {
                        terminal_writestring("\nunknown: ", 0x00FF0000);
                        terminal_writestring(cmd_buf, 0x00FFFFFF);
                        terminal_writestring("\n", 0x00FFFFFF);
                    }
                }
                print_prompt();
                cmd_len = 0;
            } else if (c && cmd_len < 31) {
                cmd_buf[cmd_len++] = c;
                char str[2] = {c, 0};
                terminal_writestring(str, 0x00FFFFFF);
            }
        }
    }
}

static void cube_task(void) {
    for (uint32_t y = CUBE_MIN_Y; y <= CUBE_MAX_Y; y++) {
        for (uint32_t x = CUBE_MIN_X; x <= CUBE_MAX_X; x++) {
            put_pixel(x, y, 0x00111122);
        }
    }

    int sx = sina[angleX], cx = cosa[angleX];
    int sy = sina[angleY], cy = cosa[angleY];

    int proj_x[8], proj_y[8], vertex_depths[8];

    for (int i = 0; i < 8; i++) {
        int x = vertices[i][0];
        int y = vertices[i][1];
        int z = vertices[i][2];

        int y1 = (y * cx - z * sx) / 1000;
        int z1 = (y * sx + z * cx) / 1000;
        int x1 = x;

        int x2 = (x1 * cy + z1 * sy) / 1000;
        int z2 = (-x1 * sy + z1 * cy) / 1000;
        int y2 = y1;

        int depth = z2 + 150;
        if (depth <= 0) depth = 1;
        vertex_depths[i] = depth;

        proj_x[i] = CUBE_CX + (x2 * 75 / depth);
        proj_y[i] = CUBE_CY + (y2 * 75 / depth);
    }

    int uvs[4][2] = { {0, 0}, {64, 0}, {64, 64}, {0, 64} };

    for (int f = 0; f < 6; f++) {
        int p0 = faces[f][0];
        int p1 = faces[f][1];
        int p2 = faces[f][2];
        int p3 = faces[f][3];

        int area1 = edge_func(proj_x[p0], proj_y[p0], proj_x[p2], proj_y[p2], proj_x[p1], proj_y[p1]);
        if (area1 > 0) {
            draw_perspective_triangle(proj_x[p0], proj_y[p0], uvs[0][0], uvs[0][1], vertex_depths[p0],
                                     proj_x[p2], proj_y[p2], uvs[2][0], uvs[2][1], vertex_depths[p2],
                                     proj_x[p1], proj_y[p1], uvs[1][0], uvs[1][1], vertex_depths[p1], face_colors[f]);
        }

        int area2 = edge_func(proj_x[p0], proj_y[p0], proj_x[p3], proj_y[p3], proj_x[p2], proj_y[p2]);
        if (area2 > 0) {
            draw_perspective_triangle(proj_x[p0], proj_y[p0], uvs[0][0], uvs[0][1], vertex_depths[p0],
                                     proj_x[p3], proj_y[p3], uvs[3][0], uvs[3][1], vertex_depths[p3],
                                     proj_x[p2], proj_y[p2], uvs[2][0], uvs[2][1], vertex_depths[p2], face_colors[f]);
        }
    }

    angleX = (angleX + 1) % LUT_SIZE;
    angleY = (angleY + 1) % LUT_SIZE;

    frame_delay();
}

void kernel_main(void) {
    uint32_t fb_addr = get_vbe_framebuffer_address();
    fb_ptr = (volatile uint32_t*)fb_addr;

    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    vbe_write(VBE_DISPI_INDEX_XRES, fb_width);
    vbe_write(VBE_DISPI_INDEX_YRES, fb_height);
    vbe_write(VBE_DISPI_INDEX_BPP, 32);
    vbe_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    for (uint32_t y = 0; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            put_pixel(x, y, 0x000A0A0A);
        }
    }

    for (uint32_t y = 0; y < fb_height; y++) {
        put_pixel(425, y, 0x00333333);
    }

    terminal_writestring("bare-metal kernel v1.0\n", 0x0000FF00);
    terminal_writestring("cpu: i386 protected mode\n", 0x00FFFFFF);
    terminal_writestring("multitasking scheduler active\n", 0x0000FFFF);
    terminal_writestring("------------------------\n", 0x00555555);

    if (fat32_init() != 0) {
        terminal_writestring("FAT32 init failed\n", 0x00FF0000);
    } else {
        terminal_writestring("FAT32 mounted successfully\n", 0x0000FF00);
    }

    list_tar_files();

    terminal_writestring("type 'ls', 'fls', 'lsblk', 'cd', 'cat', 'fcat', 'shutdown', 'restart':\n", 0x0000FFFF);
    print_prompt();

    task_list[0] = (task_t){ terminal_task, "TerminalTask", true };
    task_list[1] = (task_t){ cube_task, "CubeTask", true };

    while (true) {
        if (task_list[current_task_idx].enabled) {
            task_list[current_task_idx].func();
        }
        current_task_idx = (current_task_idx + 1) % 2;
    }
}
