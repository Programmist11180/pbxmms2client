/*
 *   settings.cpp
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

#include <iostream>
#include <inkview.h>
#include "log.hpp"
#include "settings.hpp"
#include "xmms.hpp"

using namespace std;

// Settings names
char output_name[]="output", volume_level_name[]="volume_level", volume_inc_name[]="volume_inc",
  font_name[]="font", log_name[]="log_file", log_level_name[]="log_level";

// Settings values
char *output_type[]={"Internal", "Bluetooth", NULL},  *log_var[]={"Enabled","Disabled",NULL},
    *log_level_type[]={"ERROR","WARNING","INFO","DEBUG",NULL},
    *vol_incs [] = {"1", "2", "3", "4", "5", NULL};     // WARNING: last element must be an empty element!

iconfig *settings_icfg;
iconfigedit settings_icfgedit [] =
{
  {CFG_CHOICE, NULL, "Playback output", "Internal - built-in speakers and headphones, "
   "Bluetooth - A2DP profile for audio devices.", output_name, output_type[0], output_type, NULL},
  {CFG_NUMBER, NULL, "Volume level", "Volume level must be between 0% and 100%",
   volume_level_name, "50", NULL, NULL},
  {CFG_INDEX, NULL, "Volume increment", "The value that is increased or decreased volume",
   volume_inc_name, vol_incs[0], vol_incs, NULL},
  {CFG_FONT, NULL, "Font", NULL, font_name, DEFAULTFONT, NULL, NULL},
  {CFG_CHOICE, NULL, "Log file", NULL, log_name, log_var[0], log_var, NULL},
  {CFG_CHOICE, NULL, "Log verbosity", NULL, log_level_name, log_level_type[2], log_level_type, NULL},
  {0, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};


settings::settings ()
{
  settings_icfg = OpenConfig(CONFIGPATH "/xmms2/clients/pbxmms2client.cfg", settings_icfgedit);
  output = get_output(ReadString(settings_icfg, output_name, output_type[0]));
  vol_level = ReadInt(settings_icfg, volume_level_name, 50);
  vol_inc = 1+ReadInt(settings_icfg, volume_inc_name, 2);
  orientation = ReadInt(settings_icfg, "orientation", 0);
  //font = ReadString(settings_icfg, font_name, DEFAULTFONT);
  font_size = ReadInt(settings_icfg, "font_size", 20);
  //low_power = ReadInt(settings_icfg, "low_power", 25);
  log_enabled = get_log_enabled(ReadString(settings_icfg, log_name, log_var[0]));
  log_level = get_log_level(ReadString(settings_icfg,log_level_name,log_level_type[1]));
}

void settings::save ()
{
  //WriteString(settings_icfg, output_name, output);
  WriteInt(settings_icfg, volume_level_name, vol_level);
  //WriteInt(settings_icfg, volume_inc_name, vol_inc);
  //WriteString(settings_icfg, font_name, font);
  SaveConfig(settings_icfg);
}

settings::~settings ()
{
  pbxmms_settings->save();
  CloseConfig(settings_icfg);
}

bool settings::get_log_enabled (const char *str)
{
  if (!strcmp(str,log_var[0])) return true;
  else if (!strcmp(str,log_var[1])) return false;
  else
      {
          cerr<<PBXMMS2CLIENT<<": Invalid value for setting 'log_name': '"<<str<<"', set back to default."<<endl;
          return true;
      };
}

LOG_LEVEL settings::get_log_level (const char *str)
{
  if (!strcmp(str,log_level_type[0])) return LOG_ERR;
  else if (!strcmp(str,log_level_type[1])) return LOG_WARN;
  else if (!strcmp(str,log_level_type[2])) return LOG_INFO;
  else if (!strcmp(str,log_level_type[3])) return LOG_DBG;
  else
  {
      cerr<<PBXMMS2CLIENT<<": Invalid value for setting 'log_level': "<<str<<endl;
      return LOG_INFO;
  };
}

OUTPUT settings::get_output (const char *str)
{
  if (strcmp(str, output_type[0])==0) return INTERNAL;
  else if (strcmp(str, output_type[1])==0) return BLUETOOTH;
  else
  {
      write_to_log(LOG_ERR,"Invalid value for setting '%d', set default '%d'.", output_name, output_type[0]);
      return INTERNAL;
  };
}

void settings::config_handler (void)
{
  pbxmms_settings->save();
  NotifyConfigChanged();
  write_to_log(LOG_INFO,"Configuration saved.");
  return;
}

void settings::item_handler (char *item)
{
  if (!strcmp(item, output_name))
    {
      pbxmms_settings->output = pbxmms_settings->get_output(ReadString(settings_icfg, output_name, output_type[0]));
      change_output();
      return;
    };
  if (!strcmp(item, volume_level_name))
    {
      pbxmms_settings->vol_level = ReadInt(settings_icfg, volume_level_name, 50);
      set_volume();
      return;
    };
  if (!strcmp(item, volume_inc_name))
    {
      pbxmms_settings->vol_inc = 1+ReadInt(settings_icfg, volume_inc_name, 3);
      return;
    };
  if (!strcmp(item, log_name))
    {
      pbxmms_settings->log_enabled = pbxmms_settings->get_log_enabled(ReadString(settings_icfg, log_name, log_var[0]));
      if (pbxmms_settings->log_enabled) log_init();
      else log_end();
      return;
    };
}

void settings::change_settings (void)
{
  OpenConfigEditor(PBXMMS2CLIENT, settings_icfg, settings_icfgedit, settings::config_handler,
                   settings::item_handler);
}

short int set_volume ()
{
  static char vol [60];
  //xmmsc_result_t *res;
  if (pbxmms_settings->vol_level < 0) pbxmms_settings->vol_level = 0;
  if (pbxmms_settings->vol_level > 100) pbxmms_settings->vol_level = 100;
  WriteInt(settings_icfg, volume_level_name, pbxmms_settings->vol_level);
  // BUG: setting volume via xmms currently not working on reader
  /*res = xmmsc_playback_volume_set(pbconnection, "left", pbxmms_settings->vol_level);
  result_is_error(res, true);
  res = xmmsc_playback_volume_set(pbconnection, "right", pbxmms_settings->vol_level);
  result_is_error(res, true);*/

  sprintf(vol, "/mnt/ext1/system/bin/amixer --quiet sset Softmaster %d%%", pbxmms_settings->vol_level);
#ifndef __EMU__
  system(vol);
#endif
  return 0;
}
