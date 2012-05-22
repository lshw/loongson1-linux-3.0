/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _SSD1305_H_
#define _SSD1305_H_

struct ssd1305_platform_data {
	unsigned int gpio_outpu;
	
	unsigned int gpios_res;
	unsigned int gpios_cs;
	unsigned int gpios_dc;
	unsigned int gpios_rd;
	unsigned int gpios_wr;
	
	unsigned int gpios_d0;
	unsigned int gpios_d1;
	unsigned int gpios_d2;
	unsigned int gpios_d3;
	unsigned int gpios_d4;
	unsigned int gpios_d5;
	unsigned int gpios_d6;
	unsigned int gpios_d7;
	
	int datas_offset;
};

#endif /* _SSD1305_H_ */

