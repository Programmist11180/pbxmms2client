/*
 *   client_screen.hpp
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

#ifndef CLIENT_SCREEN_HPP
#define CLIENT_SCREEN_HPP

#include <string>
#include <pblabel.h>
#include <pbfilechooser.h>
//#include <pbpagedlistbox.h>
#include <pblistbox.h>

using std::string;

const int PBXMMS2CLIENT_num = 0;

class client_screen: public PBWidget
{
public:
  client_screen (void);
  ~client_screen ();

  PBListBox *tracks_list;

  void track_clicked (PBListBox *, int);
  void key_screen_handler (int key);
private:
  // FAIL: PBPagedListBox is broken in pbtk  0.2.1
  PBListBox *playlists_list;
  //PBPagedListBox *tracks_list;
  PBLabel *tracks_title, *playtime, *battery;
  ifont *font, *active_font;
  string last_dir;

  // handlers
  static void track_add_handler (int isok, PBFileChooser *pbfc);
  static void track_delete_handler (int isok);
  static void screen_menus_handler (int index);
  static void playlist_create_handler (char *name);
  static void playlist_clear_handler (int ok);
  static void playlist_delete_handler (int ok);
  static void pos_selector_handler (int page);
  static void dir_add_handler (char *path);
  // screen functions
  void set_widgets (void);
  void show_batt_power (PBWidget *);
  void show_playtime (int ptime);
  void mark_active_track (short int track);
  void mark_active_playlist (short int playlist);
  void playlist_show (PBWidget *);
  void track_information (void);
  // other functions
  void load_all_playlists (void);
  void playlist_load (int pnum);
  void tracks_clear (int pnum);
  void track_form_info (unsigned int track_num);
  short int track_get_info (int track_id, int playlist_num);
  static void *broadcast_handler (void *);
};

extern client_screen *clsc;

#endif
