#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

volatile uint16_t* vga_memory = (volatile uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

// ================== VGA output ==================
// as the name suggests it just clears the screen
void kclear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_memory[y * VGA_WIDTH + x] = (0x0F << 8) | ' ';
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

// just a keyboard (I swiched tab to 'Z')
char scancode_to_ascii(uint8_t sc) {
    static const char map[128] = {
        0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
        'Z','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0,'a','s','d','f','g','h','j','k','l',';','\'','`',
        0,'\\','z','x','c','v','b','n','m',',','.','/',
        0,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
    };
    if (sc < 128) return map[sc];
    return 0;
}

// ================== Read a line ==================
// cooooooakjhvdsgkhnlvjsfkjsghlhskgjfhjgvsdfhjkskhjlgfsfpol function :3
void kread_line(char* buffer, int maxlen) {
    int pos = 0;
    while (1) {
        uint8_t sc = read_scancode();
        char c = scancode_to_ascii(sc);
        if (!c) continue;

        if (c == '\n') {
            kput_char('\n', 0x0F);
            buffer[pos] = '\0';
            return;
        }

        if (c == '\b') {
            if (pos > 0) {
                pos--;
                cursor_x--;
                if (cursor_x < 0) cursor_x = VGA_WIDTH - 1;
                vga_memory[cursor_y * VGA_WIDTH + cursor_x] = (0x0F << 8) | ' ';
            }
            continue;
        }

        if (pos < maxlen - 1) {
            buffer[pos++] = c;
            kput_char(c, 0x0F);
        }
    }
}

// return 1 if equal, 0 otherwise
// for string comparing btw
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
	kprint("Shutting down...", 0x0F);
    outw(0x604, 0x2000);
    for (;;); // hang if not powered off
}

// finally we can run this little goober
void kmain(void) {
	// using such a great function to clear the screen (actully just spam spaces)
    kclear();

	// to print use: text, color
    kprint("Currently running ZurOS.\n", 0x0F);
    kprint("For help (commands list) write \"help\" and click enter.\n", 0x0F);
	kprint("Any spaces or tabs before or after the command won't be removed so for example \"	help \" won't do anything at all.\n", 0x0F);
	kprint("You can use tab key to print 'Z'.\n", 0x0F);
	kprint("Use clear very often because ZurOS doesn't have any scrolling!\n\n", 0x0C);
	
	int running = 1;
    char buffer[256];
    
	while (running) {
	    kprint("~/[ZurOS >:3]$ ", 0x0A);
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
	    	kprint("Przychodzi baba do lekarza...\n", 0x0A);
	    	kprint("A lekarz tez baba!!! \\(^o^)/\n", 0x0B);
	    } else if (strcmp(buffer, "help")) {
	    	kprint("ZurOS commands list:\n", 0x0F);
	    	kprint("help - prints out the list you're currently reading\nclear - clears the screen\ntest - prints out \"Hello, World!\"\nexit - shutdowns the computer\nZ - very funny polish joke\n", 0x0F);
	    } else {
	        kprint("Unknown command: ", 0x0F);
	        kprint(buffer, 0x0F);
	        kput_char('\n', 0x0F);
	    }
	}

    for (;;);
}
