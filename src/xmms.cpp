/*
 *   xmms.cpp
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

#include <cstdarg>
#include <inkview.h>
#include <iostream>
#include <time.h>
#include "log.hpp"
#include "xmms.hpp"

using namespace std;

const char wake_lock_file [] = "/sys/power/wake_lock";
const char wake_unlock_file []  = "/sys/power/wake_unlock";

void print_error (const char *err_text)
{
  cout<<err_text<<endl;
  write_to_log(LOG_ERR, "print_error(): %s", err_text);
  //Message(ICON_ERROR, PBXMMS2CLIENT, err_text, long_timeout);
  return;
}

xmmsv_t *result_is_error (xmmsc_result_t *res, bool unref)
{
  xmmsc_result_wait(res);
  xmmsv_t *_t = xmmsc_result_get_value(res);
  if (value_is_error(_t))
    {
      /*if (unref)*/ xmmsc_result_unref(res);
      return NULL;
    };
  if (unref) xmmsc_result_unref(res);
  return _t;
}

bool value_is_error (xmmsv_t *value)
{
  if (xmmsv_is_error(value)) {
      const char *err_buf;
      xmmsv_get_error(value, &err_buf);
      print_error(err_buf);
      return true;
    }
  else return false;
}

void server_run (void)
{
  set_wake_lock(true);
  char *xmms_env = getenv("XMMS_PATH");
  if (xmmsc_connect(pbconnection, xmms_env)) {
    write_to_log(LOG_INFO,"XMMS2 server is already running.");
    free(xmms_env);
    if (server_playback_status()!=XMMS_PLAYBACK_STATUS_PLAY) set_wake_lock(false);
    return;
   };
#ifdef __EMU__
  system("xmms2-launcher");
#else
  system("/mnt/ext1/system/bin/xmms2-launcher");
#endif
  if (!xmmsc_connect(pbconnection, xmms_env))
     print_error(xmmsc_get_last_error(pbconnection));
  else write_to_log(LOG_INFO,"XMMS2 server was started.");
  free(xmms_env);
  set_wake_lock(false);
}

void server_halt (void)
{
  set_wake_lock(true);
  result_is_error(xmmsc_quit(pbconnection), true);
  write_to_log(LOG_INFO,"XMMS2 server stopped.");
  set_wake_lock(false);
}

void server_stats (void)
{
  xmmsc_result_t *res = xmmsc_main_stats(pbconnection);
  xmmsv_t *val = result_is_error(res, false);
  if (val==NULL) return;
  const char *version;
  char *st = new char [1024];
  int time;
  xmmsv_dict_entry_get_string(val, "version", &version);
  xmmsv_dict_entry_get_int(val, "uptime", &time);
  sprintf(st, "XMMS2 version: \"%s\"\nServer uptime: %d days, %d hours, %d minutes\n", version,
      div(time, 86400).quot, div(div(time, 86400).rem, 3600).quot,
          div(div(div(time, 86400).rem, 3600).rem, 60).quot);
  xmmsc_result_unref(res);
  Message(ICON_INFORMATION, PBXMMS2CLIENT, st, long_timeout);
  delete [] st;
}

void server_list_plugins (void)
{
  xmmsc_result_t *res = xmmsc_main_list_plugins(pbconnection, XMMS_PLUGIN_TYPE_ALL);
  xmmsv_t *val = result_is_error(res, false);
  if (!val) return;
  xmmsv_list_iter_t *iter;
  xmmsv_get_list_iter(val, &iter);
  xmmsv_list_iter_first(iter);
  const char *s;
  char *plugins_list = new char [3000];
  int pos = 0;
  xmmsv_t *entry;
  while (xmmsv_list_iter_valid(iter)) {
      xmmsv_list_iter_entry(iter, &val);
      xmmsv_dict_get (val, "shortname", &entry);
      xmmsv_get_string (entry, &s);
      pos += sprintf(&plugins_list[pos], "%s", s);
      xmmsv_dict_get (val, "description", &entry);
      xmmsv_get_string (entry, &s);
      pos += sprintf(&plugins_list[pos], " - %s\n", s);
      xmmsv_list_iter_next(iter);
    };
  Message(0, PBXMMS2CLIENT, plugins_list, long_timeout);
  xmmsc_result_unref(res);
  delete [] plugins_list;
  return;
}

void change_output (void)
{
#ifndef __EMU__
  switch (pbxmms_settings->output)
    {
    case INTERNAL:
      result_is_error(xmmsc_config_set_value(pbconnection, "alsa.device", "default_sm"), true);
      //result_is_error(xmmsc_config_set_value(pbconnection, "alsa.mixer", "PCM"), true);
      result_is_error(xmmsc_config_set_value(pbconnection, "alsa.mixer_dev", "default"), true);
      //result_is_error(xmmsc_config_set_value(pbconnection, "alsa.mixer_index", "0"), true);
      break;
    case BLUETOOTH:
      if (result_is_error(xmmsc_config_set_value(pbconnection, "alsa.device", "a2dpd_sm"), true))
        write_to_log(LOG_INFO,"Output set to Bluetooth.");
      else write_to_log(LOG_ERR, "Unable to set output to Bluetooth");
      result_is_error(xmmsc_config_set_value(pbconnection, "alsa.mixer_dev", "a2dpd"), true);
      break;
    };
#endif
}

short int set_wake_lock (bool on)
{
#ifndef __EMU__
  FILE *lk;
  const char chars [] = "test";
  if (on)
    {
      lk = fopen("/sys/power/wake_lock", "w");
      if (lk==NULL)
        {
          write_to_log(LOG_ERR,"Failed to open wake_lock file: %s",strerror(errno));
          return -1;
        };
      if (fwrite(chars, sizeof(char), 5, lk)==0) print_error("fwrite() wake_lock failed");
    }
  else
    {
      lk = fopen("/sys/power/wake_unlock", "w");
      if (lk==NULL)
        {
          print_error("fopen() wake_unlock fail");
          return -1;
        };
      if (fwrite(chars, sizeof(char), 5, lk)==0) print_error("fwrite() wake_unlock failed");
    };
  if (fclose(lk)==EOF) print_error("fclose() failed");
#endif
  return 0;
}

int server_playback_status (void)
{
  int status;
  xmmsc_result_t *status_res = xmmsc_playback_status(pbconnection);
  xmmsv_t *status_val = result_is_error(status_res, false);
  xmmsv_get_int(status_val, &status);
  write_to_log(LOG_DBG,"Obtained current playback status: %d",status);
  xmmsc_result_unref(status_res);
  return status;
}

void set_playback (int status, bool tickle)
{
  switch (status) {
    case XMMS_PLAYBACK_STATUS_STOP: result_is_error(xmmsc_playback_stop(pbconnection), true);
       break;
    case XMMS_PLAYBACK_STATUS_PAUSE: result_is_error(xmmsc_playback_pause(pbconnection), true);
      break;
    case XMMS_PLAYBACK_STATUS_PLAY: result_is_error(xmmsc_playback_start(pbconnection), true);
      break;
    default:
      write_to_log(LOG_WARN,"Trying to set invalid playback type %d",status);
      break;
    };
  set_volume();
  if (tickle && status) result_is_error(xmmsc_playback_tickle(pbconnection), true);
}

#ifdef __EMU__
// NOTE: This function is used only for debug
void dict_foreach (const char *name, xmmsv_t *value, void *)
{
  int t;
  const char *s;
  cout<<"dict_foreach: '"<<name<<"', value type: ";
  switch (xmmsv_get_type(value)) {
  case XMMSV_TYPE_NONE:
      break;
  case XMMSV_TYPE_ERROR:
      break;
  case XMMSV_TYPE_INT32: cout<<"INT32, value: ";
      xmmsv_get_int(value, &t);
      cout<<t<<endl;
      break;
  case XMMSV_TYPE_STRING: cout<<"STRING, value: ";
      xmmsv_get_string(value, &s);
      cout<<s<<endl;
      break;
  default: cout<<"Unknown."<<endl;
      break;
  };
  return;
}
#endif
