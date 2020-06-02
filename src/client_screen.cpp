/*
 *   client_screen.cpp
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

#include <xmmsclient/xmmsclient.h>
#include <semaphore.h>
#include <vector>
#include <inkview.h>
#include "client_screen.hpp"
#include "fork.hpp"
#include "log.hpp"
#include "settings.hpp"
#include "xmms.hpp"

using std::vector;

const int CREATE_PLAYLIST_num = 41;
const int CLEAR_PLAYLIST_num = 42;
const int DELETE_PLAYLIST_num = 43;
static imenu playlist_menu [] = {
  {ITEM_HEADER, PBXMMS2CLIENT_num, PBXMMS2CLIENT, NULL},
  {ITEM_ACTIVE, CREATE_PLAYLIST_num, "Create playlist", NULL},
  {ITEM_ACTIVE, CLEAR_PLAYLIST_num, "Clear playlist", NULL},
  {ITEM_ACTIVE, DELETE_PLAYLIST_num, "Delete playlist", NULL},
  {0, 0, NULL, NULL}
};

const int ADD_FILE_num = 51;
const int ADD_DIR_num = 52;
const int TRACK_DELETE_num = 32;
const int TRACK_INFO_num = 33;
const int GO_TO_num =34;
const int GO_TO_ACTIVE_TRACK_num = 35;
const int GO_TO_POS_num = 36;
static imenu track_menu [] = {
  {ITEM_HEADER, PBXMMS2CLIENT_num, PBXMMS2CLIENT, NULL},
  {ITEM_ACTIVE, TRACK_INFO_num, "Track information", NULL},
  {ITEM_ACTIVE, GO_TO_ACTIVE_TRACK_num, "Go to current track", NULL},
  {ITEM_ACTIVE, GO_TO_POS_num, "Go to track №...", NULL},
  {ITEM_ACTIVE, ADD_FILE_num, "Add file", NULL},
  {ITEM_ACTIVE, ADD_DIR_num, "Add directory", NULL},
  {ITEM_ACTIVE, TRACK_DELETE_num, "Delete track", NULL},
  {0, 0, NULL, NULL}
};

const char str_empty [] = "";

struct track_item
{
  xmmsc_result_t *track_result;
  const char *title, *artist, *album, *url, *filename, *genre, *date, *sample_format, *comment, *performer, *composer, *copyright, *original_artist;
  int id, duration, tracknr, bitrate, size, samplerate, channels;
};

struct playlist_item
{
  vector <track_item> tracks;
  xmmsc_result_t *playlist_result;
  const char *name;
};

extern client_screen *clsc;
extern int playback_status;
static vector <playlist_item> playlists;
static xmmsc_result_t *playlists_result;
static int active_track = -1, active_playlist = -1, prev_playtime;
pthread_t bcast_thread;

short int find_playlist (const char *name)
{
  for (unsigned short int pl=0; pl<playlists.size(); pl++)
      if (strcmp(name, playlists.at(pl).name)==0) return pl;
  return -1;
}

short int playlist_current_active ()
{
  xmmsc_result_t *res = xmmsc_playlist_current_active(pbconnection);
  xmmsv_t *val = result_is_error(res, false);
  if (val==NULL) return -1;
  const char *name;
  short int pls;
  if (xmmsv_get_string(val, &name)) pls=find_playlist(name);
  else pls= -1;
  write_to_log(LOG_DBG,"Obtained current active playlist: %s", name);
  xmmsc_result_unref(res);
  return pls;
}

void playlist_current_pos ()
{
  xmmsc_result_t *res = xmmsc_playlist_current_pos(pbconnection, playlists.at(active_playlist).name);
  xmmsv_t *val = result_is_error(res, false);
  xmmsv_t *dict_entry = NULL;
  if (!xmmsv_dict_get (val, "position", &dict_entry) || !xmmsv_get_int (dict_entry, &active_track))
      active_track = -1;
  write_to_log(LOG_DBG,"Obtained current position: %d", active_track);
  xmmsc_result_unref(res);
}

void *client_screen::broadcast_handler (void *)
{
  pipe_msg msg;
  extern int bcast_pipe[2];
  extern sem_t *reader_sem;
  short int pl=0;
  for (;;)
    {
      read(bcast_pipe[0], &msg, sizeof(pipe_msg));
      switch (msg.type)
       {
        case PLAYLIST_LOADED:
          clsc->mark_active_track(-1);
          clsc->mark_active_playlist(find_playlist(msg.name));
          break;
        case CURRENT_POS:
          write_to_log(LOG_DBG,"broadcast CURRENT_POS, playlist: '%s'; position: %d",msg.name,msg.current_position);
          pl=find_playlist(msg.name);
          clsc->mark_active_playlist(pl);
          if (pl==-1) write_to_log(LOG_WARN,"Playlist '%s' not found.",msg.name);
          else clsc->mark_active_track(msg.current_position);
          clsc->show_batt_power(NULL);
          prev_playtime=0;
          break;
        case PLAYBACK_STATUS:
          playback_status=msg.playback_status;
          switch (playback_status) {
            case XMMS_PLAYBACK_STATUS_PLAY: set_wake_lock(true);
              break;
            case XMMS_PLAYBACK_STATUS_STOP:
            case XMMS_PLAYBACK_STATUS_PAUSE: set_wake_lock(false);
              break;
            default:
              write_to_log(LOG_WARN,"Unknown playback status: %d",playback_status);
              break;
            };
          break;
        case PLAYLIST_CHANGED:
          write_to_log(LOG_DBG,"broadcast PLAYLIST_CHANGED, name: %s; type: %d,",msg.name,msg.pc.type);
          switch (msg.pc.type) {
            case XMMS_PLAYLIST_CHANGED_ADD:
              pl=find_playlist(msg.name);
              playlists.at(pl).tracks.push_back(track_item());
              if (clsc->track_get_info(msg.pc.id, pl)==-1)
                {
                  playlists.at(pl).tracks.pop_back();
                  break;
                };
              clsc->track_form_info(playlists.at(pl).tracks.size()-1);
              clsc->tracks_list->update();
              break;
            case XMMS_PLAYLIST_CHANGED_INSERT:
              write_to_log(LOG_ERR,"broadcast XMMS_PLAYLIST_CHANGED_INSERT not implemented.");
              break;
            case XMMS_PLAYLIST_CHANGED_SHUFFLE:
              break;
            case XMMS_PLAYLIST_CHANGED_REMOVE:
              if (clsc->playlists_list->getSelectedIndex()==active_playlist)
                  clsc->tracks_list->erase(clsc->tracks_list->getItem(msg.pc.position));
              clsc->update();
              pl=find_playlist(msg.name);
              if (msg.pc.position<active_track && (pl==active_playlist))
                  --active_track;
              if (msg.pc.position==active_track && (pl==active_playlist))
                  active_track=-1;
              xmmsc_result_unref(playlists.at(pl).tracks.
                  at(msg.pc.position).track_result);
              playlists.at(pl).tracks.erase (playlists.at
                      (pl).tracks.begin()+msg.pc.position);
              break;
            case XMMS_PLAYLIST_CHANGED_CLEAR:
              pl=find_playlist(msg.name);
              clsc->tracks_clear(pl);
              if (active_playlist==clsc->playlists_list->getSelectedIndex()) clsc->tracks_list->clear();
              clsc->tracks_list->update();
              break;
            case XMMS_PLAYLIST_CHANGED_MOVE:
              break;
            case XMMS_PLAYLIST_CHANGED_SORT:
              break;
            case XMMS_PLAYLIST_CHANGED_UPDATE:
              break;
            default:
              //clsc->update();
              write_to_log(LOG_WARN,"Unknown playlist broadcast type: %d",msg.pc.type);
              break;
            };
          break;
        case READER_STATUS: sem_wait(reader_sem);
          break;
        case PLAYTIME: clsc->show_playtime(msg.playtime);
          break;
        default:
          write_to_log(LOG_WARN,"Unknown broadcast type: %d",msg.type);
          break;
       };
    };
  return NULL;
}

client_screen::client_screen (void): PBWidget ("mscreen", NULL)
{
  pthread_create(&bcast_thread, NULL, client_screen::broadcast_handler, NULL);
  SetOrientation(pbxmms_settings->orientation);
  font = OpenFont(DEFAULTFONT, pbxmms_settings->font_size, 1);
  /*if (font==NULL) {
      print_error("Failed to open font!");
      font = OpenFont(DEFAULTFONT, pbxmms_settings->font_size, 1);
    };*/
  active_font = OpenFont(DEFAULTFONTB, pbxmms_settings->font_size, 1);
  /*if (active_font==NULL) {
      print_error("Failed to open font!");
      active_font = OpenFont(DEFAULTFONTB, pbxmms_settings->font_size, 1);
    };*/
  this->setWidgetFont(this->font);
  tracks_title = new PBLabel("tracks_title", this);
  tracks_title->setCanBeFocused(false);
  tracks_title->setText("         Title      -           Album        -        Artist                                     ");
  this->addWidget(tracks_title);
  tracks_list = new PBListBox ("tracks_list", this);
  this->addWidget(tracks_list);
  playlists_list = new PBListBox("playlists_list", this);
  this->addWidget(playlists_list);
  playtime = new PBLabel("", this);
  playtime->setCanBeFocused(false);
  this->addWidget(playtime);
  battery = new PBLabel("", this);
  battery->setCanBeFocused(false);
  this->addWidget(battery);
  this->set_widgets();
  playlists_list->onFocusedWidgetChanged.connect(sigc::mem_fun(this, &client_screen::playlist_show));
  load_all_playlists();
  last_dir.assign("/mnt");
  mark_active_playlist(playlist_current_active());
  playlist_current_pos();
  mark_active_track(active_track);
  playback_status = server_playback_status();
  //playlists_list->setFocused(true);
  tracks_list->setFocused(true);
  playlists_list->selectItem(active_playlist);
  tracks_list->selectItem(active_track);
  onFocusedWidgetChanged.connect(sigc::mem_fun(this, &client_screen::show_batt_power));
  tracks_list->onFocusedWidgetChanged.connect(sigc::mem_fun(this, &client_screen::show_batt_power));
}

client_screen::~client_screen ()
{
  pthread_cancel(bcast_thread);
  while (!playlists.empty()) {
      client_screen::tracks_clear (playlists.size()-1);
      xmmsc_result_unref(playlists.back().playlist_result);
      playlists.pop_back();
    };
  if (playlists_result) xmmsc_result_unref(playlists_result);
  delete tracks_title;
  delete tracks_list;
  delete playlists_list;
  delete battery;
  CloseFont(font);
  CloseFont(active_font);
}

void client_screen::set_widgets (void)
{
  switch (pbxmms_settings->orientation) {
    case ROTATE0: this->setSize(0, 0, ScreenWidth(), ScreenHeight());
      tracks_title->setSize(1, 5, this->w()-10, 25);
      tracks_list->setSize(1, tracks_title->h()+5, this->w()-10, this->h()-150-tracks_title->h());
      playlists_list->setSize(1, tracks_list->y()+tracks_list->h()+5, this->w()/2,
                             this->h()-tracks_list->y()-tracks_list->h()-10);
      playtime->setSize(playlists_list->x()+playlists_list->w()+5,tracks_list->y()+
                       tracks_list->h()+5,220,25);
      battery->setSize(playlists_list->x()+playlists_list->w()+5,playtime->y()+playtime->h()+5,
                       140,25);
      break;
    case ROTATE90:  // FIXME: add support for all orientations
    case ROTATE180:
    case ROTATE270:
      write_to_log(LOG_WARN,"Orientation %d not implemented.",pbxmms_settings->orientation);
      break;
    default:
      write_to_log(LOG_WARN,"Unknown orientation: %d, set back to default.",pbxmms_settings->orientation);
      pbxmms_settings->orientation = ROTATE0;
      client_screen::set_widgets();
      return;
   };
  FullUpdate();   //FullUpdateHQ();    // NOTE: what a difference between FullUpdate() and FullUpdateHQ() ?
}

void client_screen::show_batt_power (PBWidget *)
{
  string str1;
  char str2[50];
  sprintf(str2,"Battery: %d%%",GetBatteryPower());
  str1.assign(str2);
  clsc->battery->setText(str1);
  clsc->battery->update();
}

void client_screen::show_playtime (int ptime)
{
  string str1;
  char str2[50];
  if (ptime - prev_playtime < 5000) return;    // FIXME: add way to change playtime update interval in settings window
  else prev_playtime = ptime;
  if (GetDialogShow()) return;
  sprintf(str2,"%2d m : %2d s / %2d m : %2d s",div(div(ptime,1000).quot,60).quot,div(div(ptime,1000).quot,60).rem,
          div(div(playlists.at(active_playlist).tracks.at(active_track).duration,1000).quot,60).quot,
          div(div(playlists.at(active_playlist).tracks.at(active_track).duration,1000).quot,60).rem  );
  str1.assign(str2);
  clsc->playtime->setText(str1);
  clsc->playtime->update();
}

void client_screen::track_add_handler (int isok, PBFileChooser *pbfc)
{
  if (isok) {
      char *path = new char[pbfc->getPath().length()+10];
      sprintf(path, "file://%s", pbfc->getPath().c_str());
      result_is_error(xmmsc_playlist_add_url
                      (pbconnection, playlists.at(clsc->playlists_list->getSelectedIndex()).name, path), true);
      delete [] path;
    };
  clsc->last_dir.assign(pbfc->getPath().c_str(), pbfc->getPath().find_last_of('/'));
  clsc->update();
  return;
}

void client_screen::track_delete_handler (int isok)
{
  if (isok==1) {
      result_is_error(xmmsc_playlist_remove_entry(pbconnection,
          playlists.at(clsc->playlists_list->getSelectedIndex()).name, clsc->tracks_list->getSelectedIndex()));
    };
}

void client_screen::track_information (void)
{
  track_item & t = playlists.at
      (this->playlists_list->getSelectedIndex()).tracks.at(this->tracks_list->getSelectedIndex());
  char *inf = new char [1024];
  sprintf(inf, "Title: %s\nAlbum: %s\nArtist: %s\nPerformer: %s\nGenre: %s\nDate: %s\nOriginal artist: %s\nComposer: %s\nCopyright: %s\n"
        "Comment: %s\nDuration: %d mn %d s\n"
        "Track number: %d\nBitrate: %d kbps\nSamplerate: %d Hz\nSample format: %s\n"
        "Channels: %d\nSize: %d,%d MB\n", t.title, t.album, t.artist, t.performer, t.genre, t.date, t.original_artist, t.composer, t.copyright,
          t.comment,
        div(t.duration,60000).quot, div(div(t.duration,60000).rem,1000).quot, t.tracknr, t.bitrate/1000,
        t.samplerate, t.sample_format, t.channels, div((t.size/1000),1000).quot, div((t.size/1000),1000).rem);
  Message(ICON_INFORMATION, PBXMMS2CLIENT, inf, long_timeout);
  delete [] inf;
}

void client_screen::pos_selector_handler (int page)
{
  if (page==99999) clsc->tracks_list->selectItem(clsc->tracks_list->itemsCount()-1);
  else if (page>clsc->tracks_list->itemsCount()) Message(ICON_WARNING, PBXMMS2CLIENT, "Position is outside of the playlist.", 10000);
  else clsc->tracks_list->selectItem(page-1);
  return;
}

void client_screen::dir_add_handler (char *path)
{
  if (path)
    {
      char *p = new char[strlen(path)+10];
#ifdef __EMU__
      sprintf(p, "file://%s", strstr(path, "ext2")+4);
#else
      sprintf(p, "file://%s", path);
#endif
      result_is_error(xmmsc_playlist_radd
                      (pbconnection, playlists.at(clsc->playlists_list->getSelectedIndex()).name, p), true);
      delete [] p;
    };
  clsc->update();
  return;
}

void client_screen::screen_menus_handler (int index)
{
  char *p;
  int bsize = 1024;
  switch (index) {
    case CREATE_PLAYLIST_num: p = new char[bsize];
      strcpy(p, "new_playlist");
      OpenKeyboard(PBXMMS2CLIENT, p, bsize, KBD_NORMAL | KBD_SENDKEYBOARDSTATEEVENTS //|
                   /*KBD_PASSEVENTS*/, clsc->playlist_create_handler);
      break;
    case CLEAR_PLAYLIST_num: p = new char[bsize];
      sprintf(p, "Clear playlist \"%s\" ?", playlists.at(clsc->playlists_list->getSelectedIndex()).name);
      Dialog(ICON_QUESTION, PBXMMS2CLIENT, p, "Yes", "No", clsc->playlist_clear_handler);
      delete [] p;
      break;
    case DELETE_PLAYLIST_num: p = new char[bsize];
      sprintf(p, "Delete playlist \"%s\" ?", playlists.at(clsc->playlists_list->getSelectedIndex()).name);
      Dialog(ICON_QUESTION, PBXMMS2CLIENT, p, "Yes", "No", clsc->playlist_delete_handler);
      delete [] p;
      break;
    case ADD_FILE_num: OpenFileChooser("Add files", clsc->last_dir.data(), "*\n*.mp3\n*.flac\n*.ogg",
                                        PBFileChooser::PBFC_OPEN, (pb_dialoghandler) track_add_handler);
      break;
    case TRACK_DELETE_num: if (clsc->tracks_list->itemsCount()) {
      p = new char[bsize];
      sprintf(p, "Delete track \"%s\" ?", playlists.at(clsc->playlists_list->getSelectedIndex()).
              tracks.at(clsc->tracks_list->getSelectedIndex()).filename);
      Dialog(ICON_QUESTION, PBXMMS2CLIENT, p, "Yes", "No", client_screen::track_delete_handler);
      delete [] p;
        }
      else Message(ICON_WARNING, PBXMMS2CLIENT, "No track selected!", long_timeout);
      break;
    case TRACK_INFO_num: if (clsc->tracks_list->itemsCount()) clsc->track_information();
      else Message(ICON_WARNING, PBXMMS2CLIENT, "No track selected!", long_timeout);
      break;
    case GO_TO_ACTIVE_TRACK_num: if (active_playlist==-1 || active_track==-1) {
          Message(ICON_WARNING, PBXMMS2CLIENT, "Current track is absent.", long_timeout);
          break;
        };
      if (clsc->playlists_list->getSelectedIndex()!=active_playlist)
        clsc->playlists_list->selectItem(active_playlist);
      clsc->tracks_list->selectItem(active_track);
      break;
    case GO_TO_POS_num: OpenPageSelector(clsc->pos_selector_handler);
      break;
    case ADD_DIR_num: static char buf[512];
      OpenDirectorySelector(PBXMMS2CLIENT, buf, 512, clsc->dir_add_handler);
      break;
    default: write_to_log(LOG_WARN, "screen_menus_handler(): unknown menu namber: %d", index);
      break;
    }
}

void client_screen::key_screen_handler (int key)
{
  switch (key) {
    case KEY_MENU: if (this->tracks_list->isFocused()) {
        if (this->tracks_list->itemsCount()) OpenMenu(track_menu,1,this->tracks_list->getSelectedItem()->x(),
                              this->tracks_list->getSelectedItem()->y(), client_screen::screen_menus_handler);
        else OpenMenu(track_menu, 1, 0, 0, client_screen::screen_menus_handler);
        };
      if (this->playlists_list->isFocused())
        OpenMenu(playlist_menu, 1, this->playlists_list->getSelectedItem()->x(),
                 this->playlists_list->getSelectedItem()->y(), screen_menus_handler);
      break;
    case KEY_OK: if (this->tracks_list->isFocused() && this->tracks_list->itemsCount())
        client_screen::track_clicked(NULL, 0);
      break;
    default:
      break;
    }
}

short int client_screen::track_get_info (int track_id, int playlist_num)
{
  playlists.at(playlist_num).tracks.back().track_result=xmmsc_medialib_get_info(pbconnection, track_id);
  xmmsv_t *track_value = result_is_error(playlists.at(playlist_num).tracks.back().track_result, false);
  if (track_value == NULL) return -1;
  xmmsv_t *track_info = xmmsv_propdict_to_dict(track_value, NULL);
  if (value_is_error(track_info)) return -1;
  xmmsv_t *dict_entry = NULL;
          if (! xmmsv_dict_get (track_info, "title", &dict_entry) ||
              ! xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().title)) {
                  playlists.at(playlist_num).tracks.back().title = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "album", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().album)) {
                  playlists.at(playlist_num).tracks.back().album = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "artist", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().artist)) {
                  playlists.at(playlist_num).tracks.back().artist = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "duration", &dict_entry) ||
              !xmmsv_get_int (dict_entry, &playlists.at(playlist_num).tracks.back().duration)) {
                  playlists.at(playlist_num).tracks.back().duration = 0;
          };
          if (!xmmsv_dict_get (track_info, "genre", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().genre)) {
                  playlists.at(playlist_num).tracks.back().genre = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "tracknr", &dict_entry) ||
              !xmmsv_get_int (dict_entry, &playlists.at(playlist_num).tracks.back().tracknr)) {
                  playlists.at(playlist_num).tracks.back().tracknr = 0;
          };
          if (!xmmsv_dict_get (track_info, "bitrate", &dict_entry) ||
              !xmmsv_get_int (dict_entry, &playlists.at(playlist_num).tracks.back().bitrate)) {
                  playlists.at(playlist_num).tracks.back().bitrate = 0;
          };
          if (!xmmsv_dict_get (track_info, "date", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().date)) {
                  playlists.at(playlist_num).tracks.back().date = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "size", &dict_entry) ||
              !xmmsv_get_int (dict_entry, &playlists.at(playlist_num).tracks.back().size)) {
                  playlists.at(playlist_num).tracks.back().size = 0;
          };
          if (!xmmsv_dict_get (track_info, "samplerate", &dict_entry) ||
              !xmmsv_get_int (dict_entry, &playlists.at(playlist_num).tracks.back().samplerate)) {
                  playlists.at(playlist_num).tracks.back().samplerate = 0;
          };
          if (!xmmsv_dict_get (track_info, "sample_format", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().sample_format)) {
                  playlists.at(playlist_num).tracks.back().sample_format = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "channels", &dict_entry) ||
              !xmmsv_get_int (dict_entry, &playlists.at(playlist_num).tracks.back().channels)) {
                  playlists.at(playlist_num).tracks.back().channels = 0;
          };
          if (!xmmsv_dict_get (track_info, "comment", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().comment)) {
                  playlists.at(playlist_num).tracks.back().comment = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "performer", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().performer)) {
                  playlists.at(playlist_num).tracks.back().performer = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "composer", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().composer)) {
                  playlists.at(playlist_num).tracks.back().composer = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "copyright", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().copyright)) {
                  playlists.at(playlist_num).tracks.back().copyright = str_empty;
          };
          if (!xmmsv_dict_get (track_info, "original_artist", &dict_entry) ||
              !xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().original_artist)) {
                  playlists.at(playlist_num).tracks.back().original_artist = str_empty;
          };

     //xmmsv_dict_foreach(track_info, &dict_foreach2, NULL);

  xmmsv_dict_get (track_info, "url", &dict_entry);
  xmmsv_get_string (dict_entry, &playlists.at(playlist_num).tracks.back().url);
  playlists.at(playlist_num).tracks.back().filename=strrchr(playlists.at(playlist_num).tracks.back().url,'/')+1;
  playlists.at(playlist_num).tracks.back().id = track_id;
  xmmsv_unref (track_info);
  return 0;
}

void client_screen::playlist_load (int pnum)
{
  client_screen::tracks_clear(pnum);
  if (playlists.at(pnum).playlist_result) xmmsc_result_unref(playlists.at(pnum).playlist_result);
  playlists.at(pnum).playlist_result = xmmsc_playlist_list_entries (pbconnection, playlists.at(pnum).name);
  xmmsv_t *_tracks = result_is_error(playlists.at(pnum).playlist_result, false);
  if (_tracks == NULL) return;
  xmmsv_list_iter_t *iter;
  int tnum;
  xmmsv_t *list_entry;
  xmmsv_get_list_iter(_tracks, &iter);
  xmmsv_list_iter_first(iter);
  while (xmmsv_list_iter_valid(iter)) {
      xmmsv_list_iter_entry(iter, &list_entry);
      xmmsv_get_int(list_entry, &tnum);
      xmmsv_list_iter_next(iter);
      playlists.at(pnum).tracks.push_back(track_item());
      if (track_get_info(tnum, pnum)==-1) playlists.at(pnum).tracks.pop_back();
    };
}

void client_screen::playlist_create_handler (char *name)
{
  if (name) {
      result_is_error(xmmsc_playlist_create(pbconnection, name), true);
      clsc->load_all_playlists();    // FIXME: we must not reload all playlists in this place
      clsc->update();
      delete [] name;
    };
}

void client_screen::tracks_clear (int pnum)
{
  while (!playlists.at(pnum).tracks.empty()) {
      if (playlists.at(pnum).tracks.back().track_result)
        xmmsc_result_unref(playlists.at(pnum).tracks.back().track_result);
      playlists.at(pnum).tracks.pop_back();
    };
}

void client_screen::playlist_clear_handler (int ok)
{
  if (ok==1) result_is_error(xmmsc_playlist_clear(pbconnection,
                               playlists.at(clsc->playlists_list->getSelectedIndex()).name), true);
}

void client_screen::playlist_delete_handler (int ok)
{
  if (ok==1) {
      result_is_error(xmmsc_playlist_remove(pbconnection,
                                            playlists.at(clsc->playlists_list->getSelectedIndex()).name), true);
      clsc->tracks_list->clear();
      clsc->tracks_clear(clsc->playlists_list->getSelectedIndex());
      xmmsc_result_unref(playlists.at(clsc->playlists_list->getSelectedIndex()).playlist_result);
      playlists.erase(playlists.begin()+clsc->playlists_list->getSelectedIndex());
      if (clsc->playlists_list->getSelectedIndex()<active_playlist) --active_playlist;
      if (clsc->playlists_list->getSelectedIndex()==active_playlist) active_playlist=-1;
      clsc->mark_active_playlist(active_playlist);
      clsc->playlists_list->erase(clsc->playlists_list->getSelectedItem());
      clsc->playlists_list->onFocusedWidgetChanged.emit(NULL);
      clsc->playlists_list->update();
    };
}

void client_screen::track_form_info (unsigned int track_num)
{
  static char itm [2048];
  sprintf(itm, "%s - %s - %s\n",
          //playlists.at(client_screen::playlists_list->getSelectedIndex()).tracks.at(track_num).tracknr,
          playlists.at(client_screen::playlists_list->getSelectedIndex()).tracks.at(track_num).title,
          playlists.at(client_screen::playlists_list->getSelectedIndex()).tracks.at(track_num).album,
          playlists.at(client_screen::playlists_list->getSelectedIndex()).tracks.at(track_num).artist);
  client_screen::tracks_list->addItem(string(itm));
  return;
}

void client_screen::playlist_show (PBWidget *)
{
  client_screen::tracks_list->clear();
  for (unsigned int i=0; i<playlists.at(client_screen::playlists_list->getSelectedIndex()).tracks.size(); i++)
      track_form_info (i);
  if (playlists.at(client_screen::playlists_list->getSelectedIndex()).tracks.size()!=0)
      client_screen::mark_active_track(active_track);
  client_screen::tracks_list->update();
}

void client_screen::load_all_playlists (void)
{
  xmmsv_t *_playlist_t;
  xmmsv_list_iter_t *iter;
  const char *n;
  playlists_list->clear();
  tracks_list->clear();
  playlists.clear();
  if (playlists_result) xmmsc_result_unref(playlists_result);
  playlists_result = xmmsc_playlist_list(pbconnection);
  _playlist_t = result_is_error(playlists_result, false);
  if (!_playlist_t) return;
  xmmsv_get_list_iter(_playlist_t, &iter);
  xmmsv_list_iter_first(iter);
  while (xmmsv_list_iter_valid(iter)) {
      xmmsv_list_iter_entry(iter, &_playlist_t);
      xmmsv_get_string(_playlist_t, &n);
      if (n[0]!='_') {
      playlists.push_back(playlist_item());
      playlists.back().name = n;
      playlists.back().playlist_result = NULL;
      playlists_list->addItem(string(playlists.back().name));
      client_screen::playlist_load(playlists.size()-1);
        };
      xmmsv_list_iter_next(iter);
    };
  playlists_list->update();
  write_to_log(LOG_INFO,"All playlists loaded.");
  return;
}

void client_screen::track_clicked (PBListBox *, int)
{
  if (active_playlist==this->playlists_list->getSelectedIndex() &&
      active_track==this->tracks_list->getSelectedIndex()) {
      if (playback_status==XMMS_PLAYBACK_STATUS_PLAY)
        set_playback(XMMS_PLAYBACK_STATUS_PAUSE, false);
      else set_playback(XMMS_PLAYBACK_STATUS_PLAY, false);
      return;
    };
  if (active_playlist!=this->playlists_list->getSelectedIndex()) {
      result_is_error(xmmsc_playlist_load
                      (pbconnection, playlists.at(this->playlists_list->getSelectedIndex()).name), true);
      result_is_error(xmmsc_playlist_set_next(pbconnection, this->tracks_list->getSelectedIndex()), true);
      set_playback(XMMS_PLAYBACK_STATUS_PLAY, true);
      active_track=-1;
      return;
    };
  if (active_playlist==this->playlists_list->getSelectedIndex() &&
                active_track!=this->tracks_list->getSelectedIndex())
    {
      result_is_error(xmmsc_playlist_set_next(pbconnection, this->tracks_list->getSelectedIndex()), true);
      set_playback(XMMS_PLAYBACK_STATUS_PLAY, true);
      return;
    };
}

void client_screen::mark_active_playlist (short int playlist)
{
  if (active_playlist!=-1)
    client_screen::playlists_list->getItem(active_playlist)->setWidgetFont(client_screen::font);
  if (playlist!=-1)
    client_screen::playlists_list->getItem(playlist)->setWidgetFont(client_screen::active_font);
  client_screen::playlists_list->update();
  active_playlist=playlist;
  return;
}

void client_screen::mark_active_track (short int track)
{
  if (client_screen::playlists_list->getSelectedIndex()==active_playlist)
    {
      if (active_track!=-1)
        client_screen::tracks_list->getItem(active_track)->setWidgetFont(client_screen::font);
      if (track!=-1)
        client_screen::tracks_list->getItem(track)->setWidgetFont(active_font);
      client_screen::tracks_list->update();
    };
  active_track=track;
  return;
}
