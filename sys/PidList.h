#pragma once


#define PID_LIST_TAG    'LPGH'
#define SYSTEM_PID      0x04

typedef struct _PID_LIST_NODE
{
    ULONG Pid;

    BOOLEAN IsAllowed;

    struct _PID_LIST_NODE* next;

} PID_LIST_NODE, *PPID_LIST_NODE;

PPID_LIST_NODE FORCEINLINE PID_LIST_CREATE()
{
    PPID_LIST_NODE head = ExAllocatePoolWithTag(NonPagedPool, sizeof(PID_LIST_NODE), PID_LIST_TAG);

    if (head == NULL) {
        return head;
    }

    RtlZeroMemory(head, sizeof(PID_LIST_NODE));

    return head;
}

VOID FORCEINLINE PID_LIST_DESTROY(PID_LIST_NODE ** head)
{
    PPID_LIST_NODE current = *head;
    PPID_LIST_NODE temp_node = NULL;

    while (current->next != NULL) {
        temp_node = current;
        current = current->next;
        ExFreePoolWithTag(temp_node, PID_LIST_TAG);
    }
}

BOOLEAN FORCEINLINE PID_LIST_PUSH(PID_LIST_NODE ** head, ULONG pid, BOOLEAN allowed)
{
    PPID_LIST_NODE new_node;

    new_node = ExAllocatePoolWithTag(NonPagedPool, sizeof(PID_LIST_NODE), PID_LIST_TAG);

    if (new_node == NULL) {
        return FALSE;
    }

    new_node->Pid = pid;
    new_node->IsAllowed = allowed;
    new_node->next = *head;
    *head = new_node;

    return TRUE;
}

BOOLEAN FORCEINLINE PID_LIST_REMOVE_BY_PID(PID_LIST_NODE ** head, ULONG pid)
{
    PPID_LIST_NODE current = *head;
    PPID_LIST_NODE temp_node = NULL;

    //
    // Search for PID
    // 
    while (current->next != NULL) {
        if (current->Pid == pid && pid != SYSTEM_PID) {
            break;
        }
        current = current->next;
    }

    if (current->next == NULL) {
        //
        // PID wasn't found in the list
        // 
        return FALSE;
    }

    //
    // Re-link list and free disposed node
    // 
    temp_node = current->next;
    current->next = temp_node->next;
    ExFreePoolWithTag(temp_node, PID_LIST_TAG);

    return TRUE;
}

BOOLEAN FORCEINLINE PID_LIST_CONTAINS(PID_LIST_NODE ** head, ULONG pid, BOOLEAN* allowed)
{
    PPID_LIST_NODE current = *head;

    //
    // Search for PID
    // 
    while (current->next != NULL) {
        if (current->Pid == pid) {
            if (allowed != NULL) {
                *allowed = current->IsAllowed;
            }
            return TRUE;
        }
        current = current->next;
    }

    return FALSE;
}
