#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "HidCerberus.h"
#include "ZerberusService.h"
#include "HidCerberusInternal.h"


typedef struct _HC_ARE_HANDLE_T
{
	ULONG RequestId;

} HC_ARE_HANDLE;

HC_API PHC_HANDLE hc_init()
{
    auto handle = new HC_HANDLE;

    if (!handle) {
        return nullptr;
    }

    handle->MainManager = new TaskManager();
    handle->MainManager->start(new ZerberusService(handle));

    return handle;
}

HC_API VOID hc_shutdown(PHC_HANDLE handle)
{
    if (!handle) {
        return;
    }       

    handle->MainManager->cancelAll();
    handle->MainManager->joinAll();

    delete handle;
}

HC_API VOID hc_register_access_request_event(PHC_HANDLE handle, PFN_HC_PROCESS_ACCESS_REQUEST callback)
{
    handle->EvtProcessAccessRequest = callback;
}

HC_API VOID hc_submit_access_request_result(PHC_ARE_HANDLE Handle, BOOL IsAllowed, BOOL IsPermanent)
{
	return;
}
