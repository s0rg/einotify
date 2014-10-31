#!/usr/bin/env python
#-*- coding: utf8 -*-

import einotify as spy


def watch_cb(wd, mask, cookie, name):
    print 'wd: {} mask: {} name: {}'.format(wd, spy.decode_mask(mask), name)


inotify = spy.init_notify()
wd = spy.add_watch(inotify, '/tmp', spy.IN_CLOSE_WRITE|spy.IN_DELETE)
spy.start_watch(inotify, watch_cb)

