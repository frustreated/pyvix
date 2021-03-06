#include "pyvix.h"

static int        PyVixHost_Type_init    ( PyVixHost * self, PyObject * args, PyObject * kwds );
static void       PyVixHost_Type_dealloc ( PyVixHost * self );

static PyObject * PyVixHost_Disconnect ( PyVixHost * self );
static PyObject * PyVixHost_Running    ( PyVixHost * self );
static PyObject * PyVixHost_Registered ( PyVixHost * self );
static PyObject * PyVixHost_Register   ( PyVixHost * self , PyObject * path );
static PyObject * PyVixHost_Unregister ( PyVixHost * self , PyObject * path );
static PyObject * PyVixHost_Open       ( PyVixHost * self , PyObject * path );
static PyObject * PyVixHost_Property   ( PyVixHost * self , PyObject * prop );


static PyMethodDef PyVixHost_methods[] = {
    {
        "disconnect", (PyCFunction)PyVixHost_Disconnect, METH_NOARGS,
        "disconnect() -> None\n"
        "  disconnect from host"
    },{
        "running",    (PyCFunction)PyVixHost_Running,    METH_NOARGS,
        "running() -> []\n"
        "  return list of running VMs"
    },{
        "registered", (PyCFunction)PyVixHost_Registered, METH_NOARGS,
        "registered() -> []\n"
        "  return list of registered VMs"
    },{
        "register",   (PyCFunction)PyVixHost_Register,   METH_O,
        "register(vmxpath) -> None\n"
        "  register a VM with VMware Server"
    },{
        "unregister", (PyCFunction)PyVixHost_Unregister, METH_O,
        "unregister(vmxpath) -> None\n"
        "  unregister a VM with VMware Server"
    },{
        "open",       (PyCFunction)PyVixHost_Open,       METH_O,
        "open(vmxpath) -> pyvix.vm\n"
        "  open a vmx and return a VM object"
    },{
        "property",   (PyCFunction)PyVixHost_Property,   METH_O,
        "property(property_id) -> value\n"
        "  read a property value"
    },{
        NULL
    }
};


PyTypeObject PyVixHost_Type = {
    PyVarObject_HEAD_INIT(0, 0)
    "pyvix.host",                         // name
    sizeof(PyVixHost),                    // basicsize
    0,                                    // itemsize
    (destructor)PyVixHost_Type_dealloc,   // dealloc
    0,                                    // print
    0,                                    // getattr
    0,                                    // setattr
    0,                                    // compare
    0,                                    // repr
    0,                                    // as_number
    0,                                    // as_sequence
    0,                                    // as_mapping
    0,                                    // hash
    0,                                    // call
    0,                                    // str
    0,                                    // getattro
    0,                                    // setattro
    0,                                    // as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    "PyVix Host Class",                   // doc
    0,                                    // traverse
    0,                                    // clear
    0,                                    // richcompare
    0,                                    // weaklistoffset
    0,                                    // iter
    0,                                    // iternext
    PyVixHost_methods,                    // methods
    0,                                    // members
    0,                                    // getset
    0,                                    // base
    0,                                    // dict
    0,                                    // descr_get
    0,                                    // descr_set
    0,                                    // dictoffset
    (initproc)PyVixHost_Type_init,        // init
    0,                                    // alloc
    PyType_GenericNew,                    // new
};


static int PyVixHost_Type_init(PyVixHost *self, PyObject *args, PyObject *kwds) {
    self->host = VIX_INVALID_HANDLE;
    return 0;
}


static void PyVixHost_Type_dealloc(PyVixHost *self) {
    if(VIX_INVALID_HANDLE != self->host) {
        VixHost_Disconnect(self->host);
        self->host = VIX_INVALID_HANDLE;
    }

    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyObject * PyVixHost_Disconnect(PyVixHost *self) {
    VixHost_Disconnect(self->host);
    self->host = VIX_INVALID_HANDLE;

    Py_RETURN_NONE;
}


static PyObject * PyVixHost_Running(PyVixHost *self) {
    PyObject *list;
    VixHandle job;
    VixError error;

    list = PyList_New(0);

    if(VIX_INVALID_HANDLE == self->host) {
        return list;
    }

    job = VixHost_FindItems(
        self->host,
        VIX_FIND_RUNNING_VMS,
        VIX_INVALID_HANDLE,
        -1,
        VixDiscoveryProc,
        list
        );

    Py_BEGIN_ALLOW_THREADS
    error = VixJob_Wait(job, VIX_PROPERTY_NONE);
    Py_END_ALLOW_THREADS
    if(VIX_OK != error) {
        Py_DECREF(list);
        PyErr_SetString(PyVix_Error, Vix_GetErrorText(error, NULL));
        return NULL;
    }

    return list;
}


static PyObject * PyVixHost_Registered(PyVixHost *self) {
    PyObject *list;
    VixHandle job;
    VixError error;

    list = PyList_New(0);

    if(VIX_INVALID_HANDLE == self->host) {
        return list;
    }

    job = VixHost_FindItems(
        self->host,
        VIX_FIND_REGISTERED_VMS,
        VIX_INVALID_HANDLE,
        -1,
        VixDiscoveryProc,
        list
        );

    error = VixJob_Wait(job, VIX_PROPERTY_NONE);
    if(VIX_OK != error) {
        Py_DECREF(list);
        PyErr_SetString(PyVix_Error, Vix_GetErrorText(error, NULL));
        return NULL;
    }

    return list;
}


static PyObject * PyVixHost_Register(PyVixHost *self, PyObject *path) {
    VixHandle job;
    VixError error;
    char *vmx;
    int ok;

    vmx = NULL;

    ok = PyUnicode_Check(path);
    if(FALSE == ok) {
        PyErr_SetString(PyExc_TypeError, "Value must be a string.");
        return NULL;
    }

    vmx = PyUnicode_AsUTF8(path);

    job = VixHost_RegisterVM(
        self->host,
        vmx,
        NULL,
        NULL
        );

    Py_BEGIN_ALLOW_THREADS
        error = VixJob_Wait(job, VIX_PROPERTY_NONE);
    Py_END_ALLOW_THREADS

    Vix_ReleaseHandle(job);

    if(VIX_FAILED(error)) {
        PyErr_SetString(PyVix_Error, "Could not register VM");
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject * PyVixHost_Unregister(PyVixHost *self, PyObject *path) {
    VixHandle job;
    VixError error;
    char *vmx;
    int ok;

    vmx = NULL;

    ok = PyUnicode_Check(path);
    if(FALSE == ok) {
        PyErr_SetString(PyExc_TypeError, "Value must be a string.");
        return NULL;
    }

    vmx = PyUnicode_AsUTF8(path);

    job = VixHost_UnregisterVM(
        self->host,
        vmx,
        NULL,
        NULL
        );

    Py_BEGIN_ALLOW_THREADS
        error = VixJob_Wait(job, VIX_PROPERTY_NONE);
    Py_END_ALLOW_THREADS

    Vix_ReleaseHandle(job);

    if(VIX_FAILED(error)) {
        PyErr_SetString(PyVix_Error, "Could not register VM");
        return NULL;
    }

    Py_RETURN_NONE;
}


static PyObject * PyVixHost_Open(PyVixHost *self, PyObject *path) {
    PyVixVM *pyvixvm;
    VixHandle vm;
    VixHandle job;
    VixError error;
    char *vmx;
    int ok;

    vmx = NULL;

    if(VIX_INVALID_HANDLE == self->host) {
        Py_RETURN_NONE;
    }

    ok = PyUnicode_Check(path);
    if(FALSE == ok) {
        PyErr_SetString(PyExc_TypeError, "Value must be a string.");
        return NULL;
    }

    vmx = PyUnicode_AsUTF8(path);

    job = VixHost_OpenVM(
        self->host,
        vmx,
        VIX_VMOPEN_NORMAL,
        VIX_INVALID_HANDLE,
        NULL,
        NULL
        );

    Py_BEGIN_ALLOW_THREADS
        error = VixJob_Wait(job, VIX_PROPERTY_JOB_RESULT_HANDLE, &vm, VIX_PROPERTY_NONE);
    Py_END_ALLOW_THREADS

    Vix_ReleaseHandle(job);

    if(VIX_FAILED(error)) {
        PyErr_SetString(PyVix_Error, "Could not open VM");
        return NULL;
    }

    // allocate a pyvixvm object
    pyvixvm = (PyVixVM *)PyObject_CallObject((PyObject *)&PyVixVM_Type, NULL);
    if(NULL == pyvixvm) {
        PyErr_SetString(PyVix_Error, "Could not create pyvix.vm object");
        return NULL;
    }

    pyvixvm->vm = vm;

    return (PyObject *)pyvixvm;
}


static PyObject * PyVixHost_Property(PyVixHost *self, PyObject *prop) {
    return _PyVix_GetProperty(self->host, prop);
}
