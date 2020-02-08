#pragma once

#include <iostream>

enum eError {
    eError_NoError,
    eError_FailedToOpenFile,
    eError_FailedToCreateDirectory,
    eError_UnknownVersionNumber,
    eError_ValueOutOfBounds,
    eError_AlreadyLoaded,
    eError_InvalidData,
    eError_NoData,
    eError_FailedToCreateFile,
    eError_NumTypes
};

static void ShowError( eError leError )
{
    const char* lapErrorNames[ eError_NumTypes ] = {
        "No Error",
        "Failed to open file",
        "Failed to create directory",
        "Unknown version number",
        "Value of out bounds",
        "Already loaded",
        "Bad data",
        "Missing data",
        "Failed to create file",
    };

    std::cout << "ERROR: " << lapErrorNames[ leError ] << "\n";
}

#define SHOW_ERROR_AND_RETURN           if( leError != eError_NoError )   \
                                        {                                 \
                                            ShowError(leError );          \
                                            return leError;               \
                                        }

#define SHOW_ERROR_AND_RETURN_W( lTodo )  if( leError != eError_NoError )   \
                                          {                                 \
                                              ShowError(leError );          \
                                              lTodo;                        \
                                              return leError;               \
                                          }
