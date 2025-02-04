/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

/********************************************************
 * The native file-system
 ********************************************************/

// We need to include stdio.h below. Turn off GFILE_NEED_STDIO just for this file to prevent conflicts
#define GFILE_NEED_STDIO_MUST_BE_OFF

#include "../../gfx.h"

#if GFX_USE_GFILE && GFILE_NEED_NATIVEFS

#include "gfile_fs.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

static gBool NativeDel(const char *fname);
static gBool NativeExists(const char *fname);
static gFileSize	NativeFilesize(const char *fname);
static gBool NativeRen(const char *oldname, const char *newname);
static gBool NativeOpen(GFILE *f, const char *fname);
static void NativeClose(GFILE *f);
static int NativeRead(GFILE *f, void *buf, int size);
static int NativeWrite(GFILE *f, const void *buf, int size);
static gBool NativeSetpos(GFILE *f, gFileSize pos);
static gFileSize NativeGetsize(GFILE *f);
static gBool NativeEof(GFILE *f);
#if GFILE_NEED_FILELISTS
static gfileList *NativeFlOpen(const char *path, gBool dirs);
static const char *NativeFlRead(gfileList *pfl);
static void NativeFlClose(gfileList *pfl);
#endif

const GFILEVMT FsNativeVMT = {
#if defined(WIN32) || GFX_USE_OS_WIN32
    GFSFLG_TEXTMODES |
#else
    GFSFLG_CASESENSITIVE |
#endif
    GFSFLG_WRITEABLE | GFSFLG_SEEKABLE | GFSFLG_FAST,		// flags
    'N',												// prefix
    NativeDel, NativeExists, NativeFilesize, NativeRen,
    NativeOpen, NativeClose, NativeRead, NativeWrite,
    NativeSetpos, NativeGetsize, NativeEof,
    0, 0, 0,
#if GFILE_NEED_FILELISTS
    NativeFlOpen, NativeFlRead, NativeFlClose
#endif
};

void _gfileNativeAssignStdio(void)
{
    static GFILE NativeStdIn;
    static GFILE NativeStdOut;
    static GFILE NativeStdErr;

    NativeStdIn.flags = GFILEFLG_OPEN | GFILEFLG_READ;
    NativeStdIn.vmt = &FsNativeVMT;
    NativeStdIn.obj = (void *)stdin;
    NativeStdIn.pos = 0;
    gfileStdIn = &NativeStdIn;
    NativeStdOut.flags = GFILEFLG_OPEN | GFILEFLG_WRITE | GFILEFLG_APPEND;
    NativeStdOut.vmt = &FsNativeVMT;
    NativeStdOut.obj = (void *)stdout;
    NativeStdOut.pos = 0;
    gfileStdOut = &NativeStdOut;
    NativeStdErr.flags = GFILEFLG_OPEN | GFILEFLG_WRITE | GFILEFLG_APPEND;
    NativeStdErr.vmt = &FsNativeVMT;
    NativeStdErr.obj = (void *)stderr;
    NativeStdErr.pos = 0;
    gfileStdErr = &NativeStdErr;
}

static void Native_flags2mode(char *buf, gU16 flags)
{
    if (flags & GFILEFLG_MUSTEXIST) {
        *buf = 'r';
    } else if (flags & GFILEFLG_APPEND) {
        *buf = 'a';
    } else {
        *buf = 'w';
    }
    buf++;
    if ((flags & (GFILEFLG_READ | GFILEFLG_WRITE)) == (GFILEFLG_READ | GFILEFLG_WRITE)) {
        *buf++ = '+';
    }
    if (flags & GFILEFLG_BINARY) {
        *buf++ = 'b';
    }
    if (flags & GFILEFLG_MUSTNOTEXIST) {
        *buf++ = 'x';
    }
    *buf++ = 0;
}

static gBool NativeDel(const char *fname)
{
    return remove(fname) ? gFalse : gTrue;
}
static void NativeClose(GFILE *f)
{
    fclose((FILE *)f->obj);
}
static int NativeRead(GFILE *f, void *buf, int size)
{
    return fread(buf, 1, size, (FILE *)f->obj);
}
static int NativeWrite(GFILE *f, const void *buf, int size)
{
    return fwrite(buf, 1, size, (FILE *)f->obj);
}
static gBool NativeSetpos(GFILE *f, gFileSize pos)
{
    return fseek((FILE *)f->obj, pos, SEEK_SET) ?  gFalse : gTrue;
}
static gBool NativeEof(GFILE *f)
{
    return feof((FILE *)f->obj) ? gTrue : gFalse;
}
static gBool NativeRen(const char *oldname, const char *newname)
{
    return rename(oldname, newname) ? gFalse : gTrue;
}
static gBool NativeExists(const char *fname)
{
    // We define access this way so we don't have to include <unistd.h> which may
    //	(and does under windows) contain conflicting definitions for types such as gU16.
    extern int access(const char *pathname, int mode);
    return access(fname, 0) ? gFalse : gTrue;
}
static gFileSize	NativeFilesize(const char *fname)
{
    struct stat st;
    if (stat(fname, &st)) {
        return (gFileSize) - 1;
    }
    return (gFileSize)st.st_size;
}
static gBool NativeOpen(GFILE *f, const char *fname)
{
    FILE *fd;
    char mode[5];

    Native_flags2mode(mode, f->flags);
    if (!(fd = fopen(fname, mode))) {
        return gFalse;
    }
    f->obj = (void *)fd;
    return gTrue;
}
static gFileSize NativeGetsize(GFILE *f)
{
    struct stat st;
    if (fstat(fileno((FILE *)f->obj), &st)) {
        return (gFileSize) - 1;
    }
    return (gFileSize)st.st_size;
}

#if GFILE_NEED_FILELISTS
#if defined(WIN32) || GFX_USE_OS_WIN32
typedef struct NativeFileList {
    gfileList			fl;
    HANDLE				d;
    WIN32_FIND_DATA		f;
    gBool				first;
} NativeFileList;

static gfileList *NativeFlOpen(const char *path, gBool dirs)
{
    NativeFileList		*p;
    (void)				dirs;

    if (!(p = gfxAlloc(sizeof(NativeFileList)))) {
        return 0;
    }
    if ((p->d = FindFirstFile(path, &p->f)) == INVALID_HANDLE_VALUE) {
        gfxFree(p);
        return 0;
    }
    p->first = gTrue;
    return &p->fl;
}

static const char *NativeFlRead(gfileList *pfl)
{
#define nfl		((NativeFileList *)pfl)
    while (1) {
        if (!nfl->first && !FindNextFile(nfl->d, &nfl->f)) {
            return 0;
        }
        nfl->first = gFalse;
        if (nfl->f.cFileName[0] == '.') {
            continue;
        }
        if (nfl->fl.dirs) {
            if ((nfl->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                break;
            }
        } else {
            if (!(nfl->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                break;
            }
        }
    }
    return nfl->f.cFileName;
#undef nfl
}

static void NativeFlClose(gfileList *pfl)
{
    CloseHandle(((NativeFileList *)pfl)->d);
    gfxFree(pfl);
}

#else
#include <dirent.h>

typedef struct NativeFileList {
    gfileList			fl;
    DIR 				*d;
    struct dirent 		*f;
} NativeFileList;

static gfileList *NativeFlOpen(const char *path, gBool dirs)
{
    NativeFileList		*p;
    (void)				dirs;

    if (!(p = gfxAlloc(sizeof(NativeFileList)))) {
        return 0;
    }
    if (!(p->d = opendir(path))) {
        gfxFree(p);
        return 0;
    }
    return &p->fl;
}

static const char *NativeFlRead(gfileList *pfl)
{
#define nfl		((NativeFileList *)pfl)
    while (1) {
        if (!(nfl->f = readdir(nfl->d))) {
            return 0;
        }
        if (nfl->f->d_name[0] == '.') {
            continue;
        }

#ifdef _DIRENT_HAVE_D_TYPE
        if (nfl->fl.dirs) {
            if (nfl->f->d_type == DT_DIR) {
                break;
            }
        } else {
            if (nfl->f->d_type == DT_REG) {
                break;
            }
        }
#else
        // Oops - no type field. We could use stat() here but that would mean
        //			concatting the supplied path to the found filename.
        //			That all just seems too hard. Instead we just don't
        //			distinguish between files and directories.
        break;
#endif
    }
    return nfl->f->d_name;
#undef nfl
}

static void NativeFlClose(gfileList *pfl)
{
    closedir(((NativeFileList *)pfl)->d);
    gfxFree(pfl);
}
#endif
#endif

#endif //GFX_USE_GFILE && GFILE_NEED_NATIVEFS
