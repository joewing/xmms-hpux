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

#include "hpux.h"

#include <errno.h>
#include <sys/semaphore.h>
#include <sys/audio.h>

static int fd = 0;
static char *buffer;
static int going, prebuffer, paused = 0;
static int unpause, do_pause, remove_prebuffer;
static int buffer_size, prebuffer_size, blk_size;
static int rd_index = 0, wr_index = 0;
static int output_time_offset = 0;
static unsigned long long written = 0, output_bytes = 0;

static int flush;
static sem_t flush_signal;

static int fragsize, device_buffer_size;
static pthread_t buffer_thread;

static int (*hpux_convert_func)(void **data, int length);
static int (*hpux_stereo_convert_func)(void **data, int length, int fmt);
static int hpux_downsample(void *ob, unsigned int length,
	unsigned int speed, unsigned int espeed);


struct format_info {
	union {
		AFormat xmms;
		int hpux;
	} format;
	int frequency;
	int channels;
	int bps;
};


/*
 * The format of the data from the input plugin
 * This will never change during a song. 
 */
struct format_info input;

/*
 * The format we get from the effect plugin.
 * This will be different from input if the effect plugin does
 * some kind of format conversion.
 */
struct format_info effect;

/*
 * The format of the data we actually send to the soundcard.
 * This might be different from effect if we need to resample or do
 * some other format conversion.
 */
struct format_info output;

static int hpux_calc_bitrate(int hpux_fmt, int rate, int channels)
{
	int bitrate = rate * channels;

	if (hpux_fmt == AFMT_U16_BE || hpux_fmt == AFMT_U16_LE ||
	    hpux_fmt == AFMT_S16_BE || hpux_fmt == AFMT_S16_LE)
	{
		bitrate *= 2;
	}

	return bitrate;
}

static int hpux_get_format(AFormat fmt)
{
	int format = 0;

	switch (fmt)
	{
		case FMT_U8:
			format = AFMT_U8;
			break;
		case FMT_S8:
			format = AFMT_S8;
			break;
		case FMT_U16_LE:
			format = AFMT_U16_LE;
			break;
		case FMT_U16_BE:
			format = AFMT_U16_BE;
			break;
		case FMT_U16_NE:
			format = AFMT_U16_BE;
			break;
		case FMT_S16_LE:
			format = AFMT_S16_LE;
			break;
		case FMT_S16_BE:
			format = AFMT_S16_BE;
			break;
		case FMT_S16_NE:
			format = AFMT_S16_BE;
			break;
	}

	return format;
}

static void hpux_setup_format(AFormat fmt, int rate, int nch)
{

	struct audio_limits limits;

	effect.format.xmms = fmt;
	effect.frequency = rate;
	effect.channels = nch;
	effect.bps = hpux_calc_bitrate(hpux_get_format(fmt), rate, nch);

	output.format.hpux = hpux_get_format(fmt);
	output.frequency = rate;
	output.channels = nch;

	fragsize = 0;
	while ((1L << fragsize) < effect.bps / 25)
	{
		fragsize++;
	}
	fragsize--;

	if (ioctl(fd, AUDIO_GET_LIMITS, &limits) == -1)
	{
		perror("AUDIO_GET_LIMITS failed");
		device_buffer_size = 0x0100 << 16;
	}
	else
	{
		device_buffer_size = limits.max_transmit_buffer_size;
	}

	hpux_set_audio_params();

	output.bps = hpux_calc_bitrate(output.format.hpux, output.frequency,
				      output.channels);
}
	

int hpux_get_written_time(void)
{
	if (!going)
	{
		return 0;
	}
	else
	{
		return (written * 1000) / effect.bps;
	}
}

int hpux_get_output_time(void)
{
	unsigned long long bytes;

	if (!fd || !going)
	{
		return 0;
	}

	bytes = output_bytes;

	return output_time_offset + ((bytes * 1000) / output.bps);
}

static int hpux_used(void)
{

	int result;

	if (wr_index >= rd_index)
	{
		result = wr_index - rd_index;
	}
	else
	{
		result = buffer_size - (rd_index - wr_index);
	}

	return result;

}

int hpux_playing(void)
{

	if(!going)
	{
		return 0;
	}

	if (!hpux_used())
	{
		return 0;
	}

	return 1;

}

int hpux_free(void)
{

	if (remove_prebuffer && prebuffer)
	{
		prebuffer = 0;
		remove_prebuffer = 0;
	}
	if (prebuffer)
	{
		remove_prebuffer = 1;
	}

	if (rd_index > wr_index)
	{
		return (rd_index - wr_index) - device_buffer_size - 1;
	}

	return (buffer_size - (wr_index - rd_index)) - device_buffer_size - 1;

}

static inline ssize_t write_all(int fd, const void *buf, size_t count)
{
	ssize_t done = 0;
	do {
		ssize_t n = write(fd, buf, count - done);
		if (n == -1)
		{
			if (errno == EINTR)
			{
				continue;
			}
			else
			{
				perror("write failed in write_all");
				break;
			}
		}
		done += n;
	} while (count > done);

	return done;
}

static void hpux_write_audio(void *data, int length)
{

	AFormat new_format;
	int new_frequency, new_channels;
	EffectPlugin *ep;
	
	new_format = input.format.xmms;
	new_frequency = input.frequency;
	new_channels = input.channels;
	
	ep = get_current_effect_plugin();
	if(effects_enabled() && ep && ep->query_format)
	{
		ep->query_format(&new_format,&new_frequency,&new_channels);
	}
	
	if (new_format != effect.format.xmms ||
	    new_frequency != effect.frequency ||
	    new_channels != effect.channels)
	{
		output_time_offset += (output_bytes * 1000) / output.bps;
		output_bytes = 0;
		close(fd);
		fd = open(DEV_AUDIO, O_WRONLY);
		hpux_setup_format(new_format, new_frequency, new_channels);
	}

	if (effects_enabled() && ep && ep->mod_samples)
	{
		length = ep->mod_samples(&data, length,
					 input.format.xmms,
					 input.frequency,
					 input.channels);
	}

	if (hpux_convert_func != NULL)
	{
		length = hpux_convert_func(&data, length);
	}

	if (hpux_stereo_convert_func != NULL)
	{
		length = hpux_stereo_convert_func(&data, length,
						 output.format.hpux);
	}

	if (effect.frequency == output.frequency)
	{
		output_bytes += write_all(fd, data, length);
	}
	else
	{
		output_bytes += hpux_downsample(data, length,
					       effect.frequency,
					       output.frequency);
	}

}

int hpux_downsample(void *ob, unsigned int length, unsigned int speed,
	unsigned int espeed)
{

	static short *nbuffer = NULL;
	unsigned int nlen;
	static int nbuffer_size = 0;
	int i, in_samples, out_samples, x, delta;

	if (output.channels == 2)
	{
		const int shift = sizeof(short);
		short *inptr = (short *)ob, *outptr;
		nlen = (((length >> shift) * espeed) / speed);
		if (nlen == 0)
		{
			return 0;
		}
		nlen <<= shift;
		if(nlen > nbuffer_size)
		{
			nbuffer = g_realloc(nbuffer, nlen);
			nbuffer_size = nlen;
		}
		outptr = nbuffer;
		in_samples = length >> shift;
		out_samples = nlen >> shift;
		delta = (in_samples << 12) / out_samples;
		for (x = 0, i = 0; i < out_samples; i++)
		{
			int x1, frac;
			x1 = (x >> 12) << 12;
			frac = x - x1;
			*outptr++ =
				(short)
				((inptr[(x1 >> 12) << 1] *
				  ((1<<12) - frac) +
				  inptr[((x1 >> 12) + 1) << 1] *
				  frac) >> 12);
			*outptr++ =
				(short)
				((inptr[((x1 >> 12) << 1) + 1] *
				  ((1<<12) - frac) +
				  inptr[(((x1 >> 12) + 1) << 1) + 1] *
				  frac) >> 12);
			x += delta;
		}
	}
	else
	{
		const int shift = sizeof(short) - 1;
		short *inptr = (short*)ob, *outptr;
		nlen = (((length >> shift) * espeed) / speed);
		if (nlen == 0)
		{
			return 0;
		}
		nlen <<= shift;
		if(nlen > nbuffer_size)
		{
			nbuffer = g_realloc(nbuffer, nlen);
			nbuffer_size = nlen;
		}
		outptr = nbuffer;
		in_samples = length >> shift;
		out_samples = nlen >> shift;
		delta = ((length >> shift) << 12) / out_samples;
		for (x = 0, i = 0; i < out_samples; i++)
		{
			int x1, frac;
			x1 = (x >> 12) << 12;
			frac = x - x1;
			*outptr++ =
				(short)
				((inptr[x1 >> 12] * ((1<<12) - frac) +
				  inptr[(x1 >> 12) + 1] * frac) >> 12);
			x += delta;
		}
	}

	return write_all(fd, nbuffer, nlen);

}

void hpux_write(void *ptr, int length)
{
	int cnt, off = 0;

	remove_prebuffer = 0;

	written += length;
	while (length > 0)
	{
		cnt = MIN(length, buffer_size - wr_index);
		memcpy(buffer + wr_index, (char*)ptr + off, cnt);
		wr_index = (wr_index + cnt) % buffer_size;
		length -= cnt;
		off += cnt;
	}

}

void hpux_close(void)
{

	if (!going)
	{
		return;
	}

	going = 0;
	pthread_join(buffer_thread, NULL);

	sem_destroy(&flush_signal);

	hpux_free_convert_buffer();
	wr_index = 0;
	rd_index = 0;

}

void hpux_flush(int time)
{

	flush = time;
	sem_wait(&flush_signal);

}

void hpux_pause(short p)
{

	if (p == 1)
	{
		do_pause = 1;
	}
	else
	{
		unpause = 1;
	}

}

void *hpux_loop(void *arg)
{
	int length, cnt;
	fd_set set;
	struct timeval tv;

	while (going)
	{

		if (hpux_used() > prebuffer_size)
		{
			prebuffer = 0;
		}

		if (hpux_used() > 0 && !paused && !prebuffer)
		{
			tv.tv_sec = 0;
			tv.tv_usec = 10000;
			FD_ZERO(&set);
			FD_SET(fd, &set);
			if(select(fd + 1, NULL, &set, NULL, &tv) > 0)
			{
				length = MIN(blk_size, hpux_used());
				while (length > 0)
				{
					cnt = MIN(length, buffer_size - rd_index);
					hpux_write_audio(buffer + rd_index, cnt);
					rd_index = (rd_index + cnt) % buffer_size;
					length -= cnt;
				}
			}
		}
		else
		{
			usleep(10000);
		}

		if (do_pause && !paused)
		{
			do_pause = 0;
			paused = 1;
			if (ioctl(fd, AUDIO_RESET, 15) == -1)
			{
				perror("AUDIO_RESET failed");
			}
		}
		else if (unpause && paused)
		{
			unpause = 0;
			close(fd);
			fd = open(DEV_AUDIO, O_WRONLY);
			hpux_set_audio_params();
			paused = 0;
		}

		if (flush != -1)
		{
			if (ioctl(fd, AUDIO_RESET, 15) == -1)
			{
				perror("AUDIO_RESET failed");
			}
			close(fd);
			fd = open(DEV_AUDIO, O_WRONLY);
			hpux_set_audio_params();
			output_time_offset = flush;
			written = ((unsigned long long)flush * input.bps) / 1000;
			rd_index = wr_index = output_bytes = 0;

			flush = -1;
			prebuffer = 1;

			sem_post(&flush_signal);

		}

	}

	if (ioctl(fd, AUDIO_RESET, 15) == -1)
	{
		perror("AUDIO_RESET failed");
	}
	close(fd);
	g_free(buffer);
	pthread_exit(NULL);
	return NULL;
}

void hpux_set_audio_params(void)
{

	if (ioctl(fd, AUDIO_RESET, 15) == -1)
	{
		perror("AUDIO_RESET failed");
	}

	if (ioctl(fd, AUDIO_SET_OUTPUT, AUDIO_OUT_EXTERNAL) == -1)
	{
		perror("AUDIO_SET_OUTPUT failed");
	}

	output.format.hpux = AFMT_S16_BE;
	if (ioctl(fd, AUDIO_SET_DATA_FORMAT, AUDIO_FORMAT_LINEAR16BIT) == -1)
	{
		perror("AUDIO_SET_DATA_FORMAT failed");
	}
	
	if (ioctl(fd, AUDIO_SET_CHANNELS, output.channels) == -1)
	{
		perror("AUDIO_SET_CHANNELS failed");
	}

	hpux_stereo_convert_func = hpux_get_stereo_convert_func(output.channels,
		effect.channels);

	if (ioctl(fd, AUDIO_SET_SAMPLE_RATE, output.frequency) == -1)
	{
		perror("AUDIO_SET_SAMPLE_RATE failed");
	}

	blk_size = 1L << fragsize;

	hpux_convert_func =
		hpux_get_convert_func(output.format.hpux,
			hpux_get_format(effect.format.xmms));

	if (ioctl(fd, AUDIO_RESUME, AUDIO_RECEIVE | AUDIO_TRANSMIT) == -1)
	{
		perror("AUDIO_RESUME failed");
	}

}

int hpux_open(AFormat fmt, int rate, int nch)
{

	fd = open(DEV_AUDIO, O_WRONLY);
	if (fd == -1)
	{
		g_warning("hpux_open(): Failed to open audio device (%s): %s",
			DEV_AUDIO, strerror(errno));
		return 0;
	}

	input.format.xmms = fmt;
	input.frequency = rate;
	input.channels = nch;
	input.bps = hpux_calc_bitrate(hpux_get_format(fmt), rate, nch);

	hpux_setup_format(fmt, rate, nch);
	
	buffer_size = (HPUX_BUFFER_SIZE * input.bps) / 1000;
	if (buffer_size < 8192)
	{
		buffer_size = 8192;
	}
	prebuffer_size = (buffer_size * HPUX_PREBUFFER_SIZE) / 100;
	if (buffer_size - prebuffer_size < 4096)
	{
		prebuffer_size = buffer_size - 4096;
	}
	buffer_size += device_buffer_size;
	buffer = g_malloc0(buffer_size);

	sem_init(&flush_signal, 0, 0);
	flush = -1;

	prebuffer = 1;
	wr_index = rd_index = output_time_offset = written = output_bytes = 0;
	paused = 0;
	do_pause = 0;
	unpause = 0;
	remove_prebuffer = 0;

	going = 1;
	pthread_create(&buffer_thread, NULL, hpux_loop, NULL);
	return 1;

}

