/*
* Windows kernel-mode driver for controlling access to various input devices.
*
* MIT License
*
* Copyright (c) 2016-2019 Nefarius Software Solutions e.U.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/


#pragma once

#define PID_LIST_TAG    'LPGH'
#define SYSTEM_PID      0x04

#ifndef _KERNEL_MODE
#include <stdlib.h>
#endif

typedef struct _PID_LIST_NODE
{
    ULONG Pid;

    BOOLEAN IsAllowed;

    struct _PID_LIST_NODE* next;

} PID_LIST_NODE, *PPID_LIST_NODE;

PPID_LIST_NODE FORCEINLINE PID_LIST_CREATE()
{
    PPID_LIST_NODE head;

#ifdef _KERNEL_MODE
    head = ExAllocatePoolWithTag(NonPagedPool, sizeof(PID_LIST_NODE), PID_LIST_TAG);
#else
    head = (PPID_LIST_NODE)malloc(sizeof(PID_LIST_NODE));
#endif

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

    if (current == NULL)
        return;

    while (current->next != NULL) {
        temp_node = current;
        current = current->next;
#ifdef _KERNEL_MODE
        ExFreePoolWithTag(temp_node, PID_LIST_TAG);
#else
        free(temp_node);
#endif
    }
}

BOOLEAN FORCEINLINE PID_LIST_PUSH(PID_LIST_NODE ** head, ULONG pid, BOOLEAN allowed)
{
    PPID_LIST_NODE new_node;

    if (*head == NULL)
        return FALSE;

#ifdef _KERNEL_MODE
    new_node = ExAllocatePoolWithTag(NonPagedPool, sizeof(PID_LIST_NODE), PID_LIST_TAG);
#else
    new_node = (PPID_LIST_NODE)malloc(sizeof(PID_LIST_NODE));
#endif

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

    if (current == NULL)
        return FALSE;

    if (pid == SYSTEM_PID)
        return FALSE;

    //
    // Search for PID
    // 
    while (current->next != NULL) {
        if (current->Pid == pid) {
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
#ifdef _KERNEL_MODE
    ExFreePoolWithTag(temp_node, PID_LIST_TAG);
#else
    free(temp_node);
#endif

    return TRUE;
}

BOOLEAN FORCEINLINE PID_LIST_CONTAINS(PID_LIST_NODE ** head, ULONG pid, BOOLEAN* allowed)
{
    PPID_LIST_NODE current = *head;

    if (current == NULL)
        return FALSE;

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
