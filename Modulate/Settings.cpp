#include "pch.h"
#include "Settings.h"

const char* CSettings::mpInputFilename = "main_ps3.hdr";
std::string CSettings::mInputDirectory = "/";
const char* CSettings::mpOutputFilename = "main_ps3.hdr";
std::string CSettings::mOutputDirectory = "/";
bool        CSettings::mbVerbose = false;
bool        CSettings::mbOverwriteOutputFiles = false;
