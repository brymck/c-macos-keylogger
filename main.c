#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>

struct CGEventContext {
    FILE *file;
    UCKeyboardLayout *keyboardLayout;
    long long lastMillis;
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

size_t copyUniCharToUtf8Buffer(const UniChar ch, unsigned char *dest) {
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

size_t copyUtf8CharToUtf8Buffer(const unsigned char *ch, unsigned char *dest) {
    strcpy((char *)dest, (char *)ch);
    return strlen((char *)ch);
}

size_t copySpecialCharToUtf8Buffer(const int keyCode, unsigned char *dest) {
    switch (keyCode) {
        case 36:
            return copyUtf8CharToUtf8Buffer(RETURN, dest);
        case 48:
            return copyUtf8CharToUtf8Buffer(TAB, dest);
        case 49:
            return copyUtf8CharToUtf8Buffer(SPACE, dest);
        case 51:
            return copyUtf8CharToUtf8Buffer(DELETE, dest);
        case 53:
            return copyUtf8CharToUtf8Buffer(ESCAPE, dest);
        case 115:
            return copyUtf8CharToUtf8Buffer(HOME, dest);
        case 116:
            return copyUtf8CharToUtf8Buffer(PAGE_UP, dest);
        case 117:
            return copyUtf8CharToUtf8Buffer(FORWARD_DELETE, dest);
        case 119:
            return copyUtf8CharToUtf8Buffer(END, dest);
        case 121:
            return copyUtf8CharToUtf8Buffer(PAGE_DOWN, dest);;
        case 123:
            return copyUtf8CharToUtf8Buffer(LEFT_ARROW, dest);
        case 124:
            return copyUtf8CharToUtf8Buffer(RIGHT_ARROW, dest);
        case 125:
            return copyUtf8CharToUtf8Buffer(DOWN_ARROW, dest);
        case 126:
            return copyUtf8CharToUtf8Buffer(UP_ARROW, dest);
        default:
            return 0;
    }
}

void printBuffer(const unsigned char *buffer, const size_t length) {
    printf("  %p (len %zu): ", (void *)buffer, length);
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == '\0') {
            break;
        }
        printf("%02x ", buffer[i]);
    }
    printf("\n");
}

void handleButtonEvent(const CGEventType type,
                       const CGEventFlags flags,
                       const CGKeyCode keyCode,
                       UniChar *unicodeString,
                       struct CGEventContext *context) {
    unsigned char stdoutBuffer[1024] = {0};
    unsigned char binBuffer[1024] = {0};
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
        if (millis - context->lastMillis > LINGER_MS) {
            stdoutBuffer[i++] = '\n';
            offset = 1;
        }
        // Memcpy millis to binBuffer
        memcpy(binBuffer, &millis, sizeof(millis));
        j += sizeof(millis);
        memcpy(binBuffer + j, &flags, sizeof(flags));
        j += sizeof(flags);
        memcpy(binBuffer + j, &keyCode, sizeof(keyCode));
        j += sizeof(keyCode);
        context->lastMillis = millis;
    }

    // Print modifier keys
    if ((flags & kCGEventFlagMaskControl) != 0) {
        i += copyUtf8CharToUtf8Buffer(CTRL, stdoutBuffer + i);
    }
    if ((flags & kCGEventFlagMaskAlternate) != 0) {
        i += copyUtf8CharToUtf8Buffer(OPT, stdoutBuffer + i);
    }
    if (!isprint(unicodeString[0]) && (flags & kCGEventFlagMaskShift) != 0) {
        i += copyUtf8CharToUtf8Buffer(SHIFT, stdoutBuffer + i);
    }
    if ((flags & kCGEventFlagMaskCommand) != 0) {
        i += copyUtf8CharToUtf8Buffer(CMD, stdoutBuffer + i);
    }

    // Check if the keycode has a special Mac-style character
    size_t specialLength = copySpecialCharToUtf8Buffer(keyCode, stdoutBuffer + i);
    if (specialLength == 0) {
        i += copyUniCharToUtf8Buffer(unicodeString[0], stdoutBuffer + i);
    } else  {
        i += specialLength;
    }

    memcpy(binBuffer + j, stdoutBuffer + offset, (i - offset) * sizeof(unsigned char));
    j += i - offset;
    binBuffer[j++] = '\n';
    fwrite(binBuffer, sizeof(binBuffer[0]), j, context->file);
    fflush(context->file);

    if (type == kCGEventKeyDown) {
        printf("%s", stdoutBuffer);
    }
}

CGEventRef CGEventCallback(CGEventTapProxy proxy __attribute__((unused)),
                           CGEventType type,
                           CGEventRef event,
                           void *userInfo) {
    // Validate the input event
    if (type != kCGEventKeyDown && type != kCGEventKeyUp) {
        return event;
    }

    struct CGEventContext *context = (struct CGEventContext *)userInfo;

    // Retrieve the key code
    CGKeyCode keyCode = (CGKeyCode) CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    // Detect modifier keys
    const CGEventFlags flags = CGEventGetFlags(event);
    UInt32 modifierKeyState = (flags >> 16) & 0x02; // only care about shift

    // Determine keyboard layout
    TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef layoutData = TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
    const UCKeyboardLayout *keyboardLayout = (UCKeyboardLayout *)CFDataGetBytePtr(layoutData);

    static UInt32 deadKeyState = 0;
    UniCharCount actualStringLength = 0;
    UniChar unicodeString[MAX_STRING_LENGTH];
    OSStatus status = UCKeyTranslate(keyboardLayout,
                                     keyCode,
                                     kUCKeyActionDisplay,
                                     modifierKeyState,
                                     LMGetKbdType(),
                                     kUCKeyTranslateNoDeadKeysBit,
                                     &deadKeyState,
                                     MAX_STRING_LENGTH,
                                     &actualStringLength,
                                     unicodeString);
    if (status != noErr) {
        fprintf(stderr,
                "ERROR: Unable to translate key code to Unicode string: %d\n",
                (int)status);
        return event;
    }

    handleButtonEvent(type, flags, keyCode, unicodeString, context);

    return event;
}

void listen(FILE *file) {
    // Determine keyboard layout
    TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef layoutData = TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
    UCKeyboardLayout *keyboardLayout = (UCKeyboardLayout *)CFDataGetBytePtr(layoutData);
    struct CGEventContext userInfo = {file, keyboardLayout, 0};

    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);

    CFMachPortRef eventTap = CGEventTapCreate(kCGSessionEventTap,
                                              kCGHeadInsertEventTap,
                                              0,
                                              eventMask,
                                              CGEventCallback,
                                              &userInfo);

    if (!eventTap) {
        fprintf(stderr, "ERROR: Unable to create event tap.");
        exit(1);
    }

    CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
                       runLoopSource,
                       kCFRunLoopCommonModes);
    CGEventTapEnable(eventTap, true);

    CFRunLoopRun();
}

int main(void) {
    FILE *file = fopen("keyboard.bin", "ab");
    if (file == NULL) {
        fprintf(stderr, "ERROR: Unable to open file for writing.\n");
        exit(1);
    }
    setbuf(stdout, NULL);
    listen(file);
    return 0;
}
