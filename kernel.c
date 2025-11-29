#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

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
    if (cursor_y >= VGA_HEIGHT) {
        cursor_y = 0;
    }
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

// strncpy
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

void echo_kprint(const char* input) {
    const char* prefix = "kprint ";
    size_t prefix_len = 7;

    // Check prefix manually
    int match = 1;
    for (size_t i = 0; i < prefix_len; i++) {
        if (input[i] != prefix[i]) {
            match = 0;
            break;
        }
    }
    if (!match) {
        echo_text[0] = '\0';
        echo_color = os_color;
        return;
    }

    // Skip prefix and spaces
    const char* ptr = input + prefix_len;
    while (*ptr == ' ') ptr++;

    // Expect opening quote
    if (*ptr != '"') {
        echo_text[0] = '\0';
        echo_color = os_color;
        return;
    }
    ptr++; // skip opening quote

    // Extract text until closing quote
    char* out = echo_text;
    while (*ptr && *ptr != '"') {
        if (*ptr == '\\') { // handle escape sequences
            ptr++;
            if (*ptr == 'n') *out++ = '\n';
            else if (*ptr == 't') *out++ = '\t';
            else if (*ptr == '"') *out++ = '"';
            else if (*ptr == '\\') *out++ = '\\';
            else *out++ = *ptr; // unknown escape, copy literally
        } else {
            *out++ = *ptr;
        }
        ptr++;
    }
    *out = '\0';

    // Skip closing quote
    if (*ptr == '"') ptr++;

    // Skip spaces and comma
    while (*ptr == ' ') ptr++;
    if (*ptr != ',') {
        echo_color = os_color; // default color
        return;
    }
    ptr++; // skip comma
    while (*ptr == ' ') ptr++;

    // Parse color (hex) manually
    unsigned char color = os_color;
    if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
        ptr += 2;
        while ((*ptr >= '0' && *ptr <= '9') ||
               (*ptr >= 'A' && *ptr <= 'F') ||
               (*ptr >= 'a' && *ptr <= 'f')) {
            color <<= 4;
            if (*ptr >= '0' && *ptr <= '9') color += *ptr - '0';
            else if (*ptr >= 'A' && *ptr <= 'F') color += *ptr - 'A' + 10;
            else if (*ptr >= 'a' && *ptr <= 'f') color += *ptr - 'a' + 10;
            ptr++;
        }
    }
    echo_color = color;
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

// ----------------- new fs_write_file -----------------
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

    char line_input[ZW_WIDTH + 1];
    int running = 1;

    while (running) {
        kclear();
        cursor_y = 20;

        // Draw all 20 lines
        for (int i = 0; i < ZW_LINES; i++)
            kprintnf(zw_buffer[i], os_color, i);

		kprint("######## TYPE EXIT TO SAVE AND LEAVE, DISTRACT TO LEAVE WITHOUT SAVING. ########", (os_color & 0xF0) | 0x09);
        kprint("ZurOS writer >> ", os_color);

        // Read input
        char line_input[ZW_WIDTH + 1];
        kread_line(line_input, ZW_WIDTH + 1);

        // Exit command
        if (starts_with(line_input, "exit")) {
        	fs_delete_file(filename);
            // Save file on exit
            char zw_text[ZW_LINES * ZW_WIDTH + 1];
            int idx = 0;
            for (int i = 0; i < ZW_LINES; i++) {
                for (int j = 0; zw_buffer[i][j]; j++)
                    zw_text[idx++] = zw_buffer[i][j];
                zw_text[idx++] = '\n';
            }
            zw_text[idx] = '\0';
            fs_write_file(filename, zw_text);
            running = 0;
            break;
        } else if (starts_with(line_input, "distract")) {
            running = 0;
            break;
        }

        // Handle "<line number> <text>" input
        int line_num = 0, offset = 0;

        // Parse digits for line number
        while (line_input[offset] >= '0' && line_input[offset] <= '9') {
            line_num = line_num * 10 + (line_input[offset] - '0');
            offset++;
        }

        // Require at least one space after the number
        if (line_input[offset] != ' ') {
            kprintnf("Invalid command or line number", (os_color & 0xF0) | 0x0C, ZW_LINES + 1);
            continue;
        }

        // Skip all spaces
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

// finally we can run this little goober
void kmain(void) {
	os_color = 0x0F;
	// using such a great function to clear the screen (actully just spam spaces)
    kclear();

	// to print use: text, color
    kprint("Currently running ZurOS.\n", os_color);
    kprint("For help (commands list) write \"help\" and click enter.\n", os_color);
	kprint("Any spaces or tabs before or after the command won't be removed so for example \"	help \" won't do anything at all.\n", os_color);
	kprint("Use clear very often because ZurOS doesn't have any scrolling!\n\n", os_color);

	uint8_t sec[512];
	
	kprint("Mounting FAT32...\n", os_color);
	fat32_mount();

	int running = 1;
    char buffer[256];

    backup_and_delete_all_files();
    restore_all_files();

	kprint("\nPress enter to continue...", os_color);
	kread_line(buffer, 256);
	kclear();
    
	while (running) {
	    kprint("~/[ZurOS >:3]$ ", (os_color & 0xF0) | 0x09);
	    kread_line(buffer, 256);
	    if (strcmp(buffer, "exit")) {
	    	shutdown();
	        running = 0;
	    } else if (strcmp(buffer, "test")) {
	    	kprint("Hello, World! - 0x00\n", 0x00);
	    	kprint("Hello, World! - 0x01\n", 0x01);
	    	kprint("Hello, World! - 0x02\n", 0x02);
	    	kprint("Hello, World! - 0x03\n", 0x03);
	    	kprint("Hello, World! - 0x04\n", 0x04);
	    	kprint("Hello, World! - 0x05\n", 0x05);
	    	kprint("Hello, World! - 0x06\n", 0x06);
	    	kprint("Hello, World! - 0x07\n", 0x07);
	    	kprint("Hello, World! - 0x08\n", 0x08);
	    	kprint("Hello, World! - 0x09\n", 0x09);
	    	kprint("Hello, World! - 0x0A\n", 0x0A);
	    	kprint("Hello, World! - 0x0B\n", 0x0B);
	    	kprint("Hello, World! - 0x0C\n", 0x0C);
	    	kprint("Hello, World! - 0x0D\n", 0x0D);
	    	kprint("Hello, World! - 0x0E\n", 0x0E);
	    	kprint("Hello, World! - 0x0F\n", 0x0F);
	    } else if (strcmp(buffer, "clear")) {
	    	kclear();
	    } else if (strcmp(buffer, "Z")) {
	    	kprint("Przychodzi baba do lekarza...\n", os_color);
	    	kprint("A lekarz tez baba!!!\n", os_color);
	    } else if (starts_with(buffer, "color -themes")) {
	    	        kprint("CLASSIC - 0x0F\n", 0x0F);
	    	        kprint("LIGHT MODE - 0xF2\n", 0xF2);
	    	        kprint("TEMPLE - 0xB2\n", 0xB2);
	    	        kprint("UNREADABLE PINK - 0xDF\n", 0xDF);
	    } else if (strcmp(buffer, "ascii")) {
	    	    	        kprint("..........-+-:::::::::::::::::::::::::--\n", os_color);
	    	    	        kprint(".........:=:---+:::::::::::::::--:::=###\n", os_color);
	    	    	        kprint(".........:+:+----*::::::::-====*::::::##\n", os_color);
	    	    	        kprint(":::......:*+:=================+::*-:::##\n", os_color);
	    	    	        kprint(":.........=:-=+==============:==::::::::\n", os_color);
	    	    	        kprint(":::::::..:#*=======+=========:-:::::::::\n", os_color);
	    	    	        kprint(".........-:*::+============+==::::-::+::\n", os_color);
	    	    	        kprint(".........::=::==+=::*#==*=-=+=-+::+:::=:\n", os_color);
	    	    	        kprint(".:::-----::::++=+*%%::::::#-*===:::-::::\n", os_color);
	    	    	        kprint("--------*-#-=====:=-:::::*+:===:=+-::::-\n", os_color);
	    	    	        kprint("++++++++*#======*=-::::::::-=#=#*=#+**##\n", os_color);
	    	    	        kprint("*#********+#=+%#=+::+=%%=:-*==#####*+###\n", os_color);
	    	    	        kprint("*****#***##:::%%+#::::::::%%-:#####***%%\n", os_color);
	    	    	        kprint("*********#%-::#%%*++#--#**%%::****#%*%%%\n", os_color);
	    	    	        kprint("****#**%####::=%%%%%*+%%%%%=:#####*##%%%\n", os_color);
	    	    	        kprint("***###%%#%%#-::#%%%%%%%%%%#:-*####*#%%%%\n", os_color);
	    	    	        kprint("##***#-%%%=----+--#%#%%#+-:-::-*::+%%%%%\n", os_color);
	    	    	        kprint("****-:::::::::::-=+#%%%--::++=:::-=----+\n", os_color);
	    	    	        kprint("%%%------::::----#*%%=%%*---::::-+----++\n", os_color);
	   	} else if (starts_with(buffer, "color ")) {
	        // Skip "color " prefix
	        const char* ptr = buffer + 6;
	    
	        // Expect "0x" or "0X"
	        if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
	            ptr += 2;
	            // Expect exactly two hex digits
	            if ((ptr[0] >= '0' && ptr[0] <= '9') || (ptr[0] >= 'A' && ptr[0] <= 'F') || (ptr[0] >= 'a' && ptr[0] <= 'f')) {
	                if ((ptr[1] >= '0' && ptr[1] <= '9') || (ptr[1] >= 'A' && ptr[1] <= 'F') || (ptr[1] >= 'a' && ptr[1] <= 'f')) {
	                    os_color = (hex_to_nibble(ptr[0]) << 4) | hex_to_nibble(ptr[1]);
	                    kclear();
	                    continue;
	                }
	            }
	        }
	        kprint("Invalid color format! Use color 0xXY\n", (os_color & 0xF0) | 0x0C);
	    } else if (strcmp(buffer, "help")) {
	    	kprint("ZurOS commands list:\n", (os_color & 0xF0) | 0x0A);
	    	kprint("ascii - prints out an ascii art of Felix Argyle\n", os_color);
	    	kprint("clear - clears the screen\n", os_color);
	    	kprint("color 0xXY - sets OS's color to 0xXY (example usage: color 0x0F, color 0x92, color 0xDF. For color codes 0x00-0x0F type test. You can use color codes in first 0 after x in the same way as in the second one, for example 0x0F is white text on black background and 0xF0 is black text on white background)\n", os_color);
	    	kprint("color -themes - some cool color themes just for you\n", os_color);
	    	kprint("dir - prints out list of all files saved on ZurOS's hdd.img\n", os_color);
	    	kprint("delete X - deletes X file (example usage: delete not_important.txt)\n", os_color);
	    	kprint("exit - shutdowns the computer\n", os_color);
	    	kprint("help - prints out the list you're currently reading\n", os_color);
	    	kprint("kprint \"X\", 0xYZ - uses kernel's kprint function, for more info type kprint -help\n", os_color);
	    	kprint("read X - prints out content of X file (example usage: read important.txt)\n", os_color);
	    	kprint("test - prints out \"Hello, World!\"\n", os_color);
	    	kprint("write X Y - writes Y to X file (example usage: write important.txt Hello, World!)\n", os_color);
	    	kprint("zw X - writes into X file with ZurOS writer (for more info: zw -help | example usage: zw important.txt)\n", os_color);
	    	kprint("Z - very funny polish joke\n", os_color);
	    } else if (starts_with(buffer, "kprint -help")) {
	    	        kprint("How to use kernel's kprint function?\nkprint \"X\", YZ - prints text wrote in X with 0xYZ color\nSome examples: kprint \"Hello, World!\", 0x0F, kprint \"I like trains!\", 0x93\n", os_color);
	   	} else if (starts_with(buffer, "kprint ")) {
	        echo_kprint(buffer);
	        kprint(echo_text, echo_color);
		} else if (strcmp(buffer, "dir")) {
			fs_load();
		    fs_dir();
		} else if (starts_with(buffer, "read ")) {
		    fs_load();
		    char* name = buffer + 5; // skip "read "
		    fs_read_file(name);
		} else if (starts_with(buffer, "write ")) {
			fs_load();
		
		    char* args = buffer + 6;  // skip "write "
		    while (*args == ' ') args++; // skip leading spaces

		    // find first space separating filename from text
		    char* space = strchr(args, ' ');
		    if (!space) {
		        kprint("Usage: write <filename> <text>\n", (os_color & 0xF0) | 0x0C);
		        continue;
		    }

		    *space = '\0';         // terminate filename
		    char* name = args;     // filename
		    char* text = space + 1;
		    fs_delete_file(name);
		    while (*text == ' ') text++; // skip spaces before content

		    if (name && *text) {
		        fs_write_file(name, text);
		        kprint("File written successfully!\n", (os_color & 0xF0) | 0x0A);
		    } else {
		        kprint("Usage: write <filename> <text>\n", (os_color & 0xF0) | 0x0C);
		    }
		    text = "";
		    name = "";
	   	} else if (starts_with(buffer, "delete ")) {
	   	    fs_load();
	   	    char* name = buffer + 7;  // skip "delete "
	   	    fs_delete_file(name);
	   	} else if (starts_with(buffer, "zw -help")) {
			kprint("More about ZurOS writer\nZurOS writer, zw in short is a simple text editor with max 20 lines\nAt the bottom of the screen there will be a command prompt for zw, list of commands:\nX(int) Y(str) - writes Y to X line (\"3 Hello, World!\" will write \"Hello, World!\" at the 3th line)\nexit - exits and saves the file\ndistract - exits without saving\n", os_color);
	   	} else if (starts_with(buffer, "zw ")) {
	   	    char* filename = buffer + 3; // skip "zw "
	   	    // optional: skip leading spaces
	   	    while (*filename == ' ') filename++;
	   	    if (*filename) {
	   	        fs_load();               // make sure file table is up to date
	   	        zuros_writer(filename);  // open the file in the editor
	   	    } else {
	   	        kprint("Usage: zw <filename>\n", (os_color & 0xF0) | 0x0C);
	   	    }
	   	} else {
	        kprint("Unknown command: ", os_color);
	        kprint(buffer, os_color);
	        kput_char('\n', os_color);
	    }
	}

    for (;;);
}
