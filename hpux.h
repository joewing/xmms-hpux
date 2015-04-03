/*  HP-UX Output Plugin for XMMS
 *  Based on the OSS Output Plugin.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef HPUX_H
#define HPUX_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xmms/plugin.h>

#define IS_BIG_ENDIAN TRUE

#define DEV_AUDIO "/dev/audio"
#define DEV_AUDIO_CTL "/dev/audioCtl"
#define HPUX_BUFFER_SIZE 3000
#define HPUX_PREBUFFER_SIZE 25

#define AFMT_U8			0x00000008
#define AFMT_S16_LE		0x00000010
#define AFMT_S16_BE		0x00000020
#define AFMT_S8			0x00000040
#define AFMT_U16_LE		0x00000080
#define AFMT_U16_BE		0x00000100

extern OutputPlugin op;

void hpux_init(void);

void hpux_get_volume(int *l, int *r);
void hpux_set_volume(int l, int r);

int hpux_playing(void);
int hpux_free(void);
void hpux_write(void *ptr, int length);
void hpux_close(void);
void hpux_flush(int time);
void hpux_pause(short p);
int hpux_open(AFormat fmt, int rate, int nch);
int hpux_get_output_time(void);
int hpux_get_written_time(void);
void hpux_set_audio_params(void);

void hpux_free_convert_buffer(void);
int (*hpux_get_convert_func(int output, int input))(void **, int);
int (*hpux_get_stereo_convert_func(int output, int input))(void **, int, int);

#endif

