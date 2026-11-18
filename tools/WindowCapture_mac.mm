#include "WindowCapture_mac.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>

#include <atomic>
#include <cstdio>

#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

namespace {

// Runs Qt's event loop until done is set or the time runs out. ScreenCaptureKit answers on its own
// queues, so the main thread only has to wait.
bool waitFor(const std::atomic<bool> &done)
{
    QElapsedTimer timer;
    timer.start();
    while (!done.load() && timer.elapsed() < 10000) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
    return done.load();
}

bool savePng(CGImageRef image, const QString &out)
{
    NSURL *url = [NSURL fileURLWithPath:out.toNSString()];
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
        (__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
    if (!destination)
        return false;
    CGImageDestinationAddImage(destination, image, nullptr);
    const bool ok = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    return ok;
}

} // namespace

bool captureWindowMac(unsigned int windowNumber, const QString &out)
{
    if (@available(macOS 14, *)) {
        __block SCShareableContent *content = nil;
        __block NSError *contentError = nil;
        std::atomic<bool> haveContent{false};
        std::atomic<bool> *haveContentPtr = &haveContent;
        [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                                   onScreenWindowsOnly:YES
                                                     completionHandler:^(SCShareableContent *c,
                                                                         NSError *e) {
                                                         content = c;
                                                         contentError = e;
                                                         haveContentPtr->store(true);
                                                     }];
        if (!waitFor(haveContent) || !content) {
            std::fprintf(stderr, "ScreenCaptureKit: %s\n",
                         contentError ? contentError.localizedDescription.UTF8String
                                      : "no shareable content");
            return false;
        }

        SCWindow *window = nil;
        for (SCWindow *w in content.windows) {
            if (w.windowID == windowNumber) {
                window = w;
                break;
            }
        }
        if (!window) {
            std::fprintf(stderr, "ScreenCaptureKit: window %u not found\n", windowNumber);
            return false;
        }

        SCContentFilter *filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];
        SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
        const CGRect rect = filter.contentRect;
        const CGFloat scale = filter.pointPixelScale;
        config.width = static_cast<size_t>(rect.size.width * scale);
        config.height = static_cast<size_t>(rect.size.height * scale);
        config.showsCursor = NO;
        config.ignoreShadowsSingleWindow = YES;
        config.capturesAudio = NO;

        __block CGImageRef image = nullptr;
        __block NSError *captureError = nil;
        std::atomic<bool> haveImage{false};
        std::atomic<bool> *haveImagePtr = &haveImage;
        [SCScreenshotManager captureImageWithFilter:filter
                                      configuration:config
                                  completionHandler:^(CGImageRef i, NSError *e) {
                                      if (i)
                                          image = CGImageRetain(i);
                                      captureError = e;
                                      haveImagePtr->store(true);
                                  }];
        if (!waitFor(haveImage) || !image) {
            std::fprintf(stderr, "ScreenCaptureKit: %s\n",
                         captureError ? captureError.localizedDescription.UTF8String
                                      : "no image");
            if (image)
                CGImageRelease(image);
            return false;
        }
        const bool ok = savePng(image, out);
        CGImageRelease(image);
        return ok;
    }
    return false;
}
