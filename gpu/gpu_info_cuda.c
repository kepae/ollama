#ifndef __APPLE__  // TODO - maybe consider nvidia support on intel macs?

#include "gpu_info_cuda.h"

#include <string.h>

#ifndef _WIN32
const char *cuda_lib_paths[] = {
    "libnvidia-ml.so",
    "/usr/local/cuda/lib64/libnvidia-ml.so",
    "/usr/lib/x86_64-linux-gnu/nvidia/current/libnvidia-ml.so",
    "/usr/lib/wsl/lib/libnvidia-ml.so.1",  // TODO Maybe glob?
    NULL,
};
#else
const char *cuda_lib_paths[] = {
    "nvml.dll",
    "",
    NULL,
};
#endif

void cuda_init(cuda_init_resp_t *resp) {
  nvmlReturn_t ret;
  resp->err = NULL;
  const int buflen = 256;
  char buf[buflen + 1];
  int i;

  struct lookup {
    char *s;
    void **p;
  } l[4] = {
      {"nvmlInit_v2", (void *)&resp->ch.initFn},
      {"nvmlShutdown", (void *)&resp->ch.shutdownFn},
      {"nvmlDeviceGetHandleByIndex", (void *)&resp->ch.getHandle},
      {"nvmlDeviceGetMemoryInfo", (void *)&resp->ch.getMemInfo},
  };

  for (i = 0; cuda_lib_paths[i] != NULL && resp->ch.handle == NULL; i++) {
    resp->ch.handle = LOAD_LIBRARY(cuda_lib_paths[i], RTLD_LAZY);
  }
  if (!resp->ch.handle) {
    // TODO improve error message, as the LOAD_ERR will have typically have the
    // final path that was checked which might be confusing.
    char *msg = LOAD_ERR();
    snprintf(buf, buflen,
             "Unable to load %s library to query for Nvidia GPUs: %s",
             cuda_lib_paths[0], msg);
    free(msg);
    resp->err = strdup(buf);
    return;
  }

  for (i = 0; i < 4; i++) {  // TODO - fix this to use a null terminated list
    *l[i].p = LOAD_SYMBOL(resp->ch.handle, l[i].s);
    if (!l[i].p) {
      UNLOAD_LIBRARY(resp->ch.handle);
      resp->ch.handle = NULL;
      char *msg = LOAD_ERR();
      snprintf(buf, buflen, "symbol lookup for %s failed: %s", l[i].s,
               msg);
      free(msg);
      resp->err = strdup(buf);
      return;
    }
  }

  ret = (*resp->ch.initFn)();
  if (ret != NVML_SUCCESS) {
    snprintf(buf, buflen, "nvml vram init failure: %d", ret);
    resp->err = strdup(buf);
  }

  return;
}

void cuda_check_vram(cuda_handle_t h, mem_info_t *resp) {
  resp->err = NULL;
  nvmlDevice_t device;
  nvmlMemory_t memInfo = {0};
  nvmlReturn_t ret;
  const int buflen = 256;
  char buf[buflen + 1];
  int i;

  if (h.handle == NULL) {
    resp->err = strdup("nvml handle sn't initialized");
    return;
  }

  // TODO - handle multiple GPUs
  ret = (*h.getHandle)(0, &device);
  if (ret != NVML_SUCCESS) {
    snprintf(buf, buflen, "unable to get device handle: %d", ret);
    resp->err = strdup(buf);
    return;
  }

  ret = (*h.getMemInfo)(device, &memInfo);
  if (ret != NVML_SUCCESS) {
    snprintf(buf, buflen, "device memory info lookup failure: %d", ret);
    resp->err = strdup(buf);
    return;
  }
  resp->total = memInfo.total;
  resp->free = memInfo.free;
  return;
}
#endif  // __APPLE__