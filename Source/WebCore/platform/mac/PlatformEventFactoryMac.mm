/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformEventFactoryMac.h"

#import "KeyEventCocoa.h"
#import "Logging.h"
#import "NSMenuSPI.h"
#import "PlatformScreen.h"
#import "Scrollbar.h"
#import "WebCoreSystemInterface.h"
#import "WindowsKeyboardCodes.h"
#import <mach/mach_time.h>
#import <wtf/ASCIICType.h>

namespace WebCore {

NSPoint globalPoint(const NSPoint& windowPoint, NSWindow *window)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return flipScreenPoint([window convertBaseToScreen:windowPoint], screen(window));
#pragma clang diagnostic pop
}

static NSPoint globalPointForEvent(NSEvent *event)
{
    switch ([event type]) {
#if defined(__LP64__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101003
        case NSEventTypePressure:
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        case NSLeftMouseDown:
        case NSLeftMouseDragged:
        case NSLeftMouseUp:
        case NSMouseEntered:
        case NSMouseExited:
        case NSMouseMoved:
        case NSOtherMouseDown:
        case NSOtherMouseDragged:
        case NSOtherMouseUp:
        case NSRightMouseDown:
        case NSRightMouseDragged:
        case NSRightMouseUp:
        case NSScrollWheel:
#pragma clang diagnostic pop
            return globalPoint([event locationInWindow], [event window]);
        default:
            return { 0, 0 };
    }
}

static IntPoint pointForEvent(NSEvent *event, NSView *windowView)
{
    switch ([event type]) {
#if defined(__LP64__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101003
        case NSEventTypePressure:
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        case NSLeftMouseDown:
        case NSLeftMouseDragged:
        case NSLeftMouseUp:
        case NSMouseEntered:
        case NSMouseExited:
        case NSMouseMoved:
        case NSOtherMouseDown:
        case NSOtherMouseDragged:
        case NSOtherMouseUp:
        case NSRightMouseDown:
        case NSRightMouseDragged:
        case NSRightMouseUp:
        case NSScrollWheel: {
            // Note: This will have its origin at the bottom left of the window unless windowView is flipped.
            // In those cases, the Y coordinate gets flipped by Widget::convertFromContainingWindow.
            NSPoint location = [event locationInWindow];
            if (windowView)
                location = [windowView convertPoint:location fromView:nil];
            return IntPoint(location);
        }
#pragma clang diagnostic pop
        default:
            return IntPoint();
    }
}

static MouseButton mouseButtonForEvent(NSEvent *event)
{
    switch ([event type]) {
#if defined(__LP64__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101003
        case NSEventTypePressure:
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
            return LeftButton;
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
            return RightButton;
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
            return MiddleButton;
#pragma clang diagnostic pop
        default:
            return NoButton;
    }
}

static PlatformEvent::Type mouseEventTypeForEvent(NSEvent* event)
{
    switch ([event type]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        case NSLeftMouseDragged:
        case NSMouseEntered:
        case NSMouseExited:
        case NSMouseMoved:
        case NSOtherMouseDragged:
        case NSRightMouseDragged:
            return PlatformEvent::MouseMoved;
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
            return PlatformEvent::MousePressed;
        case NSLeftMouseUp:
        case NSRightMouseUp:
        case NSOtherMouseUp:
            return PlatformEvent::MouseReleased;
#pragma clang diagnostic pop
        default:
            return PlatformEvent::MouseMoved;
    }
}

static int clickCountForEvent(NSEvent *event)
{
    switch ([event type]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        case NSLeftMouseDown:
        case NSLeftMouseUp:
        case NSLeftMouseDragged:
        case NSRightMouseDown:
        case NSRightMouseUp:
        case NSRightMouseDragged:
        case NSOtherMouseDown:
        case NSOtherMouseUp:
        case NSOtherMouseDragged:
#pragma clang diagnostic pop
            return [event clickCount];
        default:
            return 0;
    }
}

static PlatformWheelEventPhase momentumPhaseForEvent(NSEvent *event)
{
    uint32_t phase = PlatformWheelEventPhaseNone;

    if ([event momentumPhase] & NSEventPhaseBegan)
        phase |= PlatformWheelEventPhaseBegan;
    if ([event momentumPhase] & NSEventPhaseStationary)
        phase |= PlatformWheelEventPhaseStationary;
    if ([event momentumPhase] & NSEventPhaseChanged)
        phase |= PlatformWheelEventPhaseChanged;
    if ([event momentumPhase] & NSEventPhaseEnded)
        phase |= PlatformWheelEventPhaseEnded;
    if ([event momentumPhase] & NSEventPhaseCancelled)
        phase |= PlatformWheelEventPhaseCancelled;

    return static_cast<PlatformWheelEventPhase>(phase);
}

static PlatformWheelEventPhase phaseForEvent(NSEvent *event)
{
    uint32_t phase = PlatformWheelEventPhaseNone; 
    if ([event phase] & NSEventPhaseBegan)
        phase |= PlatformWheelEventPhaseBegan;
    if ([event phase] & NSEventPhaseStationary)
        phase |= PlatformWheelEventPhaseStationary;
    if ([event phase] & NSEventPhaseChanged)
        phase |= PlatformWheelEventPhaseChanged;
    if ([event phase] & NSEventPhaseEnded)
        phase |= PlatformWheelEventPhaseEnded;
    if ([event phase] & NSEventPhaseCancelled)
        phase |= PlatformWheelEventPhaseCancelled;
    if ([event momentumPhase] & NSEventPhaseMayBegin)
        phase |= PlatformWheelEventPhaseMayBegin;

    return static_cast<PlatformWheelEventPhase>(phase);
}

static inline String textFromEvent(NSEvent* event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event type] == NSFlagsChanged)
#pragma clang diagnostic pop
        return emptyString();
    return String([event characters]);
}

static inline String unmodifiedTextFromEvent(NSEvent* event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event type] == NSFlagsChanged)
#pragma clang diagnostic pop
        return emptyString();
    return String([event charactersIgnoringModifiers]);
}

String keyIdentifierForKeyEvent(NSEvent* event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event type] == NSFlagsChanged)
#pragma clang diagnostic pop
        switch ([event keyCode]) {
            case 54: // Right Command
            case 55: // Left Command
                return String("Meta");
                
            case 57: // Capslock
                return String("CapsLock");
                
            case 56: // Left Shift
            case 60: // Right Shift
                return String("Shift");
                
            case 58: // Left Alt
            case 61: // Right Alt
                return String("Alt");
                
            case 59: // Left Ctrl
            case 62: // Right Ctrl
                return String("Control");
                
            default:
                ASSERT_NOT_REACHED();
                return emptyString();
        }
    
    NSString *s = [event charactersIgnoringModifiers];
    if ([s length] != 1) {
        LOG(Events, "received an unexpected number of characters in key event: %u", [s length]);
        return "Unidentified";
    }
    return keyIdentifierForCharCode([s characterAtIndex:0]);
}

static bool isKeypadEvent(NSEvent* event)
{
    // Check that this is the type of event that has a keyCode.
    switch ([event type]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        case NSKeyDown:
        case NSKeyUp:
        case NSFlagsChanged:
            break;
#pragma clang diagnostic pop
        default:
            return false;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event modifierFlags] & NSNumericPadKeyMask)
#pragma clang diagnostic pop
        return true;

    switch ([event keyCode]) {
        case 71: // Clear
        case 81: // =
        case 75: // /
        case 67: // *
        case 78: // -
        case 69: // +
        case 76: // Enter
        case 65: // .
        case 82: // 0
        case 83: // 1
        case 84: // 2
        case 85: // 3
        case 86: // 4
        case 87: // 5
        case 88: // 6
        case 89: // 7
        case 91: // 8
        case 92: // 9
            return true;
     }
     
     return false;
}

int windowsKeyCodeForKeyEvent(NSEvent* event)
{
    int code = 0;
    // There are several kinds of characters for which we produce key code from char code:
    // 1. Roman letters. Windows keyboard layouts affect both virtual key codes and character codes for these,
    //    so e.g. 'A' gets the same keyCode on QWERTY, AZERTY or Dvorak layouts.
    // 2. Keys for which there is no known Mac virtual key codes, like PrintScreen.
    // 3. Certain punctuation keys. On Windows, these are also remapped depending on current keyboard layout,
    //    but see comment in windowsKeyCodeForCharCode().
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (!isKeypadEvent(event) && ([event type] == NSKeyDown || [event type] == NSKeyUp)) {
#pragma clang diagnostic pop
        // Cmd switches Roman letters for Dvorak-QWERTY layout, so try modified characters first.
        NSString* s = [event characters];
        code = [s length] > 0 ? windowsKeyCodeForCharCode([s characterAtIndex:0]) : 0;
        if (code)
            return code;

        // Ctrl+A on an AZERTY keyboard would get VK_Q keyCode if we relied on -[NSEvent keyCode] below.
        s = [event charactersIgnoringModifiers];
        code = [s length] > 0 ? windowsKeyCodeForCharCode([s characterAtIndex:0]) : 0;
        if (code)
            return code;
    }

    // Map Mac virtual key code directly to Windows one for any keys not handled above.
    // E.g. the key next to Caps Lock has the same Event.keyCode on U.S. keyboard ('A') and on Russian keyboard (CYRILLIC LETTER EF).
    return windowsKeyCodeForKeyCode([event keyCode]);
}

static CFAbsoluteTime systemStartupTime;

static void updateSystemStartupTimeIntervalSince1970()
{
    // CFAbsoluteTimeGetCurrent() provides the absolute time in seconds since 2001.
    // mach_absolute_time() provides a relative system time since startup minus the time the computer was suspended.
    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    double elapsedTimeSinceStartup = static_cast<double>(mach_absolute_time()) * timebase_info.numer / timebase_info.denom / 1e9;
    systemStartupTime = kCFAbsoluteTimeIntervalSince1970 + CFAbsoluteTimeGetCurrent() - elapsedTimeSinceStartup;
}

static CFTimeInterval cachedStartupTimeIntervalSince1970()
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        void (^updateBlock)(NSNotification *) = Block_copy(^(NSNotification *){ updateSystemStartupTimeIntervalSince1970(); });
        [[[NSWorkspace sharedWorkspace] notificationCenter] addObserverForName:NSWorkspaceDidWakeNotification
                                                                        object:nil
                                                                         queue:nil
                                                                    usingBlock:updateBlock];
        [[NSNotificationCenter defaultCenter] addObserverForName:NSSystemClockDidChangeNotification
                                                          object:nil
                                                           queue:nil
                                                      usingBlock:updateBlock];
        Block_release(updateBlock);

        updateSystemStartupTimeIntervalSince1970();
    });
    return systemStartupTime;
}

double eventTimeStampSince1970(NSEvent* event)
{
    return static_cast<double>(cachedStartupTimeIntervalSince1970() + [event timestamp]);
}

static inline bool isKeyUpEvent(NSEvent *event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if ([event type] != NSFlagsChanged)
        return [event type] == NSKeyUp;
    // FIXME: This logic fails if the user presses both Shift keys at once, for example:
    // we treat releasing one of them as keyDown.
    switch ([event keyCode]) {
        case 54: // Right Command
        case 55: // Left Command
            return ([event modifierFlags] & NSCommandKeyMask) == 0;
            
        case 57: // Capslock
            return ([event modifierFlags] & NSAlphaShiftKeyMask) == 0;
            
        case 56: // Left Shift
        case 60: // Right Shift
            return ([event modifierFlags] & NSShiftKeyMask) == 0;
            
        case 58: // Left Alt
        case 61: // Right Alt
            return ([event modifierFlags] & NSAlternateKeyMask) == 0;
            
        case 59: // Left Ctrl
        case 62: // Right Ctrl
            return ([event modifierFlags] & NSControlKeyMask) == 0;
            
        case 63: // Function
            return ([event modifierFlags] & NSFunctionKeyMask) == 0;
#pragma clang diagnostic pop
    }
    return false;
}

static inline OptionSet<PlatformEvent::Modifier> modifiersForEvent(NSEvent *event)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    OptionSet<PlatformEvent::Modifier> modifiers;
    if (event.modifierFlags & NSShiftKeyMask)
        modifiers |= PlatformEvent::Modifier::ShiftKey;
    if (event.modifierFlags & NSControlKeyMask)
        modifiers |= PlatformEvent::Modifier::CtrlKey;
    if (event.modifierFlags & NSAlternateKeyMask)
        modifiers |= PlatformEvent::Modifier::AltKey;
    if (event.modifierFlags & NSCommandKeyMask)
        modifiers |= PlatformEvent::Modifier::MetaKey;
#pragma clang diagnostic pop
    return modifiers;
}

static int typeForEvent(NSEvent *event)
{
    if ([NSMenu respondsToSelector:@selector(menuTypeForEvent:)])
        return static_cast<int>([NSMenu menuTypeForEvent:event]);

    if (mouseButtonForEvent(event) == RightButton)
        return static_cast<int>(NSMenuTypeContextMenu);

    if (mouseButtonForEvent(event) == LeftButton && modifiersForEvent(event).contains(PlatformEvent::Modifier::CtrlKey))
        return static_cast<int>(NSMenuTypeContextMenu);

    return static_cast<int>(NSMenuTypeNone);
}
    
class PlatformMouseEventBuilder : public PlatformMouseEvent {
public:
    PlatformMouseEventBuilder(NSEvent *event, NSEvent *correspondingPressureEvent, NSView *windowView)
    {
        // PlatformEvent
        m_type = mouseEventTypeForEvent(event);

#if defined(__LP64__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101003
        BOOL eventIsPressureEvent = [event type] == NSEventTypePressure;
        if (eventIsPressureEvent) {
            // Since AppKit doesn't send mouse events for force down or force up, we have to use the current pressure
            // event and correspondingPressureEvent to detect if this is MouseForceDown, MouseForceUp, or just MouseForceChanged.
            if (correspondingPressureEvent.stage == 1 && event.stage == 2)
                m_type = PlatformEvent::MouseForceDown;
            else if (correspondingPressureEvent.stage == 2 && event.stage == 1)
                m_type = PlatformEvent::MouseForceUp;
            else
                m_type = PlatformEvent::MouseForceChanged;
        }
#else
        UNUSED_PARAM(correspondingPressureEvent);
#endif

        m_modifiers = modifiersForEvent(event);
        m_timestamp = eventTimeStampSince1970(event);

        // PlatformMouseEvent
        m_position = pointForEvent(event, windowView);
        m_globalPosition = IntPoint(globalPointForEvent(event));
        m_button = mouseButtonForEvent(event);
        m_clickCount = clickCountForEvent(event);

        m_force = 0;
#if defined(__LP64__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101003
        int stage = eventIsPressureEvent ? event.stage : correspondingPressureEvent.stage;
        double pressure = eventIsPressureEvent ? event.pressure : correspondingPressureEvent.pressure;
        m_force = pressure + stage;
#endif

        // Mac specific
        m_modifierFlags = [event modifierFlags];
        m_eventNumber = [event eventNumber];
        m_menuTypeForEvent = typeForEvent(event);
    }
};

PlatformMouseEvent PlatformEventFactory::createPlatformMouseEvent(NSEvent *event, NSEvent *correspondingPressureEvent, NSView *windowView)
{
    return PlatformMouseEventBuilder(event, correspondingPressureEvent, windowView);
}


class PlatformWheelEventBuilder : public PlatformWheelEvent {
public:
    PlatformWheelEventBuilder(NSEvent *event, NSView *windowView)
    {
        // PlatformEvent
        m_type = PlatformEvent::Wheel;
        m_modifiers = modifiersForEvent(event);
        m_timestamp = eventTimeStampSince1970(event);

        // PlatformWheelEvent
        m_position = pointForEvent(event, windowView);
        m_globalPosition = IntPoint(globalPointForEvent(event));
        m_granularity = ScrollByPixelWheelEvent;

        BOOL continuous;
        wkGetWheelEventDeltas(event, &m_deltaX, &m_deltaY, &continuous);
        if (continuous) {
            m_wheelTicksX = m_deltaX / static_cast<float>(Scrollbar::pixelsPerLineStep());
            m_wheelTicksY = m_deltaY / static_cast<float>(Scrollbar::pixelsPerLineStep());
        } else {
            m_wheelTicksX = m_deltaX;
            m_wheelTicksY = m_deltaY;
            m_deltaX *= static_cast<float>(Scrollbar::pixelsPerLineStep());
            m_deltaY *= static_cast<float>(Scrollbar::pixelsPerLineStep());
        }

        m_phase = phaseForEvent(event);
        m_momentumPhase = momentumPhaseForEvent(event);
        m_hasPreciseScrollingDeltas = continuous;
        m_directionInvertedFromDevice = [event isDirectionInvertedFromDevice];
    }
};

PlatformWheelEvent PlatformEventFactory::createPlatformWheelEvent(NSEvent *event, NSView *windowView)
{
    return PlatformWheelEventBuilder(event, windowView);
}


class PlatformKeyboardEventBuilder : public PlatformKeyboardEvent {
public:
    PlatformKeyboardEventBuilder(NSEvent *event)
    {
        // PlatformEvent
        m_type = isKeyUpEvent(event) ? PlatformEvent::KeyUp : PlatformEvent::KeyDown;
        m_modifiers = modifiersForEvent(event);
        m_timestamp = eventTimeStampSince1970(event);

        // PlatformKeyboardEvent
        m_text = textFromEvent(event);
        m_unmodifiedText = unmodifiedTextFromEvent(event);
        m_keyIdentifier = keyIdentifierForKeyEvent(event);
        m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(event);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        m_autoRepeat = [event type] != NSFlagsChanged && [event isARepeat];
#pragma clang diagnostic pop
        m_isKeypad = isKeypadEvent(event);
        m_isSystemKey = false; // SystemKey is always false on the Mac.

        // Always use 13 for Enter/Return -- we don't want to use AppKit's different character for Enter.
        if (m_windowsVirtualKeyCode == VK_RETURN) {
            m_text = "\r";
            m_unmodifiedText = "\r";
        }

        // AppKit sets text to "\x7F" for backspace, but the correct KeyboardEvent character code is 8.
        if (m_windowsVirtualKeyCode == VK_BACK) {
            m_text = "\x8";
            m_unmodifiedText = "\x8";
        }

        // Always use 9 for Tab -- we don't want to use AppKit's different character for shift-tab.
        if (m_windowsVirtualKeyCode == VK_TAB) {
            m_text = "\x9";
            m_unmodifiedText = "\x9";
        }

        // Mac specific.
        m_macEvent = event;
    }
};

PlatformKeyboardEvent PlatformEventFactory::createPlatformKeyboardEvent(NSEvent *event)
{
    return PlatformKeyboardEventBuilder(event);
}

} // namespace WebCore
