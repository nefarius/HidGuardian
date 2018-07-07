#include "HidCerberus.h"

typedef struct _HC_HANDLE_T
{
} HC_HANDLE;

HC_API PHC_HANDLE hc_init()
{
    return HC_API PHC_HANDLE();
}

HC_API VOID hc_shutdown(PHC_HANDLE handle)
{
    return HC_API VOID();
}

HC_API VOID hc_register_access_request_event(PHC_HANDLE handle, PFN_HC_ACCESS_REQUEST callback)
{
    return HC_API VOID();
}
