#include "precomp.h"
#include "IoCtl.h"

#pragma hdrstop

//#pragma NDIS_INIT_FUNCTION(DriverEntry)
CIpFilter*			g_Filter;
NDIS_HANDLE         ProtHandle = NULL;
NDIS_HANDLE         DriverHandle = NULL;
NDIS_MEDIUM         MediumArray[4] =
                    {
                        NdisMedium802_3,    // Ethernet
                        NdisMedium802_5,    // Token-ring
                        NdisMediumFddi,     // Fddi
                        NdisMediumWan       // NDISWAN
                    };

NDIS_SPIN_LOCK     GlobalLock;

PADAPT             pAdaptList = NULL;
LONG               MiniportCount = 0;

NDIS_HANDLE        NdisWrapperHandle;

//
// To support ioctls from user-mode:
//

#define LINKNAME_STRING     L"\\DosDevices\\Ndis 5.1 Fire Wall"
#define NTDEVICE_STRING     L"\\Device\\Ndis 5.1 Fire Wall"

NDIS_HANDLE     NdisDeviceHandle = NULL;
PDEVICE_OBJECT  ControlDeviceObject = NULL;

enum _DEVICE_STATE
{
    PS_DEVICE_STATE_READY = 0,    // ready for create/delete
    PS_DEVICE_STATE_CREATING,    // create operation in progress
    PS_DEVICE_STATE_DELETING    // delete operation in progress
} ControlDeviceState = PS_DEVICE_STATE_READY;

typedef struct _KESERVICE_DESCRIPTOR_TABLE
{
	PULONG ServiceTableBase;
	PULONG ServiceCounterTableBase;
	ULONG NumberOfServices;
	PUCHAR ParamTableBase;
}KESERVICE_DESCRIPTOR_TABLE, *PKESERVICE_DESCRIPTOR_TABLE;

extern "C" extern PKESERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;

//关闭页面保护
void PageProtectClose()
{
	__asm {
		cli
		mov eax, cr0
		and eax, not 10000h
		mov cr0, eax
	}
}

//启用页面保护
void PageProtectOpen()
{
	__asm {
		mov eax, cr0
		or eax, 10000h
		mov cr0, eax
		sti
	}
}
#define SYSTEMCALL_INDEX(ServiceFunction) (*(PULONG)((PUCHAR)ServiceFunction + 1))

typedef NTSTATUS(NTAPI * _NtOpenProcess)(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId);

_NtOpenProcess oldNtOpenProcess = NULL;

HANDLE g_hProtectProcId = 0;

NTSTATUS NTAPI MyNtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId)
{
	if (ExGetPreviousMode() == UserMode)
	{
		if (ClientId->UniqueProcess == g_hProtectProcId)
			return STATUS_ACCESS_DENIED;
	}
	return oldNtOpenProcess(ProcessHandle,  DesiredAccess,  ObjectAttributes,  ClientId);
}

extern "C" NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT        DriverObject,
    IN PUNICODE_STRING       RegistryPath
    )
/*++

Routine Description:

    First entry point to be called, when this driver is loaded.
    Register with NDIS as an intermediate driver.

Arguments:

    DriverObject - pointer to the system's driver object structure
        for this driver
    
    RegistryPath - system's registry path for this driver
    
Return Value:

    STATUS_SUCCESS if all initialization is successful, STATUS_XXX
    error code if not.

--*/
{
    NDIS_STATUS                        Status;
    NDIS_PROTOCOL_CHARACTERISTICS      PChars;
    NDIS_MINIPORT_CHARACTERISTICS      MChars;
    NDIS_STRING                        Name;
	
	PageProtectClose();
	//得到原来的地址，记录在 oldNtOpenProcess
	oldNtOpenProcess = (_NtOpenProcess)KeServiceDescriptorTable->ServiceTableBase[SYSTEMCALL_INDEX(ZwOpenProcess)];
	//修改SSDT中 NtOpenProcess 的地址，使其指向 MyNtOpenProcess
	KeServiceDescriptorTable->ServiceTableBase[SYSTEMCALL_INDEX(ZwOpenProcess)] = (ULONG)&MyNtOpenProcess;
	 
	PageProtectOpen();
	 
    Status = NDIS_STATUS_SUCCESS;
    NdisAllocateSpinLock(&GlobalLock);
	
	g_Filter = new(NonPagedPool) CIpFilter;

    NdisMInitializeWrapper(&NdisWrapperHandle, DriverObject, RegistryPath, NULL);
  
    do
    {
        //
        // Register the miniport with NDIS. Note that it is the miniport
        // which was started as a driver and not the protocol. Also the miniport
        // must be registered prior to the protocol since the protocol's BindAdapter
        // handler can be initiated anytime and when it is, it must be ready to
        // start driver instances.
        //

        NdisZeroMemory(&MChars, sizeof(NDIS_MINIPORT_CHARACTERISTICS));
		
        MChars.Ndis50Chars.MajorNdisVersion = MAJOR_NDIS_VERSION;
        MChars.Ndis50Chars.MinorNdisVersion = MINOR_NDIS_VERSION;

        MChars.Ndis50Chars.InitializeHandler = MPInitialize;
        MChars.Ndis50Chars.QueryInformationHandler = MPQueryInformation;
        MChars.Ndis50Chars.SetInformationHandler = MPSetInformation;
        MChars.Ndis50Chars.ResetHandler = NULL;
        MChars.Ndis50Chars.TransferDataHandler = MPTransferData;
        MChars.Ndis50Chars.HaltHandler = MPHalt;
#ifdef NDIS51_MINIPORT
        MChars.CancelSendPacketsHandler = MPCancelSendPackets;
        MChars.PnPEventNotifyHandler = MPDevicePnPEvent;
        MChars.AdapterShutdownHandler = MPAdapterShutdown;
#endif // NDIS51_MINIPORT

        //
        // We will disable the check for hang timeout so we do not
        // need a check for hang handler!
        //
        MChars.Ndis50Chars.CheckForHangHandler = NULL;
        MChars.Ndis50Chars.ReturnPacketHandler = MPReturnPacket;

        //
        // Either the Send or the SendPackets handler should be specified.
        // If SendPackets handler is specified, SendHandler is ignored
        //
        MChars.Ndis50Chars.SendHandler = NULL;    // MPSend;
        MChars.Ndis50Chars.SendPacketsHandler = MPSendPackets;

        Status = NdisIMRegisterLayeredMiniport(NdisWrapperHandle,
                                                  &MChars,
                                                  sizeof(MChars),
                                                  &DriverHandle);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }

#ifndef WIN9X
        NdisMRegisterUnloadHandler(NdisWrapperHandle, PtUnload);
#endif

        //
        // Now register the protocol.
        //
        NdisZeroMemory(&PChars, sizeof(NDIS_PROTOCOL_CHARACTERISTICS));
        PChars.Ndis40Chars.MajorNdisVersion = PROT_MAJOR_NDIS_VERSION;
        PChars.Ndis40Chars.MinorNdisVersion = PROT_MINOR_NDIS_VERSION;

        //
        // Make sure the protocol-name matches the service-name
        // (from the INF) under which this protocol is installed.
        // This is needed to ensure that NDIS can correctly determine
        // the binding and call us to bind to miniports below.
        //
        NdisInitUnicodeString(&Name, L"passthru");    // Protocol name
        PChars.Ndis40Chars.Name = Name;
        PChars.Ndis40Chars.OpenAdapterCompleteHandler = PtOpenAdapterComplete;
        PChars.Ndis40Chars.CloseAdapterCompleteHandler = PtCloseAdapterComplete;
        PChars.Ndis40Chars.SendCompleteHandler = PtSendComplete;
        PChars.Ndis40Chars.TransferDataCompleteHandler = PtTransferDataComplete;
    
        PChars.Ndis40Chars.ResetCompleteHandler = PtResetComplete;
        PChars.Ndis40Chars.RequestCompleteHandler = PtRequestComplete;
        PChars.Ndis40Chars.ReceiveHandler = PtReceive;
        PChars.Ndis40Chars.ReceiveCompleteHandler = PtReceiveComplete;
        PChars.Ndis40Chars.StatusHandler = PtStatus;
        PChars.Ndis40Chars.StatusCompleteHandler = PtStatusComplete;
        PChars.Ndis40Chars.BindAdapterHandler = PtBindAdapter;
        PChars.Ndis40Chars.UnbindAdapterHandler = PtUnbindAdapter;
        PChars.Ndis40Chars.UnloadHandler = PtUnloadProtocol;

        PChars.Ndis40Chars.ReceivePacketHandler = PtReceivePacket;
        PChars.Ndis40Chars.PnPEventHandler= PtPNPHandler;

        NdisRegisterProtocol(&Status,
                             &ProtHandle,
                             &PChars,
                             sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

        if (Status != NDIS_STATUS_SUCCESS)
        {
            NdisIMDeregisterLayeredMiniport(DriverHandle);
            break;
        }

        NdisIMAssociateMiniport(DriverHandle, ProtHandle);
    }
    while (FALSE);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        NdisTerminateWrapper(NdisWrapperHandle, NULL);
    }

    return(Status);
}


NDIS_STATUS
PtRegisterDevice(
    VOID
    )
/*++

Routine Description:

    Register an ioctl interface - a device object to be used for this
    purpose is created by NDIS when we call NdisMRegisterDevice.

    This routine is called whenever a new miniport instance is
    initialized. However, we only create one global device object,
    when the first miniport instance is initialized. This routine
    handles potential race conditions with PtDeregisterDevice via
    the ControlDeviceState and MiniportCount variables.

    NOTE: do not call this from DriverEntry; it will prevent the driver
    from being unloaded (e.g. on uninstall).

Arguments:

    None

Return Value:

    NDIS_STATUS_SUCCESS if we successfully register a device object.

--*/
{
    NDIS_STATUS            Status = NDIS_STATUS_SUCCESS;
    UNICODE_STRING         DeviceName;
    UNICODE_STRING         DeviceLinkUnicodeString;
    PDRIVER_DISPATCH       DispatchTable[IRP_MJ_MAXIMUM_FUNCTION+1];

    DBGPRINT(("==>PtRegisterDevice\n"));

    NdisAcquireSpinLock(&GlobalLock);

    ++MiniportCount;
    
    if (1 == MiniportCount)
    {
        ASSERT(ControlDeviceState != PS_DEVICE_STATE_CREATING);

        //
        // Another thread could be running PtDeregisterDevice on
        // behalf of another miniport instance. If so, wait for
        // it to exit.
        //
	 
        while (ControlDeviceState != PS_DEVICE_STATE_READY)
        {
            NdisReleaseSpinLock(&GlobalLock);
            NdisMSleep(1);
            NdisAcquireSpinLock(&GlobalLock);
        }

        ControlDeviceState = PS_DEVICE_STATE_CREATING;

        NdisReleaseSpinLock(&GlobalLock);

    
        NdisZeroMemory(DispatchTable, (IRP_MJ_MAXIMUM_FUNCTION+1) * sizeof(PDRIVER_DISPATCH));

        DispatchTable[IRP_MJ_CREATE] = PtDispatch;
        DispatchTable[IRP_MJ_CLEANUP] = PtDispatch;
        DispatchTable[IRP_MJ_CLOSE] = PtDispatch;
        DispatchTable[IRP_MJ_DEVICE_CONTROL] = PtDispatch;
        

        NdisInitUnicodeString(&DeviceName, NTDEVICE_STRING);
        NdisInitUnicodeString(&DeviceLinkUnicodeString, LINKNAME_STRING);

        //
        // Create a device object and register our dispatch handlers
        //
        
        Status = NdisMRegisterDevice(
                    NdisWrapperHandle, 
                    &DeviceName,
                    &DeviceLinkUnicodeString,
                    &DispatchTable[0],
                    &ControlDeviceObject,
                    &NdisDeviceHandle
                    );

        NdisAcquireSpinLock(&GlobalLock);

        ControlDeviceState = PS_DEVICE_STATE_READY;
    }

    NdisReleaseSpinLock(&GlobalLock);

    DBGPRINT(("<==PtRegisterDevice: %x\n", Status));

    return (Status);
}

NTSTATUS IoCtlProcess(ULONG ControlCode,PVOID InputBuffer,ULONG InputBufferSize,PVOID OutputBuffer,ULONG OutputBufferSize,PIO_STATUS_BLOCK IoStatus)
{
	 NTSTATUS Status = STATUS_SUCCESS;
	 ULONG_PTR Information = 0;

	switch(ControlCode)
	{
	  
	case IOCTL_STOP_D:
		g_Filter->SetState(FALSE);
		Status = STATUS_SUCCESS;
		break;

	case IOCTL_START_D:
		g_Filter->SetState(TRUE);
		Status = STATUS_SUCCESS;

		break;
   
	case IOCTL_ADD_RULE:
		{
 			PIP_RULE pRule = (PIP_RULE)InputBuffer;
 			if( g_Filter->Add(pRule))
 			{ 
 				Status = STATUS_SUCCESS; 
 			}
 			else
 				Status = STATUS_ADDRESS_ALREADY_EXISTS; 
		}
		break;

	case IOCTL_DEL_RULE:
	{
		PIP_RULE pRule = (PIP_RULE)InputBuffer;
		if (g_Filter->Del(pRule))
		{
			Status = STATUS_SUCCESS;
		}
		else
			Status = STATUS_ADDRESS_ALREADY_EXISTS;
	}
	break;

	case IOCTL_DEL_ALL_RULE:
	{
		 
		if (g_Filter->DelAll())
		{
			Status = STATUS_SUCCESS;
		}
		else
			Status = STATUS_ADDRESS_ALREADY_EXISTS;
	}
	break;

	}
	IoStatus->Information = Information;
	IoStatus->Status =		Status;
	return STATUS_SUCCESS;
}
NTSTATUS
PtDispatch(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    )
/*++
Routine Description:

    Process IRPs sent to this device.

Arguments:

    DeviceObject - pointer to a device object
    Irp      - pointer to an I/O Request Packet

Return Value:

    NTSTATUS - STATUS_SUCCESS always - change this when adding
    real code to handle ioctls.

--*/
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(DeviceObject);
    
    DBGPRINT(("==>Pt Dispatch\n"));
    irpStack = IoGetCurrentIrpStackLocation(Irp);
	
	PVOID inputBuffer             = Irp->AssociatedIrp.SystemBuffer;
	ULONG inputBufferLength       = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	PVOID outputBuffer            = Irp->AssociatedIrp.SystemBuffer;
	ULONG outputBufferLength      = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG ioControlCode           = irpStack->Parameters.DeviceIoControl.IoControlCode;
      
	
    switch (irpStack->MajorFunction)
    {
        case IRP_MJ_CREATE:
		{
			g_hProtectProcId = PsGetCurrentProcessId();
		}
            break;
            
        case IRP_MJ_CLEANUP:
            break;
            
        case IRP_MJ_CLOSE:
            break;        
            
        case IRP_MJ_DEVICE_CONTROL:
            status = IoCtlProcess(ioControlCode,inputBuffer,inputBufferLength,outputBuffer,outputBufferLength,&Irp->IoStatus);
            break;        
        default:
            break;
    }

	 
   
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    DBGPRINT(("<== Pt Dispatch\n"));

    return status;

} 


NDIS_STATUS
PtDeregisterDevice(
    VOID
    )
/*++

Routine Description:

    Deregister the ioctl interface. This is called whenever a miniport
    instance is halted. When the last miniport instance is halted, we
    request NDIS to delete the device object

Arguments:

    NdisDeviceHandle - Handle returned by NdisMRegisterDevice

Return Value:

    NDIS_STATUS_SUCCESS if everything worked ok

--*/
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    DBGPRINT(("==>PassthruDeregisterDevice\n"));

    NdisAcquireSpinLock(&GlobalLock);

    ASSERT(MiniportCount > 0);

    --MiniportCount;
    
    if (0 == MiniportCount)
    {
        //
        // All miniport instances have been halted. Deregister
        // the control device.
        //

        ASSERT(ControlDeviceState == PS_DEVICE_STATE_READY);

        //
        // Block PtRegisterDevice() while we release the control
        // device lock and deregister the device.
        // 
        ControlDeviceState = PS_DEVICE_STATE_DELETING;

        NdisReleaseSpinLock(&GlobalLock);

        if (NdisDeviceHandle != NULL)
        {
            Status = NdisMDeregisterDevice(NdisDeviceHandle);
            NdisDeviceHandle = NULL;
        }

        NdisAcquireSpinLock(&GlobalLock);
        ControlDeviceState = PS_DEVICE_STATE_READY;
    }

    NdisReleaseSpinLock(&GlobalLock);

    DBGPRINT(("<== PassthruDeregisterDevice: %x\n", Status));
    return Status;
    
}

VOID
PtUnload(
    IN PDRIVER_OBJECT        DriverObject
    )
//
// PassThru driver unload function
//
{
    UNREFERENCED_PARAMETER(DriverObject);
    
    DBGPRINT(("PtUnload: entered\n"));
	PageProtectClose();
	//修改SSDT中 NtOpenProcess 的地址，使其指向 oldNtOpenProcess
	//也就是在驱动卸载时恢复原来的地址
	KeServiceDescriptorTable->ServiceTableBase[SYSTEMCALL_INDEX(ZwOpenProcess)] = (ULONG)oldNtOpenProcess;
	PageProtectOpen();
	 
    PtUnloadProtocol();
    
    NdisIMDeregisterLayeredMiniport(DriverHandle);
    
    NdisFreeSpinLock(&GlobalLock);

    DBGPRINT(("PtUnload: done!\n"));
}

