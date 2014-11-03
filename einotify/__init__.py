#-*- coding: utf8 -*-
from os.path import join as path_join
from inspect import getmembers


# c-wrapper api
import _einotify as ei

init_notify = ei.init_notify
add_watch = ei.add_watch
del_watch = ei.del_watch
start_watch = ei.start_watch


_MASK_NAMES = {}

# import all IN_* constants
for name, value in getmembers(ei):
    if name.startswith('IN_'):
        globals()[name] = value
        _MASK_NAMES[value] = name


__doc__ = '''
lightweight python bingings to inotify (http://man7.org/linux/man-pages/man7/inotify.7.html)
with integrated reactor, built on top of epoll (http://man7.org/linux/man-pages/man7/epoll.7.html)

# EXAMPLE 1:

from einotify import init_notify, add_watch, start_watch

fd = init_notify()
wd = add_watch(fd, '/tmp')

def watch_cb(wd, mask, cookie, name):
    # note, you need to handle 'wd' here,
    # to construct full path for 'name'
    print 'wd: {} mask: {} cookie: {} name: {}'.format(wd, mask, cookie, name)

start_watch(fd, watch_cb)


========8<======== cut here ========8<========

# EXAMPLE 2:

from einotify import WatcherBase

class MyWatcher(WatcherBase):
    def on_event(self, mask, cookie, path):
        print 'event {} at {}'.format(mask, path)

w = MyWatcher()
w.add_path('/tmp') # or add_path('path', mask) or, even add_path('path', mask, recurse=True)
w.start()


========8<======== cut here ========8<========
'''

__import__ = (
    'init_notify',
    'add_watch',
    'del_watch',
    'start_watch',
    'decode_mask',
    'WatcherBase',
)


def decode_mask(mask):
    '''
    Decode given event 'mask' in human-readable form

    params:
        mask - suddenly, the 'mask' parameter in callback

    returns:
        string
    '''
    parts = []
    for k, w in _MASK_NAMES.iteritems():
        if (mask & k) and (not w.startswith('IN_ALL_')):
            parts.append(w)
    return '|'.join(parts)


def _dir_walker(path):
    '''
    internal: dummy dir-walker
    '''
    for root, dirs, _ in os.walk(path):
        for d in dirs:
            yield os.path.join(root, d)


def _get_key_by_val(d, value):
    '''
    internal: returns key in given dict 'd' whitch links to 'value'
    '''
    for k, w in d.iteritems():
        if value == w:
            return k


class WatcherBase(object):
    '''
    Watcher-base class, you need to override 'on_event' method
                          (or 'callback' for low-level access)
    '''
    def __init__(self):
        self.inotify = init_notify()
        self.dirs = {}

    def add_path(self, path, mask=ei.IN_ALL_EVENTS, recurse=False):
        '''
        Adds new path to watch points

        params:
            path - path to fs object (file or dir)
            mask - event mask (combination of IN_* constants) default - IN_ALL_EVENTS
            recurse - if set, recursively walks given path, and adds all subdirs (files omitted)
        '''
        if recurse:
            for d in _dir_walker(path):
                self.add_path(d, mask)
            return

        wd = add_watch(self.inotify, path, mask)
        self.dirs[wd] = path

    def del_path(self, path=None, wd=None):
        '''
        Removes path OR wd (not both) from watches
        params:
            path - full path to fs object
            wd - watch descriptor, obtained by add_path call
        '''
        if path is not None:
            wd = _get_key_by_val(self.dirs, path)

        if wd is None:
            raise ValueError("Bad or invalid values for 'path' or 'wd'")

        del self.dirs[wd]
        del_watch(self.inotify, wd)

    def on_event(self, mask, cookie, path):
        '''
        Default callback for event, you proppably want to override it
        params:
            mask - current event mask
            cookie - event cookie
            path - full path (if any) to fs object
        '''
        pass

    def callback(self, wd, mask, cookie, name):
        '''
        Low-level callback
        params:
            wd - watch descriptor
            mask - current event mask
            cookie - event cookie
            name - object name or None
        '''
        path = self.dirs[wd]
        if name is not None:
            path = path_join(path, name)

        self.on_event(mask, cookie, path)

    def start(self):
        '''
        Starts reactor
        '''
        start_watch(self.inotify, self.callback)


