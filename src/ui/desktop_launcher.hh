#pragma once

#include <QString>

namespace lazyfinder
{

class DesktopLauncher
{
public:
  static bool open_file(const QString &path, int page_or_timestamp = 0);
  static bool reveal_in_folder(const QString &path);
  static void copy_path(const QString &path);
};

} // namespace lazyfinder
