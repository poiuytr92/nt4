/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    security.c

Abstract:

    Routines to deal with security, user accounts, etc.

    Externally exposed routines:

        SignalLsa
        CreateSamEvent
        WaitForSam

        SetAccountsDomainSid
        CreateLocalUserAccount
        CreatePdcAccount
        SetLocalUserPassword

Author:

    Ted Miller (tedm) 5-Apr-1995
    adapted from legacy\dll\security.c

Revision History:

--*/

#include "setupp.h"
#pragma hdrstop


PCWSTR SamEventName = L"\\SAM_SERVICE_STARTED";
PCWSTR LsaEventName = L"\\INSTALLATION_SECURITY_HOLD";

PCWSTR SsiAccountNamePostfix = L"$";
PCWSTR SsiSecretName = L"$MACHINE.ACC";

HANDLE SamEvent;

#define DOMAIN_NAME_MAX 33
#define PASSWORD_MAX 14

//
// Constants used for logging that are specific to this source file.
//
PCWSTR szLsaOpenPolicy              = L"LsaOpenPolicy";
PCWSTR szSetupGenerateUniqueSid     = L"SetupGenerateUniqueSid";
PCWSTR szLsaSetInformationPolicy    = L"LsaSetInformationPolicy";
PCWSTR szLsaQueryInformationPolicy  = L"LsaQueryInformationPolicy";
PCWSTR szNtSetEvent                 = L"NtSetEvent";
PCWSTR szNtCreateEvent              = L"NtCreateEvent";
PCWSTR szSamConnect                 = L"SamConnect";
PCWSTR szGetAccountsDomainName      = L"GetAccountsDomainName";
PCWSTR szSamLookupDomainInSamServer = L"SamLookupDomainInSamServer";
PCWSTR szSamOpenDomain              = L"SamOpenDomain";
PCWSTR szSamEnumerateUsersInDomain  = L"SamEnumerateUsersInDomain";
PCWSTR szSamOpenUser                = L"SamOpenUser";
PCWSTR szSamChangePasswordUser      = L"SamChangePasswordUser";
PCWSTR szSamCreateUserInDomain      = L"SamCreateUserInDomain";
PCWSTR szSamQueryInformationUser    = L"SamQueryInformationUser";
PCWSTR szSamSetInformationUser      = L"SamSetInformationUser";
PCWSTR szMyAddLsaSecretObject       = L"MyAddLsaSecretObject";


NTSTATUS
SetupGenerateUniqueSid(
    IN  DWORD  Seed,
    OUT PSID  *Sid
    );

VOID
SetupLsaInitObjectAttributes(
    IN OUT POBJECT_ATTRIBUTES           ObjectAttributes,
    IN OUT PSECURITY_QUALITY_OF_SERVICE SecurityQualityOfService
    );

BOOL
GetAccountsDomainName(
    IN  LSA_HANDLE hPolicy,         OPTIONAL
    OUT PWSTR      Name,
    IN  DWORD      NameBufferSize
    );

LSA_HANDLE
OpenLsaPolicy(
    VOID
    );

PSID
CreateSidFromSidAndRid(
    IN PSID  DomainSid,
    IN DWORD Rid
    );

VOID
GenerateRandomPassword(
    OUT PWSTR Password
    );

NTSTATUS
MyAddLsaSecretObject(
    IN PCWSTR Password
    );


BOOL
SetAccountsDomainSid(
    IN DWORD  Seed,
    IN PCWSTR DomainName
    )

/*++

Routine Description:

    Routine to set the sid of the AccountDomain.

Arguments:

    Seed - The seed is used to generate a unique Sid.  The seed should
        be generated by looking at the systemtime before and after
        a dialog and subtracting the milliseconds field.

    DomainName - supplies name to give to local domain

Return value:

    Boolean value indicating outcome.

--*/

{
    PSID                        Sid;
    PSID                        SidPrimary ;
    OBJECT_ATTRIBUTES           ObjectAttributes;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
    LSA_HANDLE                  PolicyHandle = NULL;
    POLICY_ACCOUNT_DOMAIN_INFO  PolicyAccountDomainInfo;
    PPOLICY_PRIMARY_DOMAIN_INFO PolicyPrimaryDomainInfo = NULL ;
    NTSTATUS                    Status;
    BOOL bResult;

    //
    //
    // Open the LSA Policy object to set the account domain sid.  The access
    // mask needed for this is POLICY_TRUST_ADMIN.
    //
    SetupLsaInitObjectAttributes(&ObjectAttributes,&SecurityQualityOfService);

    Status = LsaOpenPolicy(NULL,&ObjectAttributes,MAXIMUM_ALLOWED,&PolicyHandle);
    if(!NT_SUCCESS(Status)) {

        LogItem1(
            LogSevError,
            MSG_LOG_SETACCOUNTDOMAINSID,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szLsaOpenPolicy,
            Status
            );

        return(FALSE);
    }

    //
    // Initialize the domain name unicode string.
    //
    RtlInitUnicodeString(&PolicyAccountDomainInfo.DomainName,DomainName);

    //
    // Use GenerateUniqueSid to get a unique sid and store it in the
    // AccountDomain Info structure
    //
    Status = SetupGenerateUniqueSid(Seed,&Sid);
    if (!NT_SUCCESS(Status)) {
        LsaClose(PolicyHandle);

        LogItem1(
            LogSevError,
            MSG_LOG_SETACCOUNTDOMAINSID,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSetupGenerateUniqueSid,
            Status
            );

        return(FALSE);
    }
    PolicyAccountDomainInfo.DomainSid = Sid;

    //
    // Set AccountDomain information
    //
    Status = LsaSetInformationPolicy(
                 PolicyHandle,
                 PolicyAccountDomainInformation,
                 &PolicyAccountDomainInfo
                 );

    if(NT_SUCCESS(Status)) {
        bResult = TRUE;
    } else {
        bResult = FALSE;
        LogItem1(
            LogSevError,
            MSG_LOG_SETACCOUNTDOMAINSID,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szLsaSetInformationPolicy,
            Status
            );
    }

    //
    //  See if this is a DC server; if so, set the same SID into
    //  the primary domain.
    //
    if(bResult && ISDC(ProductType)) {

        Status = LsaQueryInformationPolicy(
                     PolicyHandle,
                     PolicyPrimaryDomainInformation,
                     &PolicyPrimaryDomainInfo
                     );

        if(NT_SUCCESS(Status)) {

            PolicyPrimaryDomainInfo->Sid = Sid;

            Status = LsaSetInformationPolicy(
                         PolicyHandle,
                         PolicyPrimaryDomainInformation,
                         (PVOID) PolicyPrimaryDomainInfo
                         );

            LsaFreeMemory(PolicyPrimaryDomainInfo);

            if(!NT_SUCCESS(Status)) {
                LogItem1(
                    LogSevError,
                    MSG_LOG_SETACCOUNTDOMAINSID,
                    MSG_LOG_X_RETURNED_NTSTATUS,
                    szLsaSetInformationPolicy,
                    Status
                    );
            }
        } else {
            LogItem1(
                LogSevError,
                MSG_LOG_SETACCOUNTDOMAINSID,
                MSG_LOG_X_RETURNED_NTSTATUS,
                szLsaQueryInformationPolicy,
                Status
                );
        }

        if(!NT_SUCCESS(Status)) {
            bResult = FALSE;
        }
    }

    RtlFreeSid(Sid);
    LsaClose(PolicyHandle);
    return(bResult);
}


NTSTATUS
SetupGenerateUniqueSid(
    IN  DWORD  Seed,
    OUT PSID  *Sid
    )

/*++

Routine Description:

    Generates a (hopefully) unique SID for use by Setup. Setup uses this
    SID as the Domain SID for the Account domain.

    Use RtlFreeSid() to free the SID allocated by this routine.

Arguments:

    Seed - Seed for random-number generator.  Don't use system time as
           a seed, because this routine uses the time as an additional
           seed.  Instead, use something that depends on user input.
           A great seed would be derived from the difference between the
           two timestamps seperated by user input.  A less desirable
           approach would be to sum the characters in several user
           input strings.

    Sid  - On return points to the created SID.

Return Value:

    Status code indicating outcome.

--*/
{
    NTSTATUS Status;
    LARGE_INTEGER Time;
    KERNEL_USER_TIMES KernelUserTimes;
    DWORD r1,r2,r3;
    SID_IDENTIFIER_AUTHORITY IdentifierAuthority = SECURITY_NT_AUTHORITY;

    //
    // Generate 3 pseudo-random numbers using the Seed parameter, the
    // system time, and the user-mode execution time of this process as
    // random number generator seeds.
    //
    Status = NtQuerySystemTime(&Time);
    if(!NT_SUCCESS(Status)) {
        return(Status);
    }

    Status = NtQueryInformationThread(
                 NtCurrentThread(),
                 ThreadTimes,
                 &KernelUserTimes,
                 sizeof(KernelUserTimes),
                 NULL
                 );

    if(!NT_SUCCESS(Status)) {
        return(Status);
    }

    srand(Seed);
    r1 = ((DWORD)rand() << 16) + (DWORD)rand();

    srand(Time.LowPart);
    r2 = ((DWORD)rand() << 16) + (DWORD)rand();

    srand(KernelUserTimes.UserTime.LowPart);
    r3 = ((DWORD)rand() << 16) + (DWORD)rand();

    return(RtlAllocateAndInitializeSid(&IdentifierAuthority,4,21,r1,r2,r3,0,0,0,0,Sid));
}


VOID
SetupLsaInitObjectAttributes(
    IN OUT POBJECT_ATTRIBUTES           ObjectAttributes,
    IN OUT PSECURITY_QUALITY_OF_SERVICE SecurityQualityOfService
    )

/*++

Routine Description:

    This function initializes the given Object Attributes structure, including
    Security Quality Of Service.  Memory must be allcated for both
    ObjectAttributes and Security QOS by the caller. Borrowed from
    lsa

Arguments:

    ObjectAttributes - Pointer to Object Attributes to be initialized.

    SecurityQualityOfService - Pointer to Security QOS to be initialized.

Return Value:

    None.

--*/

{
    SecurityQualityOfService->Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
    SecurityQualityOfService->ImpersonationLevel = SecurityImpersonation;
    SecurityQualityOfService->ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    SecurityQualityOfService->EffectiveOnly = FALSE;

    InitializeObjectAttributes(ObjectAttributes,NULL,0,NULL,NULL);
    //
    // The InitializeObjectAttributes macro presently stores NULL for
    // the SecurityQualityOfService field, so we must manually copy that
    // structure for now.
    //
    ObjectAttributes->SecurityQualityOfService = SecurityQualityOfService;
}


BOOL
SignalLsa(
    VOID
    )

/*++

Routine Description:

    During initial setup, the winlogon creates a special event (unsignalled)
    before it starts up Lsa.  During initialization lsa waits on this event.
    After Gui setup is done with setting the AccountDomain sid it can
    signal the event. Lsa will then continue initialization.

    The event is named INSTALLATION_SECURITY_HOLD.

Arguments:

    None.

Return value:

    Boolean value indicating outcome.

--*/

{
    UNICODE_STRING UnicodeString;
    OBJECT_ATTRIBUTES Attributes;
    NTSTATUS Status;
    HANDLE Event;
    BOOL b;

    //
    // If the following event exists, it is an indication that
    // LSA is blocked at installation time and that we need to
    // signal this event.
    //
    // Unfortunately we have to use the NT APIs to do this, because
    // all events created/accessed via the Win32 APIs will be in the
    // BaseNamedObjects directory, and LSA doesn't know to look there.
    //
    RtlInitUnicodeString(&UnicodeString,LsaEventName);
    InitializeObjectAttributes(&Attributes,&UnicodeString,0,0,NULL);

    Status = NtOpenEvent(&Event,EVENT_MODIFY_STATE,&Attributes);
    if(NT_SUCCESS(Status)) {

        Status = NtSetEvent(Event,NULL);
        if(NT_SUCCESS(Status)) {
            b = TRUE;
        } else {
            b = FALSE;
            LogItem1(
                LogSevError,
                MSG_LOG_SIGNALLSA,
                MSG_LOG_X_PARAM_RETURNED_NTSTATUS,
                szNtSetEvent,
                Status,
                LsaEventName
                );
        }
        CloseHandle(Event);
    } else {
        LogItem1(LogSevError,MSG_LOG_SIGNALLSA,MSG_LOG_NOLSAEVENT);
        b = FALSE;
    }

    return(b);
}


BOOL
CreateSamEvent(
    VOID
    )

/*++

Routine Description:

    Create an event that SAM will use to tell us when it's finished
    initializing.

Arguments:

    None.

Return value:

    Boolean value indicating outcome.

--*/

{
    UNICODE_STRING UnicodeString;
    OBJECT_ATTRIBUTES Attributes;
    NTSTATUS Status;

    //
    // Unfortunately we have to use the NT APIs to do this, because
    // all events created/accessed via the Win32 APIs will be in the
    // BaseNamedObjects directory, and SAM doesn't know to look there.
    //
    RtlInitUnicodeString(&UnicodeString,SamEventName);
    InitializeObjectAttributes(&Attributes,&UnicodeString,0,0,NULL);

    Status = NtCreateEvent(&SamEvent,SYNCHRONIZE,&Attributes,NotificationEvent,FALSE);
    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_SAMEVENT,
            MSG_LOG_X_PARAM_RETURNED_NTSTATUS,
            szNtCreateEvent,
            Status,
            SamEventName
            );
        SamEvent = NULL;
    }

    return(NT_SUCCESS(Status));
}


BOOL
WaitForSam(
    VOID
    )

/*++

Routine Description:

    Wait for SAM to finish initializing. We can tell when it's done
    because an event we created earlier (see CreateSamEvent()) will
    become signalled.

Arguments:

    None.

Return value:

    Boolean value indicating outcome.

--*/

{
    DWORD d;
    BOOL b;

    MYASSERT(SamEvent);

    b = TRUE;
    d = WaitForSingleObject(SamEvent,INFINITE);
    if(d != WAIT_OBJECT_0) {
        b = FALSE;
        LogItem1(
            LogSevError,
            MSG_LOG_WAITFORSAM,
            MSG_LOG_X_PARAM_RETURNED_WINERR,
            szWaitForSingleObject,
            GetLastError(),
            SamEventName
            );
    }
    return(b);
}


BOOL
CreateLocalUserAccount(
    IN PCWSTR UserName,
    IN PCWSTR Password,
    IN PSID*  PointerToUserSid   OPTIONAL
    )

/*++

Routine Description:

    Routine to add a local user account to the AccountDomain. This account
    is made with the password indicated.

Arguments:

    UserName - supplies name for user account

    Password - supplies initial password for user account.

    PointerToUserSid - If this argument is present, then on return it will contain the
                       pointer to the user sid. It is the responsibility of the caller
                       to free the Sid, using MyFree.

Return value:

    Boolean value indicating outcome.

--*/
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
    UNICODE_STRING UnicodeString;
    SAM_HANDLE ServerHandle;
    SAM_HANDLE DomainHandle;
    SAM_HANDLE UserHandle;
    SAM_HANDLE AliasHandle;
    SAM_HANDLE BuiltinDomainHandle;
    WCHAR AccountsDomainName[DOMAIN_NAME_MAX];
    NTSTATUS Status;
    PSID DomainId;
    PSID BuiltinDomainId;
    PSID UserSid;
    ULONG User_RID;
    PUSER_CONTROL_INFORMATION UserControlInfo;
    BOOL b;
    USER_SET_PASSWORD_INFORMATION UserPasswordInfo;

    //
    // Assume failure.
    //
    b = FALSE;

    //
    // Use SamConnect to connect to the local domain ("") and get a handle
    // to the local sam server.
    //
    SetupLsaInitObjectAttributes(&ObjectAttributes,&SecurityQualityOfService);
    RtlInitUnicodeString(&UnicodeString,L"");
    Status = SamConnect(
                 &UnicodeString,
                 &ServerHandle,
                 SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN,
                 &ObjectAttributes
                 );

    if(!NT_SUCCESS(Status)) {
        goto err0;
    }

    //
    //  Use the LSA to retrieve the name of the Accounts domain.
    //
    if(!GetAccountsDomainName(NULL,AccountsDomainName,DOMAIN_NAME_MAX)) {
        goto err1;
    }

    //
    // Open the AccountDomain.  First find the Sid for this
    // in the Sam and then open the domain using this sid
    //
    RtlInitUnicodeString(&UnicodeString,AccountsDomainName);
    Status = SamLookupDomainInSamServer(ServerHandle,&UnicodeString,&DomainId);
    if(!NT_SUCCESS(Status)) {
        goto err1;
    }

    Status = SamOpenDomain(
                 ServerHandle,
                 DOMAIN_READ | DOMAIN_CREATE_USER | DOMAIN_READ_PASSWORD_PARAMETERS,
                 DomainId,
                 &DomainHandle
                 );

    if(!NT_SUCCESS(Status)) {
        goto err2;
    }

    //
    // Use SamCreateUserInDomain to create a new user with the username
    // specified.  This user account is created disabled with the
    // password not required.
    //
    RtlInitUnicodeString(&UnicodeString,UserName);
    Status = SamCreateUserInDomain(
                 DomainHandle,
                 &UnicodeString,
                 USER_READ_ACCOUNT | USER_WRITE_ACCOUNT | USER_FORCE_PASSWORD_CHANGE,
                 &UserHandle,
                 &User_RID
                 );

    if(!NT_SUCCESS(Status)) {
        goto err3;
    }

    //
    // Query all the default control information about the user added
    //
    Status = SamQueryInformationUser(UserHandle,UserControlInformation,&UserControlInfo);
    if(!NT_SUCCESS(Status)) {
        goto err4;
    }

    //
    // If the password is a Null password, make sure the
    // password_not required bit is set before the null
    // password is set.
    //
    if(!Password[0]) {
        UserControlInfo->UserAccountControl |= USER_PASSWORD_NOT_REQUIRED;
        Status = SamSetInformationUser(UserHandle,UserControlInformation,UserControlInfo);
        if(!NT_SUCCESS(Status)) {
            goto err5;
        }
    }

    //
    // Set the password ( NULL or non NULL )
    //
    RtlInitUnicodeString(&UserPasswordInfo.Password,Password);
    UserPasswordInfo.PasswordExpired = FALSE;
    Status = SamSetInformationUser(UserHandle,UserSetPasswordInformation,&UserPasswordInfo);
    if(!NT_SUCCESS(Status)) {
        goto err5;
    }


    //
    // Set the information bits - User Password not required is cleared
    // The normal account bit is enabled and the account disabled bit
    // is also reset
    //
    UserControlInfo->UserAccountControl &= ~USER_PASSWORD_NOT_REQUIRED;
    UserControlInfo->UserAccountControl &= ~USER_ACCOUNT_DISABLED;
    UserControlInfo->UserAccountControl |=  USER_NORMAL_ACCOUNT;
    Status = SamSetInformationUser(UserHandle,UserControlInformation,UserControlInfo);
    if(!NT_SUCCESS(Status)) {
        goto err5;
    }

    //
    //  If this is a non-standlone server we're done.
    //
    if(ISDC(ProductType)) {
        b = TRUE;
        goto err5;
    }

    //
    // Finally add this to the administrators alias in the BuiltIn Domain
    //
    RtlInitUnicodeString(&UnicodeString,L"Builtin");
    Status = SamLookupDomainInSamServer(ServerHandle,&UnicodeString,&BuiltinDomainId);
    if(!NT_SUCCESS(Status)) {
        goto err5;
    }

    Status = SamOpenDomain(
                 ServerHandle,
                 DOMAIN_READ | DOMAIN_ADMINISTER_SERVER | DOMAIN_EXECUTE,
                 BuiltinDomainId,
                 &BuiltinDomainHandle
                 );

    if(!NT_SUCCESS(Status)) {
        goto err6;
    }

    UserSid = CreateSidFromSidAndRid(DomainId,User_RID);
    if(!UserSid) {
        goto err7;
    }

    Status = SamOpenAlias(BuiltinDomainHandle,ALIAS_ADD_MEMBER,DOMAIN_ALIAS_RID_ADMINS,&AliasHandle);
    if(!NT_SUCCESS(Status)) {
        goto err8;
    }

    Status = SamAddMemberToAlias(AliasHandle,UserSid);
    if(!NT_SUCCESS(Status)) {
        goto err9;
    }

    b = TRUE;

err9:
    SamCloseHandle(AliasHandle);
err8:
    if(b && (PointerToUserSid != NULL )) {
        *PointerToUserSid = UserSid;
    } else {
        MyFree(UserSid);
    }
err7:
    SamCloseHandle(BuiltinDomainHandle);
err6:
    SamFreeMemory(BuiltinDomainId);
err5:
    SamFreeMemory(UserControlInfo);
err4:
    SamCloseHandle(UserHandle);
err3:
    SamCloseHandle(DomainHandle);
err2:
    SamFreeMemory(DomainId);
err1:
    SamCloseHandle(ServerHandle);
err0:
    return(b);
}


BOOL
GetAccountsDomainName(
    IN  LSA_HANDLE PolicyHandle,    OPTIONAL
    OUT PWSTR      Name,
    IN  DWORD      NameBufferSize
    )
{
    POLICY_ACCOUNT_DOMAIN_INFO *pPadi;
    NTSTATUS Status ;
    BOOL PolicyOpened;

    PolicyOpened = FALSE;
    if(PolicyHandle == NULL) {
        if((PolicyHandle = OpenLsaPolicy()) == NULL) {
            return(FALSE);
        }

        PolicyOpened = TRUE;
    }

    Status = LsaQueryInformationPolicy(PolicyHandle,PolicyAccountDomainInformation,&pPadi);
    if(NT_SUCCESS(Status)) {
         if(NameBufferSize <= (pPadi->DomainName.Length/sizeof(WCHAR))) {
             Status = STATUS_BUFFER_TOO_SMALL;
         } else {
             wcsncpy(Name,pPadi->DomainName.Buffer,pPadi->DomainName.Length/sizeof(WCHAR));
             Name[pPadi->DomainName.Length/sizeof(WCHAR)] = 0;
         }
         LsaFreeMemory(pPadi);
    }

    if(PolicyOpened) {
        LsaClose(PolicyHandle);
    }

    return(NT_SUCCESS(Status));
}


LSA_HANDLE
OpenLsaPolicy(
    VOID
    )
{
    OBJECT_ATTRIBUTES ObjectAttributes;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
    LSA_HANDLE PolicyHandle;
    NTSTATUS Status;

    PolicyHandle = NULL;
    SetupLsaInitObjectAttributes(&ObjectAttributes,&SecurityQualityOfService);
    Status = LsaOpenPolicy(NULL,&ObjectAttributes,GENERIC_EXECUTE,&PolicyHandle);

    return(NT_SUCCESS(Status) ? PolicyHandle : NULL);
}


PSID
CreateSidFromSidAndRid(
    IN PSID  DomainSid,
    IN DWORD Rid
    )

/*++

Routine Description:

    This function creates a domain account sid given a domain sid and
    the relative id of the account within the domain.

Arguments:

    DomainSid - supplies sid for domain of account

    Rid - supplies relative id of account

Return Value:

    Pointer to Sid, or NULL on failure.
    The returned Sid must be freed with MyFree.

--*/
{

    NTSTATUS Status;
    PSID AccountSid;
    UCHAR AccountSubAuthorityCount;
    ULONG AccountSidLength;
    PULONG RidLocation;

    AccountSubAuthorityCount = *RtlSubAuthorityCountSid(DomainSid) + (UCHAR)1;
    AccountSidLength = RtlLengthRequiredSid(AccountSubAuthorityCount);

    if(AccountSid = (PSID)MyMalloc(AccountSidLength)) {
        //
        // Copy the domain sid into the first part of the account sid
        //
        Status = RtlCopySid(AccountSidLength, AccountSid, DomainSid);

        //
        // Increment the account sid sub-authority count
        //
        *RtlSubAuthorityCountSid(AccountSid) = AccountSubAuthorityCount;

        //
        // Add the rid as the final sub-authority
        //
        RidLocation = RtlSubAuthoritySid(AccountSid, AccountSubAuthorityCount - 1);
        *RidLocation = Rid;
    }

    return(AccountSid);
}


BOOL
SetLocalUserPassword(
    IN PCWSTR AccountName,
    IN PCWSTR OldPassword,
    IN PCWSTR NewPassword
    )

/*++

Routine Description:

    Change the password for the local user account.

Arguments:

Return value:

    Boolean value indicating outcome.

--*/

{
    NTSTATUS Status;
    BOOL b;
    UNICODE_STRING UnicodeString;
    UNICODE_STRING OtherUnicodeString;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
    OBJECT_ATTRIBUTES ObjectAttributes;
    WCHAR AccountsDomainName[DOMAIN_NAME_MAX];
    SAM_HANDLE ServerHandle;
    SAM_HANDLE DomainHandle;
    SAM_HANDLE UserHandle;
    PSID DomainId;
    BOOL UserFound;
    SAM_ENUMERATE_HANDLE EnumerationContext;
    SAM_RID_ENUMERATION *SamRidEnumeration;
    UINT i;
    UINT CountOfEntries;
    ULONG UserRid;

    b = FALSE;

    //
    // Use SamConnect to connect to the local domain ("") and get a handle
    // to the local sam server
    //
    SetupLsaInitObjectAttributes(&ObjectAttributes,&SecurityQualityOfService);
    RtlInitUnicodeString(&UnicodeString,L"");
    Status = SamConnect(
                 &UnicodeString,
                 &ServerHandle,
                 SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN,
                 &ObjectAttributes
                 );

    if(!NT_SUCCESS(Status)) {
        LogItem2(
            LogSevWarning,
            MSG_LOG_CHANGING_PW_FAIL,
            AccountName,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamConnect,
            Status
            );
        goto err0;
    }

    //
    //  Use the LSA to retrieve the name of the Accounts domain.
    //
    if(!GetAccountsDomainName(NULL,AccountsDomainName,DOMAIN_NAME_MAX)) {
        LogItem2(
            LogSevWarning,
            MSG_LOG_CHANGING_PW_FAIL,
            AccountName,
            MSG_LOG_X_RETURNED_STRING,
            szGetAccountsDomainName,
            szFALSE
            );
        goto err1;
    }

    //
    // Open the AccountDomain.  First find the Sid for this
    // in the Sam and then open the domain using this sid.
    //
    RtlInitUnicodeString(&UnicodeString,AccountsDomainName);
    Status = SamLookupDomainInSamServer(ServerHandle,&UnicodeString,&DomainId);
    if(!NT_SUCCESS(Status)) {
        LogItem2(
            LogSevWarning,
            MSG_LOG_CHANGING_PW_FAIL,
            AccountName,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamLookupDomainInSamServer,
            Status
            );
        goto err1;
    }

    Status = SamOpenDomain(
                 ServerHandle,
                 DOMAIN_READ | DOMAIN_LIST_ACCOUNTS | DOMAIN_LOOKUP | DOMAIN_READ_PASSWORD_PARAMETERS,
                 DomainId,
                 &DomainHandle
                 );

    if(!NT_SUCCESS(Status)) {
        LogItem2(
            LogSevWarning,
            MSG_LOG_CHANGING_PW_FAIL,
            AccountName,
            MSG_LOG_X_PARAM_RETURNED_NTSTATUS,
            szSamOpenDomain,
            Status,
            AccountsDomainName
            );
        goto err2;
    }

    //
    // Find the account name in this domain - and extract the rid.
    //
    UserFound = FALSE;
    EnumerationContext = 0;
    RtlInitUnicodeString(&UnicodeString,AccountName);
    do {
        Status = SamEnumerateUsersInDomain(
                     DomainHandle,
                     &EnumerationContext,
                     0L,
                     &SamRidEnumeration,
                     0L,
                     &CountOfEntries
                     );

        if(!NT_SUCCESS(Status) && (Status != STATUS_MORE_ENTRIES)) {
            LogItem2(
                LogSevWarning,
                MSG_LOG_CHANGING_PW_FAIL,
                AccountName,
                MSG_LOG_X_RETURNED_NTSTATUS,
                szSamEnumerateUsersInDomain,
                Status
                );
            goto err3;
        }

        //
        // go through the the SamRidEnumeration buffer for count entries.
        //
        for(i = 0; (i<CountOfEntries) && !UserFound; i++ ) {
            if(RtlEqualUnicodeString(&UnicodeString,&SamRidEnumeration[i].Name,TRUE)) {
                UserRid = SamRidEnumeration[i].RelativeId;
                UserFound = TRUE;
            }
        }

        SamFreeMemory(SamRidEnumeration);

    } while((Status == STATUS_MORE_ENTRIES) && !UserFound);

    if(!UserFound) {
        LogItem2(LogSevWarning,MSG_LOG_CHANGING_PW_FAIL,AccountName,MSG_LOG_USERNOTFOUND);
        goto err3;
    }

    //
    // Open the user
    //
    Status = SamOpenUser(
                 DomainHandle,
                 USER_READ_ACCOUNT | USER_WRITE_ACCOUNT | USER_CHANGE_PASSWORD | USER_FORCE_PASSWORD_CHANGE,
                 UserRid,
                 &UserHandle
                 );

    if(!NT_SUCCESS(Status)) {
        LogItem2(
            LogSevWarning,
            MSG_LOG_CHANGING_PW_FAIL,
            AccountName,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamOpenUser,
            Status
            );
        goto err3;
    }

    //
    // Use SAM API to change the password for this Account.
    //
    RtlInitUnicodeString(&UnicodeString,OldPassword);
    RtlInitUnicodeString(&OtherUnicodeString,NewPassword);
    Status = SamChangePasswordUser(UserHandle,&UnicodeString,&OtherUnicodeString);
    if(!NT_SUCCESS(Status)) {
        LogItem2(
            LogSevWarning,
            MSG_LOG_CHANGING_PW_FAIL,
            AccountName,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamChangePasswordUser,
            Status
            );
        goto err4;
    }

    b = TRUE;

err4:
    SamCloseHandle(UserHandle);
err3:
    SamCloseHandle(DomainHandle);
err2:
    SamFreeMemory(DomainId);
err1:
    SamCloseHandle(ServerHandle);
err0:
    return(b);
}


BOOL
CreatePdcAccount(
    IN PCWSTR MachineName
    )

/*++

Routine Description:

    Add an account to the Accounts Domain for this machine, which is
    the PDC of a new domain.  The account is created with a random
    password.

Arguments:

    None.

Return value:

    Boolean value indicating outcome.

--*/

{
    UNICODE_STRING UnicodeString;
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    SECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
    USER_SET_PASSWORD_INFORMATION UserPasswordInfo;
    PUSER_CONTROL_INFORMATION UserControlInfo;
    SAM_HANDLE ServerHandle;
    SAM_HANDLE DomainHandle;
    SAM_HANDLE UserHandle;
    PSID DomainId;
    ULONG User_RID;
    BOOL b;
    WCHAR MachineAccountName[DOMAIN_NAME_MAX];
    WCHAR AccountsDomainName[DOMAIN_NAME_MAX];
    WCHAR GeneratedPassword[PASSWORD_MAX+1];

    //
    // Use SamConnect to connect to the local domain ("") and get a handle
    // to the local sam server
    //
    SetupLsaInitObjectAttributes(&ObjectAttributes,&SecurityQualityOfService);
    RtlInitUnicodeString(&UnicodeString,L"");
    Status = SamConnect(
                 &UnicodeString,
                 &ServerHandle,
                 SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN,
                 &ObjectAttributes
                 );

    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamConnect,
            Status
            );
        goto err0;
    }

    //
    // Use the LSA to retrieve the name of the Accounts domain.
    //
    if(!GetAccountsDomainName(NULL,AccountsDomainName,DOMAIN_NAME_MAX)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_RETURNED_STRING,
            szGetAccountsDomainName,
            szFALSE
            );
        goto err1;
    }

    //
    // Open the AccountDomain.  First find the Sid for this
    // in the Sam and then open the domain using this sid
    //
    RtlInitUnicodeString(&UnicodeString,AccountsDomainName);
    Status = SamLookupDomainInSamServer(ServerHandle,&UnicodeString,&DomainId);
    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_PARAM_RETURNED_NTSTATUS,
            szSamLookupDomainInSamServer,
            Status,
            AccountsDomainName
            );
        goto err1;
    }

    Status = SamOpenDomain(
                 ServerHandle,
                 DOMAIN_READ | DOMAIN_CREATE_USER | DOMAIN_READ_PASSWORD_PARAMETERS,
                 DomainId,
                 &DomainHandle
                 );

    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamOpenDomain,
            Status
            );
        goto err2;
    }

    //
    // Use SamCreateUserInDomain to create a new user with the username
    // specified. This user account is created disabled with the
    // password not required.
    // Create the machine account name from the machine name.
    //
    lstrcpy(MachineAccountName,MachineName);
    lstrcat(MachineAccountName,SsiAccountNamePostfix);
    RtlInitUnicodeString(&UnicodeString,MachineAccountName);

    Status = SamCreateUserInDomain(
                 DomainHandle,
                 &UnicodeString,
                 USER_READ_ACCOUNT | USER_WRITE_ACCOUNT | USER_FORCE_PASSWORD_CHANGE,
                 &UserHandle,
                 &User_RID
                 );

    if (!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_PARAM_RETURNED_NTSTATUS,
            szSamCreateUserInDomain,
            Status,
            MachineAccountName
            );
        goto err3;
    }

    //
    // Query all the default control information about the user added.
    //
    Status = SamQueryInformationUser(UserHandle,UserControlInformation,&UserControlInfo);
    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamQueryInformationUser,
            Status
            );
        goto err4;
    }

    //
    //  Generate a random password for the machine account.
    //
    GenerateRandomPassword(GeneratedPassword);
    RtlInitUnicodeString(&UserPasswordInfo.Password,GeneratedPassword);
    UserPasswordInfo.PasswordExpired = FALSE;

    Status = SamSetInformationUser(UserHandle,UserSetPasswordInformation,&UserPasswordInfo);
    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamSetInformationUser,
            Status
            );
        goto err5;
    }

    //
    // Set the information bits
    //
    UserControlInfo->UserAccountControl &= ~USER_PASSWORD_NOT_REQUIRED;
    UserControlInfo->UserAccountControl &= ~USER_ACCOUNT_DISABLED;
    UserControlInfo->UserAccountControl &= ~USER_ACCOUNT_TYPE_MASK;
    UserControlInfo->UserAccountControl |=  USER_SERVER_TRUST_ACCOUNT;

    Status = SamSetInformationUser(UserHandle,UserControlInformation,UserControlInfo);
    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szSamSetInformationUser,
            Status
            );
        goto err5;
    }

    Status = MyAddLsaSecretObject(GeneratedPassword);
    if(!NT_SUCCESS(Status)) {
        LogItem1(
            LogSevError,
            MSG_LOG_ADD_PDC_ACCT_FAIL,
            MSG_LOG_X_RETURNED_NTSTATUS,
            szMyAddLsaSecretObject,
            Status
            );
        goto err5;
    }

    b = TRUE;

err5:
    SamFreeMemory(UserControlInfo);
err4:
    SamCloseHandle(UserHandle);
err3:
    SamCloseHandle(DomainHandle);
err2:
    SamFreeMemory(DomainId);
err1:
    SamCloseHandle(ServerHandle);
err0:
    return(b);
}


VOID
GenerateRandomPassword(
    OUT PWSTR Password
    )
{
    static DWORD Seed = 98725757;
    static PCWSTR UsableChars = L"ABCDEFGHIJKLMOPQRSTUVWYZabcdefghijklmopqrstuvwyz0123456789";

    UINT UsableCount;
    UINT i;

    UsableCount = lstrlen(UsableChars);
    Seed ^= GetCurrentTime();

    for(i=0; i<PASSWORD_MAX; i++) {
        Password[i] = UsableChars[RtlRandom(&Seed) % UsableCount];
    }

    Password[i] = 0;
}


NTSTATUS
MyAddLsaSecretObject(
    IN PCWSTR Password
    )

/*++

Routine Description:

    Create the Secret Object necessary to support a machine account
    on an NT domain.

Arguments:

    Password - supplies password to machine account

Return value:

    NT Status code indicating outcome.

--*/
{
    UNICODE_STRING SecretName;
    UNICODE_STRING UnicodePassword;
    NTSTATUS Status;
    LSA_HANDLE LsaHandle;
    LSA_HANDLE SecretHandle;
    OBJECT_ATTRIBUTES ObjAttr;

    RtlInitUnicodeString(&SecretName,SsiSecretName) ;
    RtlInitUnicodeString(&UnicodePassword,Password);

    InitializeObjectAttributes(&ObjAttr,NULL,0,NULL,NULL);

    Status = LsaOpenPolicy(NULL,&ObjAttr,MAXIMUM_ALLOWED,&LsaHandle);
    if(NT_SUCCESS(Status)) {

        Status = LsaCreateSecret(LsaHandle,&SecretName,SECRET_ALL_ACCESS,&SecretHandle);
        if(NT_SUCCESS(Status)) {

            Status = LsaSetSecret(SecretHandle,&UnicodePassword,&UnicodePassword);
            LsaClose(SecretHandle);
        }

        LsaClose(LsaHandle);
    }

    return(Status);
}


BOOL
AdjustPrivilege(
    IN PCWSTR   Privilege,
    IN BOOL     Enable
    )
/*++

Routine Description:

    This routine enables or disable a priviliege of the current process.

Arguments:

    Privilege - String with the name of the privilege to be adjusted.

    Enable - TRUE if the privilege is to be enabled.
             FALSE if the privilege is to be disabled.

Return Value:

    Returns TRUE if the privilege could be adjusted.
    Returns FALSE, otherwise.


--*/
{
    HANDLE              TokenHandle;
    LUID_AND_ATTRIBUTES LuidAndAttributes;

    TOKEN_PRIVILEGES    TokenPrivileges;


    if( !OpenProcessToken( GetCurrentProcess(),
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                           &TokenHandle ) ) {
        DbgPrint( "SYSSETUP: OpenProcessToken() failed. Error = %d \n", GetLastError() );
        return( FALSE );
    }


    if( !LookupPrivilegeValue( NULL,
                               Privilege,
                               &( LuidAndAttributes.Luid ) ) ) {
        DbgPrint( "SYSSETUP: LookupPrivilegeValue failed, Error = %d \n", GetLastError() );
        CloseHandle( TokenHandle );
        return( FALSE );
    }

    if( Enable ) {
        LuidAndAttributes.Attributes |= SE_PRIVILEGE_ENABLED;
    } else {
        LuidAndAttributes.Attributes &= ~SE_PRIVILEGE_ENABLED;
    }

    TokenPrivileges.PrivilegeCount = 1;
    TokenPrivileges.Privileges[0] = LuidAndAttributes;

    if( !AdjustTokenPrivileges( TokenHandle,
                                FALSE,
                                &TokenPrivileges,
                                0,
                                NULL,
                                NULL ) ) {
        DbgPrint( "SYSSETUP: AdjustTokenPrivileges failed, Error = %d \n", GetLastError() );
        CloseHandle( TokenHandle );
        return( FALSE );
    }

    CloseHandle( TokenHandle );
    return( TRUE );
}
