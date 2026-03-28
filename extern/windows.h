#pragma once
// @HACK: This is a windows.h stub to make SDL2 compatable on macos find a less hacky way for this

typedef void*              HWND;
typedef void*              HDC;
typedef void*              HINSTANCE;
typedef void*              HGLRC;
typedef void*              HANDLE;
typedef void*              HICON;
typedef void*              HCURSOR;
typedef void*              HBRUSH;
typedef void*              HMENU;
typedef void*              HMONITOR;
typedef void*              HMODULE;
typedef void*              HBITMAP;
typedef void*              HRGN;
typedef void*              HPALETTE;
typedef void*              HPEN;
typedef void*              HFONT;
typedef void*              HTASK;
typedef void*              HFILE;
typedef void*              HRSRC;
typedef void*              HGLOBAL;
typedef void*              HLOCAL;
typedef void*              HACCEL;
typedef void*              HMETAFILE;
typedef void*              HENHMETAFILE;
typedef void*              HDWP;

typedef unsigned char      BYTE;
typedef unsigned short     WORD;
typedef unsigned long      DWORD;
typedef unsigned int       UINT;
typedef int                INT;
typedef long               LONG;
typedef unsigned long      ULONG;
typedef long long          LONGLONG;
typedef unsigned long long ULONGLONG;
typedef float              FLOAT;
typedef int                BOOL;
typedef char               CHAR;
typedef wchar_t            WCHAR;

typedef void*              LPVOID;
typedef const void*        LPCVOID;
typedef char*              LPSTR;
typedef const char*        LPCSTR;
typedef wchar_t*           LPWSTR;
typedef const wchar_t*     LPCWSTR;
typedef DWORD*             LPDWORD;
typedef BOOL*              LPBOOL;
typedef BYTE*              LPBYTE;
typedef INT*               LPINT;
typedef WORD*              LPWORD;
typedef LONG*              LPLONG;

#if defined(_WIN64)
typedef unsigned long long WPARAM;
typedef long long          LPARAM;
typedef long long          LRESULT;
#else
typedef unsigned long      WPARAM;
typedef long               LPARAM;
typedef long               LRESULT;
#endif

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *PMSG, *LPMSG;

typedef struct tagWNDCLASS {
    UINT      style;
    void*     lpfnWndProc;
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
} WNDCLASS, *PWNDCLASS, *LPWNDCLASS;

#define TRUE  1
#define FALSE 0
#define NULL  0

#define WINAPI
#define CALLBACK
#define APIENTRY
#define WINGDIAPI
#define DECLSPEC_IMPORT
#define DECLARE_HANDLE(name) typedef void* name

#define WM_NULL     0x0000
#define WM_CREATE   0x0001
#define WM_DESTROY  0x0002
#define WM_QUIT     0x0012
#define WM_SIZE     0x0005
#define WM_PAINT    0x000F
#define WM_CLOSE    0x0010
