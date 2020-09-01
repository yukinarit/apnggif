#include <Python.h>
#include "apng2gif.h"

static PyObject* py_apng2gif(PyObject *self, PyObject *args) {
  char *png, *gif, *bcolor = NULL;
  int tlevel = -1;

  if (!PyArg_ParseTuple(args, "ssis", &png, &gif, &tlevel, &bcolor)) {
        return NULL;
  }

  int ret = apng2gif(png, gif, tlevel, bcolor);

  return PyLong_FromLong(ret);
}

static PyMethodDef methods[] = {
  {"apnggif", py_apng2gif, METH_VARARGS, "Python interface for apng2gif"},
  {NULL, NULL, 0, NULL}
};

static struct PyModuleDef apnggif_sys = {
  PyModuleDef_HEAD_INIT,
  "apnggif_sys",
  "Python interface for apng2gif C library",
  -1,
  methods
};

PyMODINIT_FUNC PyInit_apnggif_sys() {
    return PyModule_Create(&apnggif_sys);
}
