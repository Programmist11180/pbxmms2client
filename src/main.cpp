/*
 *   main.cpp
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

#include <semaphore.h>
#include <signal.h>
#include <iostream>
#include "client_screen.hpp"
#include "fork.hpp"
#include "log.hpp"
#include "settings.hpp"
#include "xmms.hpp"

xmmsc_connection_t *pbconnection;
settings *pbxmms_settings;
client_screen *clsc;
sem_t *fork_sem, *reader_sem;
int bcast_pipe[2], playback_status;

pid_t fork_pid = 0;

using namespace std;

const int TRACK_PLAY_PAUSE_num=1;
const int TRACK_STOP_num= 2;
const int SETTINGS_num = 3;
const int SERVER_num = 4;
const int STATS_num = 41;
const int PLUGINS_num = 42;
const int REHASH_num = 43;
const int HELP_num = 5;
const int ABOUT_num = 51;
const int EXIT_AND_HALT_num = 6;
const int EXIT_num = 7;

// WARNING: last element must be an empty element!
static imenu server_menu [] = {
  {ITEM_HEADER, PBXMMS2CLIENT_num, PBXMMS2CLIENT, NULL},
  {ITEM_ACTIVE, STATS_num, "Stats", NULL},
  {ITEM_ACTIVE, PLUGINS_num, "List loaded plugins", NULL},
  {ITEM_ACTIVE, REHASH_num, "Rehash the medialib", NULL},
  {0, 0, NULL, NULL}
};
static imenu help_menu [] =
{
  {ITEM_HEADER, PBXMMS2CLIENT_num, PBXMMS2CLIENT, NULL},
  {ITEM_ACTIVE, ABOUT_num, "About", NULL},
  {0, 0, NULL, NULL}
};
static imenu main_menu [] = {
  {ITEM_HEADER, PBXMMS2CLIENT_num, PBXMMS2CLIENT, NULL},
  {ITEM_ACTIVE, TRACK_PLAY_PAUSE_num, "Start/pause playback", NULL},
  {ITEM_ACTIVE, TRACK_STOP_num, "Stop playback", NULL},
  {ITEM_ACTIVE, SETTINGS_num, "Settings", NULL},
  {ITEM_SUBMENU, SERVER_num, "Server", server_menu},
  {ITEM_SUBMENU, HELP_num, "Help", help_menu},
  {ITEM_ACTIVE, EXIT_AND_HALT_num, "Exit and shutdown server", NULL},
  {ITEM_ACTIVE, EXIT_num, "Exit", NULL},
  {0, 0, NULL, NULL}
};

static void rehash_handler (int b)
{
  if (b==1) result_is_error(xmmsc_medialib_rehash(pbconnection, 0), true);
  return;
}

static void main_menu_handler (int index)
{
  switch (index) {
    case ABOUT_num: Message(ICON_INFORMATION, PBXMMS2CLIENT,
                            "PBXMMS2client v0.006 - graphical client for XMMS2 player on PocketBook E-Ink readers\n"
                            "Copyright © 2014-2020 Programmist11180\n"
                            "Website https://github.com/Programmist11180/pbxmms2client\n"
                            "The PBXMMS2client is licensed under the GPL 3\n", long_timeout);
      break;
    case SETTINGS_num: pbxmms_settings->change_settings();
      break;
    case EXIT_AND_HALT_num: server_halt();
      CloseApp();
      break;
    case EXIT_num: CloseApp();
      break;
    case STATS_num: server_stats();
      break;
    case PLUGINS_num: server_list_plugins();
      break;
    case REHASH_num:
      Dialog(ICON_QUESTION, PBXMMS2CLIENT, "Rehash the medialib?", "Yes", "No", rehash_handler);
      break;
    case TRACK_PLAY_PAUSE_num: if (playback_status==XMMS_PLAYBACK_STATUS_PLAY)
        set_playback(XMMS_PLAYBACK_STATUS_PAUSE);
      else set_playback(XMMS_PLAYBACK_STATUS_PLAY);
      break;
    case TRACK_STOP_num: set_playback(XMMS_PLAYBACK_STATUS_STOP);
      break;
    case -1:
      break;
    default: write_to_log(LOG_WARN, "main_menu_handler(): unknown menu namber: %d", index);
      break;
    };
  return;
}

int inkview_handler (int event_type, int param_one, int param_two)
{
  write_to_log(LOG_DBG, "iv_handler() - type: %d (%s), par1: 0x%x, par2: 0x%x", event_type, iv_evttype(event_type), param_one, param_two);
  switch (event_type) {
    case EVT_INIT:
      clsc = new client_screen;
      write_to_log(LOG_DBG, "Object 'client_screen' created.");
      sem_post(fork_sem);
      clsc->tracks_list->onFocusedWidgetChanged.emit(NULL);
      write_to_log(LOG_INFO,"%s started successfully.",PBXMMS2CLIENT);
      break;
    case EVT_ORIENTATION: write_to_log(LOG_WARN,"Change orientation not implemented.");
      break;
    case EVT_POINTERLONG: if (clsc->tracks_list->isFocused() && clsc->tracks_list->itemsCount())
          clsc->track_clicked(NULL, 0);
      break;
    case EVT_KEYPRESS:
      switch (param_one) {
        // BUG: Don't call OpenMenu at position x=0  y=0  ! It crashes the application
        case KEY_BACK: OpenMenu(main_menu, 1, 10, 10, main_menu_handler);
          break;
        case KEY_OK:
        case KEY_MENU:
          clsc->key_screen_handler (param_one);
          break;
        case KEY_NEXT: pbxmms_settings->vol_level += pbxmms_settings->vol_inc;
          set_volume();
          break;
        case KEY_PREV: pbxmms_settings->vol_level -= pbxmms_settings->vol_inc;
          set_volume();
          break;
        default:
          break;
        };
      break;
    case EVT_EXIT:
      kill(fork_pid, SIGTERM);
      sem_close(fork_sem);
      xmmsc_unref(pbconnection);
      if (clsc) delete clsc;
      write_to_log(LOG_INFO,"%s shutdown.",PBXMMS2CLIENT);
      log_end();
      if (pbxmms_settings) delete pbxmms_settings;
      set_wake_lock(false);
      break;
    case EVT_CONFIGCHANGED: clsc->update();
      break;
    default: clsc->handle (event_type, param_one, param_two);
      break;
    };
  return 0;
}

void main_segfault_handler (int)
{
  kill(fork_pid, SIGKILL);
  signal(SIGSEGV, SIG_DFL);
  if (pbxmms_settings)
      if (pbxmms_settings->log_enabled)
       {
          write_to_log(LOG_ERR, "Segmentation fault in main process!");
          log_end();
       };
}

void main_sigabort_handler (int)
{
  kill(fork_pid, SIGTERM);
  signal(SIGABRT, SIG_DFL);
  if (pbxmms_settings)
      if (pbxmms_settings->log_enabled)
       {
          write_to_log(LOG_ERR, "Main process aborted!");
          log_end();
       };
}

void main_main (void)
{
  if (signal(SIGSEGV,main_segfault_handler)==SIG_ERR)
       cerr<<"Failed to set SIGSEGV handler!"<<endl;
  if (signal(SIGABRT,main_sigabort_handler)==SIG_ERR)
       cerr<<"Failed to set SIGABRT handler!"<<endl;
  OpenScreen();
  pbxmms_settings = new settings;
  if (pbxmms_settings->log_enabled)
      if (log_init()==-1)
      {
          pbxmms_settings->log_enabled = 0;
          Message(ICON_ERROR,PBXMMS2CLIENT,"Log thread initialization failed!",200);
      };
  write_to_log(LOG_INFO,"%s startup.",PBXMMS2CLIENT);
  write_to_log(LOG_INFO,"Model: %s; Hardware: %s; Software: %s",GetDeviceModel(),GetHardwareType(),GetSoftwareVersion());
  write_to_log(LOG_DBG,"Screen: width: %d, heigth: %d",ScreenWidth(),ScreenHeight());
  if ( (pbconnection = xmmsc_init("PBXMMS2client_main"))==NULL)
  {
      write_to_log(LOG_ERR, "Connection initialization  failed: not enough memory.");
      exit(EXIT_FAILURE);
  };
  server_run();
  change_output();
  InkViewMain(inkview_handler);
}

int main (int , char **)
{
  if (pipe(bcast_pipe)==-1) {
      cerr<<"Failed to create a pipe: "<<strerror(errno)<<endl;
      exit(EXIT_FAILURE);
    };
  fork_sem = sem_open("fork_sem", O_CREAT, 0644, 0);
  if (fork_sem==SEM_FAILED) {
      cerr<<"Failed to create a fork semaphore: "<<strerror(errno)<<endl;
      exit(EXIT_FAILURE);
    };
  reader_sem = sem_open("bcast_sem", O_CREAT, 0644, 0);
  if (reader_sem==SEM_FAILED) {
      cerr<<"Failed to create a reader semaphore: "<<strerror(errno)<<endl;
      exit(EXIT_FAILURE);
    };
  /* FAIL: InkViewMain() and g_main_loop_run() conflicts among themselves if runs in one process,
   * so we need to use a fork() */
  switch (fork_pid = fork()) {
    case 0: sem_wait(fork_sem);
      fork_main();
      break;
    case -1: cerr<<"Failed to create a fork: "<<strerror(errno)<<endl;
      exit(EXIT_FAILURE);
    default: main_main();
      break;
    };
  exit(EXIT_SUCCESS);
}
