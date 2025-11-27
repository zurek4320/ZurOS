#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

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

// prints out a bunch of single characters (chad function)
void kprint(const char* str, uint8_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        kput_char(str[i], color);
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
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
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

// finally we can run this little goober
void kmain(void) {
	os_color = 0x0F;
	// using such a great function to clear the screen (actully just spam spaces)
    kclear();

	// to print use: text, color
    kprint("Currently running ZurOS.\n", os_color);
    kprint("For help (commands list) write \"help\" and click enter.\n", os_color);
	kprint("Any spaces or tabs before or after the command won't be removed so for example \"	help \" won't do anything at all.\n", os_color);
	kprint("You can use tab key to print 'Z' and ctrl key to print 'R'.\n", os_color);
	kprint("Use clear very often because ZurOS doesn't have any scrolling!\n\n", os_color);
	
	int running = 1;
    char buffer[256];
    
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
	    	kprint("A lekarz tez baba!!! \\(^o^)/\n", os_color);
	    } else if (starts_with(buffer, "color -themes")) {
	    	        kprint("CLASSIC - 0x0F\n", 0x0F);
	    	        kprint("LIGHT MODE - 0xF2\n", 0xF2);
	    	        kprint("TEMPLE - 0xB2\n", 0xB2);
	    	        kprint("SILLY UNREADABLE PINK - 0xDF\n", 0xDF);
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
	    	kprint("ascii - prints out an ascii art of Felix Argyle\nclear - clears the screen\ncolor 0xXY - sets OS's color to 0xXY (example usage: color 0x0F, color 0x92, color 0xDF. For color codes 0x00-0x0F type test. You can use color codes in first 0 after x in the same way as in the second one, for example 0x0F is white text on black background and 0xF0 is black text on white background)\ncolor -themes - some cool color themes just for you\nexit - shutdowns the computer\nhelp - prints out the list you're currently reading\nkprint \"X\", 0xYZ - uses kernel's kprint function, for more info type kprint -help\ntest - prints out \"Hello, World!\"\nZ - very funny polish joke\n", os_color);
	    } else if (starts_with(buffer, "kprint -help")) {
	    	        kprint("How to use kernel's kprint function?\nkprint \"X\", YZ - prints text wrote in X with 0xYZ color\nSome examples: kprint \"Hello, World!\n\", 0x0F, kprint \"I like trains!\n\", 0x93", os_color);
	   	} else if (starts_with(buffer, "kprint ")) {
	        echo_kprint(buffer);
	        kprint(echo_text, echo_color);
	   	}  else {
	        kprint("Unknown command: ", os_color);
	        kprint(buffer, os_color);
	        kput_char('\n', os_color);
	    }
	}

    for (;;);
}
