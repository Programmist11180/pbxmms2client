/*
 *   settings.hpp
 *
 *   Copyright (C) 2014-2020 Programmist11180 <programmer11180@programist.ru>
 *
 *   This file is part of PBXMMS2client.
 *
 *   PBXMMS2client is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   PBXMMS2client is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with PBXMMS2client.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SETTINGS_HPP
#define SETTINGS_HPP

static char PBXMMS2CLIENT [] = "PBXMMS2client";

enum OUTPUT {INTERNAL, BLUETOOTH};
enum LOG_LEVEL {LOG_ERR, LOG_WARN, LOG_INFO, LOG_DBG};

class settings
{
public:
  settings ();
  ~settings ();
  void save ();
  void change_settings (void);

  OUTPUT output;    // NOTE: Internal - speakers and headphones connector, Bluetooth - a2dpd
  int vol_level;
  int vol_inc;
  int orientation;
  char *font;
  int font_size;
  //int low_power;
  bool log_enabled;
  LOG_LEVEL log_level;

private:
  static void config_handler (void);
  static void item_handler (char *item);
  bool get_log_enabled (const char *str);
  LOG_LEVEL get_log_level (const char *str);
  OUTPUT get_output (const char *str);
};

extern settings *pbxmms_settings;

short int set_volume ();

#endif // SETTINGS_HPP
