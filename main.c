#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#define LOG(...) if (verbose_flag) { printf(__VA_ARGS__); }

const size_t MAX_FILES = 1;

int verbose_flag = 0;
int debug_flag = 0;
int stdout_flag = 0;
FILE *files[MAX_FILES] = {NULL};

struct CGEventContext {
    FILE *file;
    UCKeyboardLayout *keyboard_layout;
    long long last_millis;
};

const UniCharCount MAX_STRING_LENGTH = 4;
const long long LINGER_MS = 500;
const unsigned char CTRL[] = {0xE2, 0x8C, 0x83, 0x00};
const unsigned char OPT[] = {0xE2, 0x8C, 0xA5, 0x00};
const unsigned char SHIFT[] = {0xE2, 0x87, 0xA7, 0x00};
const unsigned char CMD[] = {0xE2, 0x8C, 0x98, 0x00};
const unsigned char RETURN[] = {0xE2, 0x86, 0xA9, 0x00};
const unsigned char TAB[] = {0xE2, 0x87, 0xA5, 0x00};
const unsigned char SPACE[] = {0xE2, 0x90, 0xA3, 0x00};
const unsigned char ESCAPE[] = {0xE2, 0x8E, 0x8B, 0x00};
const unsigned char DELETE[] = {0xE2, 0x8C, 0xAB, 0x00};
const unsigned char LEFT_ARROW[] = {0xE2, 0x86, 0x90, 0x00};
const unsigned char RIGHT_ARROW[] = {0xE2, 0x86, 0x92, 0x00};
const unsigned char DOWN_ARROW[] = {0xE2, 0x86, 0x91, 0x00};
const unsigned char UP_ARROW[] = {0xE2, 0x86, 0x93, 0x00};
const unsigned char FORWARD_DELETE[] = {0xE2, 0x8C, 0xA6, 0x00};
const unsigned char HOME[] = {0xE2, 0x86, 0x96, 0x00};
const unsigned char END[] = {0xE2, 0x86, 0x98, 0x00};
const unsigned char PAGE_UP[] = {0xE2, 0x87, 0x9E, 0x00};
const unsigned char PAGE_DOWN[] = {0xE2, 0x87, 0x9F, 0x00};
const unsigned char NO_CHAR[] = {0x00};

void flush_files(int signum) {
    for (size_t i = 0; i < MAX_FILES; i++) {
        FILE *file = files[i];
        if (file != NULL) {
            fflush(file);
        }
    }
    exit(signum);
}

size_t copy_uni_char_to_utf8_buffer(const UniChar ch, unsigned char *dest) {
    if (ch < 0x80) {
        dest[0] = (unsigned char)ch;
        return 1;
    } else if (ch < 0x800) {
        dest[0] = 0xC0 | (ch >> 6);
        dest[1] = 0x80 | (ch & 0x3F);
        return 2;
    } else {
        dest[0] = 0xE0 | (ch >> 12);
        dest[1] = 0x80 | ((ch >> 6) & 0x3F);
        dest[2] = 0x80 | (ch & 0x3F);
        return 3;
    }
}

size_t copy_utf8_char_to_utf8_buffer(const unsigned char *ch, unsigned char *dest) {
    strcpy((char *)dest, (char *)ch);
    return strlen((char *)ch);
}

size_t copy_special_char_to_utf8_buffer(const int key_code, unsigned char *dest) {
    switch (key_code) {
        case 36:
            return copy_utf8_char_to_utf8_buffer(RETURN, dest);
        case 48:
            return copy_utf8_char_to_utf8_buffer(TAB, dest);
        case 49:
            return copy_utf8_char_to_utf8_buffer(SPACE, dest);
        case 51:
            return copy_utf8_char_to_utf8_buffer(DELETE, dest);
        case 53:
            return copy_utf8_char_to_utf8_buffer(ESCAPE, dest);
        case 115:
            return copy_utf8_char_to_utf8_buffer(HOME, dest);
        case 116:
            return copy_utf8_char_to_utf8_buffer(PAGE_UP, dest);
        case 117:
            return copy_utf8_char_to_utf8_buffer(FORWARD_DELETE, dest);
        case 119:
            return copy_utf8_char_to_utf8_buffer(END, dest);
        case 121:
            return copy_utf8_char_to_utf8_buffer(PAGE_DOWN, dest);;
        case 123:
            return copy_utf8_char_to_utf8_buffer(LEFT_ARROW, dest);
        case 124:
            return copy_utf8_char_to_utf8_buffer(RIGHT_ARROW, dest);
        case 125:
            return copy_utf8_char_to_utf8_buffer(DOWN_ARROW, dest);
        case 126:
            return copy_utf8_char_to_utf8_buffer(UP_ARROW, dest);
        default:
            return 0;
    }
}

void print_buffer(const unsigned char *buffer, const size_t length) {
    printf("  %p (len %zu): ", (void *)buffer, length);
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == '\0') {
            break;
        }
        printf("%02x ", buffer[i]);
    }
    printf("\n");
}

void handle_button_event(const CGEventType type,
                         const CGEventFlags flags,
                         const CGKeyCode key_code,
                         UniChar *unicode_string,
                         struct CGEventContext *context) {
    unsigned char stdout_buffer[1024] = {0};
    unsigned char bin_buffer[1024] = {0};
    size_t i = 0;
    size_t j = 0;
    int offset = 0;

    struct timeval tv;
    int ret;

    ret = gettimeofday(&tv, NULL);
    if (ret == -1) {
        fprintf(stderr, "ERROR: Unable to get time of day.\n");
    } else {
        // Print a newline if we've been idle for a while
        long long millis = (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
        if (millis - context->last_millis > LINGER_MS) {
            stdout_buffer[i++] = '\n';
            offset = 1;
        }
        // Memcpy millis to bin_buffer
        memcpy(bin_buffer, &millis, sizeof(millis));
        j += sizeof(millis);
        memcpy(bin_buffer + j, &flags, sizeof(flags));
        j += sizeof(flags);
        memcpy(bin_buffer + j, &key_code, sizeof(key_code));
        j += sizeof(key_code);
        context->last_millis = millis;
    }

    // Print modifier keys
    if ((flags & kCGEventFlagMaskControl) != 0) {
        i += copy_utf8_char_to_utf8_buffer(CTRL, stdout_buffer + i);
    }
    if ((flags & kCGEventFlagMaskAlternate) != 0) {
        i += copy_utf8_char_to_utf8_buffer(OPT, stdout_buffer + i);
    }
    if (!isprint(unicode_string[0]) && (flags & kCGEventFlagMaskShift) != 0) {
        i += copy_utf8_char_to_utf8_buffer(SHIFT, stdout_buffer + i);
    }
    if ((flags & kCGEventFlagMaskCommand) != 0) {
        i += copy_utf8_char_to_utf8_buffer(CMD, stdout_buffer + i);
    }

    // Check if the keycode has a special Mac-style character
    size_t special_length = copy_special_char_to_utf8_buffer(key_code, stdout_buffer + i);
    if (special_length == 0) {
        i += copy_uni_char_to_utf8_buffer(unicode_string[0], stdout_buffer + i);
    } else  {
        i += special_length;
    }

    memcpy(bin_buffer + j, stdout_buffer + offset, (i - offset) * sizeof(unsigned char));
    j += i - offset;
    bin_buffer[j++] = '\n';
    FILE *file = context->file;
    if (file != NULL) {
        if (debug_flag) {
            for (size_t k = 0; k < j; k++) {
                printf("%02x ", bin_buffer[k]);
            }
            printf("\n");
        }
        fwrite(bin_buffer, sizeof(bin_buffer[0]), j, context->file);
    }

    if (stdout_flag && type == kCGEventKeyDown) {
        printf("%s", stdout_buffer);
    }
}

CGEventRef cg_event_callback(CGEventTapProxy proxy __attribute__((unused)),
                             CGEventType type,
                             CGEventRef event,
                             void *user_info) {
    // Validate the input event
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return event;
    }

    struct CGEventContext *context = (struct CGEventContext *)user_info;

    // Retrieve the key code
    CGKeyCode key_code = (CGKeyCode) CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    // Detect modifier keys
    const CGEventFlags flags = CGEventGetFlags(event);
    UInt32 modifier_key_state = (flags >> 16) & 0x02; // only care about shift

    // Determine keyboard layout
    TISInputSourceRef current_keyboard = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef layout_data = TISGetInputSourceProperty(current_keyboard, kTISPropertyUnicodeKeyLayoutData);
    const UCKeyboardLayout *keyboard_layout = (UCKeyboardLayout *)CFDataGetBytePtr(layout_data);

    static UInt32 dead_key_state = 0;
    UniCharCount actual_string_length = 0;
    UniChar unicode_string[MAX_STRING_LENGTH];
    OSStatus status = UCKeyTranslate(keyboard_layout,
                                     key_code,
                                     kUCKeyActionDisplay,
                                     modifier_key_state,
                                     LMGetKbdType(),
                                     kUCKeyTranslateNoDeadKeysBit,
                                     &dead_key_state,
                                     MAX_STRING_LENGTH,
                                     &actual_string_length,
                                     unicode_string);
    if (status != noErr) {
        fprintf(stderr,
                "ERROR: Unable to translate key code to Unicode string: %d\n",
                (int)status);
        return event;
    }

    handle_button_event(type, flags, key_code, unicode_string, context);

    return event;
}

void listen(FILE *file) {
    // Determine keyboard layout
    TISInputSourceRef current_keyboard = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef layout_data = TISGetInputSourceProperty(current_keyboard, kTISPropertyUnicodeKeyLayoutData);
    UCKeyboardLayout *keyboard_layout = (UCKeyboardLayout *)CFDataGetBytePtr(layout_data);
    struct CGEventContext user_info = {file, keyboard_layout, 0};

    CGEventMask event_mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);

    CFMachPortRef event_tap = CGEventTapCreate(kCGSessionEventTap,
                                               kCGHeadInsertEventTap,
                                               0,
                                               event_mask,
                                               cg_event_callback,
                                               &user_info);

    if (!event_tap) {
        fprintf(stderr, "ERROR: Unable to create event tap.");
        exit(1);
    }

    CFRunLoopSourceRef run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       run_loop_source,
                       kCFRunLoopCommonModes);
    CGEventTapEnable(event_tap, true);

    CFRunLoopRun();
}

void print_usage(FILE *stream, const char *program_name) {
    fprintf(stream, "Usage: %s [-sfvh] [-o file]\n", program_name);
    fprintf(stream, "Options:\n");
    fprintf(stream, "  -s, --stdout         Print to stdout even if writing to a file\n");
    fprintf(stream, "  -f, --flush          Flush to file after every keypress\n");
    fprintf(stream, "  -v, --verbose        Print verbose output\n");
    fprintf(stream, "  -d, --debug          Print debug output\n");
    fprintf(stream, "  -h, --help           Print this help message\n");
    fprintf(stream, "  -o, --output FILE    Write to file instead of stdout\n");
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, flush_files);

    int opt;
    int flush_flag = 0;
    char *output_file = NULL;

    struct option long_options[] = {
        {"stdout", no_argument, NULL, 0},
        {"flush", no_argument, NULL, 0},
        {"verbose", no_argument, NULL, 0},
        {"debug", no_argument, NULL, 0},
        {"output", required_argument, NULL, 0},
        {"help", no_argument, NULL, 0},
        {NULL, 0, NULL, 0}
    };


    while ((opt = getopt_long(argc, argv, "sfvdo:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 's':
                stdout_flag = 1;
                break;
            case 'f':
                flush_flag = 1;
                break;
            case 'v':
                verbose_flag = 1;
                break;
            case 'd':
                debug_flag = 1;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'h':
                print_usage(stdout, argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(stderr, argv[0]);
                return EXIT_FAILURE;
        }
    }

    FILE *file;
    if (output_file != NULL) {
        LOG("Appending to binary file %s\n", output_file);
        file = fopen(output_file, "ab");
        files[0] = file;
        if (file == NULL) {
            fprintf(stderr, "ERROR: Unable to open %s for writing.\n", output_file);
            return EXIT_FAILURE;
        }
        if (flush_flag) {
            LOG("Flushing to file after every keypress\n");
            setbuf(file, NULL);
        }
    } else {
        file = NULL;
        stdout_flag = 1;
    }

    if (stdout_flag) {
        LOG("Printing to stdout and flushing after every keypress\n");
        setbuf(stdout, NULL);
    }

    listen(file);
    fclose(file);
    return EXIT_SUCCESS;
}
