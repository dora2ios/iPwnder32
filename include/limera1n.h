/*
 * limera1n.h
 * copyright (C) 2020/05/25 dora2ios
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef LIMERA1N_H
#define LIMERA1N_H

int gen_limera1n(int cpid, int rom, unsigned char** payload, size_t* payload_len);
int limera1n_exploit(irecv_client_t client, irecv_device_t device_info, const struct irecv_device_info *info);

#endif
