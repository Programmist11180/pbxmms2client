/*
 *   fork.hpp
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

#ifndef FORK_HPP
#define FORK_HPP

enum MSG_TYPE {PLAYLIST_LOADED, CURRENT_POS, PLAYBACK_STATUS, PLAYLIST_CHANGED,
              READER_STATUS, PLAYTIME};

const int MAX_PLAYLIST_NAME = 512;

struct _playlist_changed {
  int id, position, type;
};

struct pipe_msg
{
  MSG_TYPE type;
  char name[MAX_PLAYLIST_NAME];
  union {
    int current_position;
    int playback_status;
    int playtime;
    struct _playlist_changed pc;
  };
};

void fork_main (void);

#endif // FORK_HPP
