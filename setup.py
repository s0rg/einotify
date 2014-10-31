#!/usr/bin/env python

import distutils.core
import distutils.util


platform = distutils.util.get_platform()

if not platform.startswith('linux'):
    raise Exception('inotify is linux-specific, and does not work on %s' %
                    platform)

distutils.core.setup(
    name='einotify',
    version='0.1.0',
    description='Interface to Linux inotify subsystem',
    author="s0rg",
    author_email='al3x.s0rg@gmail.com',
    license='SimPL-2.0',
    platforms='Linux',
    packages=['einotify'],
    url='https://github.com/s0rg/einotify',
    classifiers=['Development Status :: Stable',
                 'Intended Audience :: Developers',
                 'License :: OSI Approved :: Simple Public License (SimPL-2.0)',
                 'Operating System :: POSIX :: Linux',
                 'Programming Language :: Python',
                 'Programming Language :: Python :: 2',
                 'Topic :: System :: Filesystems',
                 'Topic :: System :: Monitoring'],
    ext_modules=[distutils.core.Extension('einotify._einotify',
                                          ['einotify/_einotify.c'],
                                          extra_compile_args=['-std=c99']
                                          )],
)

