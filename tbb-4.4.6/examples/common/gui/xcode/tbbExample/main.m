/*
    Copyright 2005-2016 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks. Threading Building Blocks is free software;
    you can redistribute it and/or modify it under the terms of the GNU General Public License
    version 2  as  published  by  the  Free Software Foundation.  Threading Building Blocks is
    distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See  the GNU General Public License for more details.   You should have received a copy of
    the  GNU General Public License along with Threading Building Blocks; if not, write to the
    Free Software Foundation, Inc.,  51 Franklin St,  Fifth Floor,  Boston,  MA 02110-1301 USA

    As a special exception,  you may use this file  as part of a free software library without
    restriction.  Specifically,  if other files instantiate templates  or use macros or inline
    functions from this file, or you compile this file and link it with other files to produce
    an executable,  this file does not by itself cause the resulting executable to be covered
    by the GNU General Public License. This exception does not however invalidate any other
    reasons why the executable file might be covered by the GNU General Public License.
*/

#import <Availability.h>
#import <Foundation/Foundation.h>

#if TARGET_OS_IPHONE

#import <UIKit/UIKit.h>
#import "tbbAppDelegate.h"

void get_screen_resolution(int *x, int *y) {
    // Getting landscape screen resolution in any case
    CGRect imageRect = [[UIScreen mainScreen] bounds];
    *x=imageRect.size.width>imageRect.size.height?imageRect.size.width:imageRect.size.height;
    *y=imageRect.size.width<imageRect.size.height?imageRect.size.width:imageRect.size.height;
    return;
}

int cocoa_main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([tbbAppDelegate class]));
    }
}

#elif TARGET_OS_MAC

#import <Cocoa/Cocoa.h>

int cocoa_main(int argc, char *argv[])
{
    return NSApplicationMain(argc, (const char **)argv);
}
#endif
