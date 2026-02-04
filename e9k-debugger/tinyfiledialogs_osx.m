#import <Cocoa/Cocoa.h>
#import <dispatch/dispatch.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *
tinyfiledialogs_osxMallocUtf8StringFromNSString(NSString *string)
{
	if (!string) {
		return NULL;
	}

	const char *utf8 = [string UTF8String];
	if (!utf8) {
		return NULL;
	}

	size_t len = strlen(utf8);
	char *result = (char *)malloc(len + 1);
	if (!result) {
		return NULL;
	}

	memcpy(result, utf8, len + 1);
	return result;
}


static NSString *
tinyfiledialogs_osxNSStringFromUtf8(char const *string)
{
	if (!string || !string[0]) {
		return nil;
	}

	return [NSString stringWithUTF8String:string];
}


static NSString *
tinyfiledialogs_osxAbsolutePathNSString(char const *path)
{
	NSString *nsPath = tinyfiledialogs_osxNSStringFromUtf8(path);
	if (!nsPath) {
		return nil;
	}

	if ([nsPath hasPrefix:@"/"]) {
		return [nsPath stringByStandardizingPath];
	}

	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd))) {
		return [nsPath stringByStandardizingPath];
	}

	NSString *nsCwd = [NSString stringWithUTF8String:cwd];
	if (!nsCwd) {
		return [nsPath stringByStandardizingPath];
	}

	return [[nsCwd stringByAppendingPathComponent:nsPath] stringByStandardizingPath];
}


static void
tinyfiledialogs_osxSplitDefaultPath(
	char const *defaultPathAndOrFile,
	NSURL **outDirectoryUrl,
	NSString **outFileName)
{
	if (outDirectoryUrl) {
		*outDirectoryUrl = nil;
	}
	if (outFileName) {
		*outFileName = nil;
	}

	NSString *absPath = tinyfiledialogs_osxAbsolutePathNSString(defaultPathAndOrFile);
	if (!absPath || !absPath.length) {
		return;
	}

	if ([absPath hasSuffix:@"/"]) {
		if (outDirectoryUrl) {
			*outDirectoryUrl = [NSURL fileURLWithPath:absPath isDirectory:YES];
		}
		return;
	}

	NSString *dirPath = [absPath stringByDeletingLastPathComponent];
	if (outDirectoryUrl && dirPath.length) {
		*outDirectoryUrl = [NSURL fileURLWithPath:dirPath isDirectory:YES];
	}

	NSString *fileName = [absPath lastPathComponent];
	if (outFileName && fileName.length) {
		*outFileName = fileName;
	}
}


static NSArray<NSString *> *
tinyfiledialogs_osxAllowedFileTypesFromPatterns(
	int numFilterPatterns,
	char const * const *filterPatterns)
{
	if (!filterPatterns || numFilterPatterns <= 0) {
		return nil;
	}

	NSMutableArray<NSString *> *types = [NSMutableArray array];
	for (int i = 0; i < numFilterPatterns; i++) {
		char const *pattern = filterPatterns[i];
		if (!pattern) {
			continue;
		}

		if (strncmp(pattern, "*.", 2) != 0) {
			continue;
		}

		char const *ext = pattern + 2;
		if (!ext[0]) {
			continue;
		}

		if (strchr(ext, '*') || strchr(ext, '?') || strchr(ext, '[') || strchr(ext, ']')) {
			continue;
		}

		NSString *extString = [NSString stringWithUTF8String:ext];
		if (!extString || !extString.length) {
			continue;
		}

		[types addObject:extString];
	}

	if (!types.count) {
		return nil;
	}

	return types;
}


static void
tinyfiledialogs_osxRunOnMainThreadSync(void (^block)(void))
{
	if ([NSThread isMainThread]) {
		block();
	}
	else {
		dispatch_sync(dispatch_get_main_queue(), block);
	}
}


static void
tinyfiledialogs_osxRestoreFocus(NSWindow *previousKeyWindow)
{
	[NSApplication sharedApplication];
	[NSApp activateIgnoringOtherApps:YES];

	if (previousKeyWindow && previousKeyWindow.visible) {
		[previousKeyWindow makeKeyAndOrderFront:nil];
		[previousKeyWindow makeMainWindow];
		NSView *contentView = previousKeyWindow.contentView;
		if (contentView) {
			[previousKeyWindow makeFirstResponder:contentView];
		}
		return;
	}

	for (NSWindow *window in [NSApp orderedWindows]) {
		if (!window.visible) {
			continue;
		}
		if ([window isKindOfClass:[NSSavePanel class]]) {
			continue;
		}
		[window makeKeyAndOrderFront:nil];
		[window makeMainWindow];
		NSView *contentView = window.contentView;
		if (contentView) {
			[window makeFirstResponder:contentView];
		}
		break;
	}
}


char *
tinyfiledialogs_osxOpenFileDialog(
	char const *aTitle,
	char const *aDefaultPathAndOrFile,
	int aNumOfFilterPatterns,
	char const * const *aFilterPatterns,
	char const *aSingleFilterDescription,
	int aAllowMultipleSelects)
{
	(void)aSingleFilterDescription;

	__block char *result = NULL;
	tinyfiledialogs_osxRunOnMainThreadSync(^{
		@autoreleasepool {
			[NSApplication sharedApplication];
			[NSApp activateIgnoringOtherApps:YES];
			NSWindow *previousKeyWindow = [NSApp keyWindow] ?: [NSApp mainWindow];

			NSOpenPanel *panel = [NSOpenPanel openPanel];
			panel.canChooseFiles = YES;
			panel.canChooseDirectories = NO;
			panel.allowsMultipleSelection = aAllowMultipleSelects ? YES : NO;
			panel.resolvesAliases = YES;

			NSString *title = tinyfiledialogs_osxNSStringFromUtf8(aTitle);
			if (title) {
				panel.title = title;
				panel.message = title;
			}

			NSURL *directoryUrl = nil;
			NSString *fileName = nil;
			tinyfiledialogs_osxSplitDefaultPath(aDefaultPathAndOrFile, &directoryUrl, &fileName);
			if (directoryUrl) {
				panel.directoryURL = directoryUrl;
			}

			NSArray<NSString *> *types = tinyfiledialogs_osxAllowedFileTypesFromPatterns(
				aNumOfFilterPatterns, aFilterPatterns);
			if (types) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
				panel.allowedFileTypes = types;
#pragma clang diagnostic pop
			}

			if ([panel runModal] != NSModalResponseOK) {
				tinyfiledialogs_osxRestoreFocus(previousKeyWindow);
				return;
			}

				if (aAllowMultipleSelects) {
					NSArray<NSURL *> *urls = panel.URLs;
					NSMutableArray<NSString *> *paths = [NSMutableArray arrayWithCapacity:urls.count];
					for (NSURL *url in urls) {
					NSString *path = url.path;
					if (path) {
						[paths addObject:path];
					}
				}
					NSString *joined = [paths componentsJoinedByString:@"|"];
					result = tinyfiledialogs_osxMallocUtf8StringFromNSString(joined);
				}
				else {
					NSString *path = panel.URL.path;
					result = tinyfiledialogs_osxMallocUtf8StringFromNSString(path);
				}

			tinyfiledialogs_osxRestoreFocus(previousKeyWindow);
		}
	});

	return result;
}


char *
tinyfiledialogs_osxSaveFileDialog(
	char const *aTitle,
	char const *aDefaultPathAndOrFile,
	int aNumOfFilterPatterns,
	char const * const *aFilterPatterns,
	char const *aSingleFilterDescription)
{
	(void)aSingleFilterDescription;

	__block char *result = NULL;
	tinyfiledialogs_osxRunOnMainThreadSync(^{
		@autoreleasepool {
			[NSApplication sharedApplication];
			[NSApp activateIgnoringOtherApps:YES];
			NSWindow *previousKeyWindow = [NSApp keyWindow] ?: [NSApp mainWindow];

			NSSavePanel *panel = [NSSavePanel savePanel];
			panel.canCreateDirectories = YES;

			NSString *title = tinyfiledialogs_osxNSStringFromUtf8(aTitle);
			if (title) {
				panel.title = title;
				panel.message = title;
			}

			NSURL *directoryUrl = nil;
			NSString *fileName = nil;
			tinyfiledialogs_osxSplitDefaultPath(aDefaultPathAndOrFile, &directoryUrl, &fileName);
			if (directoryUrl) {
				panel.directoryURL = directoryUrl;
			}
			if (fileName) {
				panel.nameFieldStringValue = fileName;
			}

			NSArray<NSString *> *types = tinyfiledialogs_osxAllowedFileTypesFromPatterns(
				aNumOfFilterPatterns, aFilterPatterns);
			if (types) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
				panel.allowedFileTypes = types;
#pragma clang diagnostic pop
			}

			if ([panel runModal] != NSModalResponseOK) {
				tinyfiledialogs_osxRestoreFocus(previousKeyWindow);
				return;
			}

			NSString *path = panel.URL.path;
			result = tinyfiledialogs_osxMallocUtf8StringFromNSString(path);
			tinyfiledialogs_osxRestoreFocus(previousKeyWindow);
		}
	});

	return result;
}


char *
tinyfiledialogs_osxSelectFolderDialog(
	char const *aTitle,
	char const *aDefaultPath)
{
	__block char *result = NULL;
	tinyfiledialogs_osxRunOnMainThreadSync(^{
		@autoreleasepool {
			[NSApplication sharedApplication];
			[NSApp activateIgnoringOtherApps:YES];
			NSWindow *previousKeyWindow = [NSApp keyWindow] ?: [NSApp mainWindow];

			NSOpenPanel *panel = [NSOpenPanel openPanel];
			panel.canChooseFiles = NO;
			panel.canChooseDirectories = YES;
			panel.allowsMultipleSelection = NO;
			panel.resolvesAliases = YES;

			NSString *title = tinyfiledialogs_osxNSStringFromUtf8(aTitle);
			if (title) {
				panel.title = title;
				panel.message = title;
			}

			NSURL *directoryUrl = nil;
			tinyfiledialogs_osxSplitDefaultPath(aDefaultPath, &directoryUrl, NULL);
			if (directoryUrl) {
				panel.directoryURL = directoryUrl;
			}

			if ([panel runModal] != NSModalResponseOK) {
				tinyfiledialogs_osxRestoreFocus(previousKeyWindow);
				return;
			}

			NSString *path = panel.URL.path;
			result = tinyfiledialogs_osxMallocUtf8StringFromNSString(path);
			tinyfiledialogs_osxRestoreFocus(previousKeyWindow);
		}
	});

	return result;
}
