#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void unescape_newlines(char* str) {
    int read = 0, write = 0;
    while (str[read]) {
        if (str[read] == '\\' && str[read+1] == 'n') {
            str[write++] = '\n';
            read += 2;
        } else {
            str[write++] = str[read++];
        }
    }
    str[write] = '\0';
}


// ================== C funtions from C libs ==================
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void* memset(void* dest, int val, size_t n) {
    unsigned char* d = dest;
    for (size_t i = 0; i < n; i++)
        d[i] = (unsigned char)val;
    return dest;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

// very minimal strtok (doesn't handle all edge cases)
char* strtok(char* str, const char* delim) {
    static char* pos;
    if (str) pos = str;
    if (!pos) return NULL;

    char* start = pos;
    while (*pos && !strchr(delim, *pos)) pos++;
    if (*pos) *pos++ = 0;
    return start;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return NULL;
}

int strncmp(const char* a, const char* b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

volatile uint16_t* vga_memory = (volatile uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;
int shift_pressed = 0;
unsigned char os_color;

char echo_text[256];
unsigned char echo_color;

// ================== VGA output ==================
// as the name suggests it just clears the screen
void kclear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_memory[y * VGA_WIDTH + x] = (os_color << 8) | ' ';
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

void scroll() {
    // If we're still within the screen, nothing to do
    if (cursor_y < VGA_HEIGHT)
        return;

    // Move everything up by one line
    memcpy(
        (void*)vga_memory,
        (void*)(vga_memory + VGA_WIDTH),
        (VGA_HEIGHT - 1) * VGA_WIDTH * 2     // two bytes per cell
    );

    // Clear last line
    for (int x = 0; x < VGA_WIDTH; x++)
        vga_memory[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (os_color << 8) | ' ';

    // Place cursor on last line
    cursor_y = VGA_HEIGHT - 1;
}

// prints out a single character (virgin function)
void kput_char(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        vga_memory[cursor_y * VGA_WIDTH + cursor_x] = (color << 8) | c;
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    scroll();
}

void kput_charnf(char c, uint8_t color, int y) {
    int cursor_y_back = cursor_y;
    cursor_y = y;
    cursor_x = 0;               // start at beginning of the given row
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;            // increment the row correctly
    } else {
        vga_memory[cursor_y * VGA_WIDTH + cursor_x] = (color << 8) | c;
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;       // increment the row correctly
        }
    }
    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = 0;
    }
    cursor_y = cursor_y_back;   // restore the global cursor_y so kprintnf controls the row
}

// prints out a bunch of single characters (chad function)
void kprint(const char* str, uint8_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        kput_char(str[i], color);
    }
}

void kprintnf(const char* str, uint8_t color, int y) {
    int x = 0; // local cursor for this line
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        if (c == '\n') break; // ignore newlines in kprintnf
        if (x >= VGA_WIDTH) break;
        vga_memory[y * VGA_WIDTH + x] = (color << 8) | c;
        x++;
    }
}

// ================== PS/2 keyboard input ==================
// some weird shit I don't understand
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint8_t read_scancode(void) {
    while (!(inb(0x64) & 1)) { } // wait for output buffer full
    return inb(0x60);
}

// just a keyboard keys map (I swiched tab to 'Z')
char scancode_to_ascii(uint8_t sc) {
    static const char map[128] = {
        0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
        'Z','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0,'a','s','d','f','g','h','j','k','l',';','\'','`',
        0,'\\','z','x','c','v','b','n','m',',','.','/',
        0,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
    };

    static const char map_shift[128] = {
        0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
        'Z','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
        0,'A','S','D','F','G','H','J','K','L',':','"','~',
        0,'|','Z','X','C','V','B','N','M','<','>','?',
        0,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
    };

    if (sc == 0x2A || sc == 0x36) { // shift pressed
        shift_pressed = 1;
        return 0;
    }
    if (sc == 0xAA || sc == 0xB6) { // shift released
        shift_pressed = 0;
        return 0;
    }

    if (sc < 128) {
        return shift_pressed ? map_shift[sc] : map[sc];
    }
    return 0;
}

// ================== Read a line ==================
// cooooooakjhvdsgkhnlvjsfkjsghlhskgjfhjgvsdfhjkskhjlgfsfpol function :3
#define HISTORY_SIZE 32
#define MAX_CMD_LEN 256

char history[HISTORY_SIZE][MAX_CMD_LEN];
int history_count = 0;    // number of stored commands
int history_index = 0;    // index for navigating history

void kread_line(char* buffer, int maxlen) {
    int pos = 0;
    int extended = 0;      // for detecting arrow keys
    history_index = history_count; // start at “newest” position

    while (1) {
        uint8_t sc = read_scancode();
        
        // Check for extended code prefix
        if (sc == 0xE0) {
            extended = 1;
            continue; // next scancode will be arrow
        }

        // Handle extended codes (arrows)
        if (extended) {
            extended = 0;
            if (sc == 0x48) { // Up arrow
                if (history_count > 0 && history_index > 0) {
                    history_index--;
                    // Clear current line
                    while (pos > 0) {
                        pos--;
                        cursor_x--;
                        if (cursor_x < 0) cursor_x = VGA_WIDTH - 1;
                        vga_memory[cursor_y * VGA_WIDTH + cursor_x] = (os_color << 8) | ' ';
                    }
                    // Load command from history
                    strncpy(buffer, history[history_index], maxlen);
                    buffer[maxlen-1] = '\0';
                    pos = strlen(buffer);
                    kprint(buffer, os_color);
                }
                continue;
            }
            else if (sc == 0x50) { // Down arrow
                if (history_count > 0 && history_index < history_count - 1) {
                    history_index++;
                    // Clear current line
                    while (pos > 0) {
                        pos--;
                        cursor_x--;
                        if (cursor_x < 0) cursor_x = VGA_WIDTH - 1;
                        vga_memory[cursor_y * VGA_WIDTH + cursor_x] = (os_color << 8) | ' ';
                    }
                    strncpy(buffer, history[history_index], maxlen);
                    buffer[maxlen-1] = '\0';
                    pos = strlen(buffer);
                    kprint(buffer, os_color);
                } else if (history_index == history_count - 1) {
                    // Clear to empty line if at the newest
                    while (pos > 0) {
                        pos--;
                        cursor_x--;
                        if (cursor_x < 0) cursor_x = VGA_WIDTH - 1;
                        vga_memory[cursor_y * VGA_WIDTH + cursor_x] = (os_color << 8) | ' ';
                    }
                    buffer[0] = '\0';
                    pos = 0;
                    history_index = history_count;
                }
                continue;
            }
            continue; // ignore left/right arrows for now
        }

        char c = scancode_to_ascii(sc);
        if (!c) continue;

        // Enter
        if (c == '\n') {
            kput_char('\n', os_color);
            buffer[pos] = '\0';

            // Save to history
            if (pos > 0) {
                if (history_count < HISTORY_SIZE) {
                    strncpy(history[history_count], buffer, MAX_CMD_LEN);
                    history[history_count][MAX_CMD_LEN-1] = '\0';
                    history_count++;
                } else {
                    // Shift older commands up
                    for (int i = 1; i < HISTORY_SIZE; i++)
                        strncpy(history[i-1], history[i], MAX_CMD_LEN);
                    strncpy(history[HISTORY_SIZE-1], buffer, MAX_CMD_LEN);
                    history[HISTORY_SIZE-1][MAX_CMD_LEN-1] = '\0';
                }
            }
            history_index = history_count;
            return;
        }

        // Backspace
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                cursor_x--;
                if (cursor_x < 0) cursor_x = VGA_WIDTH - 1;
                vga_memory[cursor_y * VGA_WIDTH + cursor_x] = (os_color << 8) | ' ';
            }
            continue;
        }

        // Normal character
        if (pos < maxlen - 1) {
            buffer[pos++] = c;
            kput_char(c, os_color);
        }
    }
}

// the same as strcmp but checks if the string1 starts with string2
int starts_with(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

// strlen
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

int strcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;  // mismatch
        i++;
    }
    // both must end at the same time
    return (a[i] == b[i]) ? 1 : 0;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

void shutdown(void) {
    // shutting down (I dunno if it will work on a real computer, it works in qemu for shure)
	kprint("Shutting down...", os_color);
    outw(0x604, 0x2000);
    for (;;); // hang if not powered off
}

typedef struct {
    enum { ARG_STR, ARG_INT } type;
    union {
        char* s;
        int i;
    } v;
} PrintArg;

void kprintf_dynamic(const char* fmt, uint8_t color, PrintArg* list, int count) {
    int idx = 0;

    const char* p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == 's') {
                if (idx < count && list[idx].type == ARG_STR)
                    kprint(list[idx++].v.s, color);
            } else if (*p == 'i') {
                if (idx < count && list[idx].type == ARG_INT) {
                    char buf[32];
                    int v = list[idx++].v.i;

                    int pos = 0, neg = 0;
                    if (v < 0) { neg = 1; v = -v; }

                    if (v == 0) buf[pos++] = '0';
                    else {
                        char tmp[16];
                        int t = 0;
                        while (v) { tmp[t++] = (v % 10) + '0'; v /= 10; }
                        if (neg) buf[pos++] = '-';
                        while (t--) buf[pos++] = tmp[t];
                    }
                    buf[pos] = 0;
                    kprint(buf, color);
                }
            }
            else kput_char(*p, color);
        } else {
            kput_char(*p, color);
        }
        p++;
    }
}

unsigned char hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0; // fallback
}

// outb: write 8-bit value (byte)
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// inw: read 16-bit value (word)
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// outl: write 32-bit value (dword)
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

// inl: read 32-bit value (dword)
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void ata_read28(uint32_t lba, uint8_t *buffer) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);                    // sector count
    outb(0x1F3, (uint8_t) lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20);                 // READ SECTORS
    
    // wait for data
    while (!(inb(0x1F7) & 0x08));

    // read 256 words = 512 bytes
    for (int i = 0; i < 256; i++)
        ((uint16_t*)buffer)[i] = inw(0x1F0);
}

void ata_write28(uint32_t lba, uint8_t *buffer) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t) lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); // WRITE SECTORS

    // Write 256 words immediately
    for (int i = 0; i < 256; i++)
        outw(0x1F0, ((uint16_t*)buffer)[i]);
}

typedef struct {
    uint32_t fat_start;
    uint32_t fat_size;
    uint32_t data_start;
    uint32_t sectors_per_cluster;
} fat32_t;

fat32_t fat;

void fat32_mount() {
    uint8_t sector[512];
    ata_read28(0, sector);
    
	uint16_t reserved = *(uint16_t*)&sector[0x0E];
	uint8_t fats = sector[0x10];
	uint32_t fat_size = *(uint32_t*)&sector[0x24];

	fat.fat_start = reserved;
	fat.fat_size = fat_size;
	fat.data_start = reserved + fats * fat_size;
	fat.sectors_per_cluster = sector[13];
}

struct FileEntry {
    char     name[16];   // "file.txt"
    uint32_t start;      // LBA where data starts
    uint32_t size;       // size in bytes
    uint8_t  used;       // 1 = used, 0 = empty
};

#define MAX_FILES 128
#define FILETABLE_LBA 4096
#define FIRST_DATA_LBA 4104

uint32_t next_free_lba = FIRST_DATA_LBA;

typedef struct {
    char name[16];
    uint32_t start;
    uint32_t size;
    uint8_t used;
    uint8_t _pad[7];
} FileEntry;

FileEntry files[MAX_FILES];

void fs_load() {
    uint8_t buffer[512];

    // Clear files first so unused entries are clean
    memset(files, 0, sizeof(files));

    for (int i = 0; i < 8; i++) {               // 8 sectors = 4096 bytes
        ata_read28(FILETABLE_LBA + i, buffer);
        memcpy(((uint8_t*)files) + i*512, buffer, 512);
    }

    // Ensure every filename is NUL-terminated (defensive)
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].name[15] = '\0'; // ensure last byte is NUL
    }

    // Recompute next_free_lba based on existing files so allocations never overlap
    next_free_lba = FIRST_DATA_LBA;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            uint32_t file_sectors = (files[i].size + 511) / 512;
            uint32_t end_lba = files[i].start + file_sectors;
            if (end_lba > next_free_lba) next_free_lba = end_lba;
        }
    }
}

void fs_save() {
    uint8_t buffer[512];

    for (int i = 0; i < 8; i++) {
        memcpy(buffer, ((uint8_t*)files) + i*512, 512);
        ata_write28(FILETABLE_LBA + i, buffer);
    }
}

uint32_t fs_allocate_sectors(uint32_t sectors) {
    uint32_t start = next_free_lba;
    next_free_lba += sectors;
    return start;
}

static int ata_write_safe(uint32_t lba, uint8_t* buf) {
    int timeout = 1000000;

    // Wait for drive not busy
    while ((inb(0x1F7) & 0x80) && timeout--) { }
    if (timeout <= 0) { kprint("ATA write timeout (BSY)\n", (os_color & 0xF0) | 0x0C); return 0; }

    // Select LBA and sector count
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t) lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); // WRITE SECTOR

    timeout = 1000000;
    // Wait for DRQ = ready for data
    while (!(inb(0x1F7) & 0x08) && timeout--) { }
    if (timeout <= 0) { kprint("ATA write timeout (DRQ)\n", (os_color & 0xF0) | 0x0C); return 0; }

    // Write the 512-byte sector
    for (int i = 0; i < 256; i++)
        outw(0x1F0, ((uint16_t*)buf)[i]);

    timeout = 1000000;
    // Wait for drive to finish writing
    while ((inb(0x1F7) & 0x80) && timeout--) { }
    if (timeout <= 0) { kprint("ATA write timeout after data!\n", (os_color & 0xF0) | 0x0C); return 0; }

    return 1; // success
}

#define MAX_SECTORS 65536 // adjust to your disk size
uint8_t sector_bitmap[MAX_SECTORS / 8]; // 1 bit per sector: 0 = free, 1 = used

// ----------------- bitmap helpers -----------------
void mark_sector(uint32_t lba, int used) {
    uint32_t byte = lba / 8;
    uint8_t bit = 1 << (lba % 8);
    if (used)
        sector_bitmap[byte] |= bit;
    else
        sector_bitmap[byte] &= ~bit;
}

int is_sector_free(uint32_t lba) {
    uint32_t byte = lba / 8;
    uint8_t bit = 1 << (lba % 8);
    return !(sector_bitmap[byte] & bit);
}

// ----------------- scan and allocate -----------------
uint32_t fs_allocate_sectors_safe(uint32_t sectors_needed) {
    for (uint32_t start = FIRST_DATA_LBA; start + sectors_needed < MAX_SECTORS; start++) {
        int free = 1;
        for (uint32_t s = 0; s < sectors_needed; s++) {
            if (!is_sector_free(start + s)) { free = 0; break; }
        }
        if (free) {
            for (uint32_t s = 0; s < sectors_needed; s++)
                mark_sector(start + s, 1);
            return start;
        }
    }
    kprint("No free sectors available!\n", (os_color & 0xF0) | 0x0C);
    return 0;
}

// ----------------- rebuild bitmap after loading -----------------
void fs_build_bitmap() {
    memset(sector_bitmap, 0, sizeof(sector_bitmap));
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            uint32_t sectors = (files[i].size + 511) / 512;
            for (uint32_t s = 0; s < sectors; s++)
                mark_sector(files[i].start + s, 1);
        }
    }
}

void fs_write_file(const char* name, const char* text) {
    // convert "\n" to real newlines
    uint32_t len = 0;
    for (const char* p = text; *p; p++) {
        if (*p == '\\' && *(p+1) == 'n') { len++; p++; }
        else len++;
    }

    char temp[len + 1];
    uint32_t idx = 0;
    for (const char* p = text; *p; p++) {
        if (*p == '\\' && *(p+1) == 'n') { temp[idx++] = '\n'; p++; }
        else temp[idx++] = *p;
    }
    temp[idx] = '\0';

    uint32_t sectors = (len + 511) / 512;
    uint8_t buffer[512];

    // --- check if file exists ---
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 1) {
            uint32_t old_sectors = (files[i].size + 511) / 512;

            if (sectors <= old_sectors) {
                // write in place
                for (uint32_t s = 0; s < sectors; s++) {
                    memset(buffer, 0, 512);
                    uint32_t remaining = len - s*512;
                    if (remaining > 512) remaining = 512;
                    memcpy(buffer, temp + s*512, remaining);
                    if (!ata_write_safe(files[i].start + s, buffer)) return;
                }
                // zero leftover sectors
                for (uint32_t s = sectors; s < old_sectors; s++) {
                    memset(buffer, 0, 512);
                    if (!ata_write_safe(files[i].start + s, buffer)) return;
                    mark_sector(files[i].start + s, 0); // free old sectors
                }
                files[i].size = len;
                fs_save();
                return;
            } else {
                // allocate new sectors safely
                uint32_t new_lba = fs_allocate_sectors_safe(sectors);
                for (uint32_t s = 0; s < sectors; s++) {
                    memset(buffer, 0, 512);
                    uint32_t remaining = len - s*512;
                    if (remaining > 512) remaining = 512;
                    memcpy(buffer, temp + s*512, remaining);
                    if (!ata_write_safe(new_lba + s, buffer)) return;
                }
                // free old sectors
                for (uint32_t s = 0; s < old_sectors; s++)
                    mark_sector(files[i].start + s, 0);
                files[i].start = new_lba;
                files[i].size = len;
                fs_save();
                return;
            }
        }
    }

    // --- new file ---
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            uint32_t lba = fs_allocate_sectors_safe(sectors);
            files[i].used = 1;
            strcpy(files[i].name, name);
            files[i].start = lba;
            files[i].size = len;

            for (uint32_t s = 0; s < sectors; s++) {
                memset(buffer, 0, 512);
                uint32_t remaining = len - s*512;
                if (remaining > 512) remaining = 512;
                memcpy(buffer, temp + s*512, remaining);
                if (!ata_write_safe(lba + s, buffer)) return;
            }
            fs_save();
            return;
        }
    }

    kprint("No free file slots!\n", (os_color & 0xF0) | 0x0C);
}

void fs_dir() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            kprint(files[i].name, os_color);
            kput_char(' ', os_color);
            char size_str[12];
            int sz = files[i].size;
            int pos = 0;
            do {
                size_str[pos++] = '0' + sz % 10;
                sz /= 10;
            } while (sz > 0);
            // reverse
            for (int j = pos-1; j >= 0; j--) kput_char(size_str[j], os_color);
            kput_char('\n', os_color);
        }
    }
}

#define MAX_FILE_PRINT 4096

void fs_read_file(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 1) {
            uint32_t len = files[i].size;
            uint32_t sectors = (len + 511) / 512;
            uint8_t buffer[512];

            for (uint32_t s = 0; s < sectors; s++) {
                ata_read28(files[i].start + s, buffer);
                uint32_t remaining = len - s*512;
                if (remaining > 512) remaining = 512;
                char temp[remaining + 1];
                memcpy(temp, buffer, remaining);
                temp[remaining] = '\0';
                kprint(temp, os_color);
            }
            kput_char('\n', os_color);
            return;
        }
    }
    kprint("File not found!\n", (os_color & 0xF0) | 0x0C);
}

void fs_delete_file(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 1) {
            files[i].used = 0;  // mark as unused
            files[i].name[0] = '\0';
            files[i].size = 0;
            fs_save();          // save the updated file table
            kprint("File deleted successfully!\n", (os_color & 0xF0) | 0x0A);
            return;
        }
    }
    kprint("File not found!\n", (os_color & 0xF0) | 0x0C);
}

#define ZW_LINES 20
#define ZW_WIDTH 80

char zw_buffer[ZW_LINES][ZW_WIDTH + 1]; // +1 for null terminator

void zuros_writer(const char* filename) {
    // Initialize buffer
    for (int i = 0; i < ZW_LINES; i++)
        zw_buffer[i][0] = '\0';

    int running = 1;

    while (running) {
        kclear();
        cursor_y = 20;

        // Draw all 20 lines
        for (int i = 0; i < ZW_LINES; i++)
            kprintnf(zw_buffer[i], os_color, i);

        kprint("######## TYPE EXIT TO SAVE AND LEAVE, DISTRACT TO LEAVE WITHOUT SAVING. ########", (os_color & 0xF0) | 0x09);
        kprint("ZurOS writer >> ", os_color);

        char line_input[ZW_WIDTH + 1];
        kread_line(line_input, ZW_WIDTH + 1);

        // Exit command
        if (starts_with(line_input, "exit")) {
            fs_delete_file(filename);

            // Build zw_text from buffer
            char zw_text[ZW_LINES * (ZW_WIDTH + 1) + 1]; // +1 for null terminator
            int idx = 0;
            for (int i = 0; i < ZW_LINES; i++) {
                for (int j = 0; zw_buffer[i][j]; j++)
                    zw_text[idx++] = zw_buffer[i][j];
                zw_text[idx++] = '\n'; // add line break in buffer only
            }
            zw_text[idx] = '\0';

            char escaped_text[ZW_LINES * (ZW_WIDTH + 1) + 1]; // enough space
            // Escape '\n' only
            int j = 0;
            for (int i = 0; zw_text[i]; i++) {
                if (zw_text[i] == '\n') {
                    escaped_text[j++] = '\\';
                    escaped_text[j++] = 'n';
                } else {
                    escaped_text[j++] = zw_text[i];
                }
            }
            escaped_text[j] = '\0';

            fs_write_file(filename, escaped_text);
            running = 0;
            break;
        } else if (starts_with(line_input, "distract")) {
            running = 0;
            break;
        }

        // Handle "<line number> <text>" input
        int line_num = 0, offset = 0;

        while (line_input[offset] >= '0' && line_input[offset] <= '9') {
            line_num = line_num * 10 + (line_input[offset] - '0');
            offset++;
        }

        if (line_input[offset] != ' ') {
            kprintnf("Invalid command or line number", (os_color & 0xF0) | 0x0C, ZW_LINES + 1);
            continue;
        }

        while (line_input[offset] == ' ') offset++;

        if (line_num >= 1 && line_num <= ZW_LINES) {
            int i = 0;
            while (line_input[offset] && i < ZW_WIDTH)
                zw_buffer[line_num - 1][i++] = line_input[offset++];
            zw_buffer[line_num - 1][i] = '\0';
        } else {
            kprintnf("Invalid line number", (os_color & 0xF0) | 0x0C, ZW_LINES + 1);
        }
    }
}

typedef void (*command_func_t)(const char*);

typedef struct {
    const char* name;
    command_func_t func;
} Commands;

#define MAX_SAVED_FILES 128
#define MAX_FILE_CONTENT 4096

typedef struct {
    char name[16];
    char content[MAX_FILE_CONTENT];
} SavedFile;

SavedFile saved_files[MAX_SAVED_FILES];
int saved_count = 0;

// Save all files into memory and delete them
void backup_and_delete_all_files() {
    fs_load();
    saved_count = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && saved_count < MAX_SAVED_FILES) {
            // Store name
            strncpy(saved_files[saved_count].name, files[i].name, 16);

            // Read file content into memory
            uint32_t len = files[i].size;
            if (len >= MAX_FILE_CONTENT) len = MAX_FILE_CONTENT - 1;

            uint32_t sectors = (len + 511) / 512;
            uint8_t buffer[512];
            uint32_t offset = 0;

            for (uint32_t s = 0; s < sectors; s++) {
                ata_read28(files[i].start + s, buffer);
                uint32_t remaining = len - s*512;
                if (remaining > 512) remaining = 512;
                memcpy(saved_files[saved_count].content + offset, buffer, remaining);
                offset += remaining;
            }
            saved_files[saved_count].content[offset] = '\0';

            saved_count++;

            // Delete the file from FS
            fs_delete_file(files[i].name);
        }
    }
}

// Restore all files from memory
void restore_all_files() {
    for (int i = 0; i < saved_count; i++) {
        fs_write_file(saved_files[i].name, saved_files[i].content);
    }
}

void delay(uint32_t milliseconds) {    volatile uint32_t count;
    for (uint32_t ms = 0; ms < milliseconds; ms++) {
        for (count = 0; count < 100000; count++) {
            __asm__ volatile("nop"); // prevent compiler optimizing it away
        }
    }
}

void handle_command(char* buffer);

// ----------------- Shell variables adsgfadgad -----------------

#define MAX_VARS 64
#define MAX_VAR_NAME 32
#define MAX_VAR_VALUE 256

typedef enum { VAR_INT, VAR_STR } VarType;

typedef struct {
    char name[MAX_VAR_NAME];
    VarType type;
    int int_value;
    char str_value[MAX_VAR_VALUE];
    uint8_t used;
} Variable;

Variable vars[MAX_VARS];

Variable* find_var(const char* name) {
    for (int i = 0; i < MAX_VARS; i++) {
        if (vars[i].used && strcmp(vars[i].name, name)) {
            return &vars[i];
        }
    }
    return NULL;
}

Variable* set_var_int(const char* name, int value) {
    Variable* v = find_var(name);
    if (!v) {
        for (int i = 0; i < MAX_VARS; i++) {
            if (!vars[i].used) {
                v = &vars[i];
                v->used = 1;
                strncpy(v->name, name, MAX_VAR_NAME);
                break;
            }
        }
    }
    if (!v) return NULL;

    v->type = VAR_INT;
    v->int_value = value;
    return v;
}

Variable* set_var_str(const char* name, const char* value) {
    Variable* v = find_var(name);
    if (!v) {
        for (int i = 0; i < MAX_VARS; i++) {
            if (!vars[i].used) {
                v = &vars[i];
                v->used = 1;
                strncpy(v->name, name, MAX_VAR_NAME);
                break;
            }
        }
    }
    if (!v) return NULL;

    v->type = VAR_STR;
    strncpy(v->str_value, value, MAX_VAR_VALUE);
    return v;
}

void cmd_str(char* args) {
    char* p = args;

    // read name
    char name[MAX_VAR_NAME];
    int ni = 0;

    while (*p && *p != ' ' && *p != '=' && ni < MAX_VAR_NAME - 1) {
        name[ni++] = *p++;
    }
    name[ni] = 0;

    if (ni == 0) {
        kprint("missing name\n", os_color);
        return;
    }

    // skip spaces
    while (*p == ' ') p++;

    // require =
    if (*p != '=') {
        kprint("syntax error: expected '='\n", os_color);
        return;
    }
    p++;

    // skip spaces
    while (*p == ' ') p++;

    // require starting quote
    if (*p != '"') {
        kprint("syntax error: expected opening '\"'\n", os_color);
        return;
    }
    p++;

    // read string value
    char value[MAX_VAR_VALUE];
    int vi = 0;

    while (*p && *p != '"' && vi < MAX_VAR_VALUE - 1) {
        value[vi++] = *p++;
    }
    value[vi] = 0;

    if (*p != '"') {
        kprint("syntax error: missing closing '\"'\n", os_color);
        return;
    }

    set_var_str(name, value);
    kprint("ok\n", os_color);
}

void cmd_int(char* args) {
    char* p = args;

    // read name
    char name[MAX_VAR_NAME];
    int ni = 0;

    while (*p && *p != ' ' && *p != '=' && ni < MAX_VAR_NAME - 1) {
        name[ni++] = *p++;
    }
    name[ni] = 0;

    if (ni == 0) {
        kprint("missing name\n", os_color);
        return;
    }

    // skip spaces
    while (*p == ' ') p++;

    // require =
    if (*p != '=') {
        kprint("syntax error: expected '='\n", os_color);
        return;
    }
    p++;

    // skip spaces
    while (*p == ' ') p++;

    // read integer (supports sign)
    int neg = 0;
    int value = 0;

    if (*p == '-') { neg = 1; p++; }

    if (*p < '0' || *p > '9') {
        kprint("syntax error: expected number\n", os_color);
        return;
    }

    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }

    if (neg) value = -value;

    set_var_int(name, value);
    kprint("ok\n", os_color);
}

// ----------------- Command functions -----------------
void cmd_clear(char* args) {
    (void)args;
    kclear();
}

void cmd_help(char* args) {
    (void)args;
    kprint("ZurOS commands list:\n", (os_color & 0xF0) | 0x0A);
    kprint("ascii - prints out an ascii art\n", os_color);
    kprint("beep X Y - plays music from X notes (c-b) or pauses (x), for Yms (1000ms - 1s) separated by ':', for example: \"beep c 100: d 100: e 100: g 250: x 1000: c 100\"", os_color);
    kprint("clear - clears the screen\n", os_color);
    kprint("color 0xXY - sets OS's color\n", os_color);
    kprint("color -themes - shows color themes\n", os_color);
    kprint("dir - lists all files\n", os_color);
    kprint("delete X - deletes file X\n", os_color);
    kprint("exit - shuts down computer\n", os_color);
    kprint("int X = Y - sets X integer variable to Y", os_color);
    kprint("kprint \"X\", Y, Z - prints X in Z color (Z arg is optional) with Y args (for example kprint \"Hello, %s you are %i years old\", name, age, 0x0F)\n", os_color);
    kprint("read X - prints file X\n", os_color);
    kprint("str X = \"Y\" - sets X string variable to \"Y\"", os_color);
    kprint("test - prints test messages\n", os_color);
    kprint("write X Y - writes Y to X file\n", os_color);
    kprint("zscript X - runs X zscript file (.zs) with shell commands inside", os_color);
    kprint("zw X - opens ZurOS writer for file X\n", os_color);
    kprint("Z - very funny polish joke\n", os_color);
}

void cmd_exit(char* args) {
	fs_dir();
	delay(500);
	kclear();
	kprint("Goodbye...", os_color);
	delay(2500);
    (void)args;
    shutdown();
}

void cmd_test(char* args) {
    (void)args;
    for (int i = 0; i < 16; i++) {
        kprint("Hello, World! - 0x", i);
        kput_char("0123456789ABCDEF"[i], i);
        kput_char('\n', i);
    }
}

void cmd_Z(char* args) {
    (void)args;
    kprint("Przychodzi baba do lekarza...\n", os_color);
    kprint("A lekarz tez baba!!!\n", os_color);
}

void cmd_ascii(char* args) {
    (void)args;
    kprint("ASCII art output...\n", os_color); // you can put the full art here
}

void cmd_color(char* args) {
    while (*args == ' ') args++;
    if (*args == '-') {
        if (strcmp(args, "-themes")) {
            kprint("CLASSIC - 0x0F\n", 0x0F);
            kprint("LIGHT MODE - 0xF2\n", 0xF2);
            kprint("TEMPLE - 0xB2\n", 0xB2);
            kprint("UNREADABLE PINK - 0xDF\n", 0xDF);
        }
        return;
    }
    if (args[0] == '0' && (args[1] == 'x' || args[1] == 'X')) {
        args += 2;
        if ((args[0] >= '0' && args[0] <= '9') || (args[0] >= 'A' && args[0] <= 'F') || (args[0] >= 'a' && args[0] <= 'f')) {
            if ((args[1] >= '0' && args[1] <= '9') || (args[1] >= 'A' && args[1] <= 'F') || (args[1] >= 'a' && args[1] <= 'f')) {
                os_color = (hex_to_nibble(args[0]) << 4) | hex_to_nibble(args[1]);
                kclear();
                return;
            }
        }
    }
    kprint("Invalid color format! Use color 0xXY\n", (os_color & 0xF0) | 0x0C);
}

void cmd_dir(char* args) {
    (void)args;
    fs_load();
    fs_dir();
}

void cmd_read(char* args) {
    while (*args == ' ') args++;
    fs_load();
    fs_read_file(args);
}

void cmd_write(char* args) {
    while (*args == ' ') args++;
    char* space = strchr(args, ' ');
    if (!space) {
        kprint("Usage: write <filename> <text>\n", (os_color & 0xF0) | 0x0C);
        return;
    }
    *space = '\0';
    char* filename = args;
    char* text = space + 1;
    while (*text == ' ') text++;
    fs_delete_file(filename);
    fs_write_file(filename, text);
    kprint("File written successfully!\n", (os_color & 0xF0) | 0x0A);
}

void cmd_delete(char* args) {
    while (*args == ' ') args++;
    fs_load();
    fs_delete_file(args);
}

void cmd_zw(char* args) {
    while (*args == ' ') args++;
    if (*args) {
        fs_load();
        zuros_writer(args);
    } else {
        kprint("Usage: zw <filename>\n", (os_color & 0xF0) | 0x0C);
    }
}

static char* skip_leading_whitespace(char* str) {
    while (*str == '\n' || *str == ' ' || *str == '\t') str++;
    return str;
}

void cmd_zscript(char* args) {
    fs_load();
    uint8_t buffer[MAX_FILE_CONTENT];

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, args)) {
            if (files[i].size >= MAX_FILE_CONTENT) return;

            uint32_t len = files[i].size;
            uint32_t sectors = (len + 511)/512;
            uint32_t offset = 0;
            for (uint32_t s = 0; s < sectors; s++) {
                ata_read28(files[i].start + s, buffer);
                uint32_t chunk = len - offset;
                if (chunk > 512) chunk = 512;
                memcpy(saved_files[0].content + offset, buffer, chunk);
                offset += chunk;
            }
            saved_files[0].content[offset] = '\0';

            // Split by ';' and execute each command except "exit"
            char* cmd = saved_files[0].content;
            char* next;
            while ((next = strchr(cmd, ';'))) {
                *next = '\0';
                cmd = skip_leading_whitespace(cmd);

                if (*cmd && !starts_with(cmd, "exit")) {
                    handle_command(cmd);
                }

                cmd = next + 1;
            }

            // Handle the last command
            cmd = skip_leading_whitespace(cmd);
            if (*cmd && !starts_with(cmd, "exit")) {
                handle_command(cmd);
            }

            return;
        }
    }

    kprint("zscript file not found!\n", (os_color & 0xF0) | 0x0C);
}

void cmd_kprint(char* input) {
    const char* ptr = input;

    if (strncmp(ptr, "kprint", 6) != 0)
        return;

    ptr += 6;
    while (*ptr == ' ') ptr++;

    if (*ptr != '"') {
        kprint("err: missing quote\n", os_color);
        return;
    }
    ptr++;

    static char fmt[256];
    int fi = 0;

    while (*ptr && *ptr != '"' && fi < 255) {
        fmt[fi++] = *ptr++;
    }
    fmt[fi] = 0;

    if (*ptr != '"') {
        kprint("err: missing closing quote\n", os_color);
        return;
    }
    ptr++; // skip closing quote

    PrintArg args_list[8];
    int arg_count = 0;

    while (*ptr == ' ') ptr++;
    if (*ptr == ',') ptr++;

    while (*ptr) {
        while (*ptr == ' ') ptr++;

        // stop at color specifier
        if (ptr[0] == '0' && ptr[1] == 'x')
            break;

        // read variable name
        char name[32];
        int ni = 0;
        while (*ptr && *ptr != ',' && *ptr != ' ' && ni < 31)
            name[ni++] = *ptr++;
        name[ni] = 0;

        Variable* v = find_var(name);
        if (!v) {
            kprint("err: unknown var\n", os_color);
            return;
        }

        if (v->type == VAR_STR) {
            args_list[arg_count].type = ARG_STR;
            args_list[arg_count].v.s = v->str_value;
        } else {
            args_list[arg_count].type = ARG_INT;
            args_list[arg_count].v.i = v->int_value;
        }
        arg_count++;

        while (*ptr == ' ') ptr++;
        if (*ptr == ',') ptr++;
        else break;
    }

    uint8_t color = os_color;

    if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
        ptr += 2;
        color = 0;
        while ( (*ptr >= '0' && *ptr <= '9') ||
                (*ptr >= 'A' && *ptr <= 'F') ||
                (*ptr >= 'a' && *ptr <= 'f') ) {

            color <<= 4;
            if (*ptr >= '0' && *ptr <= '9') color += *ptr - '0';
            else if (*ptr >= 'A' && *ptr <= 'F') color += *ptr - 'A' + 10;
            else color += *ptr - 'a' + 10;

            ptr++;
        }
    }

    kprintf_dynamic(fmt, color, args_list, arg_count);
}

static void pcspk_play(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;

    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if (!(tmp & 3)) {
        outb(0x61, tmp | 3);
    }
}

static void pcspk_stop() {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

static uint32_t note_to_freq(char n) {
    switch (n) {
        case 'c': return 261;
        case 'd': return 294;
        case 'e': return 329;
        case 'f': return 349;
        case 'g': return 392;
        case 'a': return 440;
        case 'b': return 494;
    	case 'h': return 494;
        default: return 0;
    }
}

static void play_note(char note, int duration_ms) {
    uint32_t freq = note_to_freq(note);
    if (freq == 0) return;
    pcspk_play(freq);
    delay(duration_ms);
    pcspk_stop();
}

static void play_melody(const char* melody) {
    int i = 0;
    while (melody[i]) {
        // Skip spaces/tabs
        while (melody[i] == ' ' || melody[i] == '\t') i++;

        char note = melody[i];
        if (!note) break;  // end of string

        // Only valid notes or 'x'
        if ((note < 'a' || note > 'h') && note != 'x') {
            i++;
            continue;
        }

        i++; // move past note

        // skip spaces before duration
        while (melody[i] == ' ' || melody[i] == '\t') i++;

        // parse duration
        int duration = 0;
        while (melody[i] >= '0' && melody[i] <= '9') {
            duration = duration * 10 + (melody[i] - '0');
            i++;
        }

        // enforce minimum duration for safety
        if (duration == 0) duration = 100;  // default 100ms if not specified

        // play or rest
        if (note == 'x') {
            delay(duration);
        } else {
            play_note(note, duration);
        }

        // skip until semicolon (optional)
        while (melody[i] && melody[i] != ':') i++;
        if (melody[i] == ':') i++;
    }
}

void cmd_beep(char *args) {
    while (*args == ' ') args++;  // skip leading spaces

    if (strchr(args, ':')) {
        // Already handles full melody
        play_melody(args);
    } else {
        char note = args[0];
        int i = 1;
        while (args[i] == ' ') i++;  // skip space between note and duration

        int ms = 0;
        while (args[i] >= '0' && args[i] <= '9') {
            ms = ms * 10 + (args[i] - '0');
            i++;
        }

        // 'x' outside melody = rest
        if (note == 'x' || note == 'X') {
            if (ms > 0) delay(ms);
        } else {
            if (ms > 0) play_note(note, ms);
        }
    }
}

// ----------------- Command struct -----------------
typedef struct {
    const char* name;
    void (*func)(char*);
} Command;

Command commands[] = {
    {"clear", cmd_clear},
    {"help", cmd_help},
    {"exit", cmd_exit},
    {"test", cmd_test},
    {"Z", cmd_Z},
    {"ascii", cmd_ascii},
    {"color", cmd_color},
    {"dir", cmd_dir},
    {"read", cmd_read},
    {"write", cmd_write},
    {"delete", cmd_delete},
    {"zw", cmd_zw},
    {"kprint", cmd_kprint},
    {"zscript", cmd_zscript},
    {"beep", cmd_beep},
    {"str", cmd_str},
    {"int", cmd_int}
};
const int command_count = sizeof(commands)/sizeof(commands[0]);

#define COMMAND_COUNT (sizeof(commands)/sizeof(commands[0]))

void handle_command(char* buffer) {
    while (*buffer == ' ') buffer++; // skip leading spaces

    for (int i = 0; i < COMMAND_COUNT; i++) {
        size_t len = strlen(commands[i].name);
        if (starts_with(buffer, commands[i].name)) {

            // special case: kprint wants the full buffer
            if (strcmp(commands[i].name, "kprint") == 1) {
                commands[i].func(buffer);  // pass entire buffer
                return;
            }

            // normal case: pass args only
            char* args = buffer + len;
            while (*args == ' ') args++; // skip spaces after command
            commands[i].func(args);
            return;
        }
    }

    kprint("Unknown command: ", os_color);
    kprint(buffer, os_color);
    kput_char('\n', os_color);
}

void kmain(void) {
    os_color = 0x0F;
    kclear();
    delay(2000);

    kprint("Currently running ZurOS.\n", os_color);
    kprint("For help (commands list) write \"help\" and click enter.\n", os_color);
    kprint("Exiting in any other way than typing \"exit\" can cause file corruption and data loss!\n", (os_color & 0xF0) | 0x0C);
    kprint("The file \"autostart.zs\" will always run when ZurOS starts up!\n\n", (os_color & 0xF0) | 0x0C);

    uint8_t sec[512];

    kprint("Mounting FAT32...\n", os_color);
    fat32_mount();
	fs_dir();

    int running = 1;
    char buffer[256];

    //backup_and_delete_all_files();
    //restore_all_files();

    kprint("\nPress enter to continue...", os_color);
    kread_line(buffer, 256);
    kclear();

    handle_command("zscript autostart.zs");

    while (running) {
        kprint("~/[ZurOS >:3]$ ", (os_color & 0xF0) | 0x09);
        kread_line(buffer, 256);

        handle_command(buffer);
    }
    for (;;);
}
