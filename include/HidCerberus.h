/*
MIT License

Copyright (c) 2018 Benjamin "Nefarius" Höglinger

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#ifndef HidCerberus_h__
#define HidCerberus_h__


#ifdef HC_EXPORTS
#define HC_API __declspec(dllexport)
#else
#define HC_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _HC_HANDLE_T *PHC_HANDLE;

    typedef
        _Function_class_(EVT_HC_PROCESS_ACCESS_REQUEST)
        BOOL
        EVT_HC_PROCESS_ACCESS_REQUEST(
            PCSTR HardwareId,
            PCSTR DeviceId,
            PCSTR InstanceId,
            DWORD ProcessId,
            const BOOL *bIsAllowed,
            const BOOL *bIsPermanent
        );

    typedef EVT_HC_PROCESS_ACCESS_REQUEST *PFN_HC_PROCESS_ACCESS_REQUEST;

    HC_API PHC_HANDLE hc_init();

    HC_API VOID hc_shutdown(PHC_HANDLE handle);

    HC_API VOID hc_register_access_request_event(PHC_HANDLE handle, PFN_HC_PROCESS_ACCESS_REQUEST callback);

#ifdef __cplusplus
}
#endif

#endif // HidCerberus_h__
