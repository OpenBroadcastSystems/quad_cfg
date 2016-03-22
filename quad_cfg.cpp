/*
 * Copyright (C) 2016 Open Broadcast Systems Ltd.
 * Author        2016 Rostislav Pehlivanov <atomnuker@gmail.com>
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>

#include "DeckLinkAPI.h"

#define MAX_DEVICES 16

struct DeviceProps {
    int idx;
    int pair_idx;
    const char *name;

    /* Persistent ids */
    int64_t uid;
    int64_t pair_uid;

    int64_t duplex_mode;

    IDeckLink              *deck_link_main;
    IDeckLinkIterator      *deck_link_iterator;
    IDeckLinkAttributes    *deck_link_attribs;
    IDeckLinkStatus        *deck_link_status;
    IDeckLinkConfiguration *deck_link_config;
};

struct DeviceProps device_props[MAX_DEVICES];

int get_device_idx_from_uid(int64_t uid)
{
    for (int i = 0; i < MAX_DEVICES; i++)
        if (device_props[i].uid == uid)
            return device_props[i].idx;
    return -1;
}

int load_device_props(int device_idx)
{
    int ret = 0, idx = device_idx;
    IDeckLink              *deck_link_main     = NULL;
    IDeckLinkIterator      *deck_link_iterator = NULL;
    IDeckLinkAttributes    *deck_link_attribs  = NULL;
    IDeckLinkStatus        *deck_link_status   = NULL;
    IDeckLinkConfiguration *deck_link_config   = NULL;

    deck_link_iterator = CreateDeckLinkIteratorInstance();
    if (!deck_link_iterator) {
        fprintf(stderr, "Unable to init a IDeckLinkIterator context!\n");
        fprintf(stderr, "Are you sure you have the drivers installed?\n");
		return -1;
    }

    while ((ret = deck_link_iterator->Next(&deck_link_main)) == S_OK) {
        if (idx-- == 0)
            break;
        deck_link_main->Release();
    }

    if (!deck_link_main || ret != S_OK) {
        fprintf(stderr, "Unable to init a IDeckLink context with device index = %u\n", device_idx);
        deck_link_iterator->Release();
        return -1;
    }

    ret = deck_link_main->QueryInterface(IID_IDeckLinkAttributes, (void **)&deck_link_attribs);
	if (ret != S_OK) {
	    fprintf(stderr, "Unable to init a IID_IDeckLinkAttributes context!\n");
	    deck_link_iterator->Release();
        deck_link_main->Release();
		return -1;
    }

    ret = deck_link_main->QueryInterface(IID_IDeckLinkStatus, (void **)&deck_link_status);
	if (ret != S_OK) {
	    fprintf(stderr, "Unable to init a IID_IDeckLinkStatus context!\n");
	    deck_link_attribs->Release();
	    deck_link_iterator->Release();
        deck_link_main->Release();
		return -1;
    }

    ret = deck_link_main->QueryInterface(IID_IDeckLinkConfiguration, (void **)&deck_link_config);
	if (ret != S_OK) {
	    fprintf(stderr, "Unable to init a IID_IDeckLinkConfiguration context!\n");
	    deck_link_status->Release();
	    deck_link_attribs->Release();
	    deck_link_iterator->Release();
        deck_link_main->Release();
		return -1;
    }

    deck_link_main->GetModelName(&device_props[device_idx].name);
    deck_link_attribs->GetInt(BMDDeckLinkPersistentID, &device_props[device_idx].uid);

    device_props[device_idx].deck_link_main     = deck_link_main;
    device_props[device_idx].deck_link_iterator = deck_link_iterator;
    device_props[device_idx].deck_link_attribs  = deck_link_attribs;
    device_props[device_idx].deck_link_status   = deck_link_status;
    device_props[device_idx].deck_link_config   = deck_link_config;
    device_props[device_idx].idx                = device_idx;

    return 0;
}

int unload_device_props(int device_idx)
{
    struct DeviceProps *s = &device_props[device_idx];

    free((void *)s->name);
    s->deck_link_config->Release();
    s->deck_link_status->Release();
    s->deck_link_attribs->Release();
    s->deck_link_iterator->Release();
    s->deck_link_main->Release();

    return 0;
}

/* Returns 1 if Half duplex, else 0 */
int report_device_status(int device_idx)
{
    struct DeviceProps *s = &device_props[device_idx];

    s->deck_link_status->GetInt(bmdDeckLinkStatusDuplexMode, &s->duplex_mode);

    s->deck_link_attribs->GetInt(BMDDeckLinkPairedDevicePersistentID, &s->pair_uid);

    s->pair_idx = get_device_idx_from_uid(s->pair_uid);

    if (s->duplex_mode == bmdDuplexStatusFullDuplex)
        fprintf(stderr, "Device %i (%s) set to full duplex mode (paired with device %i)\n", device_idx, s->name, s->pair_idx);
    else if (s->duplex_mode == bmdDuplexStatusHalfDuplex)
        fprintf(stderr, "Device %i (%s) set to half duplex mode (single port)\n", device_idx, s->name);
    else if (s->duplex_mode == bmdDuplexStatusSimplex)
        fprintf(stderr, "Device %i (%s) is currently in capture or playback only mode\n", device_idx, s->name);
    else if (s->duplex_mode == bmdDuplexStatusInactive)
        fprintf(stderr, "Device %i (%s) is currently inactive (port is paired with %i)\n", device_idx, s->name, s->pair_idx);
    else
        fprintf(stderr, "Invalid duplex mode for device %i (%s) = 0x%x\n", device_idx, s->name, (uint32_t)s->duplex_mode);

    return s->duplex_mode == bmdDuplexStatusHalfDuplex;
}

/* Mode -> 1 if Half duplex, 0 for Full duplex (2 ports) */
int set_device_duplex(int device_idx, int64_t mode)
{
    struct DeviceProps *s = &device_props[device_idx];

    s->deck_link_config->SetInt(bmdDeckLinkConfigDuplexMode,
                                mode ? bmdDuplexModeHalf : bmdDuplexModeFull);

    s->deck_link_config->WriteConfigurationToPreferences();

    return 0;
}

int main(int argc, char *argv[])
{
    int state;

    for (int i = 0; i < 8; i++)
        if (load_device_props(i))
            return 1;

    if (argv[1] && !strncmp(argv[1], "full", sizeof("full")))
        state = 0;
    else
        state = 1;

    for (int i = 0; i < 8; i++)
        if (set_device_duplex(i, state))
            return 1;

    sleep(1);

    for (int i = 0; i < 8; i++)
        report_device_status(i);

    for (int i = 0; i < 8; i++)
        if (unload_device_props(i))
            return 1;

    return 0;
}
