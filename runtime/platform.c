/* SPDX-FileCopyrightText: 2026 jerbanpls
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * port: platform.h. */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

static char last_error[256];

static void set_error(const char *what, const char *path)
{
    snprintf(last_error, sizeof last_error, "%s: %s (Windows error %lu)", path ? path : "", what,
             (unsigned long)GetLastError());
}

void *ct_dlopen(const char *path)
{
    wchar_t w[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, w, (int)(sizeof w / sizeof w[0]))) {
        set_error("path not usable", path);
        return NULL;
    }
    for (wchar_t *p = w; *p; p++)   /* LoadLibrary searches the PATH for a bare name only */
        if (*p == L'/')
            *p = L'\\';
    HMODULE h = LoadLibraryW(w);
    if (!h)
        set_error("cannot load", path);
    return (void *)h;
}

void *ct_dlsym(void *lib, const char *name)
{
    FARPROC f = GetProcAddress((HMODULE)lib, name);
    void *p = NULL;
    memcpy(&p, &f, sizeof p);   /* function -> object pointer */
    if (!p)
        set_error("symbol not found", name);
    return p;
}

void ct_dlclose(void *lib)
{
    if (lib)
        FreeLibrary((HMODULE)lib);
}

const char *ct_dlerror(void)
{
    return last_error[0] ? last_error : "unknown error";
}

const char *ct_home(void)
{
    const char *h = getenv("HOME");
    if (!h || !*h)
        h = getenv("USERPROFILE");
    return h && *h ? h : NULL;
}

const char *ct_tmpdir(void)
{
    /* (TMPDIR may be an MSYS path like /tmp, which a native program can't use) */
    const char *t = getenv("TEMP");
    if (!t || !*t)
        t = getenv("TMP");
    if (!t || !*t)
        t = getenv("TMPDIR");
    return t && *t ? t : ".";
}

int ct_rename(const char *from, const char *to)
{
    wchar_t a[MAX_PATH * 2], b[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_UTF8, 0, from, -1, a, (int)(sizeof a / sizeof a[0])) ||
        !MultiByteToWideChar(CP_UTF8, 0, to, -1, b, (int)(sizeof b / sizeof b[0]))) {
        return -1;
    }
    if (!MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return -1;
    return 0;
}

int ct_link(const char *existing, const char *name)
{
    wchar_t a[MAX_PATH * 2], b[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_UTF8, 0, existing, -1, a, (int)(sizeof a / sizeof a[0])) ||
        !MultiByteToWideChar(CP_UTF8, 0, name, -1, b, (int)(sizeof b / sizeof b[0]))) {
        return -1;
    }
    return CreateHardLinkW(b, a, NULL) ? 0 : -1;
}

#else   /* POSIX */

#include <dlfcn.h>

const char *ct_tmpdir(void)
{
    const char *t = getenv("TMPDIR");
    return t && *t ? t : "/tmp";
}

void *ct_dlopen(const char *path)
{
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void *ct_dlsym(void *lib, const char *name)
{
    return dlsym(lib, name);
}

void ct_dlclose(void *lib)
{
    dlclose(lib);
}

const char *ct_dlerror(void)
{
    const char *e = dlerror();
    return e ? e : "unknown error";
}

const char *ct_home(void)
{
    return getenv("HOME");
}

#endif
