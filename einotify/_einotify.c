/*
 *  DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *          Version 2, December 2004
 *
 *  Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 *
 *  Everyone is permitted to copy and distribute verbatim or modified
 *  copies of this license document, and changing it is allowed as long
 *  as the name is changed.
 *
 *  DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *  TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 *
 * */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/epoll.h>
#include <sys/inotify.h>

#include <Python.h>


#define MAX_POLL_EVENTS  20
#define DEFAULT_BUF_SIZE 8196
#define WAIT_FOREVER     -1

int
init_epoll(int inotify_fd)
{
    int fd;
    struct epoll_event ev;

    do {
        fd = epoll_create(1); // dummy value, MUST be > 0, see
                              // http://man7.org/linux/man-pages/man2/epoll_create.2.html 
                              // for details
        if (-1 == fd) {
            PyErr_SetFromErrno(PyExc_IOError);
            break;
        }

        ev.data.fd = inotify_fd;
        ev.events = EPOLLIN | EPOLLET;

        if (-1 != epoll_ctl(fd, EPOLL_CTL_ADD, inotify_fd, &ev)) {
            break; // success
        }

        PyErr_SetFromErrno(PyExc_IOError);
       
        close(fd);
        close(inotify_fd);

        fd = -1;

    } while (false);

    return fd;
}


static PyObject *
start_watch(PyObject *self, PyObject *args) {

    int      buffer_size = DEFAULT_BUF_SIZE;
    int      inotify_fd, epoll_fd, len, nfds;

    struct   epoll_event events[MAX_POLL_EVENTS];
    char     *buf = NULL;
    static   PyObject *callback = NULL;
    PyObject *result;
    PyObject *arglist;
    
    if (!PyArg_ParseTuple(args, "iO|i:watch_start", &inotify_fd, &callback, &buffer_size)) {
        return NULL;
    }

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "'callback' must be callable");
        return NULL;
    }

    Py_XINCREF(callback);

    Py_BEGIN_ALLOW_THREADS;
    epoll_fd = init_epoll(inotify_fd);
    Py_END_ALLOW_THREADS;

    if (-1 == epoll_fd) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    buf = calloc(buffer_size, sizeof(char));
    if (NULL == buf) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    bool running = true;

    /* micro reactor */
    while (running)
    {
        Py_BEGIN_ALLOW_THREADS;
        nfds = epoll_wait(epoll_fd, events, MAX_POLL_EVENTS, WAIT_FOREVER);
        Py_END_ALLOW_THREADS;

        if (-1 == nfds && errno != EINTR) {
            PyErr_SetFromErrno(PyExc_IOError);
            break;
        }

        if (nfds <= 0) { // timed-out, strange...
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd != inotify_fd) {
                continue;
            }

            Py_BEGIN_ALLOW_THREADS;
            len = read(inotify_fd, buf, buffer_size);
            Py_END_ALLOW_THREADS;

            if (-1 == len && errno != EAGAIN) {
                running = false;
                PyErr_SetFromErrno(PyExc_IOError);
                break;
            }
            
            if (len <= 0) {
                break;
            }
            
            const struct inotify_event *event;
            char *max_ptr = buf + len;

            for (char *ptr = buf; ptr < max_ptr;
                        ptr += (sizeof(struct inotify_event) + event->len)) {

               event = (const struct inotify_event *) ptr;

               PyGILState_STATE gstate = PyGILState_Ensure();

               arglist = Py_BuildValue("iiis" , event->wd, 
                                                event->mask, 
                                                event->cookie, 
                                                (event->len) ? event->name : NULL );
               result = PyObject_CallObject(callback, arglist);
               Py_DECREF(arglist);

               PyGILState_Release(gstate);

               if (result == NULL)
                   return NULL; /* Pass error back */

               Py_DECREF(result);
            }
        }
    }

    /* clean-up */
    Py_XDECREF(callback);

    free( buf );

    close( epoll_fd );
    close( inotify_fd );

    Py_RETURN_NONE;
}


static PyObject* 
init_notify(PyObject *self, PyObject *args) {
    int fd;

    Py_BEGIN_ALLOW_THREADS;
    fd = inotify_init1(IN_NONBLOCK);
    Py_END_ALLOW_THREADS;

    if (-1 == fd) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }
    
    return Py_BuildValue("i", fd);
}


static PyObject* 
add_watch(PyObject *self, PyObject *args) {
    int fd, wd;
    char *path;
    uint32_t mask = IN_ALL_EVENTS;

    if (!PyArg_ParseTuple(args, "is|i:add_watch", &fd, &path, &mask)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS;
    wd = inotify_add_watch(fd, (const char *)path, mask);
    Py_END_ALLOW_THREADS;

    if (-1 == wd) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    return Py_BuildValue("i", wd);
}


static PyObject* 
del_watch(PyObject *self, PyObject *args) {
    int fd, wd;
    int retvalue;

    if (!PyArg_ParseTuple(args, "ii:rm_watch", &fd, &wd)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS;
    retvalue = inotify_rm_watch(fd, wd);
    Py_END_ALLOW_THREADS;

    if (-1 == retvalue) {
        PyErr_SetFromErrno(PyExc_IOError);
        return NULL;
    }

    return Py_BuildValue("i", retvalue);
}


static PyMethodDef methods[] = {
  {
    "init_notify",
    init_notify,
    METH_NOARGS,
    (
      "init_notify()\n\n"
      "Initialize an inotify instance and return the associated file\n"
      "descriptor."
    )
  },
  {
    "add_watch",
    add_watch,
    METH_VARARGS,
    (
      "add_watch(fd, path[, mask=IN_ALL_EVENTS])\n\n"
      "Add a watch for path and return the watch descriptor.\n"
      "fd should be the file descriptor returned by init.\n"
      "If left unspecified, mask defaults to IN_ALL_EVENTS.\n"
      "See the inotify documentation for details."
    )
  },
  {
    "del_watch",
    del_watch,
    METH_VARARGS,
    (
      "del_watch(fd, wd)\n\n"
      "Remove the watch associated with watch descriptor wd.\n"
      "fd should be the file descriptor returned by init.\n"
    )
  },
  {
     "start_watch",
     start_watch,
     METH_VARARGS,
     (
        "start_watch(fd, callback, [buffer_size=8196])\n\n"
        "fd should be the file descriptor returned by init_notify.\n"
        "callback - callback function with signature: fn(wd, mask, cookie, name)\n"
        " see inotify docs: http://man7.org/linux/man-pages/man7/inotify.7.html\n"
        "buffer_size - optional arg, size for read buffer, can be tuned in 'heavy load' cases\n"
        "be prepared for endless blocking upon call, it acts as reactor"
    )
  },
  {NULL, NULL, 0, NULL}
};


PyMODINIT_FUNC init_einotify(void) {
    PyObject* module = Py_InitModule3(
      "_einotify",
      methods,
      "Low-level interface to inotify"
    );

    if ( NULL == module )
        return;
 
    // Make sure the GIL has been created since we need to acquire it in our
    if ( !PyEval_ThreadsInitialized() ) {
        PyEval_InitThreads();
    }
   
    PyModule_AddIntConstant(module, "IN_ACCESS", IN_ACCESS);
    PyModule_AddIntConstant(module, "IN_MODIFY", IN_MODIFY);
    PyModule_AddIntConstant(module, "IN_ATTRIB", IN_ATTRIB);
    PyModule_AddIntConstant(module, "IN_CLOSE_WRITE", IN_CLOSE_WRITE);
    PyModule_AddIntConstant(module, "IN_CLOSE_NOWRITE", IN_CLOSE_NOWRITE);
    PyModule_AddIntConstant(module, "IN_CLOSE", IN_CLOSE);
    PyModule_AddIntConstant(module, "IN_OPEN", IN_OPEN);
    PyModule_AddIntConstant(module, "IN_MOVED_FROM", IN_MOVED_FROM);
    PyModule_AddIntConstant(module, "IN_MOVED_TO", IN_MOVED_TO);
    PyModule_AddIntConstant(module, "IN_MOVE", IN_MOVE);
    PyModule_AddIntConstant(module, "IN_CREATE", IN_CREATE);
    PyModule_AddIntConstant(module, "IN_DELETE", IN_DELETE);
    PyModule_AddIntConstant(module, "IN_DELETE_SELF", IN_DELETE_SELF);
    PyModule_AddIntConstant(module, "IN_MOVE_SELF", IN_MOVE_SELF);
    PyModule_AddIntConstant(module, "IN_UNMOUNT", IN_UNMOUNT);
    PyModule_AddIntConstant(module, "IN_Q_OVERFLOW", IN_Q_OVERFLOW);
    PyModule_AddIntConstant(module, "IN_IGNORED", IN_IGNORED);
    PyModule_AddIntConstant(module, "IN_ONLYDIR", IN_ONLYDIR);
    PyModule_AddIntConstant(module, "IN_DONT_FOLLOW", IN_DONT_FOLLOW);
    PyModule_AddIntConstant(module, "IN_MASK_ADD", IN_MASK_ADD);
    PyModule_AddIntConstant(module, "IN_ISDIR", IN_ISDIR);
    PyModule_AddIntConstant(module, "IN_ONESHOT", IN_ONESHOT);
    PyModule_AddIntConstant(module, "IN_ALL_EVENTS", IN_ALL_EVENTS);
}

