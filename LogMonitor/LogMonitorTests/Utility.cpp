//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
#include "pch.h"

using namespace std;

///
/// Utility.cpp
///
/// Contains utility functions that are used across the tests
///

///
/// Creates a new random-name directory inside the Temp directory. 
///
/// \return The new directory path. If an error occurs, it's empty.
///
wstring CreateTempDirectory()
{
    WCHAR tempDirectory[L_tmpnam_s];
    ZeroMemory(tempDirectory, sizeof(tempDirectory));

    errno_t err = _wtmpnam_s(tempDirectory, L_tmpnam_s);
    if (err)
    {
        return L"";
    }

    long status = CreateDirectoryW(tempDirectory, NULL);
    if (status == 0)
    {
        return L"";
    }

    return wstring(tempDirectory);
}