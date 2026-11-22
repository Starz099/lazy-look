#pragma once

#include <QString>

namespace lazylook
{

class DesktopLauncher
{
public:
  static bool open_file(const QString &path, int page_or_timestamp = 0);
  static bool reveal_in_folder(const QString &path);
  static void copy_path(const QString &path);
  // Copies an arbitrary text string to the system clipboard.
  static void copy_to_clipboard(const QString &text);
};

} // namespace lazylook
