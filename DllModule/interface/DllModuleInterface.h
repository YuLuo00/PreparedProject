#ifndef DLLMODULE_INTERFACE_H
#define DLLMODULE_INTERFACE_H

#ifdef DLL_MODULE_EXPORT
#define DLL_MODULE_API __declspec(dllexport)
#else
#define DLL_MODULE_API __declspec(dllimport)

#endif // DLL_MODULE_EXPORT


DLL_MODULE_API void test_function();

#endif // !DLLMODULE_INTERFACE_H


