/*
 *   fork.cpp
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
#include <xmmsclient/xmmsclient-glib.h>
#include "fork.hpp"

GMainLoop *mloop;
pthread_mutex_t pipe_mutex;
xmmsc_result_t *playlist_loaded_res, *current_pos_res, *playback_status_res, *playlist_changed_res,
  *reader_status_res, *playtime_res;
xmmsc_connection_t *fork_connection;
void *gmain;
extern sem_t *reader_sem;
extern int bcast_pipe[2];

void msg_to_pipe (const pipe_msg *msg)
{
  pthread_mutex_lock(&pipe_mutex);
  write(bcast_pipe[1], msg, sizeof(pipe_msg));
  pthread_mutex_unlock(&pipe_mutex);
  return;
}

int playlist_loaded_handler (xmmsv_t *value, void *)
{
  pipe_msg msg;
  const char *s;
  msg.type=PLAYLIST_LOADED;
  xmmsv_get_string(value, &s);
  strcpy(msg.name, s);
  msg_to_pipe(&msg);
  return 1;
}

int current_pos_handler (xmmsv_t *value, void *)
{
  pipe_msg msg;
  xmmsv_t *dict_entry=NULL;
  const char *s;
  msg.type=CURRENT_POS;
  xmmsv_dict_get (value, "position", &dict_entry);
  xmmsv_get_int (dict_entry, &msg.current_position);
  xmmsv_dict_get (value, "name", &dict_entry);
  xmmsv_get_string(dict_entry, &s);
  strcpy(msg.name, s);
  msg_to_pipe(&msg);
  return 1;
}

int playback_status_handler (xmmsv_t *value, void *)
{
  pipe_msg msg;
  msg.type=PLAYBACK_STATUS;
  xmmsv_get_int (value, &msg.playback_status);
  msg_to_pipe(&msg);
  return 1;
}

int playlist_changed_handler (xmmsv_t *value, void *)
{
  pipe_msg msg;
  xmmsv_t *dict_entry=NULL;
  const char *s;
  msg.type=PLAYLIST_CHANGED;
  xmmsv_dict_get (value, "type", &dict_entry);
  xmmsv_get_int (dict_entry, &msg.pc.type);
  xmmsv_dict_get (value, "name", &dict_entry);
  xmmsv_get_string(dict_entry, &s);
  strcpy(msg.name, s);
  switch (msg.pc.type) {
    case XMMS_PLAYLIST_CHANGED_ADD:
      xmmsv_dict_get (value, "id", &dict_entry);
      xmmsv_get_int (dict_entry, &msg.pc.id);
      xmmsv_dict_get (value, "position", &dict_entry);
      xmmsv_get_int (dict_entry, &msg.pc.position);
      break;
    case XMMS_PLAYLIST_CHANGED_CLEAR:
      break;
    case XMMS_PLAYLIST_CHANGED_REMOVE:
      xmmsv_dict_get (value, "position", &dict_entry);
      xmmsv_get_int (dict_entry, &msg.pc.position);
      break;
    default:
      break;
    };
  //xmmsv_dict_foreach(value, dict_foreach, NULL);
  msg_to_pipe(&msg);
  return 1;
}

int reader_status_handler (xmmsv_t *value, void *)
{
  int reader_status;
  xmmsv_get_int(value, &reader_status);
  if (reader_status==XMMS_MEDIAINFO_READER_STATUS_RUNNING)
    {
      pipe_msg msg;
      msg.type=READER_STATUS;
      msg_to_pipe(&msg);
    }
  else sem_post(reader_sem);
  return 1;
}

int playtime_handler (xmmsv_t *value, void *)
{
  static unsigned int count;
  pipe_msg msg;
  msg.type=PLAYTIME;
  if (!xmmsv_get_int(value, &msg.playtime))
      return 1;
  count++;
  if (count >10)
   {
     count=0;
     msg_to_pipe(&msg);
   };
  return 1;
}

void fork_sigterm_handler (int)
{
  g_main_loop_quit (mloop);
  xmmsc_result_disconnect (playlist_loaded_res);
  xmmsc_result_unref (playlist_loaded_res);
  xmmsc_result_disconnect (current_pos_res);
  xmmsc_result_unref (current_pos_res);
  xmmsc_result_disconnect (playback_status_res);
  xmmsc_result_unref (playback_status_res);
  xmmsc_result_disconnect (reader_status_res);
  xmmsc_result_unref (reader_status_res);
  xmmsc_result_disconnect (playtime_res);
  xmmsc_result_unref (playtime_res);
  xmmsc_mainloop_gmain_shutdown(fork_connection, gmain);
  pthread_mutex_destroy(&pipe_mutex);
  xmmsc_unref(fork_connection);
  g_main_loop_unref (mloop);
  return;
}

void fork_main (void)
{
  signal(SIGTERM, fork_sigterm_handler);
  pthread_mutex_init(&pipe_mutex, NULL);
  fork_connection = xmmsc_init("PBXMMS2client_fork");
  if (fork_connection==NULL) exit(EXIT_FAILURE);
  char *xmms_env = getenv("XMMS_PATH");
  if (!xmmsc_connect(fork_connection, xmms_env)) exit(EXIT_FAILURE);
  free(xmms_env);
  gmain = xmmsc_mainloop_gmain_init(fork_connection);
  mloop = g_main_loop_new(NULL, false);
  playlist_loaded_res = xmmsc_broadcast_playlist_loaded(fork_connection);
  xmmsc_result_notifier_set(playlist_loaded_res, playlist_loaded_handler, NULL);
  current_pos_res = xmmsc_broadcast_playlist_current_pos(fork_connection);
  xmmsc_result_notifier_set(current_pos_res, current_pos_handler, NULL);
  playback_status_res = xmmsc_broadcast_playback_status(fork_connection);
  xmmsc_result_notifier_set(playback_status_res, playback_status_handler, NULL);
  playlist_changed_res = xmmsc_broadcast_playlist_changed(fork_connection);
  xmmsc_result_notifier_set(playlist_changed_res, playlist_changed_handler, NULL);
  reader_status_res = xmmsc_broadcast_mediainfo_reader_status(fork_connection);
  xmmsc_result_notifier_set(reader_status_res, reader_status_handler, NULL);
  playtime_res = xmmsc_signal_playback_playtime(fork_connection);
  xmmsc_result_notifier_set(playtime_res, playtime_handler, NULL);
  g_main_loop_run (mloop);
  return;
}
