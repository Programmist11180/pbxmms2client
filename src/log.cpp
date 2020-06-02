/*
 *   log.cpp
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
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "log.hpp"

#define LOG_DIR USERDATA "/log"
FILE *log_file=NULL;
int log_pipe[2];
bool log_worked;
pthread_t log_thread;
pthread_mutex_t log_mutex;

using namespace std;

short int log_init (void)
{
  if (log_file && log_worked) return 0;   // log already started
  const char log_file_path [] = LOG_DIR "/pbxmms2client.log";
  if (access(LOG_DIR, W_OK | R_OK))      // NOTE: log dir may absent in some causes
  {
      if (errno==ENOENT)
      {
          if (mkdir(LOG_DIR,  S_IRWXU | S_IRWXG | S_IRWXO))
          {
            cerr<<PBXMMS2CLIENT<<": Error while creating log dir: "<<strerror(errno)<<endl;
            return -1;
          }
       }
      else
      {
          cerr<<"Error while accessing log dir: "<<strerror(errno)<<endl;
          return -1;
      };
  };
  if ((log_file=fopen(log_file_path,"a+"))==NULL)
    {
      cerr<<"Error while opening log file '"<<log_file_path<<"' : "<<strerror(errno)<<endl;
      return -1;
    };
  log_worked = true;
  pthread_mutex_init(&log_mutex,NULL);
  pipe(log_pipe);
  pthread_create(&log_thread, NULL, log_handler, NULL);
  write_to_log(LOG_INFO,"log thread started.");
  write_to_log(LOG_DBG,"Path to log file: %s",log_file_path);
  return 0;
}

short int log_end (void)
{
  if (log_file==NULL) return 0;   // log not started
  log_worked = false;
  write_to_log(LOG_INFO,"Log thread shutdown.");
  pthread_join(log_thread, NULL);
  close(log_pipe[0]);
  close(log_pipe[1]);
  fclose(log_file);
  pthread_mutex_destroy(&log_mutex);
  log_file=NULL;
  return 0;
}

void *log_handler (void*)
{
  char msgbuf[256], timebuf[32];
  int lenght;
  time_t time1;
  struct tm *tm;
  LOG_LEVEL level;
  do
    {
      read(log_pipe[0],&level,sizeof(level));
      read(log_pipe[0],&lenght,sizeof(lenght));
      read(log_pipe[0],msgbuf,lenght);
      time(&time1);
      tm = localtime(&time1);
      strftime(timebuf, sizeof(timebuf),"%F %T%t ",tm);
      fputs(timebuf,log_file);
      switch (level) {
       case LOG_ERR:
          fputs("ERROR  :  ",log_file);
          break;
       case LOG_WARN:
          fputs("WARNING:  ",log_file);
          break;
       case LOG_INFO:
          fputs("INFO   :  ",log_file);
          break;
      case LOG_DBG:
          fputs("DEBUG  :  ",log_file);
          break;
       default:
          break;
       };
      fputs(msgbuf,log_file);
      fputs("\n",log_file);
      fflush(log_file);
    }
  while (log_worked);
  pthread_exit(NULL);
}

short int write_to_log (LOG_LEVEL level,  const char *msg, ...)
{
  if (!pbxmms_settings->log_enabled || level>pbxmms_settings->log_level) return -1;
  char buffer[256];
  int len;
  va_list args;
  va_start(args,msg);
  len=1+vsprintf(buffer,msg,args);
  va_end(args);
  if (len<0) return -1;
  pthread_mutex_lock(&log_mutex);
  write(log_pipe[1],&level,sizeof(level));
  write(log_pipe[1],&len,sizeof(len));
  write(log_pipe[1],buffer,len);
  pthread_mutex_unlock(&log_mutex);
  return 0;
}
