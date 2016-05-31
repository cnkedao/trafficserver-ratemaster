#include <string.h>
#include <ts/ts.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <pthread.h>
#include "rate-limiter.h"
#include "configuration.h"

#define INT64_MAX 0x7fffffffffffffffLL

using namespace std;
using namespace ATS_RL;
using namespace ATS;
RateLimiter * rate_limiter = new RateLimiter();
static int enable_limit = 0;

typedef struct
{
  TSVIO output_vio;
  TSIOBuffer output_buffer;
  TSIOBufferReader output_reader;
  TSHttpTxn txnp;
  bool ready;
  LimiterState *state;
} TransformData;


static TransformData *
my_data_alloc()
{
  TransformData *data;

  data = (TransformData *) TSmalloc(sizeof(TransformData));
  data->output_vio = NULL;
  data->output_buffer = NULL;
  data->output_reader = NULL;
  data->txnp = NULL;
  data->state = NULL;
  data->ready = false;
  return data;
}

static void
my_data_destroy(TransformData * data)
{
  TSReleaseAssert(data);
  if (data) {
    if (data->output_buffer){
	  TSIOBufferReaderFree(data->output_reader);
      TSIOBufferDestroy(data->output_buffer);
	}
	
	if(data->state){
	   delete data->state;
	   data->state = NULL;
	}
    TSfree(data);
	data = NULL;
  }
}

static void
handle_rate_limiting_transform(TSCont contp)
{
  TSVConn output_conn;
  TSIOBuffer buf_test;
  TSVIO input_vio;
  TransformData *data;
  int64_t towrite;
  int64_t avail;
  
  output_conn = TSTransformOutputVConnGet(contp);
  input_vio = TSVConnWriteVIOGet(contp);

  data = (TransformData *)TSContDataGet(contp);
  if (!data->ready) {
    data->output_buffer = TSIOBufferCreate();
    data->output_reader = TSIOBufferReaderAlloc(data->output_buffer);
    data->output_vio = TSVConnWrite(output_conn, contp, data->output_reader, TSVIONBytesGet(input_vio));
    data->ready=true;
  }

  buf_test = TSVIOBufferGet(input_vio);

  if (!buf_test) {
    TSVIONBytesSet(data->output_vio, TSVIONDoneGet(input_vio));
    TSVIOReenable(data->output_vio);
    return;
  }

  towrite = TSVIONTodoGet(input_vio);

  if (towrite > 0) {
    avail = TSIOBufferReaderAvail(TSVIOReaderGet(input_vio));

    if (towrite > avail) 
      towrite = avail;

    if (towrite > 0) {
	
	   int64_t rl_max = rate_limiter->GetMaxUnits(towrite,data->state);
	   //if (rl_max <= towrite)
	   towrite = rl_max;
	   
	   if (towrite) {
		  TSIOBufferCopy(TSVIOBufferGet(data->output_vio), TSVIOReaderGet(input_vio), towrite, 0);
		  TSIOBufferReaderConsume(TSVIOReaderGet(input_vio), towrite);
		  TSVIONDoneSet(input_vio, TSVIONDoneGet(input_vio) + towrite);
	   } else {
		  //dbg("towrite == 0, schedule a pulse");
		  TSContSchedule(contp, 100, TS_THREAD_POOL_DEFAULT);
		  return;
	   }
    }
  }
  
  if (TSVIONTodoGet(input_vio) > 0) {
    if (towrite > 0) {
      TSVIOReenable(data->output_vio);
      TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_READY, input_vio);
    }
  } else {
    TSVIONBytesSet(data->output_vio, TSVIONDoneGet(input_vio));
    TSVIOReenable(data->output_vio);
    TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_COMPLETE, input_vio);
  }
  
}

static int
rate_limiting_transform(TSCont contp, TSEvent event, void *edata)
{
  if (TSVConnClosedGet(contp)) {
    my_data_destroy((TransformData *)TSContDataGet(contp));
    TSContDestroy(contp);
    return 0;
  } else {
    switch (event) {
    case TS_EVENT_ERROR:
      {
        TSVIO input_vio = TSVConnWriteVIOGet(contp);
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_ERROR, input_vio);
      }
      break;
    case TS_EVENT_VCONN_WRITE_COMPLETE:
      TSVConnShutdown(TSTransformOutputVConnGet(contp), 0, 1);
      break;
    case TS_EVENT_VCONN_WRITE_READY:
    default:
		handle_rate_limiting_transform(contp);
      break;
    }
  }

  return 0;
}

static void
transform_add(TSHttpTxn txnp)
{
  TSMBuffer bufp;
  TSMLoc hdr_loc;

  if (TSHttpTxnServerRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
    return ;
  }
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  TSVConn contp = TSTransformCreate(rate_limiting_transform, txnp);
  TransformData * data = my_data_alloc();
  data->state = rate_limiter->Register();
  data->txnp = txnp;
  TSContDataSet(contp, data);
  //TSVConnActiveTimeoutCancel(contp);
  TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, contp);
}


static int
transformable(TSHttpTxn txnp)
{
  TSMBuffer bufp;
  TSMLoc hdr_loc;

  if (TSHttpTxnServerRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
    return 0;
  }
  TSHttpTxnServerRespBodyBytesSet(txnp, TSHttpTxnClientRespBodyBytesGet(txnp));
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
  return 1;
}

static int
txn_handle(TSCont contp, TSEvent event, void *edata)
{
   TSHttpTxn txnp = (TSHttpTxn) edata;
   switch (event) {
	  case TS_EVENT_HTTP_READ_RESPONSE_HDR:
           transform_add(txnp);
	  break;
	  case TS_EVENT_HTTP_TXN_CLOSE:
		   transformable(txnp);
	  break;
	  case TS_EVENT_ERROR:
	  {
	     TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
		 return 0;
	  }
	  break;
	  default:
	  break;
  }
  TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
  return 1;
}

static void
read_configuration() {
   Configuration * conf = new Configuration();
   conf->Parse("/sysconfig/limitconfig.config");
   std::map<const char *,uint64_t>::iterator it;
   uint64_t ssize = 0;
   int i;
   char key[100];
   it = conf->limitconf.find("limit_on");
   if (it == conf->limitconf.end()) {
     enable_limit = 0;
     goto done;
   }
   ssize = it->second;
   //TSDebug(TAG,"%ld",ssize);
   if(ssize == 0){
	  enable_limit = 0;
      goto done;
   }
   for(i=0;i<24;i++){
	  memset(key,'\0',sizeof(key));
	  sprintf(key,"%d%s",i,"hour");
	  it = conf->limitconf.find(key);
	  if (it != conf->limitconf.end()) {
		 ssize = it->second;
		 rate_limiter->AddCounter(ssize, 1000);
	  }else{
	     rate_limiter->AddCounter(1048576, 1000);
	  }
   }
   enable_limit = 1;
done:
   delete conf;
   conf = NULL;
}

void
TSPluginInit(int argc, const char *argv[])
{
  /*
  TSPluginRegistrationInfo info;
   
  info.plugin_name = (char*)"stateam";
  info.vendor_name = (char*)"stateam";
  info.support_email = (char*)"stateam@MyCompany.com";

  if (TSPluginRegister(TS_SDK_VERSION_3_0, &info) != TS_SUCCESS) {
	return;
  }*/
  read_configuration();
  if(enable_limit == 0){
      return;
  }
  TSCont txn_contp = TSContCreate(txn_handle, TSMutexCreate());
  TSHttpHookAdd(TS_HTTP_READ_RESPONSE_HDR_HOOK, txn_contp);
  TSHttpHookAdd(TS_HTTP_TXN_CLOSE_HOOK, txn_contp);

}
