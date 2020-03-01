/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <string>

namespace AppConfig
{

#define APPPARAM(T, NAME, VALUE) inline T NAME() { return VALUE; }

APPPARAM(std::wstring, translationPrefix, L"organizer")
APPPARAM(std::wstring, pluginPath, L"plugins")
APPPARAM(std::wstring, profilesPath, L"profiles")
APPPARAM(std::wstring, modsPath, L"mods")
APPPARAM(std::wstring, downloadPath, L"downloads")
APPPARAM(std::wstring, overwritePath, L"overwrite")
APPPARAM(std::wstring, stylesheetsPath, L"stylesheets")
APPPARAM(std::wstring, cachePath, L"webcache")
APPPARAM(std::wstring, tutorialsPath, L"tutorials")
APPPARAM(std::wstring, logPath, L"logs")
APPPARAM(std::wstring, dumpsDir, L"crashDumps")
APPPARAM(std::wstring, profileTweakIni, L"profile_tweaks.ini")
APPPARAM(std::wstring, iniFileName, L"ModOrganizer.ini")
APPPARAM(std::wstring, portableLockFileName, L"portable.txt")
APPPARAM(std::wstring, firstStepsTutorial, L"tutorial_firststeps_main.js")

#undef APPPARAM

} // namespace

#endif // APPCONFIG_H
