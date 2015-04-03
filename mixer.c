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
#include <sys/audio.h>

void hpux_get_volume(int *l, int *r)
{

	struct audio_describe describe;
	struct audio_gain gains;
	int temp;
	int range, min;
	int fd;

	fd = open(DEV_AUDIO_CTL, O_RDONLY);

	if (fd != -1)
	{

		if (ioctl(fd, AUDIO_DESCRIBE, &describe) == -1)
		{
			perror("ioctl AUDIO_DESCRIBE failed in get_volume");
			close(fd);
			return;
		}

		if (ioctl(fd, AUDIO_GET_GAINS, &gains) == -1)
		{
			perror("ioctl AUDIO_GET_GAINS failed in get_volume");
			close(fd);
			return;
		}

		range = describe.max_transmit_gain - describe.min_transmit_gain;
		min = describe.min_transmit_gain;

		temp = gains.cgain[0].transmit_gain - min;
		*l = (int)((float)temp * 100.0 / (float)range);

		temp = gains.cgain[1].transmit_gain - min;
		*r = (int)((float)temp * 100.0 / (float)range);

		close(fd);

	}

}

void hpux_set_volume(int l, int r)
{

	struct audio_describe describe;
	struct audio_gain gains;
	float temp;
	int fd;
	int range, min;

	fd = open(DEV_AUDIO_CTL, O_RDONLY);

	if (fd != -1)
	{

		if (ioctl(fd, AUDIO_DESCRIBE, &describe) == -1)
		{
			perror("ioctl AUDIO_DESCRIBE failed in set_volume");
			close(fd);
			return;
		}

		memset(&gains, 0, sizeof(gains));

		range = describe.max_transmit_gain - describe.min_transmit_gain;
		min = describe.min_transmit_gain;

		temp = (float)l / 100.0;
		gains.cgain[0].transmit_gain = (int)(temp * (float)range + min);

		temp = (float)r / 100.0;
		gains.cgain[1].transmit_gain = (int)(temp * (float)range + min);

		gains.channel_mask = 0x03;

		if (ioctl(fd, AUDIO_SET_GAINS, &gains) == -1)
		{
			perror("AUDIO_SET_GAINS failed");
		}

		close(fd);

	}

}

