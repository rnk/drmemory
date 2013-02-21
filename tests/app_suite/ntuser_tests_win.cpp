/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; 
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <windows.h>

#include <winuser.h>

#include "os_version_win.h"
#include "gtest/gtest.h"

// For InitCommonControlsEx
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

// A potentially externally visible global.  Useful if you want to make a
// statement the compiler can't delete.
int global_for_side_effects;

// FIXME i#735: Re-enable once doesn't hang and passes on xp32.
TEST(NtUserTests, DISABLED_SystemParametersInfo) {
    // Was: http://code.google.com/p/drmemory/issues/detail?id=10
    NONCLIENTMETRICS metrics;
    ZeroMemory(&metrics, sizeof(NONCLIENTMETRICS));
    metrics.cbSize = sizeof(NONCLIENTMETRICS);
    BOOL success = SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                                        sizeof(NONCLIENTMETRICS), &metrics, 0);
    ASSERT_EQ(TRUE, success);
    success = SystemParametersInfo(SPI_SETNONCLIENTMETRICS,
                                   sizeof(NONCLIENTMETRICS), &metrics, 0);
    ASSERT_EQ(TRUE, success);
}

namespace Clipboard_Tests {

void WriteStringToClipboard(const std::string& str) {
    HWND hWnd = ::GetDesktopWindow();
    ASSERT_NE(0, ::OpenClipboard(hWnd));
    ::EmptyClipboard();
    HGLOBAL data = ::GlobalAlloc(2 /*GMEM_MOVABLE*/, str.size() + 1);
    ASSERT_NE((HGLOBAL)NULL, data);

    char* raw_data = (char*)::GlobalLock(data);
    memcpy(raw_data, str.data(), str.size() * sizeof(char));
    raw_data[str.size()] = '\0';
    ::GlobalUnlock(data);

    ASSERT_EQ(data, ::SetClipboardData(CF_TEXT, data));
    ::CloseClipboard();
}

void ReadAsciiStringFromClipboard(std::string *result) {
    assert(result != NULL);

    HWND hWnd = ::GetDesktopWindow();
    ASSERT_NE(0, ::OpenClipboard(hWnd));

    HANDLE data = ::GetClipboardData(CF_TEXT);
    ASSERT_NE((HANDLE)NULL, data);

    result->assign((const char*)::GlobalLock(data));

    ::GlobalUnlock(data);
    ::CloseClipboard();
}

// FIXME i#734: Re-enable when no uninits.
TEST(NtUserTests, ClipboardPutGet) {
    if (GetWindowsVersion() >= WIN_VISTA) {
        printf("WARNING: Disabling ClipboardPutGet on Win Vista+, see i#734.\n");
        return;
    }

    // Was: http://code.google.com/p/drmemory/issues/detail?id=45
    std::string tmp, str = "ASCII";
    WriteStringToClipboard(str);
    ReadAsciiStringFromClipboard(&tmp);
    ASSERT_STREQ("ASCII", tmp.c_str());
}

} /* Clipboard_Tests */

TEST(NtUserTests, CoInitializeUninitialize) {
    // Was: http://code.google.com/p/drmemory/issues/detail?id=65
    CoInitialize(NULL);
    CoUninitialize();
}

TEST(NtUserTests, InitCommonControlsEx) {
    // Was: http://code.google.com/p/drmemory/issues/detail?id=362
    INITCOMMONCONTROLSEX InitCtrlEx;

    InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    InitCtrlEx.dwICC  = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&InitCtrlEx);  // initialize common control sex
}

TEST(NtUserTests, CursorTest) {
    // test NtUserCall* GETCURSORPOS, SETCURSORPOS, SHOWCURSOR
    POINT point;
    BOOL success = GetCursorPos(&point);
    if (!success) {
        // FIXME i#755: This seems to happen when a user over RDP disconnected?
        // In any case, not worth the time to track down now.
        printf("WARNING: GetCursorPos failed with error %d\n", GetLastError());
    } else {
        // Check uninits
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery((VOID*)(point.x + point.y), &mbi, sizeof(mbi));

        success = SetCursorPos(point.x, point.y);
        if (!success) {
            // FIXME i#755: This seems to happen when a user over RDP disconnected?
            // In any case, not worth the time to track down now.
            printf("WARNING: SetCursorPos failed with error %d\n", GetLastError());
        }
    }

    int display_count = ShowCursor(TRUE);
    if (display_count != 1) {
        printf("WARNING: display_count != 1, got %d\n", display_count);
    }
}

TEST(NtUserTests, WindowRgnTest) {
    // test NtUserCall* VALIDATERGN, 
    HWND hwnd = ::GetDesktopWindow();
    HRGN hrgn = CreateRectRgn(0, 0, 0, 0);
    ASSERT_NE((HRGN)NULL, hrgn);
    BOOL success = ValidateRgn(hwnd, hrgn);
    ASSERT_EQ(TRUE, success);
    int type = GetWindowRgn(hwnd, hrgn);
    // FIXME: somehow type comes out as ERROR so skipping ASSERT_NE(ERROR, type)
}

TEST(NtUserTests, MenuTest) {
    // FIXME i#736: Re-enable on XP when passes.
    if (GetWindowsVersion() < WIN_VISTA) {
        printf("WARNING: Disabling MenuTest on Pre-Vista, see i#736.\n");
        return;
    }

    // test NtUserCall* DRAWMENUBAR
    HWND hwnd = ::GetDesktopWindow();
    BOOL success = DrawMenuBar(hwnd);
    ASSERT_EQ(FALSE, success); /* no menu on desktop window */

    // test NtUserCall* CREATEMENU + CREATEPOPUPMENU and NtUserDestroyMenu
    HMENU menu = CreateMenu();
    ASSERT_NE((HMENU)NULL, menu);
    success = DestroyMenu(menu);
    ASSERT_EQ(TRUE, success);
    menu = CreatePopupMenu();
    ASSERT_NE((HMENU)NULL, menu);
    success = DestroyMenu(menu);
    ASSERT_EQ(TRUE, success);
}

TEST(NtUserTests, BeepTest) {
    // test NtUserCall* MESSAGEBEEP
    BOOL success = MessageBeep(0xFFFFFFFF/*simple beep*/);
    ASSERT_EQ(TRUE, success);
}

TEST(NtUserTests, CaretTest) {
    // test NtUserGetCaretBlinkTime and NtUserCall* SETCARETBLINKTIME + DESTROY_CARET
    UINT blink = GetCaretBlinkTime();
    ASSERT_NE(0, blink);
    BOOL success = SetCaretBlinkTime(blink);
    ASSERT_EQ(TRUE, success);
    success = DestroyCaret();
    ASSERT_EQ(FALSE, success); // no caret to destroy
}

TEST(NtUserTests, DeferWindowPosTest) {
    // test NtUserCall* BEGINDEFERWINDOWPOS and NtUserDeferWindowPos
    HWND hwnd = ::GetDesktopWindow();
    HDWP hdwp = BeginDeferWindowPos(1);
    if (hdwp) {
        hdwp = DeferWindowPos(hdwp, hwnd, NULL, 0, 0, 5, 10,
                              SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
    }
    if (hdwp) {
        // XXX: not getting here: need to set up a successful defer
        EndDeferWindowPos(hdwp);
    }
}

TEST(NtUserTests, EnumDisplayDevices) {
    DISPLAY_DEVICE device_info;
    device_info.cb = sizeof(device_info);
    BOOL success = EnumDisplayDevices(NULL, 0, /* display adapter #0 */
                                      &device_info, 0);
    ASSERT_EQ(TRUE, success);
}

TEST(NtUserTests, WindowStation) {
    BOOL success;
    HWINSTA def_ws = GetProcessWindowStation();

    HWINSTA ws = CreateWindowStation(NULL, 0, READ_CONTROL | DELETE, NULL);
    ASSERT_NE(ws, (HWINSTA)NULL);

    success = SetProcessWindowStation(ws);
    ASSERT_EQ(success, TRUE);

    // XXX: I tried CreateDesktop but it fails with ERROR_NOT_ENOUGH_MEMORY
    // and I'm not sure we want to go tweaking the default desktop to
    // free memory.

    success = SetProcessWindowStation(def_ws);
    ASSERT_EQ(success, TRUE);

    success = CloseWindowStation(ws);
    ASSERT_EQ(success, TRUE);
}

static DWORD WINAPI
thread_func(void *arg)
{
    MessageBox(NULL, "<will be automatically closed>", "NtUserTests.MessageBox", MB_OK);
    return 0;
}

static BOOL CALLBACK
enum_windows(HWND hwnd, LPARAM param)
{
    DWORD target_tid = (DWORD) param;
    DWORD target_pid = GetCurrentProcessId();
    DWORD window_pid;
    DWORD window_tid = GetWindowThreadProcessId(hwnd, &window_pid);
    // We really only need to test tid but we test both:
    if (window_pid == target_pid && window_tid == target_tid) {
        // We're not allowed to call DestroyWindow() on another thread's window,
        // and calling TerminateThread() seems to destabilize our own
        // process shutdown, so we send a message:
        LRESULT res = SendMessageTimeout(hwnd, WM_CLOSE, 0, 0, SMTO_BLOCK, 
                                         0, NULL);
        printf("Found msgbox window: closing.\n");
        if (res != 0)
            SetLastError(NO_ERROR);
        return FALSE;
    }
    return TRUE;
}

TEST(NtUserTests, Msgbox) {
    BOOL success;
    DWORD tid;
    DWORD res;

    // Strategy: have a separate thread open the msgbox so we can close
    // it automatically.
    HANDLE thread = CreateThread(NULL, 0, thread_func, NULL, 0, &tid);
    ASSERT_NE(thread, (HANDLE)NULL);

    Sleep(0); // Avoid initial spin
    do {
        // Close the window as soon as we can.  On an unloaded machine
        // this kills it even before it's visible to avoid an annoying
        // popup natively, though under DrMem full mode it is visible.
        // This exercises ~35 NtGdi and ~60 NtUser syscalls.
        // Unfortunately the timing makes it non-deterministic.
        // Ideally we would write our own tests of all 95 of those
        // syscalls but for now MessageBox is by far the easiest way
        // to run them.
        success = EnumWindows(enum_windows, (LPARAM) tid);
        ASSERT_EQ(GetLastError(), NO_ERROR);
    } while (success /* we went through all windows */);

    // I thought I could wait on thread INFINITE but that hangs so don't wait
    // at all.
    success = CloseHandle(thread);
    ASSERT_EQ(success, TRUE);
}
