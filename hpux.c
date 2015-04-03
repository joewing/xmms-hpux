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

#define VERSION "0.1"

OutputPlugin hpux_op =
{
	NULL,
	NULL,
	NULL, /* Description */
	NULL, /* Initialize */
	NULL, /* About */
	NULL, /* Configure */
	hpux_get_volume,
	hpux_set_volume,
	hpux_open,
	hpux_write,
	hpux_close,
	hpux_flush,
	hpux_pause,
	hpux_free,
	hpux_playing,
	hpux_get_output_time,
	hpux_get_written_time,
};

OutputPlugin *get_oplugin_info(void)
{
	hpux_op.description = g_strdup_printf("HP-UX Driver %s", VERSION);
	return &hpux_op;
}

