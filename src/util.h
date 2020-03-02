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

#ifndef UTIL_H
#define UTIL_H

#include <log.h>
#include <string>
#include <filesystem>
#include <versioninfo.h>

class Executable;

/// Test if a file (or directory) by the specified name exists
bool FileExists(const std::string &filename);
bool FileExists(const std::wstring &filename);

bool FileExists(const std::wstring &searchPath, const std::wstring &filename);

std::string ToString(const std::wstring &source, bool utf8);
std::wstring ToWString(const std::string &source, bool utf8);

std::string& ToLowerInPlace(std::string& text);
std::string ToLowerCopy(const std::string& text);

std::wstring& ToLowerInPlace(std::wstring& text);
std::wstring ToLowerCopy(const std::wstring& text);
std::wstring ToLowerCopy(std::wstring_view text);

bool CaseInsensitiveEqual(const std::wstring &lhs, const std::wstring &rhs);

MOBase::VersionInfo createVersionInfo();

void SetThisThreadName(const QString& s);
void checkDuplicateShortcuts(const QMenu& m);

inline FILETIME toFILETIME(std::filesystem::file_time_type t)
{
  FILETIME ft;
  static_assert(sizeof(t) == sizeof(ft));

  std::memcpy(&ft, &t, sizeof(FILETIME));
  return ft;
}

inline std::filesystem::file_time_type toStdFileTime(LARGE_INTEGER i)
{
  std::filesystem::file_time_type ft;
  static_assert(sizeof(ft) == sizeof(i));

  std::memcpy(&ft, &i, sizeof(LARGE_INTEGER));
  return ft;
}

inline std::filesystem::file_time_type toStdFileTime(FILETIME ft)
{
  std::filesystem::file_time_type ftt;
  static_assert(sizeof(ftt) == sizeof(ft));

  std::memcpy(&ftt, &ft, sizeof(FILETIME));
  return ftt;
}

int naturalCompare(const QString& a, const QString& b);

void reportError(LPCSTR format, ...);
void reportError(LPCWSTR format, ...);

class windows_error : public std::runtime_error {
public:
  windows_error(const std::string& message, int errorcode = ::GetLastError())
    : runtime_error(constructMessage(message, errorcode)), m_ErrorCode(errorcode)
  {}
  int getErrorCode() const { return m_ErrorCode; }
private:
  std::string constructMessage(const std::string& input, int errorcode);
private:
  int m_ErrorCode;
};


enum class Exit
{
  None    = 0x00,
  Normal  = 0x01,
  Restart = 0x02,
  Force   = 0x04
};

const int RestartExitCode = INT_MAX;

using ExitFlags = QFlags<Exit>;
Q_DECLARE_OPERATORS_FOR_FLAGS(ExitFlags);

// these are defined in mainwindow.cpp
bool ExitModOrganizer(ExitFlags e=Exit::Normal);
bool ModOrganizerExiting();
bool ModOrganizerCanCloseNow();
void ResetExitFlag();

#endif // UTIL_H
