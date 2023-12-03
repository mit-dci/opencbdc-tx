#include "pyutil.hpp"

#include <Python.h>
#include <stdio.h>

namespace cbdc::parsec::pyutils {
    auto formContract(const std::string& filename,
                      const std::string& contract_formatter,
                      const std::string& funcname) -> std::string {
        // Leveraging Python's convenient ability to manipulate strings
        Py_Initialize();

        PyObject* main = PyImport_AddModule("__main__");

        PyObject* globalDictionary = PyModule_GetDict(main);
        PyObject* localDictionary = PyDict_New();

        // This module is required by pythonContractConverter.py. This is how
        // Python modules can be imported into the Python VM
        PyImport_ImportModuleEx("re", globalDictionary, localDictionary, NULL);

        PyObject* obj = Py_BuildValue("s", contract_formatter.c_str());
        FILE* file = _Py_fopen_obj(obj, "r");

        PyObject* value = PyUnicode_FromString(filename.c_str());
        PyDict_SetItemString(localDictionary, "file", value);
        value = PyUnicode_FromString(funcname.c_str());
        PyDict_SetItemString(localDictionary, "funcname", value);
        value = PyUnicode_FromString("");

        // The variable being named "contract" is due to its implementation in
        // pythonContractConverter.py. This should perhaps be an input
        // argument.
        PyDict_SetItemString(localDictionary, "contract", value);

        if(file != NULL) {
            [[maybe_unused]] auto r = PyRun_File(file,
                                                 contract_formatter.c_str(),
                                                 Py_file_input,
                                                 globalDictionary,
                                                 localDictionary);
            auto word = PyDict_GetItemString(localDictionary, "contract");
            char* res;
            if(PyUnicode_Check(word)) {
                res = PyBytes_AS_STRING(
                    PyUnicode_AsEncodedString(word, "UTF-8", "strict"));
            } else {
                res = PyBytes_AsString(word);
            }
            if(res) {
                Py_Finalize();
                return std::string(res);
            } else {
                Py_Finalize();
                return "";
            }
        } else {
            /// \todo We should interpret this as an error
            Py_Finalize();
            return "";
        }
    }
}
