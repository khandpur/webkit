/*
 * Copyright (C) 2008, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebAccessibilityObjectWrapperIOS.h"

#if HAVE(ACCESSIBILITY) && PLATFORM(IOS)

#import "AccessibilityAttachment.h"
#import "AccessibilityRenderObject.h"
#import "AccessibilityScrollView.h"
#import "AccessibilityTable.h"
#import "AccessibilityTableCell.h"
#import "Chrome.h"
#import "ChromeClient.h"
#import "FontCascade.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "HitTestResult.h"
#import "HTMLFrameOwnerElement.h"
#import "HTMLInputElement.h"
#import "HTMLNames.h"
#import "IntRect.h"
#import "IntSize.h"
#import "LocalizedStrings.h"
#import "Page.h"
#import "Range.h"
#import "RenderView.h"
#import "RuntimeApplicationChecks.h"
#import "SVGNames.h"
#import "SVGElement.h"
#import "TextIterator.h"
#import "WAKScrollView.h"
#import "WAKView.h"
#import "WAKWindow.h"
#import "WebCoreThread.h"
#import "VisibleUnits.h"

#import <CoreText/CoreText.h>

enum {
    NSAttachmentCharacter = 0xfffc    /* To denote attachments. */
};

@interface NSObject (AccessibilityPrivate)
- (void)_accessibilityUnregister;
- (NSString *)accessibilityLabel;
- (NSString *)accessibilityValue;
- (BOOL)isAccessibilityElement;
- (NSInteger)accessibilityElementCount;
- (id)accessibilityElementAtIndex:(NSInteger)index;
- (NSInteger)indexOfAccessibilityElement:(id)element;
@end

@interface WebAccessibilityObjectWrapper (AccessibilityPrivate)
- (id)_accessibilityWebDocumentView;
- (id)accessibilityContainer;
- (void)setAccessibilityLabel:(NSString *)label;
- (void)setAccessibilityValue:(NSString *)value;
- (BOOL)containsUnnaturallySegmentedChildren;
- (NSInteger)positionForTextMarker:(id)marker;
@end

@interface WAKView (iOSAccessibility)
- (BOOL)accessibilityIsIgnored;
@end

using namespace WebCore;
using namespace HTMLNames;

typedef NS_ENUM(NSInteger, UIAccessibilityScrollDirection) {
    UIAccessibilityScrollDirectionRight = 1,
    UIAccessibilityScrollDirectionLeft,
    UIAccessibilityScrollDirectionUp,
    UIAccessibilityScrollDirectionDown,
    UIAccessibilityScrollDirectionNext,
    UIAccessibilityScrollDirectionPrevious
};

// These are tokens accessibility uses to denote attributes. 
static NSString * const UIAccessibilityTokenBlockquoteLevel = @"UIAccessibilityTokenBlockquoteLevel";
static NSString * const UIAccessibilityTokenHeadingLevel = @"UIAccessibilityTokenHeadingLevel";
static NSString * const UIAccessibilityTokenFontName = @"UIAccessibilityTokenFontName";
static NSString * const UIAccessibilityTokenFontFamily = @"UIAccessibilityTokenFontFamily";
static NSString * const UIAccessibilityTokenFontSize = @"UIAccessibilityTokenFontSize";
static NSString * const UIAccessibilityTokenBold = @"UIAccessibilityTokenBold";
static NSString * const UIAccessibilityTokenItalic = @"UIAccessibilityTokenItalic";
static NSString * const UIAccessibilityTokenUnderline = @"UIAccessibilityTokenUnderline";
static NSString * const UIAccessibilityTokenLanguage = @"UIAccessibilityTokenLanguage";
static NSString * const UIAccessibilityTokenAttachment = @"UIAccessibilityTokenAttachment";

static AccessibilityObjectWrapper* AccessibilityUnignoredAncestor(AccessibilityObjectWrapper *wrapper)
{
    while (wrapper && ![wrapper isAccessibilityElement]) {
        AccessibilityObject* object = [wrapper accessibilityObject];
        if (!object)
            break;
        
        if ([wrapper isAttachment] && ![[wrapper attachmentView] accessibilityIsIgnored])
            break;
            
        AccessibilityObject* parentObject = object->parentObjectUnignored();
        if (!parentObject)
            break;

        wrapper = parentObject->wrapper();
    }
    return wrapper;
}

template<typename MatchFunction>
static AccessibilityObject* matchedParent(AccessibilityObject* object, bool includeSelf, const MatchFunction& matches)
{
    if (!object)
        return nullptr;

    AccessibilityObject* parent = includeSelf ? object : object->parentObject();
    for (; parent; parent = parent->parentObject()) {
        if (matches(parent))
            return parent;
    }
    
    return nullptr;
}


#pragma mark Accessibility Text Marker

@interface WebAccessibilityTextMarker : NSObject
{
    AXObjectCache* _cache;
    TextMarkerData _textMarkerData;
}

+ (WebAccessibilityTextMarker *)textMarkerWithVisiblePosition:(VisiblePosition&)visiblePos cache:(AXObjectCache*)cache;
+ (WebAccessibilityTextMarker *)textMarkerWithCharacterOffset:(CharacterOffset&)characterOffset cache:(AXObjectCache*)cache;
+ (WebAccessibilityTextMarker *)startOrEndTextMarkerForRange:(const RefPtr<Range>)range isStart:(BOOL)isStart cache:(AXObjectCache*)cache;

@end

@implementation WebAccessibilityTextMarker

- (id)initWithTextMarker:(TextMarkerData *)data cache:(AXObjectCache*)cache
{
    if (!(self = [super init]))
        return nil;
    
    _cache = cache;
    memcpy(&_textMarkerData, data, sizeof(TextMarkerData));
    return self;
}

- (id)initWithData:(NSData *)data cache:(AXObjectCache*)cache
{
    if (!(self = [super init]))
        return nil;
    
    _cache = cache;
    [data getBytes:&_textMarkerData length:sizeof(TextMarkerData)];
    
    return self;
}

// This is needed for external clients to be able to create a text marker without having a pointer to the cache.
- (id)initWithData:(NSData *)data accessibilityObject:(AccessibilityObjectWrapper *)wrapper
{
    WebCore::AccessibilityObject* axObject = [wrapper accessibilityObject]; 
    if (!axObject)
        return nil;
    
    return [self initWithData:data cache:axObject->axObjectCache()];
}

+ (WebAccessibilityTextMarker *)textMarkerWithVisiblePosition:(VisiblePosition&)visiblePos cache:(AXObjectCache*)cache
{
    TextMarkerData textMarkerData;
    cache->textMarkerDataForVisiblePosition(textMarkerData, visiblePos);
    
    return [[[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache] autorelease];
}

+ (WebAccessibilityTextMarker *)textMarkerWithCharacterOffset:(CharacterOffset&)characterOffset cache:(AXObjectCache*)cache
{
    if (!cache)
        return nil;
    
    if (characterOffset.isNull())
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID && !textMarkerData.ignored)
        return nil;
    return [[[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache] autorelease];
}

+ (WebAccessibilityTextMarker *)startOrEndTextMarkerForRange:(const RefPtr<Range>)range isStart:(BOOL)isStart cache:(AXObjectCache*)cache
{
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->startOrEndTextMarkerDataForRange(textMarkerData, range, isStart);
    if (!textMarkerData.axID)
        return nil;
    return [[[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache] autorelease];
}

- (NSData *)dataRepresentation
{
    return [NSData dataWithBytes:&_textMarkerData length:sizeof(TextMarkerData)];
}

- (VisiblePosition)visiblePosition
{
    return _cache->visiblePositionForTextMarkerData(_textMarkerData);
}

- (CharacterOffset)characterOffset
{
    return _cache->characterOffsetForTextMarkerData(_textMarkerData);
}

- (BOOL)isIgnored
{
    return _textMarkerData.ignored;
}

- (AccessibilityObject*)accessibilityObject
{
    if (_textMarkerData.ignored)
        return nullptr;
    return _cache->accessibilityObjectForTextMarkerData(_textMarkerData);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"[AXTextMarker %p] = node: %p offset: %d", self, _textMarkerData.node, _textMarkerData.offset];
}

@end

@implementation WebAccessibilityObjectWrapper

- (id)initWithAccessibilityObject:(AccessibilityObject*)axObject
{
    self = [super initWithAccessibilityObject:axObject];
    if (!self)
        return nil;
    
    // Initialize to a sentinel value.
    m_accessibilityTraitsFromAncestor = ULLONG_MAX;
    m_isAccessibilityElement = -1;
    
    return self;
}

- (void)detach
{
    // rdar://8798960 Make sure the object is gone early, so that anything _accessibilityUnregister 
    // does can't call back into the render tree.
    m_object = nullptr;

    if ([self respondsToSelector:@selector(_accessibilityUnregister)])
        [self _accessibilityUnregister];
}

- (void)dealloc
{
    // We should have been detached before deallocated.
    ASSERT(!m_object);
    [super dealloc];
}

- (BOOL)_prepareAccessibilityCall
{
    // rdar://7980318 if we start a call, then block in WebThreadLock(), then we're dealloced on another, thread, we could
    // crash, so we should retain ourself for the duration of usage here.
    [[self retain] autorelease];

    WebThreadLock();
    
    // If we came back from our thread lock and we were detached, we will no longer have an m_object.
    if (!m_object)
        return NO;
    
    m_object->updateBackingStore();
    if (!m_object)
        return NO;
    
    return YES;
}

// These are here so that we don't have to import AXRuntime.
// The methods will be swizzled when the accessibility bundle is loaded.

- (uint64_t)_axLinkTrait { return (1 << 0); }
- (uint64_t)_axVisitedTrait { return (1 << 1); }
- (uint64_t)_axHeaderTrait { return (1 << 2); }
- (uint64_t)_axContainedByListTrait { return (1 << 3); }
- (uint64_t)_axContainedByTableTrait { return (1 << 4); }
- (uint64_t)_axContainedByLandmarkTrait { return (1 << 5); }
- (uint64_t)_axWebContentTrait { return (1 << 6); }
- (uint64_t)_axSecureTextFieldTrait { return (1 << 7); }
- (uint64_t)_axTextEntryTrait { return (1 << 8); }
- (uint64_t)_axHasTextCursorTrait { return (1 << 9); }
- (uint64_t)_axTextOperationsAvailableTrait { return (1 << 10); }
- (uint64_t)_axImageTrait { return (1 << 11); }
- (uint64_t)_axTabButtonTrait { return (1 << 12); }
- (uint64_t)_axButtonTrait { return (1 << 13); }
- (uint64_t)_axToggleTrait { return (1 << 14); }
- (uint64_t)_axPopupButtonTrait { return (1 << 15); }
- (uint64_t)_axStaticTextTrait { return (1 << 16); }
- (uint64_t)_axAdjustableTrait { return (1 << 17); }
- (uint64_t)_axMenuItemTrait { return (1 << 18); }
- (uint64_t)_axSelectedTrait { return (1 << 19); }
- (uint64_t)_axNotEnabledTrait { return (1 << 20); }
- (uint64_t)_axRadioButtonTrait { return (1 << 21); }
- (uint64_t)_axContainedByFieldsetTrait { return (1 << 22); }
- (uint64_t)_axSearchFieldTrait { return (1 << 23); }
- (uint64_t)_axTextAreaTrait { return (1 << 24); }
- (uint64_t)_axUpdatesFrequentlyTrait { return (1 << 25); }

- (BOOL)accessibilityCanFuzzyHitTest
{
    if (![self _prepareAccessibilityCall])
        return false;
    
    AccessibilityRole role = m_object->roleValue();
    // Elements that can be returned when performing fuzzy hit testing.
    switch (role) {
    case ButtonRole:
    case CheckBoxRole:
    case ComboBoxRole:
    case DisclosureTriangleRole:
    case HeadingRole:
    case ImageMapLinkRole:
    case ImageRole:
    case LinkRole:
    case ListBoxRole:
    case ListBoxOptionRole:
    case MenuButtonRole:
    case MenuItemRole:
    case MenuItemCheckboxRole:
    case MenuItemRadioRole:
    case PopUpButtonRole:
    case RadioButtonRole:
    case ScrollBarRole:
    case SearchFieldRole:
    case SliderRole:
    case StaticTextRole:
    case SwitchRole:
    case TabRole:
    case TextFieldRole:
    case ToggleButtonRole:
        return !m_object->accessibilityIsIgnored();
    default:
        return false;
    }
}

- (AccessibilityObjectWrapper *)accessibilityPostProcessHitTest:(CGPoint)point
{
    UNUSED_PARAM(point);
    // The UIKit accessibility wrapper will override this and perform the post process hit test.
    return nil;
}

- (id)accessibilityHitTest:(CGPoint)point
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    // Try a fuzzy hit test first to find an accessible element.
    RefPtr<AccessibilityObject> axObject;
    {
        AXAttributeCacheEnabler enableCache(m_object->axObjectCache());
        axObject = m_object->accessibilityHitTest(IntPoint(point));
    }

    if (!axObject)
        return nil;
    
    // If this is a good accessible object to return, no extra work is required.
    if ([axObject->wrapper() accessibilityCanFuzzyHitTest])
        return AccessibilityUnignoredAncestor(axObject->wrapper());
    
    // Check to see if we can post-process this hit test to find a better candidate.
    AccessibilityObjectWrapper* wrapper = [axObject->wrapper() accessibilityPostProcessHitTest:point];
    if (wrapper)
        return AccessibilityUnignoredAncestor(wrapper);
    
    // Fall back to default behavior.
    return AccessibilityUnignoredAncestor(axObject->wrapper());    
}

- (void)enableAttributeCaching
{
    if (AXObjectCache* cache = m_object->axObjectCache())
        cache->startCachingComputedObjectAttributesUntilTreeMutates();
}

- (void)disableAttributeCaching
{
    if (AXObjectCache* cache = m_object->axObjectCache())
        cache->stopCachingComputedObjectAttributes();
}

- (NSInteger)accessibilityElementCount
{
    if (![self _prepareAccessibilityCall])
        return 0;

    if ([self isAttachment]) {
        if (id attachmentView = [self attachmentView])
            return [attachmentView accessibilityElementCount];
    }
    
    return m_object->children().size();
}

- (id)accessibilityElementAtIndex:(NSInteger)index
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if ([self isAttachment]) {
        if (id attachmentView = [self attachmentView])
            return [attachmentView accessibilityElementAtIndex:index];
    }
    
    const auto& children = m_object->children();
    size_t elementIndex = static_cast<size_t>(index);
    if (elementIndex >= children.size())
        return nil;
    
    AccessibilityObjectWrapper* wrapper = children[elementIndex]->wrapper();
    if (children[elementIndex]->isAttachment()) {
        if (id attachmentView = [wrapper attachmentView])
            return attachmentView;
    }

    return wrapper;
}

- (NSInteger)indexOfAccessibilityElement:(id)element
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;
    
    if ([self isAttachment]) {
        if (id attachmentView = [self attachmentView])
            return [attachmentView indexOfAccessibilityElement:element];
    }
    
    const auto& children = m_object->children();
    unsigned count = children.size();
    for (unsigned k = 0; k < count; ++k) {
        AccessibilityObjectWrapper* wrapper = children[k]->wrapper();
        if (wrapper == element || (children[k]->isAttachment() && [wrapper attachmentView] == element))
            return k;
    }
    
    return NSNotFound;
}

- (CGPathRef)_accessibilityPath
{
    if (![self _prepareAccessibilityCall])
        return NULL;

    if (!m_object->supportsPath())
        return NULL;
    
    Path path = m_object->elementPath();
    if (path.isEmpty())
        return NULL;
    
    return [self convertPathToScreenSpace:path];
}

- (BOOL)accessibilityHasPopup
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return m_object->ariaHasPopup();
}

- (NSString *)accessibilityLanguage
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return m_object->language();
}

- (BOOL)accessibilityIsDialog
{
    if (![self _prepareAccessibilityCall])
        return NO;

    AccessibilityRole roleValue = m_object->roleValue();
    return roleValue == ApplicationDialogRole || roleValue == ApplicationAlertDialogRole;
}

- (BOOL)_accessibilityIsLandmarkRole:(AccessibilityRole)role
{
    switch (role) {
    case DocumentRole:
    case DocumentArticleRole:
    case DocumentNoteRole:
    case FooterRole:
    case LandmarkBannerRole:
    case LandmarkComplementaryRole:
    case LandmarkContentInfoRole:
    case LandmarkMainRole:
    case LandmarkNavigationRole:
    case LandmarkRegionRole:
    case LandmarkSearchRole:
        return YES;
    default:
        return NO;
    }    
}

- (AccessibilityObjectWrapper*)_accessibilityListAncestor
{
    auto matchFunc = [] (AccessibilityObject* object) {
        AccessibilityRole role = object->roleValue();
        return role == ListRole || role == ListBoxRole;
    };
    
    if (AccessibilityObject* parent = matchedParent(m_object, false, matchFunc))
        return parent->wrapper();
    return nil;
}

- (AccessibilityObjectWrapper*)_accessibilityLandmarkAncestor
{
    auto matchFunc = [self] (AccessibilityObject* object) {
        return [self _accessibilityIsLandmarkRole:object->roleValue()];
    };
    
    if (AccessibilityObject* parent = matchedParent(m_object, false, matchFunc))
        return parent->wrapper();
    return nil;
}

- (AccessibilityObjectWrapper*)_accessibilityTableAncestor
{
    auto matchFunc = [] (AccessibilityObject* object) {
        return object->roleValue() == TableRole || object->roleValue() == GridRole;
    };
    
    if (AccessibilityObject* parent = matchedParent(m_object, false, matchFunc))
        return parent->wrapper();
    return nil;
}

- (AccessibilityObjectWrapper*)_accessibilityFieldsetAncestor
{
    auto matchFunc = [] (AccessibilityObject* object) {
        return object->isFieldset();
    };
    
    if (AccessibilityObject* parent = matchedParent(m_object, false, matchFunc))
        return parent->wrapper();
    return nil;
}

- (uint64_t)_accessibilityTraitsFromAncestors
{
    uint64_t traits = 0;
    AccessibilityRole role = m_object->roleValue();
    
    // Trait information also needs to be gathered from the parents above the object.
    // The parentObject is needed instead of the unignoredParentObject, because a table might be ignored, but information still needs to be gathered from it.    
    for (AccessibilityObject* parent = m_object->parentObject(); parent != nil; parent = parent->parentObject()) {
        AccessibilityRole parentRole = parent->roleValue();
        if (parentRole == WebAreaRole)
            break;
        
        switch (parentRole) {
            case LinkRole:
            case WebCoreLinkRole:
                traits |= [self _axLinkTrait];
                if (parent->isVisited())
                    traits |= [self _axVisitedTrait];
                break;
            case HeadingRole:
            {
                traits |= [self _axHeaderTrait];
                // If this object has the header trait, we should set the value
                // to the heading level. If it was a static text element, we need to store
                // the value as the label, because the heading level needs to the value.
                AccessibilityObjectWrapper* wrapper = parent->wrapper();
                if (role == StaticTextRole) {
                    // We should only set the text value as the label when there's no
                    // alternate text on the heading parent.
                    NSString *headingLabel = [wrapper baseAccessibilityDescription];
                    if (![headingLabel length])
                        [self setAccessibilityLabel:m_object->stringValue()];
                    else
                        [self setAccessibilityLabel:headingLabel];
                }
                [self setAccessibilityValue:[wrapper accessibilityValue]];
                break;
            }
            case ListBoxRole:
            case ListRole:
                traits |= [self _axContainedByListTrait];
                break;
            case GridRole:
            case TableRole:
                traits |= [self _axContainedByTableTrait];
                break;
            default:
                if ([self _accessibilityIsLandmarkRole:parentRole])
                    traits |= [self _axContainedByLandmarkTrait];
                break;
        }
        
        // If this object has fieldset parent, we should add containedByFieldsetTrait to it.
        if (parent->isFieldset())
            traits |= [self _axContainedByFieldsetTrait];
    }
    
    return traits;
}

- (uint64_t)_accessibilityTextEntryTraits
{
    uint64_t traits = [self _axTextEntryTrait];
    if (m_object->isFocused())
        traits |= ([self _axHasTextCursorTrait] | [self _axTextOperationsAvailableTrait]);
    if (m_object->isPasswordField())
        traits |= [self _axSecureTextFieldTrait];
    if (m_object->roleValue() == SearchFieldRole)
        traits |= [self _axSearchFieldTrait];
    if (m_object->roleValue() == TextAreaRole)
        traits |= [self _axTextAreaTrait];
    return traits;
}

- (uint64_t)accessibilityTraits
{
    if (![self _prepareAccessibilityCall])
        return 0;
    
    AccessibilityRole role = m_object->roleValue();
    uint64_t traits = [self _axWebContentTrait];
    switch (role) {
        case LinkRole:
        case WebCoreLinkRole:
            traits |= [self _axLinkTrait];
            if (m_object->isVisited())
                traits |= [self _axVisitedTrait];
            break;
        case TextFieldRole:
        case SearchFieldRole:
        case TextAreaRole:
            traits |= [self _accessibilityTextEntryTraits];
            break;
        case ImageRole:
            traits |= [self _axImageTrait];
            break;
        case TabRole:
            traits |= [self _axTabButtonTrait];
            break;
        case ButtonRole:
            traits |= [self _axButtonTrait];
            if (m_object->isPressed())
                traits |= [self _axToggleTrait];
            break;
        case PopUpButtonRole:
            traits |= [self _axPopupButtonTrait];
            break;
        case RadioButtonRole:
            traits |= [self _axRadioButtonTrait] | [self _axToggleTrait];
            break;
        case ToggleButtonRole:
        case CheckBoxRole:
        case SwitchRole:
            traits |= ([self _axButtonTrait] | [self _axToggleTrait]);
            break;
        case HeadingRole:
            traits |= [self _axHeaderTrait];
            break;
        case StaticTextRole:
            traits |= [self _axStaticTextTrait];
            break;
        case SliderRole:
            traits |= [self _axAdjustableTrait];
            break;
        case MenuButtonRole:
        case MenuItemRole:
            traits |= [self _axMenuItemTrait];
            break;
        case MenuItemCheckboxRole:
        case MenuItemRadioRole:
            traits |= ([self _axMenuItemTrait] | [self _axToggleTrait]);
            break;
        default:
            break;
    }

    if (m_object->isAttachmentElement())
        traits |= [self _axUpdatesFrequentlyTrait];
    
    if (m_object->isSelected())
        traits |= [self _axSelectedTrait];

    if (!m_object->isEnabled())
        traits |= [self _axNotEnabledTrait];
    
    if (m_accessibilityTraitsFromAncestor == ULLONG_MAX)
        m_accessibilityTraitsFromAncestor = [self _accessibilityTraitsFromAncestors];
    
    traits |= m_accessibilityTraitsFromAncestor;

    return traits;
}

- (BOOL)isSVGGroupElement
{
    // If an SVG group element has a title, it should be an accessible element on iOS.
    Node* node = m_object->node();
    if (node && node->hasTagName(SVGNames::gTag) && [[self accessibilityLabel] length] > 0)
        return YES;
    
    return NO;
}

- (BOOL)determineIsAccessibilityElement
{
    if (!m_object)
        return false;
    
    // Honor when something explicitly makes this an element (super will contain that logic) 
    if ([super isAccessibilityElement])
        return YES;
    
    m_object->updateBackingStore();
    
    switch (m_object->roleValue()) {
        case TextFieldRole:
        case TextAreaRole:
        case ButtonRole:
        case ToggleButtonRole:
        case PopUpButtonRole:
        case CheckBoxRole:
        case RadioButtonRole:
        case SliderRole:
        case MenuButtonRole:
        case ValueIndicatorRole:
        case ImageRole:
        case ImageMapLinkRole:
        case ProgressIndicatorRole:
        case MenuItemRole:
        case MenuItemCheckboxRole:
        case MenuItemRadioRole:
        case IncrementorRole:
        case ComboBoxRole:
        case DisclosureTriangleRole:
        case ImageMapRole:
        case ListMarkerRole:
        case ListBoxOptionRole:
        case TabRole:
        case DocumentMathRole:
        case HorizontalRuleRole:
        case SliderThumbRole:
        case SwitchRole:
        case SearchFieldRole:
        case SpinButtonRole:
            return true;
        case StaticTextRole:
        {
            // Many text elements only contain a space.
            if (![[[self accessibilityLabel] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length])
                return false;

            // Text elements that are just pieces of links or headers should not be exposed.
            if ([AccessibilityUnignoredAncestor([self accessibilityContainer]) containsUnnaturallySegmentedChildren])
                return false;
            return true;
        }
            
        // Don't expose headers as elements; instead expose their children as elements, with the header trait (unless they have no children)
        case HeadingRole:
            if (![self accessibilityElementCount])
                return true;
            return false;
            
        // Links can sometimes be elements (when they only contain static text or don't contain anything).
        // They should not be elements when containing text and other types.
        case WebCoreLinkRole:
        case LinkRole:
            if ([self containsUnnaturallySegmentedChildren] || ![self accessibilityElementCount])
                return true;
            return false;
        case GroupRole:
            if ([self isSVGGroupElement])
                return true;
            FALLTHROUGH;
        // All other elements are ignored on the iphone.
        case AnnotationRole:
        case ApplicationRole:
        case ApplicationAlertRole:
        case ApplicationAlertDialogRole:
        case ApplicationDialogRole:
        case ApplicationLogRole:
        case ApplicationMarqueeRole:
        case ApplicationStatusRole:
        case ApplicationTimerRole:
        case AudioRole:
        case BlockquoteRole:
        case BrowserRole:
        case BusyIndicatorRole:
        case CanvasRole:
        case CaptionRole:
        case CellRole:
        case ColorWellRole:
        case ColumnRole:
        case ColumnHeaderRole:
        case DefinitionRole:
        case DescriptionListRole:
        case DescriptionListTermRole:
        case DescriptionListDetailRole:
        case DetailsRole:
        case DirectoryRole:
        case DivRole:
        case DocumentRole:
        case DocumentArticleRole:
        case DocumentNoteRole:
        case DrawerRole:
        case EditableTextRole:
        case FooterRole:
        case FormRole:
        case GridRole:
        case GridCellRole:
        case GrowAreaRole:
        case HelpTagRole:
        case IgnoredRole:
        case InlineRole:
        case LabelRole:
        case LandmarkBannerRole:
        case LandmarkComplementaryRole:
        case LandmarkContentInfoRole:
        case LandmarkMainRole:
        case LandmarkNavigationRole:
        case LandmarkRegionRole:
        case LandmarkSearchRole:
        case LegendRole:
        case ListRole:
        case ListBoxRole:
        case ListItemRole:
        case MathElementRole:
        case MatteRole:
        case MenuRole:
        case MenuBarRole:
        case MenuListPopupRole:
        case MenuListOptionRole:
        case OutlineRole:
        case ParagraphRole:
        case PreRole:
        case PresentationalRole:
        case RadioGroupRole:
        case RowHeaderRole:
        case RowRole:
        case RubyBaseRole:
        case RubyBlockRole:
        case RubyInlineRole:
        case RubyRunRole:
        case RubyTextRole:
        case RulerRole:
        case RulerMarkerRole:
        case ScrollAreaRole:
        case ScrollBarRole:
        case SheetRole:
        case SpinButtonPartRole:
        case SplitGroupRole:
        case SplitterRole:
        case SummaryRole:
        case SystemWideRole:
        case SVGRootRole:
        case SVGTextPathRole:
        case SVGTextRole:
        case SVGTSpanRole:
        case TabGroupRole:
        case TabListRole:
        case TabPanelRole:
        case TableRole:
        case TableHeaderContainerRole:
        case TreeRole:
        case TreeItemRole:
        case TreeGridRole:
        case ToolbarRole:
        case UnknownRole:
        case UserInterfaceTooltipRole:
        case VideoRole:
        case WebApplicationRole:
        case WebAreaRole:
        case WindowRole:
        case RowGroupRole:
            return false;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

- (BOOL)isAccessibilityElement
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    if (m_isAccessibilityElement == -1)
        m_isAccessibilityElement = [self determineIsAccessibilityElement];
    
    return m_isAccessibilityElement;
}

- (BOOL)stringValueShouldBeUsedInLabel
{
    if (m_object->isTextControl())
        return NO;
    if (m_object->roleValue() == PopUpButtonRole)
        return NO;
    if (m_object->isFileUploadButton())
        return NO;

    return YES;
}

- (BOOL)fileUploadButtonReturnsValueInTitle
{
    return NO;
}

static void appendStringToResult(NSMutableString *result, NSString *string)
{
    ASSERT(result);
    if (![string length])
        return;
    if ([result length])
        [result appendString:@", "];
    [result appendString:string];
}

- (BOOL)_accessibilityHasTouchEventListener
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return m_object->hasTouchEventListener();
}

- (BOOL)_accessibilityValueIsAutofilled
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return m_object->isValueAutofilled();
}

- (CGFloat)_accessibilityMinValue
{
    return m_object->minValueForRange();
}

- (CGFloat)_accessibilityMaxValue
{
    return m_object->maxValueForRange();
}

- (NSString *)accessibilityRoleDescription
{
    return m_object->roleDescription();
}

- (NSString *)accessibilityLabel
{
    if (![self _prepareAccessibilityCall])
        return nil;

    // check if the label was overriden
    NSString *label = [super accessibilityLabel];
    if (label)
        return label;

    // iOS doesn't distinguish between a title and description field,
    // so concatentation will yield the best result.
    NSString *axTitle = [self baseAccessibilityTitle];
    NSString *axDescription = [self baseAccessibilityDescription];
    NSString *landmarkDescription = [self ariaLandmarkRoleDescription];

    // Footer is not considered a landmark, but we want the role description.
    if (m_object->roleValue() == FooterRole)
        landmarkDescription = AXFooterRoleDescriptionText();

    NSMutableString *result = [NSMutableString string];
    if (m_object->roleValue() == HorizontalRuleRole)
        appendStringToResult(result, AXHorizontalRuleDescriptionText());
        
    appendStringToResult(result, axTitle);
    appendStringToResult(result, axDescription);
    if ([self stringValueShouldBeUsedInLabel]) {
        NSString *valueLabel = m_object->stringValue();
        valueLabel = [valueLabel stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
        appendStringToResult(result, valueLabel);
    }
    appendStringToResult(result, landmarkDescription);
    
    return [result length] ? result : nil;
}

- (AccessibilityTableCell*)tableCellParent
{
    // Find if this element is in a table cell.
    auto matchFunc = [] (AccessibilityObject* object) {
        return object->isTableCell();
    };
    
    if (AccessibilityObject* parent = matchedParent(m_object, true, matchFunc))
        return static_cast<AccessibilityTableCell*>(parent);
    return nil;
}

- (AccessibilityTable*)tableParent
{
    // Find if the parent table for the table cell.
    auto matchFunc = [] (AccessibilityObject* object) {
        return is<AccessibilityTable>(*object) && downcast<AccessibilityTable>(*object).isExposableThroughAccessibility();
    };
    
    if (AccessibilityObject* parent = matchedParent(m_object, true, matchFunc))
        return static_cast<AccessibilityTable*>(parent);
    return nil;
}

- (id)accessibilityTitleElement
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AccessibilityObject* titleElement = m_object->titleUIElement();
    if (titleElement)
        return titleElement->wrapper();

    return nil;
}

// Meant to return row or column headers (or other things as the future permits).
- (NSArray *)accessibilityHeaderElements
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return nil;
    
    AccessibilityTable* table = [self tableParent];
    if (!table)
        return nil;
    
    // Get the row and column range, so we can use them to find the headers.
    std::pair<unsigned, unsigned> rowRange;
    std::pair<unsigned, unsigned> columnRange;
    tableCell->rowIndexRange(rowRange);
    tableCell->columnIndexRange(columnRange);
    
    AccessibilityObject::AccessibilityChildrenVector rowHeaders;
    AccessibilityObject::AccessibilityChildrenVector columnHeaders;
    table->rowHeaders(rowHeaders);
    table->columnHeaders(columnHeaders);
    
    NSMutableArray *headers = [NSMutableArray array];
    
    unsigned columnRangeIndex = static_cast<unsigned>(columnRange.first);
    if (columnRangeIndex < columnHeaders.size()) {
        RefPtr<AccessibilityObject> columnHeader = columnHeaders[columnRange.first];
        AccessibilityObjectWrapper* wrapper = columnHeader->wrapper();
        if (wrapper)
            [headers addObject:wrapper];
    }

    unsigned rowRangeIndex = static_cast<unsigned>(rowRange.first);
    // We should consider the cases where the row number does NOT match the index in
    // rowHeaders, the most common case is when row0/col0 does not have a header.
    for (const auto& rowHeader : rowHeaders) {
        if (!is<AccessibilityTableCell>(*rowHeader))
            break;
        std::pair<unsigned, unsigned> rowHeaderRange;
        downcast<AccessibilityTableCell>(*rowHeader).rowIndexRange(rowHeaderRange);
        if (rowRangeIndex >= rowHeaderRange.first && rowRangeIndex < rowHeaderRange.first + rowHeaderRange.second) {
            if (AccessibilityObjectWrapper* wrapper = rowHeader->wrapper())
                [headers addObject:wrapper];
            break;
        }
    }

    return headers;
}

- (id)accessibilityElementForRow:(NSInteger)row andColumn:(NSInteger)column
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AccessibilityTable* table = [self tableParent];
    if (!table)
        return nil;

    AccessibilityTableCell* cell = table->cellForColumnAndRow(column, row);
    if (!cell)
        return nil;
    return cell->wrapper();
}

- (NSUInteger)accessibilityRowCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    return table->rowCount();
}

- (NSUInteger)accessibilityColumnCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    return table->columnCount();
}

- (NSUInteger)accessibilityARIARowCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    NSInteger rowCount = table->ariaRowCount();
    return rowCount > 0 ? rowCount : 0;
}

- (NSUInteger)accessibilityARIAColumnCount
{
    if (![self _prepareAccessibilityCall])
        return 0;
    AccessibilityTable *table = [self tableParent];
    if (!table)
        return 0;
    
    NSInteger colCount = table->ariaColumnCount();
    return colCount > 0 ? colCount : 0;
}

- (NSUInteger)accessibilityARIARowIndex
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;
    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSNotFound;
    
    NSInteger rowIndex = tableCell->ariaRowIndex();
    return rowIndex > 0 ? rowIndex : NSNotFound;
}

- (NSUInteger)accessibilityARIAColumnIndex
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;
    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSNotFound;
    
    NSInteger columnIndex = tableCell->ariaColumnIndex();
    return columnIndex > 0 ? columnIndex : NSNotFound;
}

- (NSRange)accessibilityRowRange
{
    if (![self _prepareAccessibilityCall])
        return NSMakeRange(NSNotFound, 0);

    if (m_object->isRadioButton()) {
        AccessibilityObject::AccessibilityChildrenVector radioButtonSiblings;
        m_object->linkedUIElements(radioButtonSiblings);
        if (radioButtonSiblings.size() <= 1)
            return NSMakeRange(NSNotFound, 0);
        
        return NSMakeRange(radioButtonSiblings.find(m_object), radioButtonSiblings.size());
    }
    
    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSMakeRange(NSNotFound, 0);
    
    std::pair<unsigned, unsigned> rowRange;
    tableCell->rowIndexRange(rowRange);
    return NSMakeRange(rowRange.first, rowRange.second);
}

- (NSRange)accessibilityColumnRange
{
    if (![self _prepareAccessibilityCall])
        return NSMakeRange(NSNotFound, 0);

    AccessibilityTableCell* tableCell = [self tableCellParent];
    if (!tableCell)
        return NSMakeRange(NSNotFound, 0);
    
    std::pair<unsigned, unsigned> columnRange;
    tableCell->columnIndexRange(columnRange);
    return NSMakeRange(columnRange.first, columnRange.second);
}

- (NSString *)accessibilityPlaceholderValue
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->placeholderValue();
}

- (NSString *)accessibilityValue
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    // check if the value was overriden
    NSString *value = [super accessibilityValue];
    if (value)
        return value;
    
    AccessibilityRole role = m_object->roleValue();
    if (m_object->isCheckboxOrRadio() || role == MenuItemCheckboxRole || role == MenuItemRadioRole) {
        switch (m_object->checkboxOrRadioValue()) {
        case ButtonStateOff:
            return [NSString stringWithFormat:@"%d", 0];
        case ButtonStateOn:
            return [NSString stringWithFormat:@"%d", 1];
        case ButtonStateMixed:
            return [NSString stringWithFormat:@"%d", 2];
        }
        ASSERT_NOT_REACHED();
        return [NSString stringWithFormat:@"%d", 0];
    }
    
    if (m_object->isButton() && m_object->isPressed())
        return [NSString stringWithFormat:@"%d", 1];

    // rdar://8131388 WebKit should expose the same info as UIKit for its password fields.
    if (m_object->isPasswordField()) {
        int passwordLength = m_object->accessibilityPasswordFieldLength();
        NSMutableString* string = [NSMutableString string];
        for (int k = 0; k < passwordLength; ++k)
            [string appendString:@"•"];
        return string;
    }
    
    // A text control should return its text data as the axValue (per iPhone AX API).
    if (![self stringValueShouldBeUsedInLabel])
        return m_object->stringValue();
    
    if (m_object->isRangeControl()) {
        // Prefer a valueDescription if provided by the author (through aria-valuetext).
        String valueDescription = m_object->valueDescription();
        if (!valueDescription.isEmpty())
            return valueDescription;

        return [NSString stringWithFormat:@"%.2f", m_object->valueForRange()];
    }

    if (is<AccessibilityAttachment>(m_object) && downcast<AccessibilityAttachment>(m_object)->hasProgress())
        return [NSString stringWithFormat:@"%.2f", m_object->valueForRange()];
    
    if (m_object->isHeading())
        return [NSString stringWithFormat:@"%d", m_object->headingLevel()];
    
    return nil;
}

- (BOOL)accessibilityIsAttachmentElement
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return is<AccessibilityAttachment>(m_object);
}

- (BOOL)accessibilityIsComboBox
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return m_object->roleValue() == ComboBoxRole;
}

- (NSString *)accessibilityHint
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return [self baseAccessibilityHelpText];
}

- (NSURL *)accessibilityURL
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    URL url = m_object->url();
    if (url.isNull())
        return nil;
    return (NSURL*)url;
}

- (CGPoint)_accessibilityConvertPointToViewSpace:(CGPoint)point
{
    if (![self _prepareAccessibilityCall])
        return point;
    
    FloatPoint floatPoint = FloatPoint(point);
    return [self convertPointToScreenSpace:floatPoint];
}

- (BOOL)_accessibilityScrollToVisible
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    m_object->scrollToMakeVisible();
    return YES;
}


- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    AccessibilityObject::ScrollByPageDirection scrollDirection;
    switch (direction) {
    case UIAccessibilityScrollDirectionRight:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Right;
        break;
    case UIAccessibilityScrollDirectionLeft:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Left;
        break;
    case UIAccessibilityScrollDirectionUp:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Up;
        break;
    case UIAccessibilityScrollDirectionDown:
        scrollDirection = AccessibilityObject::ScrollByPageDirection::Down;
        break;
    default:
        return NO;
    }

    BOOL result = m_object->scrollByPage(scrollDirection);
    
    if (result) {
        [self postScrollStatusChangeNotification];

        CGPoint scrollPos = [self _accessibilityScrollPosition];
        NSString *testString = [NSString stringWithFormat:@"AXScroll [position: %.2f %.2f]", scrollPos.x, scrollPos.y];
        [self accessibilityPostedNotification:@"AXScrollByPage" userInfo:@{ @"status" : testString }];
    }
    
    // This means that this object handled the scroll and no other ancestor should attempt scrolling.
    return result;
}

- (CGPoint)convertPointToScreenSpace:(FloatPoint &)point
{
    if (!m_object)
        return CGPointZero;
    
    CGPoint cgPoint = CGPointMake(point.x(), point.y());
    
    FrameView* frameView = m_object->documentFrameView();
    WAKView* documentView = frameView ? frameView->documentView() : nullptr;
    if (documentView) {
        cgPoint = [documentView convertPoint:cgPoint toView:nil];

        // we need the web document view to give us our final screen coordinates
        // because that can take account of the scroller
        id webDocument = [self _accessibilityWebDocumentView];
        if (webDocument)
            cgPoint = [webDocument convertPoint:cgPoint toView:nil];
    }
    else {
        // Find the appropriate scroll view to use to convert the contents to the window.
        auto matchFunc = [] (AccessibilityObject* object) {
            return is<AccessibilityScrollView>(*object);
        };
        
        ScrollView* scrollView = nullptr;
        AccessibilityObject* parent = matchedParent(m_object, false, matchFunc);
        if (parent)
            scrollView = downcast<AccessibilityScrollView>(*parent).scrollView();
        
        IntPoint intPoint = flooredIntPoint(point);
        if (scrollView)
            intPoint = scrollView->contentsToRootView(intPoint);
        
        Page* page = m_object->page();
        
        // If we have an empty chrome client (like SVG) then we should use the page
        // of the scroll view parent to help us get to the screen rect.
        if (parent && page && page->chrome().client().isEmptyChromeClient())
            page = parent->page();
        
        if (page) {
            IntRect rect = IntRect(intPoint, IntSize(0, 0));
            intPoint = page->chrome().rootViewToAccessibilityScreen(rect).location();
        }
        
        cgPoint = (CGPoint)intPoint;
    }
    
    return cgPoint;
}

- (CGRect)convertRectToScreenSpace:(IntRect &)rect
{
    if (!m_object)
        return CGRectZero;
    
    CGSize size = CGSizeMake(rect.size().width(), rect.size().height());
    CGPoint point = CGPointMake(rect.x(), rect.y());
    
    CGRect frame = CGRectMake(point.x, point.y, size.width, size.height);
    
    FrameView* frameView = m_object->documentFrameView();
    WAKView* documentView = frameView ? frameView->documentView() : nil;
    if (documentView) {
        frame = [documentView convertRect:frame toView:nil];
        
        // we need the web document view to give us our final screen coordinates
        // because that can take account of the scroller
        id webDocument = [self _accessibilityWebDocumentView];
        if (webDocument)
            frame = [webDocument convertRect:frame toView:nil];
        
    } else {
        // Find the appropriate scroll view to use to convert the contents to the window.
        auto matchFunc = [] (AccessibilityObject* object) {
            return is<AccessibilityScrollView>(*object);
        };

        ScrollView* scrollView = nullptr;
        AccessibilityObject* parent = matchedParent(m_object, false, matchFunc);
        if (parent)
            scrollView = downcast<AccessibilityScrollView>(*parent).scrollView();
        
        if (scrollView)
            rect = scrollView->contentsToRootView(rect);
        
        Page* page = m_object->page();
        
        // If we have an empty chrome client (like SVG) then we should use the page
        // of the scroll view parent to help us get to the screen rect.
        if (parent && page && page->chrome().client().isEmptyChromeClient())
            page = parent->page();
        
        if (page)
            rect = page->chrome().rootViewToAccessibilityScreen(rect);
        
        frame = (CGRect)rect;
    }
    
    return frame;
}

// Used by UIKit accessibility bundle to help determine distance during a hit-test.
- (CGRect)accessibilityElementRect
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    
    LayoutRect rect = m_object->elementRect();
    return CGRectMake(rect.x(), rect.y(), rect.width(), rect.height());
}

// The "center point" is where VoiceOver will "press" an object. This may not be the actual
// center of the accessibilityFrame
- (CGPoint)accessibilityActivationPoint
{
    if (![self _prepareAccessibilityCall])
        return CGPointZero;
    
    IntRect rect = snappedIntRect(m_object->boundingBoxRect());
    CGRect cgRect = [self convertRectToScreenSpace:rect];
    return CGPointMake(CGRectGetMidX(cgRect), CGRectGetMidY(cgRect));
}

- (CGRect)accessibilityFrame
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    
    IntRect rect = snappedIntRect(m_object->elementRect());
    return [self convertRectToScreenSpace:rect];
}

// Checks whether a link contains only static text and images (and has been divided unnaturally by <spans> and other nefarious mechanisms).
- (BOOL)containsUnnaturallySegmentedChildren
{
    if (!m_object)
        return NO;
    
    AccessibilityRole role = m_object->roleValue();
    if (role != LinkRole && role != WebCoreLinkRole)
        return NO;
    
    const auto& children = m_object->children();
    unsigned childrenSize = children.size();

    // If there's only one child, then it doesn't have segmented children. 
    if (childrenSize == 1)
        return NO;
    
    for (unsigned i = 0; i < childrenSize; ++i) {
        AccessibilityRole role = children[i]->roleValue();
        if (role != StaticTextRole && role != ImageRole && role != GroupRole)
            return NO;
    }
    
    return YES;
}

- (id)accessibilityContainer
{
    if (![self _prepareAccessibilityCall])
        return nil;

    AXAttributeCacheEnabler enableCache(m_object->axObjectCache());
    
    // As long as there's a parent wrapper, that's the correct chain to climb.
    AccessibilityObject* parent = m_object->parentObjectUnignored(); 
    if (parent)
        return parent->wrapper();

    // Mock objects can have their parents detached but still exist in the cache.
    if (m_object->isDetachedFromParent())
        return nil;
    
    // The only object without a parent wrapper at this point should be a scroll view.
    ASSERT(m_object->isAccessibilityScrollView());
    
    // Verify this is the top document. If not, we might need to go through the platform widget.
    FrameView* frameView = m_object->documentFrameView();
    Document* document = m_object->document();
    if (document && frameView && document != &document->topDocument())
        return frameView->platformWidget();
    
    // The top scroll view's parent is the web document view.
    return [self _accessibilityWebDocumentView];
}

- (id)accessibilityFocusedUIElement
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    AccessibilityObject* focusedObj = m_object->focusedUIElement();
    
    if (!focusedObj)
        return nil;
    
    return focusedObj->wrapper();
}

- (id)_accessibilityWebDocumentView
{
    if (![self _prepareAccessibilityCall])
        return nil;

    // This method performs the crucial task of connecting to the UIWebDocumentView.
    // This is needed to correctly calculate the screen position of the AX object.
    static Class webViewClass = nil;
    if (!webViewClass)
        webViewClass = NSClassFromString(@"WebView");

    if (!webViewClass)
        return nil;
    
    FrameView* frameView = m_object->documentFrameView();

    if (!frameView)
        return nil;
    
    // If this is the top level frame, the UIWebDocumentView should be returned.
    id parentView = frameView->documentView();
    while (parentView && ![parentView isKindOfClass:webViewClass])
        parentView = [parentView superview];
    
    // The parentView should have an accessibilityContainer, if the UIKit accessibility bundle was loaded. 
    // The exception is DRT, which tests accessibility without the entire system turning accessibility on. Hence,
    // this check should be valid for everything except DRT.
    ASSERT([parentView accessibilityContainer] || IOSApplication::isDumpRenderTree());
    
    return [parentView accessibilityContainer];
}

- (NSArray *)_accessibilityNextElementsWithCount:(UInt32)count
{    
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [[self _accessibilityWebDocumentView] _accessibilityNextElementsWithCount:count];
}

- (NSArray *)_accessibilityPreviousElementsWithCount:(UInt32)count
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [[self _accessibilityWebDocumentView] _accessibilityPreviousElementsWithCount:count];
}

- (BOOL)accessibilityCanSetValue
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return m_object->canSetValueAttribute();
}

- (BOOL)accessibilityRequired
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return m_object->isRequired();
}

- (NSArray *)accessibilityFlowToElements
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    AccessibilityObject::AccessibilityChildrenVector children;
    m_object->ariaFlowToElements(children);
    
    unsigned length = children.size();
    NSMutableArray* array = [NSMutableArray arrayWithCapacity:length];
    for (unsigned i = 0; i < length; ++i) {
        AccessibilityObjectWrapper* wrapper = children[i]->wrapper();
        ASSERT(wrapper);
        if (!wrapper)
            continue;

        if (children[i]->isAttachment() && [wrapper attachmentView])
            [array addObject:[wrapper attachmentView]];
        else
            [array addObject:wrapper];
    }
    return array;
}

- (id)accessibilityLinkedElement
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    // If this static text inside of a link, it should use its parent's linked element.
    AccessibilityObject* element = m_object;
    if (m_object->roleValue() == StaticTextRole && m_object->parentObjectUnignored()->isLink())
        element = m_object->parentObjectUnignored();
    
    AccessibilityObject::AccessibilityChildrenVector children;
    element->linkedUIElements(children);
    if (children.size() == 0)
        return nil;
    
    return children[0]->wrapper();
}


- (BOOL)isAttachment
{
    if (!m_object)
        return NO;
    
    return m_object->isAttachment();
}

- (void)_accessibilityActivate
{
    if (![self _prepareAccessibilityCall])
        return;

    m_object->press();
}

- (id)attachmentView
{
    if (![self _prepareAccessibilityCall])
        return nil;

    ASSERT([self isAttachment]);
    Widget* widget = m_object->widgetForAttachmentView();
    if (!widget)
        return nil;
    return widget->platformWidget();    
}

static RenderObject* rendererForView(WAKView* view)
{
    if (![view conformsToProtocol:@protocol(WebCoreFrameView)])
        return nil;
    
    WAKView<WebCoreFrameView>* frameView = (WAKView<WebCoreFrameView>*)view;
    Frame* frame = [frameView _web_frame];
    if (!frame)
        return nil;
    
    Node* node = frame->document()->ownerElement();
    if (!node)
        return nil;
    
    return node->renderer();
}

- (id)_accessibilityParentForSubview:(id)subview
{   
    RenderObject* renderer = rendererForView(subview);
    if (!renderer)
        return nil;
    
    AccessibilityObject* obj = renderer->document().axObjectCache()->getOrCreate(renderer);
    if (obj)
        return obj->parentObjectUnignored()->wrapper();
    return nil;
}

- (void)postFocusChangeNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.
}

- (void)postSelectedTextChangeNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.    
}

- (void)postLayoutChangeNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.        
}

- (void)postLiveRegionChangeNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.    
}

- (void)postLiveRegionCreatedNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.    
}

- (void)postLoadCompleteNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.    
}

- (void)postChildrenChangedNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.    
}

- (void)postInvalidStatusChangedNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.        
}

- (void)postValueChangedNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.
}

- (void)postExpandedChangedNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.
}

- (void)postScrollStatusChangeNotification
{
    // The UIKit accessibility wrapper will override and post appropriate notification.
}

// These will be used by the UIKit wrapper to calculate an appropriate description of scroll status.
- (CGPoint)_accessibilityScrollPosition
{
    if (![self _prepareAccessibilityCall])
        return CGPointZero;
    
    return m_object->scrollPosition();
}

- (CGSize)_accessibilityScrollSize
{
    if (![self _prepareAccessibilityCall])
        return CGSizeZero;
    
    return m_object->scrollContentsSize();
}

- (CGRect)_accessibilityScrollVisibleRect
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;
    
    return m_object->scrollVisibleContentRect();
}

- (AccessibilityObject*)detailParentForSummaryObject:(AccessibilityObject*)object
{
    // Use this to check if an object is the child of a summary object.
    // And return the summary's parent, which is the expandable details object.
    auto matchFunc = [] (AccessibilityObject* object) {
        return object->hasTagName(summaryTag);
    };
    
    if (AccessibilityObject* summary = matchedParent(object, true, matchFunc))
        return summary->parentObject();
    return nil;
}

- (AccessibilityObject*)detailParentForObject:(AccessibilityObject*)object
{
    // Use this to check if an object is inside a details object.
    auto matchFunc = [] (AccessibilityObject* object) {
        return object->hasTagName(detailsTag);
    };
    
    if (AccessibilityObject* details = matchedParent(object, true, matchFunc))
        return details;
    return nil;
}

- (void)accessibilityElementDidBecomeFocused
{
    if (![self _prepareAccessibilityCall])
        return;
    
    // The focused VoiceOver element might be the text inside a link.
    // In those cases we should focus on the link itself.
    for (AccessibilityObject* object = m_object; object != nil; object = object->parentObject()) {
        if (object->roleValue() == WebAreaRole)
            break;

        if (object->canSetFocusAttribute()) {
            // webkit.org/b/162041 Taking focus onto elements inside a details node will cause VO focusing onto random items.
            if ([self detailParentForObject:object])
                break;
            
            // webkit.org/b/162322 When a dialog is focusable, allowing focusing onto the dialog node will cause VO cursor jumping
            // back and forward while navigating its children.
            if ([object->wrapper() accessibilityIsDialog])
                break;
            
            object->setFocused(true);
            break;
        }
    }
}

- (void)accessibilityModifySelection:(TextGranularity)granularity increase:(BOOL)increase
{
    if (![self _prepareAccessibilityCall])
        return;
    
    FrameSelection& frameSelection = m_object->document()->frame()->selection();
    VisibleSelection selection = m_object->selection();
    VisiblePositionRange range = m_object->visiblePositionRange();
    
    // Before a selection with length exists, the cursor position needs to move to the right starting place.
    // That should be the beginning of this element (range.start). However, if the cursor is already within the 
    // range of this element (the cursor is represented by selection), then the cursor does not need to move.
    if (frameSelection.isNone() && (selection.visibleStart() < range.start || selection.visibleEnd() > range.end))
        frameSelection.moveTo(range.start, UserTriggered);
    
    frameSelection.modify(FrameSelection::AlterationExtend, (increase) ? DirectionRight : DirectionLeft, granularity, UserTriggered);
}

- (void)accessibilityIncreaseSelection:(TextGranularity)granularity
{
    [self accessibilityModifySelection:granularity increase:YES];
}

- (void)accessibilityDecreaseSelection:(TextGranularity)granularity
{
    [self accessibilityModifySelection:granularity increase:NO];
}

- (void)accessibilityMoveSelectionToMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return;
    
    VisiblePosition visiblePosition = [marker visiblePosition];
    if (visiblePosition.isNull())
        return;

    FrameSelection& frameSelection = m_object->document()->frame()->selection();
    frameSelection.moveTo(visiblePosition, UserTriggered);
}

- (void)accessibilityIncrement
{
    if (![self _prepareAccessibilityCall])
        return;

    m_object->increment();
}

- (void)accessibilityDecrement
{
    if (![self _prepareAccessibilityCall])
        return;

    m_object->decrement();
}

#pragma mark Accessibility Text Marker Handlers

- (BOOL)_addAccessibilityObject:(AccessibilityObject*)axObject toTextMarkerArray:(NSMutableArray *)array
{
    if (!axObject)
        return NO;
    
    AccessibilityObjectWrapper* wrapper = axObject->wrapper();
    if (!wrapper)
        return NO;

    // Don't add the same object twice, but since this has already been added, we should return
    // YES because we want to inform that it's in the array
    if ([array containsObject:wrapper])
        return YES;
    
    // Explicity set that this is now an element (in case other logic tries to override).
    [wrapper setValue:[NSNumber numberWithBool:YES] forKey:@"isAccessibilityElement"];    
    [array addObject:wrapper];
    return YES;
}

- (void)_accessibilitySetValue:(NSString *)string
{
    if (![self _prepareAccessibilityCall])
        return;
    m_object->setValue(string);
}

- (NSString *)stringForTextMarkers:(NSArray *)markers
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    RefPtr<Range> range = [self rangeForTextMarkers:markers];
    if (!range)
        return nil;
    
    return m_object->stringForRange(range);
}

static int blockquoteLevel(RenderObject* renderer)
{
    if (!renderer)
        return 0;
    
    int result = 0;
    for (Node* node = renderer->node(); node; node = node->parentNode()) {
        if (node->hasTagName(blockquoteTag))
            result += 1;
    }
    
    return result;
}

static void AXAttributeStringSetLanguage(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    if (!renderer)
        return;
    
    AccessibilityObject* axObject = renderer->document().axObjectCache()->getOrCreate(renderer);
    NSString *language = axObject->language();
    if ([language length])
        [attrString addAttribute:UIAccessibilityTokenLanguage value:language range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenLanguage range:range];
}

static void AXAttributeStringSetBlockquoteLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    int quoteLevel = blockquoteLevel(renderer);
    
    if (quoteLevel)
        [attrString addAttribute:UIAccessibilityTokenBlockquoteLevel value:[NSNumber numberWithInt:quoteLevel] range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenBlockquoteLevel range:range];
}

static void AXAttributeStringSetHeadingLevel(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    if (!renderer)
        return;
    
    AccessibilityObject* parentObject = renderer->document().axObjectCache()->getOrCreate(renderer->parent());
    int parentHeadingLevel = parentObject->headingLevel();
    
    if (parentHeadingLevel)
        [attrString addAttribute:UIAccessibilityTokenHeadingLevel value:[NSNumber numberWithInt:parentHeadingLevel] range:range];
    else
        [attrString removeAttribute:UIAccessibilityTokenHeadingLevel range:range];
}

static void AXAttributeStringSetFont(NSMutableAttributedString* attrString, CTFontRef font, NSRange range)
{
    if (!font)
        return;
    
    RetainPtr<CFStringRef> fullName = adoptCF(CTFontCopyFullName(font));
    RetainPtr<CFStringRef> familyName = adoptCF(CTFontCopyFamilyName(font));

    NSNumber* size = [NSNumber numberWithFloat:CTFontGetSize(font)];
    CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(font);
    NSNumber* bold = [NSNumber numberWithBool:(traits & kCTFontTraitBold)];
    if (fullName)
        [attrString addAttribute:UIAccessibilityTokenFontName value:(NSString*)fullName.get() range:range];
    if (familyName)
        [attrString addAttribute:UIAccessibilityTokenFontFamily value:(NSString*)familyName.get() range:range];
    if ([size boolValue])
        [attrString addAttribute:UIAccessibilityTokenFontSize value:size range:range];
    if ([bold boolValue] || (traits & kCTFontTraitBold))
        [attrString addAttribute:UIAccessibilityTokenBold value:[NSNumber numberWithBool:YES] range:range];
    if (traits & kCTFontTraitItalic)
        [attrString addAttribute:UIAccessibilityTokenItalic value:[NSNumber numberWithBool:YES] range:range];

}

static void AXAttributeStringSetNumber(NSMutableAttributedString* attrString, NSString* attribute, NSNumber* number, NSRange range)
{
    if (number)
        [attrString addAttribute:attribute value:number range:range];
    else
        [attrString removeAttribute:attribute range:range];
}

static void AXAttributeStringSetStyle(NSMutableAttributedString* attrString, RenderObject* renderer, NSRange range)
{
    auto& style = renderer->style();
    
    // set basic font info
    AXAttributeStringSetFont(attrString, style.fontCascade().primaryFont().getCTFont(), range);
                
    int decor = style.textDecorationsInEffect();
    if (decor & TextDecorationUnderline)
        AXAttributeStringSetNumber(attrString, UIAccessibilityTokenUnderline, [NSNumber numberWithBool:YES], range);
}

static void AXAttributedStringAppendText(NSMutableAttributedString* attrString, Node* node, NSString *text)
{
    // skip invisible text
    if (!node->renderer())
        return;
    
    // easier to calculate the range before appending the string
    NSRange attrStringRange = NSMakeRange([attrString length], [text length]);
    
    // append the string from this node
    [[attrString mutableString] appendString:text];
    
    // set new attributes
    AXAttributeStringSetStyle(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetHeadingLevel(attrString, node->renderer(), attrStringRange);
    AXAttributeStringSetBlockquoteLevel(attrString, node->renderer(), attrStringRange);    
    AXAttributeStringSetLanguage(attrString, node->renderer(), attrStringRange);
}


// This method is intended to return an array of strings and accessibility elements that 
// represent the objects on one line of rendered web content. The array of markers sent
// in should be ordered and contain only a start and end marker.
- (NSArray *)arrayOfTextForTextMarkers:(NSArray *)markers attributed:(BOOL)attributed
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if ([markers count] != 2)
        return nil;
    
    WebAccessibilityTextMarker* startMarker = [markers objectAtIndex:0];
    WebAccessibilityTextMarker* endMarker = [markers objectAtIndex:1];
    if (![startMarker isKindOfClass:[WebAccessibilityTextMarker class]] || ![endMarker isKindOfClass:[WebAccessibilityTextMarker class]])
        return nil;
    
    // extract the start and end VisiblePosition
    VisiblePosition startVisiblePosition = [startMarker visiblePosition];
    if (startVisiblePosition.isNull())
        return nil;
    
    VisiblePosition endVisiblePosition = [endMarker visiblePosition];
    if (endVisiblePosition.isNull())
        return nil;
    
    // iterate over the range to build the AX attributed string
    NSMutableArray* array = [[NSMutableArray alloc] init];
    TextIterator it(makeRange(startVisiblePosition, endVisiblePosition).get());
    for (; !it.atEnd(); it.advance()) {
        // locate the node and starting offset for this range
        Node& node = it.range()->startContainer();
        ASSERT(&node == &it.range()->endContainer());
        int offset = it.range()->startOffset();
        
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.text().length() != 0) {
            if (!attributed) {
                // First check if this is represented by a link.
                AccessibilityObject* linkObject = AccessibilityObject::anchorElementForNode(&node);
                if ([self _addAccessibilityObject:linkObject toTextMarkerArray:array])
                    continue;
                
                // Next check if this region is represented by a heading.
                AccessibilityObject* headingObject = AccessibilityObject::headingElementForNode(&node);
                if ([self _addAccessibilityObject:headingObject toTextMarkerArray:array])
                    continue;
                
                String listMarkerText = m_object->listMarkerTextForNodeAndPosition(&node, VisiblePosition(it.range()->startPosition()));
                
                if (!listMarkerText.isEmpty()) 
                    [array addObject:listMarkerText];
                // There was not an element representation, so just return the text.
                [array addObject:it.text().createNSString().get()];
            }
            else
            {
                String listMarkerText = m_object->listMarkerTextForNodeAndPosition(&node, VisiblePosition(it.range()->startPosition()));

                if (!listMarkerText.isEmpty()) {
                    NSMutableAttributedString* attrString = [[NSMutableAttributedString alloc] init];
                    AXAttributedStringAppendText(attrString, &node, listMarkerText);
                    [array addObject:attrString];
                    [attrString release];
                }
                
                NSMutableAttributedString *attrString = [[NSMutableAttributedString alloc] init];
                AXAttributedStringAppendText(attrString, &node, it.text().createNSStringWithoutCopying().get());
                [array addObject:attrString];
                [attrString release];
            }
        } else {
            Node* replacedNode = node.traverseToChildAt(offset);
            if (replacedNode) {
                AccessibilityObject* obj = m_object->axObjectCache()->getOrCreate(replacedNode->renderer());
                if (obj && !obj->accessibilityIsIgnored())
                    [self _addAccessibilityObject:obj toTextMarkerArray:array];
            }
        }
    }
    
    return [array autorelease];
}

- (NSRange)_convertToNSRange:(Range *)range
{
    if (!range)
        return NSMakeRange(NSNotFound, 0);
    
    Document* document = m_object->document();
    Element* selectionRoot = document->frame()->selection().selection().rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : document->documentElement();
    
    // Mouse events may cause TSM to attempt to create an NSRange for a portion of the view
    // that is not inside the current editable region.  These checks ensure we don't produce
    // potentially invalid data when responding to such requests.
    if (&range->startContainer() != scope && !range->startContainer().isDescendantOf(scope))
        return NSMakeRange(NSNotFound, 0);
    if (&range->endContainer() != scope && !range->endContainer().isDescendantOf(scope))
        return NSMakeRange(NSNotFound, 0);
    
    RefPtr<Range> testRange = Range::create(scope->document(), scope, 0, &range->startContainer(), range->startOffset());
    ASSERT(&testRange->startContainer() == scope);
    int startPosition = TextIterator::rangeLength(testRange.get());
    
    ExceptionCode ec;
    testRange->setEnd(range->endContainer(), range->endOffset(), ec);
    ASSERT(&testRange->startContainer() == scope);
    int endPosition = TextIterator::rangeLength(testRange.get());
    return NSMakeRange(startPosition, endPosition - startPosition);
}

- (RefPtr<Range>)_convertToDOMRange:(NSRange)nsrange
{
    if (nsrange.location > INT_MAX)
        return nullptr;
    if (nsrange.length > INT_MAX || nsrange.location + nsrange.length > INT_MAX)
        nsrange.length = INT_MAX - nsrange.location;
        
    // our critical assumption is that we are only called by input methods that
    // concentrate on a given area containing the selection
    // We have to do this because of text fields and textareas. The DOM for those is not
    // directly in the document DOM, so serialization is problematic. Our solution is
    // to use the root editable element of the selection start as the positional base.
    // That fits with AppKit's idea of an input context.
    Document* document = m_object->document();
    Element* selectionRoot = document->frame()->selection().selection().rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : document->documentElement();
    return TextIterator::rangeFromLocationAndLength(scope, nsrange.location, nsrange.length);
}

// This method is intended to take a text marker representing a VisiblePosition and convert it
// into a normalized location within the document.
- (NSInteger)positionForTextMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return NSNotFound;

    if (!marker)
        return NSNotFound;    

    if (AXObjectCache* cache = m_object->axObjectCache()) {
        CharacterOffset characterOffset = [marker characterOffset];
        // Create a collapsed range from the CharacterOffset object.
        RefPtr<Range> range = cache->rangeForUnorderedCharacterOffsets(characterOffset, characterOffset);
        NSRange nsRange = [self _convertToNSRange:range.get()];
        return nsRange.location;
    }
    return NSNotFound;
}

- (NSArray *)textMarkerRange
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    RefPtr<Range> range = m_object->elementRange();
    return [self textMarkersForRange:range];
}

// A method to get the normalized text cursor range of an element. Used in DumpRenderTree.
- (NSRange)elementTextRange
{
    if (![self _prepareAccessibilityCall])
        return NSMakeRange(NSNotFound, 0);

    NSArray *markers = [self textMarkerRange];
    if ([markers count] != 2)
        return NSMakeRange(NSNotFound, 0);
    
    WebAccessibilityTextMarker *startMarker = [markers objectAtIndex:0];
    WebAccessibilityTextMarker *endMarker = [markers objectAtIndex:1];
    
    NSInteger startPosition = [self positionForTextMarker:startMarker];
    NSInteger endPosition = [self positionForTextMarker:endMarker];
    
    return NSMakeRange(startPosition, endPosition - startPosition);
}

- (AccessibilityObjectWrapper *)accessibilityObjectForTextMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    AccessibilityObject* obj = [marker accessibilityObject];
    if (!obj)
        return nil;
    
    return AccessibilityUnignoredAncestor(obj->wrapper());
}

- (NSArray *)textMarkerRangeForSelection
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    VisibleSelection selection = m_object->selection();
    if (selection.isNone())
        return nil;
    
    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nil;
    
    RefPtr<Range> range = selection.toNormalizedRange();
    CharacterOffset start = cache->startOrEndCharacterOffsetForRange(range, true);
    CharacterOffset end = cache->startOrEndCharacterOffsetForRange(range, false);

    WebAccessibilityTextMarker* startMarker = [WebAccessibilityTextMarker textMarkerWithCharacterOffset:start cache:cache];
    WebAccessibilityTextMarker* endMarker = [WebAccessibilityTextMarker textMarkerWithCharacterOffset:end cache:cache];
    if (!startMarker || !endMarker)
        return nil;
    
    return [NSArray arrayWithObjects:startMarker, endMarker, nil];
}

- (WebAccessibilityTextMarker *)textMarkerForPosition:(NSInteger)position
{
    if (![self _prepareAccessibilityCall])
        return nil;

    RefPtr<Range> range = [self _convertToDOMRange:NSMakeRange(position, 0)];
    if (!range)
        return nil;

    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nil;
    
    CharacterOffset characterOffset = cache->startOrEndCharacterOffsetForRange(range, true);
    return [WebAccessibilityTextMarker textMarkerWithCharacterOffset:characterOffset cache:cache];
}

- (id)_stringForRange:(NSRange)range attributed:(BOOL)attributed
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    WebAccessibilityTextMarker* startMarker = [self textMarkerForPosition:range.location];
    WebAccessibilityTextMarker* endMarker = [self textMarkerForPosition:NSMaxRange(range)];
    
    // Clients don't always know the exact range, rather than force them to compute it,
    // allow clients to overshoot and use the max text marker range.
    if (!startMarker || !endMarker) {
        NSArray *markers = [self textMarkerRange];
        if ([markers count] != 2)
            return nil;
        if (!startMarker)
            startMarker = [markers objectAtIndex:0];
        if (!endMarker)
            endMarker = [markers objectAtIndex:1];
    }
    
    NSArray* array = [self arrayOfTextForTextMarkers:[NSArray arrayWithObjects:startMarker, endMarker, nil] attributed:attributed];
    Class returnClass = attributed ? [NSMutableAttributedString class] : [NSMutableString class];
    id returnValue = [[(NSString *)[returnClass alloc] init] autorelease];
    
    const unichar attachmentChar = NSAttachmentCharacter;
    NSInteger count = [array count];
    for (NSInteger k = 0; k < count; ++k) {
        id object = [array objectAtIndex:k];

        if (attributed && [object isKindOfClass:[WebAccessibilityObjectWrapper class]])
            object = [[[NSMutableAttributedString alloc] initWithString:[NSString stringWithCharacters:&attachmentChar length:1] attributes:@{ UIAccessibilityTokenAttachment : object }] autorelease];
        
        if (![object isKindOfClass:returnClass])
            continue;
        
        if (attributed)
            [(NSMutableAttributedString *)returnValue appendAttributedString:object];
        else
            [(NSMutableString *)returnValue appendString:object];
    }
    return returnValue;
}


// A convenience method for getting the text of a NSRange. Currently used only by DRT.
- (NSString *)stringForRange:(NSRange)range
{
    return [self _stringForRange:range attributed:NO];
}

- (NSAttributedString *)attributedStringForRange:(NSRange)range
{
    return [self _stringForRange:range attributed:YES];
}

- (NSRange)_accessibilitySelectedTextRange
{
    if (![self _prepareAccessibilityCall] || !m_object->isTextControl())
        return NSMakeRange(NSNotFound, 0);
    
    PlainTextRange textRange = m_object->selectedTextRange();
    if (textRange.isNull())
        return NSMakeRange(NSNotFound, 0);
    return NSMakeRange(textRange.start, textRange.length);    
}

- (void)_accessibilitySetSelectedTextRange:(NSRange)range
{
    if (![self _prepareAccessibilityCall] || !m_object->isTextControl())
        return;
    
    m_object->setSelectedTextRange(PlainTextRange(range.location, range.length));
}

// A convenience method for getting the accessibility objects of a NSRange. Currently used only by DRT.
- (NSArray *)elementsForRange:(NSRange)range
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    WebAccessibilityTextMarker* startMarker = [self textMarkerForPosition:range.location];
    WebAccessibilityTextMarker* endMarker = [self textMarkerForPosition:NSMaxRange(range)];
    if (!startMarker || !endMarker)
        return nil;
    
    NSArray* array = [self arrayOfTextForTextMarkers:[NSArray arrayWithObjects:startMarker, endMarker, nil] attributed:NO];
    NSMutableArray* elements = [NSMutableArray array];
    for (id element in array) {
        if (![element isKindOfClass:[AccessibilityObjectWrapper class]])
            continue;
        [elements addObject:element];
    }
    return elements;
}

- (NSString *)selectionRangeString
{
    NSArray *markers = [self textMarkerRangeForSelection];
    return [self stringForTextMarkers:markers];
}

- (WebAccessibilityTextMarker *)selectedTextMarker
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    VisibleSelection selection = m_object->selection();
    VisiblePosition position = selection.visibleStart();
    
    // if there's no selection, start at the top of the document
    if (position.isNull())
        position = startOfDocument(m_object->document());
    
    return [WebAccessibilityTextMarker textMarkerWithVisiblePosition:position cache:m_object->axObjectCache()];
}

// This method is intended to return the marker at the end of the line starting at
// the marker that is passed into the method.
- (WebAccessibilityTextMarker *)lineEndMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    VisiblePosition start = [marker visiblePosition];
    VisiblePosition lineEnd = m_object->nextLineEndPosition(start);
    
    return [WebAccessibilityTextMarker textMarkerWithVisiblePosition:lineEnd cache:m_object->axObjectCache()];
}

// This method is intended to return the marker at the start of the line starting at
// the marker that is passed into the method.
- (WebAccessibilityTextMarker *)lineStartMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    VisiblePosition start = [marker visiblePosition];
    VisiblePosition lineStart = m_object->previousLineStartPosition(start);
    
    return [WebAccessibilityTextMarker textMarkerWithVisiblePosition:lineStart cache:m_object->axObjectCache()];
}

- (WebAccessibilityTextMarker *)nextMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    CharacterOffset start = [marker characterOffset];
    return [self nextMarkerForCharacterOffset:start];
}

- (WebAccessibilityTextMarker *)previousMarkerForMarker:(WebAccessibilityTextMarker *)marker
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (!marker)
        return nil;
    
    CharacterOffset start = [marker characterOffset];
    return [self previousMarkerForCharacterOffset:start];
}

// This method is intended to return the bounds of a text marker range in screen coordinates.
- (CGRect)frameForTextMarkers:(NSArray *)array
{
    if (![self _prepareAccessibilityCall])
        return CGRectZero;

    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return CGRectZero;
    RefPtr<Range> range = [self rangeForTextMarkers:array];
    if (!range)
        return CGRectZero;
    
    IntRect rect = m_object->boundsForRange(range);
    return [self convertRectToScreenSpace:rect];
}

- (WebAccessibilityTextMarker *)textMarkerForPoint:(CGPoint)point
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nil;
    CharacterOffset characterOffset = cache->characterOffsetForPoint(IntPoint(point), m_object);
    return [WebAccessibilityTextMarker textMarkerWithCharacterOffset:characterOffset cache:cache];
}

- (WebAccessibilityTextMarker *)nextMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForNextCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return [[[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache] autorelease];
}

- (WebAccessibilityTextMarker *)previousMarkerForCharacterOffset:(CharacterOffset&)characterOffset
{
    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nil;
    
    TextMarkerData textMarkerData;
    cache->textMarkerDataForPreviousCharacterOffset(textMarkerData, characterOffset);
    if (!textMarkerData.axID)
        return nil;
    return [[[WebAccessibilityTextMarker alloc] initWithTextMarker:&textMarkerData cache:cache] autorelease];
}

- (RefPtr<Range>)rangeForTextMarkers:(NSArray *)textMarkers
{
    if ([textMarkers count] != 2)
        return nullptr;
    
    WebAccessibilityTextMarker *startMarker = [textMarkers objectAtIndex:0];
    WebAccessibilityTextMarker *endMarker = [textMarkers objectAtIndex:1];
    
    if (![startMarker isKindOfClass:[WebAccessibilityTextMarker class]] || ![endMarker isKindOfClass:[WebAccessibilityTextMarker class]])
        return nullptr;
    
    AXObjectCache* cache = m_object->axObjectCache();
    if (!cache)
        return nullptr;
    
    CharacterOffset startCharacterOffset = [startMarker characterOffset];
    CharacterOffset endCharacterOffset = [endMarker characterOffset];
    return cache->rangeForUnorderedCharacterOffsets(startCharacterOffset, endCharacterOffset);
}

- (NSInteger)lengthForTextMarkers:(NSArray *)textMarkers
{
    if (![self _prepareAccessibilityCall])
        return 0;
    
    RefPtr<Range> range = [self rangeForTextMarkers:textMarkers];
    int length = AXObjectCache::lengthForRange(range.get());
    return length < 0 ? 0 : length;
}

- (WebAccessibilityTextMarker *)startOrEndTextMarkerForTextMarkers:(NSArray *)textMarkers isStart:(BOOL)isStart
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    RefPtr<Range> range = [self rangeForTextMarkers:textMarkers];
    if (!range)
        return nil;
    
    return [WebAccessibilityTextMarker startOrEndTextMarkerForRange:range isStart:isStart cache:m_object->axObjectCache()];
}

- (NSArray *)textMarkerRangeForMarkers:(NSArray *)textMarkers
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    RefPtr<Range> range = [self rangeForTextMarkers:textMarkers];
    return [self textMarkersForRange:range];
}

- (NSArray *)textMarkersForRange:(RefPtr<Range>)range
{
    if (!range)
        return nil;
    
    WebAccessibilityTextMarker* start = [WebAccessibilityTextMarker startOrEndTextMarkerForRange:range isStart:YES cache:m_object->axObjectCache()];
    WebAccessibilityTextMarker* end = [WebAccessibilityTextMarker startOrEndTextMarkerForRange:range isStart:NO cache:m_object->axObjectCache()];
    if (!start || !end)
        return nil;
    return [NSArray arrayWithObjects:start, end, nil];
}

- (NSString *)accessibilityExpandedTextValue
{
    if (![self _prepareAccessibilityCall])
        return nil;
    return m_object->expandedTextValue();
}

- (NSString *)accessibilityIdentifier
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return m_object->getAttribute(HTMLNames::idAttr);
}

- (NSString *)accessibilitySpeechHint
{
    if (![self _prepareAccessibilityCall])
        return nil;

    switch (m_object->speakProperty()) {
    default:
    case SpeakNormal:
        return @"normal";
    case SpeakNone:
        return @"none";
    case SpeakSpellOut:
        return @"spell-out";
    case SpeakDigits:
        return @"digits";
    case SpeakLiteralPunctuation:
        return @"literal-punctuation";
    case SpeakNoPunctuation:
        return @"no-punctuation";
    }
    
    return nil;
}

- (BOOL)accessibilityARIAIsBusy
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return m_object->ariaLiveRegionBusy();
}

- (NSString *)accessibilityARIALiveRegionStatus
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->ariaLiveRegionStatus();
}

- (NSString *)accessibilityARIARelevantStatus
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return m_object->ariaLiveRegionRelevant();
}

- (BOOL)accessibilityARIALiveRegionIsAtomic
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return m_object->ariaLiveRegionAtomic();
}

- (BOOL)accessibilitySupportsARIAPressed
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return m_object->supportsARIAPressed();
}

- (BOOL)accessibilityIsPressed
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    return m_object->isPressed();
}

- (BOOL)accessibilitySupportsARIAExpanded
{
    if (![self _prepareAccessibilityCall])
        return NO;
    
    // Since details element is ignored on iOS, we should expose the expanded status on its
    // summary's accessible children.
    if (AccessibilityObject* detailParent = [self detailParentForSummaryObject:m_object])
        return detailParent->supportsExpanded();
    
    return m_object->supportsExpanded();
}

- (BOOL)accessibilityIsExpanded
{
    if (![self _prepareAccessibilityCall])
        return NO;

    // Since details element is ignored on iOS, we should expose the expanded status on its
    // summary's accessible children.
    if (AccessibilityObject* detailParent = [self detailParentForSummaryObject:m_object])
        return detailParent->isExpanded();
    
    return m_object->isExpanded();
}

- (NSString *)accessibilityInvalidStatus
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return m_object->invalidStatus();
}

- (NSString *)accessibilityARIACurrentStatus
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    switch (m_object->ariaCurrentState()) {
    case ARIACurrentFalse:
        return @"false";
    case ARIACurrentPage:
        return @"page";
    case ARIACurrentStep:
        return @"step";
    case ARIACurrentLocation:
        return @"location";
    case ARIACurrentTime:
        return @"time";
    case ARIACurrentDate:
        return @"date";
    default:
    case ARIACurrentTrue:
        return @"true";
    }
}

- (WebAccessibilityObjectWrapper *)accessibilityMathRootIndexObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathRootIndexObject() ? m_object->mathRootIndexObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathRadicandObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathRadicandObject() ? m_object->mathRadicandObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathNumeratorObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathNumeratorObject() ? m_object->mathNumeratorObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathDenominatorObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathDenominatorObject() ? m_object->mathDenominatorObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathBaseObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathBaseObject() ? m_object->mathBaseObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathSubscriptObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathSubscriptObject() ? m_object->mathSubscriptObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathSuperscriptObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathSuperscriptObject() ? m_object->mathSuperscriptObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathUnderObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathUnderObject() ? m_object->mathUnderObject()->wrapper() : 0;
}

- (WebAccessibilityObjectWrapper *)accessibilityMathOverObject
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathOverObject() ? m_object->mathOverObject()->wrapper() : 0;
}

- (NSString *)accessibilityPlatformMathSubscriptKey
{
    return @"AXMSubscriptObject";
}

- (NSString *)accessibilityPlatformMathSuperscriptKey
{
    return @"AXMSuperscriptObject";
}

- (NSArray *)accessibilityMathPostscripts
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [self accessibilityMathPostscriptPairs];
}

- (NSArray *)accessibilityMathPrescripts
{
    if (![self _prepareAccessibilityCall])
        return nil;
    
    return [self accessibilityMathPrescriptPairs];
}

- (NSString *)accessibilityMathFencedOpenString
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathFencedOpenString();
}

- (NSString *)accessibilityMathFencedCloseString
{
    if (![self _prepareAccessibilityCall])
        return nil;

    return m_object->mathFencedCloseString();
}

- (BOOL)accessibilityIsMathTopObject
{
    if (![self _prepareAccessibilityCall])
        return NO;

    return m_object->roleValue() == DocumentMathRole;
}

- (NSInteger)accessibilityMathLineThickness
{
    if (![self _prepareAccessibilityCall])
        return 0;

    return m_object->mathLineThickness();
}

- (NSString *)accessibilityMathType
{
    if (![self _prepareAccessibilityCall])
        return nil;

    if (m_object->roleValue() == MathElementRole) {
        if (m_object->isMathFraction())
            return @"AXMathFraction";
        if (m_object->isMathFenced())
            return @"AXMathFenced";
        if (m_object->isMathSubscriptSuperscript())
            return @"AXMathSubscriptSuperscript";
        if (m_object->isMathRow())
            return @"AXMathRow";
        if (m_object->isMathUnderOver())
            return @"AXMathUnderOver";
        if (m_object->isMathSquareRoot())
            return @"AXMathSquareRoot";
        if (m_object->isMathRoot())
            return @"AXMathRoot";
        if (m_object->isMathText())
            return @"AXMathText";
        if (m_object->isMathNumber())
            return @"AXMathNumber";
        if (m_object->isMathIdentifier())
            return @"AXMathIdentifier";
        if (m_object->isMathTable())
            return @"AXMathTable";
        if (m_object->isMathTableRow())
            return @"AXMathTableRow";
        if (m_object->isMathTableCell())
            return @"AXMathTableCell";
        if (m_object->isMathFenceOperator())
            return @"AXMathFenceOperator";
        if (m_object->isMathSeparatorOperator())
            return @"AXMathSeparatorOperator";
        if (m_object->isMathOperator())
            return @"AXMathOperator";
        if (m_object->isMathMultiscript())
            return @"AXMathMultiscript";
    }
    
    return nil;
}

- (CGPoint)accessibilityClickPoint
{
    return m_object->clickPoint();
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@: %@", [self class], [self accessibilityLabel]];
}

@end

#endif // HAVE(ACCESSIBILITY) && PLATFORM(IOS)
