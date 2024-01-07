#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>

struct CGEventContext {
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

void printUniCharAsUtf8(UniChar ch) {
    unsigned char utf8[4] = {0};

    if (ch < 0x80) {
        utf8[0] = (unsigned char)ch;
    } else if (ch < 0x800) {
        utf8[0] = 0xC0 | (ch >> 6);
        utf8[1] = 0x80 | (ch & 0x3F);
    } else {
        // 3-byte UTF-8 sequence
        utf8[0] = 0xE0 | (ch >> 12);
        utf8[1] = 0x80 | ((ch >> 6) & 0x3F);
        utf8[2] = 0x80 | (ch & 0x3F);
    }

    printf("%s", utf8);
}

const unsigned char * findSpecialCharacter(int keyCode) {
    switch (keyCode) {
        case 36:
            return RETURN;
        case 48:
            return TAB;
        case 49:
            return SPACE;
        case 51:
            return DELETE;
        case 53:
            return ESCAPE;
        case 115:
            return HOME;
        case 116:
            return PAGE_UP;
        case 117:
            return FORWARD_DELETE;
        case 119:
            return END;
        case 121:
            return PAGE_DOWN;;
        case 123:
            return LEFT_ARROW;
        case 124:
            return RIGHT_ARROW;
        case 125:
            return DOWN_ARROW;
        case 126:
            return UP_ARROW;
        default:
            return NO_CHAR;
    }
}


void handleButtonEvent(const CGEventType type,
                       const CGEventFlags flags,
                       const CGKeyCode keyCode,
                       UniChar *unicodeString,
                       struct CGEventContext *userInfo) {
    if (type != kCGEventKeyDown) {
        return;
    }

    struct timeval tv;
    int ret;

    ret = gettimeofday(&tv, NULL);
    if (ret == -1) {
        fprintf(stderr, "ERROR: Unable to get time of day.\n");
    } else {
        // Print a newline if we've been idle for a while
        long long millis = (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
        if (millis - userInfo->lastMillis > LINGER_MS) {
            printf("\n");
        }
        userInfo->lastMillis = millis;
    }

    // Print modifier keys
    if ((flags & kCGEventFlagMaskControl) != 0) {
        printf("%s", CTRL);
    }
    if ((flags & kCGEventFlagMaskAlternate) != 0) {
        printf("%s", OPT);
    }
    if (!isprint(unicodeString[0]) && (flags & kCGEventFlagMaskShift) != 0) {
        printf("%s", SHIFT);
    }
    if ((flags & kCGEventFlagMaskCommand) != 0) {
        printf("%s", CMD);
    }

    // Check if the keycode has a special Mac-style character
    const unsigned char *special = findSpecialCharacter(keyCode);
    if (special == NO_CHAR) {
        printUniCharAsUtf8(unicodeString[0]);
    } else {
        printf("%s", special);
    }
    fflush(stdout);
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

void listen() {
    // Determine keyboard layout
    TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef layoutData = TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
    UCKeyboardLayout *keyboardLayout = (UCKeyboardLayout *)CFDataGetBytePtr(layoutData);
    struct CGEventContext userInfo = {keyboardLayout, 0};

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
    listen();
    return 0;
}
