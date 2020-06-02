/*
 *   xmms.hpp
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

#ifndef COMMON_HPP
#define COMMON_HPP

#include <xmmsclient/xmmsclient.h>

const int long_timeout = 60000;

extern xmmsc_connection_t *pbconnection;

// result
//void print_error (const char *err_text);
xmmsv_t *result_is_error (xmmsc_result_t *res, bool unref = false);
bool value_is_error (xmmsv_t *value);
// server
void server_run (void);
void server_halt (void);
void server_stats (void);
void server_list_plugins (void);
int server_playback_status (void);
void set_playback (int status, bool tickle = false);
void change_output (void);
// reader
short set_wake_lock (bool on);

// debug
#ifdef __EMU__
void dict_foreach (const char *name, xmmsv_t *value, void *);
#endif

#endif // COMMON_HPP
