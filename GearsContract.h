#pragma once
#include <maf/messaging/client-server/CSContractDefinesBegin.mc.h>

#define SERVER_DOMAIN_NAME "com.opswat.agent"
#define SERVICE_ID_SCANNING_SERVICE 1


function(ScanningStatus)
        has_result((int, percentage))
        request_noparams()
endfunc()

#include <maf/messaging/client-server/CSContractDefinesEnd.mc.h>
