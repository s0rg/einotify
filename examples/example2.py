#!/usr/bin/env python
#-*- coding: utf8 -*-

import einotify as spy


class MyWatcher(spy.WatcherBase):
    def on_event(self, mask, cookie, path):
        print 'event {} at {}'.format(spy.decode_mask(mask), path)


w = MyWatcher()
w.add_path('/tmp')
w.start()

