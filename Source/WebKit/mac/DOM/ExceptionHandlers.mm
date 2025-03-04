/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#import "ExceptionHandlers.h"

#import "DOMEventException.h"
#import "DOMException.h"
#import "DOMRangeException.h"
#import "DOMXPathException.h"
#import <WebCore/ExceptionCode.h>
#import <WebCore/ExceptionCodeDescription.h>

NSString * const DOMException = @"DOMException";
NSString * const DOMRangeException = @"DOMRangeException";
NSString * const DOMEventException = @"DOMEventException";
NSString * const DOMXPathException = @"DOMXPathException";

void raiseDOMException(WebCore::ExceptionCode ec)
{
    ASSERT(ec);

    WebCore::ExceptionCodeDescription description(ec);

    // FIXME: This should use type and code exclusively and not try to use typeName.
    NSString *exceptionName;
    if (strcmp(description.typeName, "DOM Range") == 0)
        exceptionName = DOMRangeException;
    else if (strcmp(description.typeName, "DOM Events") == 0)
        exceptionName = DOMEventException;
    else if (strcmp(description.typeName, "DOM XPath") == 0)
        exceptionName = DOMXPathException;
    else
        exceptionName = DOMException;

    NSString *reason;
    if (description.name)
        reason = [[NSString alloc] initWithFormat:@"*** %s: %@ %d", description.name, exceptionName, description.code];
    else
        reason = [[NSString alloc] initWithFormat:@"*** %@ %d", exceptionName, description.code];

    NSDictionary *userInfo = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithInt:description.code], exceptionName, nil];

    NSException *exception = [NSException exceptionWithName:exceptionName reason:reason userInfo:userInfo];

    [reason release];
    [userInfo release];

    [exception raise];

    RELEASE_ASSERT_NOT_REACHED();
}

void raiseTypeErrorException()
{
    raiseDOMException(WebCore::TypeError);
}
