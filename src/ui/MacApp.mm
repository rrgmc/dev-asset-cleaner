#include "MacApp.h"

#import <AppKit/AppKit.h>

@interface DacReopenHandler : NSObject
- (instancetype)initWithCallback:(std::function<void()>)callback;
- (void)handleReopen:(NSAppleEventDescriptor *)event
      withReplyEvent:(NSAppleEventDescriptor *)reply;
@end

@implementation DacReopenHandler {
    std::function<void()> _callback;
}

- (instancetype)initWithCallback:(std::function<void()>)callback
{
    if ((self = [super init]))
        _callback = std::move(callback);
    return self;
}

- (void)handleReopen:(NSAppleEventDescriptor *)event
      withReplyEvent:(NSAppleEventDescriptor *)reply
{
    _callback();
}

@end

namespace MacApp {

void whenReopened(std::function<void()> callback)
{
    static DacReopenHandler *handler = nil;
    handler = [[DacReopenHandler alloc] initWithCallback:std::move(callback)];
    // AppKit installs its own reopen handler while launching, so replace it afterwards.
    __block id observer = [NSNotificationCenter.defaultCenter
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                    [NSNotificationCenter.defaultCenter removeObserver:observer];
                    [NSAppleEventManager.sharedAppleEventManager
                        setEventHandler:handler
                            andSelector:@selector(handleReopen:withReplyEvent:)
                          forEventClass:kCoreEventClass
                             andEventID:kAEReopenApplication];
                }];
}

void lowerModalWindowsWhenInactive()
{
    // The windows lowered while the app is in the background; weak, so closed ones drop out.
    static NSHashTable<NSWindow *> *lowered = [NSHashTable weakObjectsHashTable];
    NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
    [center addObserverForName:NSApplicationDidResignActiveNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification *) {
                        for (NSWindow *window in NSApp.windows) {
                            // Leave menus, the status item and auto-hiding panels alone.
                            if (!window.visible || window.hidesOnDeactivate
                                || window.level != NSModalPanelWindowLevel)
                                continue;
                            window.level = NSNormalWindowLevel;
                            [lowered addObject:window];
                        }
                    }];
    [center addObserverForName:NSApplicationDidBecomeActiveNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification *) {
                        for (NSWindow *window in lowered.allObjects) {
                            if (window.visible && window.level == NSNormalWindowLevel)
                                window.level = NSModalPanelWindowLevel;
                        }
                        [lowered removeAllObjects];
                    }];
}

} // namespace MacApp
