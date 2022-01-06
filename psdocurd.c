// PSeuDO CURrentDirectory module (psdocurd.c)
//
// This was swiped from cmd.exe sources to use the environment to emulate
// per process per drive current directory stuff.
//
// This uses the special '=A:=' environment strings to remember the
// current directories.
//
// Since the library is a DLL, all globals are process based.
//
#define UNICODE 1

#include "shellprv.h"
#pragma  hdrstop

#define SUCCESS FALSE
#define FAILURE TRUE
#define GD_DEFAULT 0

WCHAR CurDrvDirW[MAX_PATH] = L"";

#define DBL_BSLASH(sz) \
    (*(WCHAR *)(sz) == CHAR_BSLASH) && \
(*(WCHAR *)((sz)+1) == CHAR_BSLASH)

INT
SheSetEnvVarW(
              WCHAR *varname,
              WCHAR *varvalue
              )
{
    if (!wcslen(varvalue)) {
        varvalue = NULL; // null to remove from env
    }
    return (!SetEnvironmentVariable(varname, varvalue));
}

WCHAR *
SheGetEnvVarW(
              WCHAR *varname)
{
    static WCHAR GetEnvVarBuffer[ 1024 ];
    
    if (GetEnvironmentVariableW(varname, GetEnvVarBuffer, 1024)) {
        return(GetEnvVarBuffer);
    } else {
        return(NULL);
    }
}

INT
SheGetPathOffsetW(
                  LPWSTR lpszDir)
{
    // sanity
    if (!lpszDir || !*lpszDir) {
        return(-1);
    }
    
    if ((*(lpszDir+1) == WCHAR_COLON) &&
        ((*(lpszDir+2) == WCHAR_BSLASH) ||
        (*(lpszDir+2) == WCHAR_SLASH) ||
        (*(lpszDir+2) == WCHAR_NULL)) ) {
        
        return(2);
        
    } else if (DBL_BSLASH(lpszDir)) {
        
        // Deviceless, assume LanMan Provided UNC names (i.e. DBL_BSLASH above)
        
        INT cchServerShareLen = -1;
        DWORD dwSlashesSeen = 0;
        
        // There are exactly 4 b-slashes from here
        
        while (dwSlashesSeen != 4) {
            cchServerShareLen++;
            
            if (!*lpszDir) {
                if (dwSlashesSeen == 3) {
                    return(cchServerShareLen);
                } else {
                    return(-1);
                }
            }
            
            if ((*lpszDir == CHAR_BSLASH) || (*lpszDir == CHAR_SLASH)) {
                dwSlashesSeen++;
            }
            
            lpszDir++;
        }
        
        return(cchServerShareLen);
        
    } else {
        
        // unrecognized format
        return(-1);
    }
}

INT
SheGetDirW(
           INT  iDrive,             // 0 = default, 1=A....
           WCHAR *str
           )
{
    WCHAR        denvname[ 4 ];
    WCHAR        *denvvalue;
    WCHAR        *strT = str;
    
    if (iDrive-- == GD_DEFAULT) {
        GetCurrentDirectoryW(MAX_PATH, str); // pray that str is at least MAX_PATH
        return(SUCCESS);
    }
    
    denvname[ 0 ] = WCHAR_EQUAL;
    denvname[ 1 ] = (WCHAR)(WCHAR_CAP_A + (WCHAR)iDrive);
    denvname[ 2 ] = WCHAR_COLON;
    denvname[ 3 ] = WCHAR_NULL;
    
    denvvalue = SheGetEnvVarW( denvname );
    if (!denvvalue) {
        *str++ = (WCHAR)(WCHAR_CAP_A + (WCHAR)iDrive);
        *str++ = WCHAR_COLON;
        *str++ = WCHAR_BSLASH;
        *str   = WCHAR_NULL;
    }
    else {
        
        if (FAILED(StringCchCopy(str, MAX_PATH, denvvalue)))
        {
            return FAILURE;
        }
    }
    
    // there is a subtle difference between getcurrentdirectory() of DOS and the
    // equivalent SheGetDir() of NT. On dos, getcurrentdirectory() accesses the
    // floppy drives while in NT SheGetDir() returns the values that are set in
    // the environment variables. So we hit the disk on NT too, by calling
    // GetFileAttributes. We return SUCCESS or FAILURE based on the return value
    // of GetFileAttributes.
    
    if (GetFileAttributesW(strT) == (DWORD)0xffffffff)
        return(FAILURE);
    else
        return(SUCCESS);
}

// Should be BOOL, but possible mips breakage
INT
SheGetDirA(
           INT iDrive,             // 0 = default, 1=A....
           CHAR *szDirA)
{
    WCHAR szDirW[MAX_PATH];
    BOOL fDefCharUsed;
    
    INT nRet;
    
    if (!szDirA) {
        return(FAILURE);
    }
    
    nRet = SheGetDirW(iDrive, szDirW);
    if (SUCCESS == nRet)
    {
        WideCharToMultiByte(CP_ACP, 0, szDirW, -1, (LPSTR)szDirA,
                            MAX_PATH, NULL, &fDefCharUsed);
    }
    
    return(nRet);
}

INT
SheChangeDirW(
              WCHAR *newdir
              )
{
    WCHAR       denvname[ 4 ];
    WCHAR       newpath[ MAX_PATH ];
    WCHAR       denvvalue[ MAX_PATH ];
    WCHAR       c, *s;
    DWORD       attr;
    
    GetCurrentDirectoryW( MAX_PATH, denvvalue );
    c = (WCHAR)(DWORD_PTR)CharUpperW((LPTSTR)(DWORD_PTR)denvvalue[0]);
    
    denvname[0] = WCHAR_EQUAL;
    if (IsCharAlphaW(*newdir) && newdir[1] == WCHAR_COLON) {
        denvname[1] = (WCHAR)(DWORD_PTR)CharUpperW((LPTSTR)(DWORD_PTR)*newdir);
        newdir += 2;
    } else {
        denvname[ 1 ] = c;
    }
    denvname[ 2 ] = WCHAR_COLON;
    denvname[ 3 ] = WCHAR_NULL;
    
    if ((*newdir == WCHAR_BSLASH) || (*newdir == WCHAR_SLASH)) {
        newpath[ 0 ] = denvname[ 1 ];
        newpath[ 1 ] = denvname[ 2 ];
        if (FAILED(StringCchCopy( &newpath[ 2 ], ARRAYSIZE(newpath) - 2, newdir)))
        {
            return ERROR_BUFFER_OVERFLOW;
        }
    } else {
        if (NULL != (s = SheGetEnvVarW( denvname ))) 
        {
            if (FAILED(StringCchCopy(newpath, ARRAYSIZE(newpath), s)))
            {
                return ERROR_BUFFER_OVERFLOW;
            }
        } 
        else 
        {
            newpath[ 0 ] = denvname[ 1 ];
            newpath[ 1 ] = denvname[ 2 ];
            newpath[ 2 ] = WCHAR_NULL;
        }
        s = newpath + wcslen( newpath );
        *s++ = WCHAR_BSLASH;
        if (FAILED(StringCchCopy(s, ARRAYSIZE(newpath) - (s - newpath), newdir)))
        {
            return ERROR_BUFFER_OVERFLOW;
        }
    }
    
    if (!GetFullPathNameW(newpath, MAX_PATH, denvvalue, &s )) {
        return( ERROR_ACCESS_DENIED );
    }
    
    attr = GetFileAttributesW( denvvalue );
    if (attr == -1 || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return( ERROR_ACCESS_DENIED );
    }
    
    if (SheSetEnvVarW(denvname,denvvalue)) {
        return( ERROR_NOT_ENOUGH_MEMORY );
    }
    
    SetCurrentDirectoryW( denvvalue );
    
    // this seems wrong... SheGetDir(GD_DEFAULT, CurDrvDirW) ;
    if (FAILED(StringCchCopy(CurDrvDirW, ARRAYSIZE(CurDrvDirW), denvvalue))) // this seems right to me.
    {
        return (ERROR_BUFFER_OVERFLOW);
    }
    
    return(SUCCESS) ;
}

INT
SheChangeDirA(
              CHAR *szNewDirA
              )
{
    INT iRet;
    WCHAR szNewDirW[MAX_PATH];
    
    if (szNewDirA &&
        MultiByteToWideChar(CP_ACP, 0, (LPSTR)szNewDirA, -1, szNewDirW, MAX_PATH))
    {
        iRet = SheChangeDirW(szNewDirW);
    }
    else 
    {
        iRet = FAILURE;
    }
    return iRet;
}

INT
SheChangeDirExW(
                WCHAR *newdir)
{
    WCHAR wcEnvName[MAX_PATH];
    WCHAR wcNewPath[MAX_PATH];
    WCHAR wcEnvValue[MAX_PATH];
    
    DWORD cchPathOffset;
    
    WCHAR *s;
    DWORD attr;
    BOOL bUsedEnv = FALSE;
    
    if (newdir && *newdir &&
        ((cchPathOffset = SheGetPathOffsetW(newdir)) != 0xFFFFFFFF)) {
        WCHAR wc = newdir[cchPathOffset];
        
        newdir[cchPathOffset] = WCHAR_NULL;
        if (FAILED(StringCchCopy(&wcEnvName[1], 
                                 ARRAYSIZE(wcEnvName) - 1,
                                 newdir)))
        {
            return ERROR_BUFFER_OVERFLOW;
        }
        
        newdir[cchPathOffset] = wc;
        newdir += cchPathOffset;
        
    } else {
        
        GetCurrentDirectoryW(MAX_PATH, wcEnvValue);
        
        if (FAILED(StringCchCopy(&wcEnvName[1], 
                                 ARRAYSIZE(wcEnvName) - 1,
                                 wcEnvValue)))
        {
            return ERROR_BUFFER_OVERFLOW;
        }
    }
    
    wcEnvName[0] = WCHAR_EQUAL;
    if ((cchPathOffset = SheGetPathOffsetW(&wcEnvName[1])) != 0xFFFFFFFF) {
        
        // add one since the first character is "="
        wcEnvName[cchPathOffset + 1] = WCHAR_NULL;
    }
    
    //
    // If return value is 2, then we are a drive letter like A:
    // We need to uppercase the drive letter here, since the environ
    // vars are usually set as uppercase.
    //
    if (cchPathOffset == 2) {
        
        wcEnvName[1] = (WCHAR)(DWORD_PTR)CharUpper((LPWSTR)(DWORD_PTR) wcEnvName[1]);
    }
    
    if (newdir && *newdir &&
        ((*newdir == WCHAR_BSLASH) || (*newdir == WCHAR_SLASH))) {
        
        if (FAILED(StringCchCopy(wcNewPath, 
                                 ARRAYSIZE(wcNewPath),
                                 &wcEnvName[1])))
        {
            return ERROR_BUFFER_OVERFLOW;
        }
        if (FAILED(StringCchCat(wcNewPath, 
                                ARRAYSIZE(wcNewPath),
                                newdir)))
        {
            return ERROR_BUFFER_OVERFLOW;
        }
        
    } else {
        LPWSTR lpszEnvName;
        
        if (NULL != (lpszEnvName = SheGetEnvVarW(wcEnvName))) {
            
            if (FAILED(StringCchCopy(wcNewPath, 
                                     ARRAYSIZE(wcNewPath),
                                     lpszEnvName)))
            {
                return ERROR_BUFFER_OVERFLOW;
            }
            bUsedEnv = TRUE;
            
        } else {
            if (FAILED(StringCchCopy(wcNewPath, 
                                     ARRAYSIZE(wcNewPath),
                                     &wcEnvName[1])))
            {
                return ERROR_BUFFER_OVERFLOW;
            }
        }
        
        if (newdir && *newdir) 
        {
            if (!PathAppend(wcNewPath, newdir))
            {
                return ERROR_BUFFER_OVERFLOW;
            }
        }
    }
    
    if (!GetFullPathNameW(wcNewPath, MAX_PATH, wcEnvValue, &s)) {
        return( ERROR_ACCESS_DENIED );
    }
    
    attr = GetFileAttributesW(wcEnvValue);
    
    if ((attr == 0xFFFFFFFF ||
        ((attr & FILE_ATTRIBUTE_DIRECTORY) &&
        !SetCurrentDirectoryW(wcEnvValue)))
        && bUsedEnv ) 
    {
        if (FAILED(StringCchCopy(wcEnvValue, ARRAYSIZE(wcEnvValue), &wcEnvName[1])) ||
            FAILED(StringCchCat(wcEnvValue, ARRAYSIZE(wcEnvValue), TEXT("\\"))))
        {
            return ERROR_BUFFER_OVERFLOW;
        }
            
        attr = GetFileAttributesW(wcEnvValue);
        
    }
    
    if (attr == 0xFFFFFFFF || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        
        return(ERROR_ACCESS_DENIED);
    }
    
    if (!SetCurrentDirectoryW(wcEnvValue)) {
        return( ERROR_ACCESS_DENIED );
    }
    
    // It's ok if this fails
    SheSetEnvVarW(wcEnvName, wcEnvValue);
    
    // this seems right to me.
    if (FAILED(StringCchCopy(CurDrvDirW, ARRAYSIZE(CurDrvDirW), wcEnvValue)))
    {
        return ERROR_BUFFER_OVERFLOW;
    }
        
    return(SUCCESS) ;
}

INT
SheChangeDirExA(
                CHAR *szNewDirA
                )
{
    INT iRet;
    WCHAR szNewDirW[MAX_PATH];
    
    if (szNewDirA &&
        MultiByteToWideChar(CP_ACP, 0, (LPSTR)szNewDirA, -1, szNewDirW, MAX_PATH))
    {
        iRet = SheChangeDirExW(szNewDirW);
    }
    else
    {
        iRet = FAILURE;
    }
    return iRet;
}

INT
SheGetCurDrive()
{
    if (!CurDrvDirW[0]) {
        SheGetDirW(GD_DEFAULT, CurDrvDirW) ;
    }
    return(CurDrvDirW[0] - WCHAR_CAP_A);
}


INT
SheSetCurDrive(
               INT iDrive
               )
{
    WCHAR chT[MAX_PATH];
    
    if (CurDrvDirW[0] != (WCHAR)(WCHAR_CAP_A + iDrive)) {
        SheGetDirW(iDrive + 1, chT);
        return(SheChangeDirW(chT));
    }
    return(SUCCESS);
}

INT
SheFullPathA(
             CHAR *fname,
             DWORD sizpath,
             CHAR *buf)
{
    DWORD rc = SUCCESS;         /* prime with good rc */
    DWORD buflen;               /* buffer length      */
    CHAR *filepart;
    
    if (*fname == CHAR_NULL) {
        SheGetDirA(GD_DEFAULT, buf);
        buf += 2;                           /* Inc past drivespec      */
        buflen = strlen(buf);             /* Is curdir root only?    */
        if (buflen >= MAX_PATH-3) {  /* If too big then stop    */
            rc = FAILURE;
        } else if (buflen != 1) {              /* if not root then append */
            *(buf+buflen++) = CHAR_BSLASH;      /* ...a pathchar and...    */
            *(buf+buflen) = CHAR_NULL ;              /* ...a null CHAR...       */
        }                                 /*                         */
    } else {
        if ((strlen(fname) == 2) &&
            (*(fname + 1) == CHAR_COLON)
            // && (!is_dbcsleadchar(*fname))
            ) {
            SheGetDirA((CHAR)(DWORD_PTR)CharUpperA((LPSTR)(DWORD_PTR)*fname) - CHAR_CAP_A, buf);                 /* Get curdrvdir           */
            if ((buflen = strlen(buf)) > 3) {
                *(buf+buflen++) = CHAR_BSLASH;      /* ...a pathchar and...    */
                *(buf+buflen) = CHAR_NULL ;          /* ...a null CHAR...           */
            }
        } else {
            if (!GetFullPathNameA( fname, sizpath, buf, &filepart )) {
                rc = FAILURE;
            }
        }
    }
    return(rc);
}

INT
SheFullPathW(
             WCHAR *fname,
             DWORD sizpath,
             WCHAR *buf)
{
    DWORD rc = SUCCESS;         /* prime with good rc */
    DWORD buflen;               /* buffer length      */
    WCHAR *filepart;
    
    if (*fname == WCHAR_NULL) {
        SheGetDirW(GD_DEFAULT, buf);
        buf += 2;                           /* Inc past drivespec      */
        buflen = wcslen(buf);             /* Is curdir root only?    */
        if (buflen >= MAX_PATH-3) {  /* If too big then stop    */
            rc = FAILURE;
        } else if (buflen != 1) {              /* if not root then append */
            *(buf+buflen++) = WCHAR_BSLASH;      /* ...a pathchar and...    */
            *(buf+buflen) = WCHAR_NULL ;              /* ...a null CHAR...       */
        }                                 /*                         */
    } else {
        if ((wcslen(fname) == 2) &&
            (*(fname + 1) == WCHAR_COLON)
            // && (!is_dbcsleadchar(*fname))
            ) {
            SheGetDirW((WCHAR)(DWORD_PTR)CharUpperW((LPWSTR)(DWORD_PTR)*fname) - WCHAR_CAP_A, buf);                 /* Get curdrvdir           */
            if ((buflen = wcslen(buf)) > 3) {
                *(buf+buflen++) = WCHAR_BSLASH;      /* ...a pathchar and...    */
                *(buf+buflen) = WCHAR_NULL ;          /* ...a null CHAR...           */
            }
        } else {
            if (!GetFullPathNameW( fname, sizpath, buf, &filepart )) {
                rc = FAILURE;
            }
        }
    }
    return(rc);
}



BOOL
SheGetDirExW(
             LPWSTR lpszCurDisk,
             LPDWORD lpcchCurDir,
             LPWSTR lpszCurDir)
{
    WCHAR wcEnvName[MAX_PATH];
    LPWSTR lpszEnvValue;
    INT cchPathOffset;
    
    // if lpszCurDisk NULL, do default
    if (!lpszCurDisk) {
        DWORD dwRet = GetCurrentDirectoryW(*lpcchCurDir, lpszCurDir);
        
        if (dwRet) {
            *lpcchCurDir = dwRet;
            return(TRUE);
        } else {
            return(FALSE);
        }
        
    }
    
    cchPathOffset = SheGetPathOffsetW(lpszCurDisk);
    
    if (cchPathOffset != 0xFFFFFFFF) {
        WCHAR wc = *(lpszCurDisk + cchPathOffset);
        
        *(lpszCurDisk + cchPathOffset) = WCHAR_NULL;
        if (FAILED(StringCchCopy(&wcEnvName[1], ARRAYSIZE(wcEnvName) - 1, lpszCurDisk)))
        {
            return FALSE;
        }
        wcEnvName[0] = WCHAR_EQUAL;
        
        *(lpszCurDisk + cchPathOffset) = wc;
        
        if (NULL != (lpszEnvValue = SheGetEnvVarW(wcEnvName))) 
        {
            if (FAILED(StringCchCopy(lpszCurDir, *lpcchCurDir, lpszEnvValue)))
            {
                *lpcchCurDir = lstrlen(lpszEnvValue) + 1;
                return FALSE;
            }
            else
            {
                return TRUE;
            }
        }
        
        if (FAILED(StringCchCopy(lpszCurDir, *lpcchCurDir, lpszCurDisk)))
        {
            return FALSE;
        }
        
        *(lpszCurDir + cchPathOffset) = WCHAR_BSLASH;
        *(lpszCurDir + cchPathOffset + 1) = WCHAR_NULL;
        
        return(TRUE);
        
    } else {
        
        // parsing error
        return(FALSE);
        
    }
}